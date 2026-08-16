// lib/acr/backends/classic/classic_kernels.cpp — 经典实验内核注册实现
//
// CPU launcher：纯 host 实现（正确性基准）。
// CUDA launcher：通过 bridge::api（TLS 句柄）启动真实 GPU kernel；
// 桥接不可用时抛异常 → executor 如实报告 Failed（不伪装 GPU 执行）。
#include "classic_kernels.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "../cuda/bridge/cuda_bridge_api.hpp"

#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace astro::compute::classic {

namespace {

// ==================== CPU launchers ====================
void cpu_copy_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!yb || !xb) throw std::runtime_error("copy: missing buffers");
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        y[i] = x[i];
    }
}

void cpu_axpy_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!yb || !xb) throw std::runtime_error("axpy: missing buffers");
    auto a = read_scalar<float>(inv.scalars, 0);
    if (!a) throw std::runtime_error("axpy: missing alpha");
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        y[i] = *a * x[i] + y[i];
    }
}

void cpu_reduce_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* xb = inv.buffers.find(0);
    const BufferBinding* pb = inv.buffers.find(1);
    if (!xb || !pb) throw std::runtime_error("reduce: missing buffers");
    const float* x = static_cast<const float*>(xb->data);
    // 24 §5.1：声明 FP64 accumulator，必须真实 FP64 局部累加
    double* partials = static_cast<double*>(pb->data);
    double sum = 0.0;
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        sum += x[i];
    }
    // 每 chunk 写入独立区域（token_id * kReduceBlocks）
    partials[inv.token_id * kReduceBlocks] = sum;
}

void cpu_conv3x3_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!yb || !xb) throw std::runtime_error("conv: missing buffers");
    auto width = read_scalar<size_t>(inv.scalars, 0);
    auto height = read_scalar<size_t>(inv.scalars, sizeof(size_t));
    float k[9];
    bool k_ok = true;
    for (int i = 0; i < 9; ++i) {
        auto kv = read_scalar<float>(inv.scalars, 2 * sizeof(size_t) + i * sizeof(float));
        if (!kv) { k_ok = false; break; }
        k[i] = *kv;
    }
    if (!width || !height) throw std::runtime_error("conv: missing scalars");
    if (!k_ok) throw std::runtime_error("conv: missing kernel9");
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    const int w = static_cast<int>(*width);
    const int h = static_cast<int>(*height);
    for (std::size_t p = inv.domain.begin; p < inv.domain.end; ++p) {
        const int px = static_cast<int>(p % *width);
        const int py = static_cast<int>(p / *width);
        float acc = 0.0f;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = px + dx;
                const int ny = py + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                acc += x[static_cast<size_t>(ny) * *width + nx] *
                       k[(dy + 1) * 3 + (dx + 1)];
            }
        }
        y[p] = acc;
    }
}

// ==================== CUDA launchers（经桥接）====================
void cuda_copy_launcher(const KernelInvocation& inv, void*) {
    auto& api = cuda::bridge::api();
    void* h = cuda::bridge::get_tls_handle();
    if (!h || !api.loaded()) throw std::runtime_error("cuda bridge not available");
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!yb || !xb) throw std::runtime_error("cuda copy: missing buffers");
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = api.submit_copy(h, inv.domain.begin, inv.domain.end,
                                   static_cast<float*>(yb->data),
                                   static_cast<const float*>(xb->data),
                                   &elapsed, &err);
    if (rc != 0) throw std::runtime_error(err ? err : "cuda copy failed");
    cuda::bridge::set_tls_elapsed(elapsed);
}

void cuda_axpy_launcher(const KernelInvocation& inv, void*) {
    auto& api = cuda::bridge::api();
    void* h = cuda::bridge::get_tls_handle();
    if (!h || !api.loaded()) throw std::runtime_error("cuda bridge not available");
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    auto a = read_scalar<float>(inv.scalars, 0);
    if (!yb || !xb || !a) throw std::runtime_error("cuda axpy: missing args");
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = api.submit_axpy(h, inv.domain.begin, inv.domain.end,
                                   static_cast<float*>(yb->data),
                                   static_cast<const float*>(xb->data),
                                   *a, &elapsed, &err);
    if (rc != 0) throw std::runtime_error(err ? err : "cuda axpy failed");
    cuda::bridge::set_tls_elapsed(elapsed);
}

