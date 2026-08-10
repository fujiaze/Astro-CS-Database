// lib/phase2/src/acr_kernels.cpp — Phase2 × ACR 合成 Operation 注册
//
// W9（控制包 34A532A2...B2EB308，08_ACR_INTEGRATION）：
//   - CPU reference 是权威 science semantics；
//   - ACR 只加速热点（block calibration / rejection / weighted reduction）；
//   - 首版注册合成 Operation `synthetic.mosaic_reject.fp64acc`：
//     legacy_parallel launcher 直接执行 phase2 CPU 语义（逐像素栈 rejection+
//     加权叠加），保证 CPU/ACR 等价；GPU kernel 后续在 profile 后添加。
#include "astro/phase2/rejection.h"
#include "astro/phase2/integrate.h"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "backends/cuda/bridge/cuda_bridge_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace astro::compute::phase2 {

const char* kOpMosaicReject =
    "synthetic.mosaic_reject.fp64acc";

namespace {

// legacy launcher：invocation 约定
//   buffer0 = 输出 signal（独占范围）
//   buffer1 = 输入样本栈 values（N 样本 × pixel_count，frame-major）
//   buffer2 = 输入 support/weights（可选）
//   scalars: [0]=pixel_count, [1]=stack_depth,
//            [2]=rejection method, [3]=sigma_low, [4]=sigma_high
void mosaic_reject_legacy(const KernelInvocation& inv, void*) {
    const BufferBinding* out = inv.buffers.find(0);
    const BufferBinding* vals = inv.buffers.find(1);
    if (!out || !vals) {
        throw std::runtime_error("mosaic_reject: missing buffers");
    }
    const auto px = read_scalar<std::size_t>(inv.scalars, 0);
    const auto depth = read_scalar<std::size_t>(inv.scalars, sizeof(std::size_t));
    const auto method = read_scalar<int>(inv.scalars, 2 * sizeof(std::size_t));
    const auto lo = read_scalar<double>(inv.scalars,
                                        2 * sizeof(std::size_t) + sizeof(int));
    const auto hi = read_scalar<double>(inv.scalars,
                                        2 * sizeof(std::size_t) + sizeof(int) +
                                            sizeof(double));
    if (!px || !depth || *px == 0 || *depth == 0) {
        throw std::runtime_error("mosaic_reject: missing scalars");
    }
    const std::size_t n_px = *px;
    const std::size_t n_depth = *depth;
    const std::uint32_t method_v =
        method ? static_cast<std::uint32_t>(*method) : 1u;
    const double lo_v = lo ? *lo : -4.0;
    const double hi_v = hi ? *hi : 3.0;

    std::vector<double> stack(n_depth);
    std::vector<std::uint8_t> accepted(n_depth);
    // 合成 op 首版 FP32 输入（buffer 元素为 float）；科学语义以
    // CPU reference 为准，输入精度由 buffer element_size 声明。
    const float* src = static_cast<const float*>(vals->data);
    float* dst = static_cast<float*>(out->data);

    for (std::size_t p = 0; p < n_px; ++p) {
        for (std::size_t s = 0; s < n_depth; ++s) {
            stack[s] = static_cast<double>(src[s * n_px + p]);
        }
        P2SampleStackView rv{};
        rv.values = stack.data();
        rv.count = static_cast<std::uint32_t>(n_depth);
        rv.method = static_cast<int>(method_v);
        rv.sigma_low = lo_v;
        rv.sigma_high = hi_v;
        rv.min_samples = 1;
        P2RejectionResult rr{};
        rr.accepted = accepted.data();
        if (p2_reject_stack(&rv, &rr) != 0) {
            throw std::runtime_error("mosaic_reject: rejection failed");
        }
        P2PixelStack pi{};
        pi.values = stack.data();
        pi.accepted = accepted.data();
        pi.count = static_cast<std::uint32_t>(n_depth);
        P2PixelResult pr{};
        if (p2_integrate_pixel(&pi, &pr) != 0) {
            throw std::runtime_error("mosaic_reject: integrate failed");
        }
        dst[p] = (pr.status == 0) ? static_cast<float>(pr.signal) : 0.0f;
    }
}

// CUDA launcher：与 legacy（CPU reference）同一科学语义；等权模式
// （support/frame_snr 不传 → kernel 内等权，与 CPU legacy 完全一致）。
void mosaic_reject_cuda(const KernelInvocation& inv, void*) {
    using namespace astro::compute::cuda::bridge;
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded()) {
        throw std::runtime_error("mosaic_reject: cuda bridge not loaded");
    }
    void* h = get_tls_handle();
    if (!h) throw std::runtime_error("mosaic_reject: no cuda handle");
    const BufferBinding* out = inv.buffers.find(0);
    const BufferBinding* vals = inv.buffers.find(1);
    if (!out || !vals) {
        throw std::runtime_error("mosaic_reject: missing buffers");
    }
    const auto px = read_scalar<std::size_t>(inv.scalars, 0);
    const auto depth = read_scalar<std::size_t>(inv.scalars, sizeof(std::size_t));
    const auto method = read_scalar<int>(inv.scalars, 2 * sizeof(std::size_t));
    const auto lo = read_scalar<double>(inv.scalars,
                                        2 * sizeof(std::size_t) + sizeof(int));
    const auto hi = read_scalar<double>(inv.scalars,
                                        2 * sizeof(std::size_t) + sizeof(int) +
                                            sizeof(double));
    if (!px || !depth || *px == 0 || *depth == 0) {
        throw std::runtime_error("mosaic_reject: missing scalars");
    }
    const std::uint32_t method_v =
        method ? static_cast<std::uint32_t>(*method) : 1u;
    if (method_v != P2_REJECT_SIGMA &&
        method_v != P2_REJECT_WINSORIZED_SIGMA) {
        // CUDA kernel 实现统一 sigma-clip（Sigma/WinsorizedSigma 同语义）；
        // 其他方法回退 CPU（legacy launcher），由调用方路由。
        throw std::runtime_error("mosaic_reject cuda: unsupported method");
    }
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = api.submit_mosaic_reject(
        h, inv.domain.begin, inv.domain.end,
        static_cast<float*>(out->data),
        static_cast<const float*>(vals->data),
        nullptr, nullptr,                       // 等权（与 CPU legacy 一致）
        *depth, *px,
        static_cast<float>(lo ? *lo : -4.0),
        static_cast<float>(hi ? *hi : 3.0),
        8, 1, &elapsed, &err);
    if (rc != 0) {
        throw std::runtime_error(err ? err : "mosaic_reject cuda failed");
    }
    set_tls_elapsed(elapsed);
}

} // namespace

void register_phase2_acr_kernels() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        KernelRegistration r;
        r.id = kOpMosaicReject;
        r.args.buffer_count = 3;
        r.args.scalar_bytes = 2 * sizeof(std::size_t) + sizeof(int) +
                              2 * sizeof(double);
        r.cpu = &mosaic_reject_legacy;  // CPU 即 legacy reference 语义
        r.legacy_parallel = &mosaic_reject_legacy;
        r.cuda = &mosaic_reject_cuda;
        r.numeric.compute = NumericPolicy::Compute::fp64;
        r.numeric.accumulator = NumericPolicy::Accumulator::fp64;
        global_kernel_registry().register_kernel(r);
    });
}

} // namespace astro::compute::phase2