void cuda_reduce_launcher(const KernelInvocation& inv, void*) {
    auto& api = cuda::bridge::api();
    void* h = cuda::bridge::get_tls_handle();
    if (!h || !api.loaded()) throw std::runtime_error("cuda bridge not available");
    const BufferBinding* xb = inv.buffers.find(0);
    const BufferBinding* pb = inv.buffers.find(1);
    if (!xb || !pb) throw std::runtime_error("cuda reduce: missing buffers");
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = api.submit_reduce(
        h, inv.domain.begin, inv.domain.end,
        static_cast<const float*>(xb->data),
        static_cast<double*>(pb->data),
        kReduceBlocks, inv.token_id, &elapsed, &err);
    if (rc != 0) throw std::runtime_error(err ? err : "cuda reduce failed");
    cuda::bridge::set_tls_elapsed(elapsed);
}

void cuda_conv3x3_launcher(const KernelInvocation& inv, void*) {
    auto& api = cuda::bridge::api();
    void* h = cuda::bridge::get_tls_handle();
    if (!h || !api.loaded()) throw std::runtime_error("cuda bridge not available");
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    auto width = read_scalar<size_t>(inv.scalars, 0);
    auto height = read_scalar<size_t>(inv.scalars, sizeof(size_t));
    float k[9];
    bool k_ok = true;
    for (int i = 0; i < 9; ++i) {
        auto kv = read_scalar<float>(inv.scalars, 2 * sizeof(size_t) + i * sizeof(float));
        if (!kv) { k_ok = false; break; }
        k[i] = *kv;
    }
    if (!yb || !xb || !width || !height || !k_ok) {
        throw std::runtime_error("cuda conv: missing args");
    }
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = api.submit_conv3x3(
        h, inv.domain.begin, inv.domain.end,
        static_cast<float*>(yb->data),
        static_cast<const float*>(xb->data),
        *width, *height, k, &elapsed, &err);
    if (rc != 0) throw std::runtime_error(err ? err : "cuda conv failed");
    cuda::bridge::set_tls_elapsed(elapsed);
}

} // anonymous namespace

void register_classic_kernels() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        auto& reg = global_kernel_registry();
        {
            KernelRegistration r;
            r.id = "kernel.copy";
            r.args.buffer_count = 2;
            r.cpu = &cpu_copy_launcher;
            r.cuda = &cuda_copy_launcher;
            r.numeric.compute = NumericPolicy::Compute::fp32;
            reg.register_kernel(r);
        }
        {
            KernelRegistration r;
            r.id = "kernel.axpy";
            r.args.buffer_count = 2;
            r.args.scalar_bytes = sizeof(float);
            r.cpu = &cpu_axpy_launcher;
            r.cuda = &cuda_axpy_launcher;
            r.numeric.compute = NumericPolicy::Compute::fp32;
            reg.register_kernel(r);
        }
        {
            KernelRegistration r;
            r.id = "kernel.reduce";
            r.args.buffer_count = 2;
            r.cpu = &cpu_reduce_launcher;
            r.cuda = &cuda_reduce_launcher;
            r.numeric.compute = NumericPolicy::Compute::fp32;
            r.numeric.accumulator = NumericPolicy::Accumulator::fp64;
            reg.register_kernel(r);
        }
        {
            KernelRegistration r;
            r.id = "kernel.conv3x3";
            r.args.buffer_count = 2;
            r.args.scalar_bytes = 2 * sizeof(size_t) + 9 * sizeof(float);
            r.cpu = &cpu_conv3x3_launcher;
            r.cuda = &cuda_conv3x3_launcher;
            r.numeric.compute = NumericPolicy::Compute::fp32;
            reg.register_kernel(r);
        }
    });
}

} // namespace astro::compute::classic
