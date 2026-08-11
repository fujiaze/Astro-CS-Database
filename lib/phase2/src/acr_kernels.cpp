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

#include <cmath>
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
    const BufferBinding* sup = inv.buffers.find(2);      // 可空
    const BufferBinding* snr = inv.buffers.find(3);      // 可空
    const BufferBinding* out_sup = inv.buffers.find(4);  // 可空
    const BufferBinding* out_rej = inv.buffers.find(5);  // 可空
    const BufferBinding* out_valid = inv.buffers.find(6); // 可空
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

    const auto ms = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + sizeof(int) +
                         2 * sizeof(double));
    const int min_samples_v = ms ? *ms : 3;
    std::vector<double> stack(n_depth);
    std::vector<double> stack_w(n_depth);
    std::vector<double> stack_sup(n_depth);
    std::vector<std::uint8_t> accepted(n_depth);
    // 合成 op 首版 FP32 输入（buffer 元素为 float）；科学语义以
    // CPU reference 为准，输入精度由 buffer element_size 声明。
    const float* src = static_cast<const float*>(vals->data);
    float* dst = static_cast<float*>(out->data);

    for (std::size_t p = 0; p < n_px; ++p) {
        std::uint32_t n_valid = 0;
        for (std::size_t s = 0; s < n_depth; ++s) {
            const double v = static_cast<double>(src[s * n_px + p]);
            const double sval = (sup != nullptr)
                ? static_cast<double>(static_cast<const float*>(sup->data)[s * n_px + p])
                : 1.0;
            if (!std::isfinite(v) || sval <= 0.0) continue;
            stack[n_valid] = v;
            stack_sup[n_valid] = sval;
            const double snr_v = (snr != nullptr)
                ? static_cast<double>(static_cast<const float*>(snr->data)[s])
                : 1.0;
            stack_w[n_valid] = (sup != nullptr)
                ? sval * snr_v * snr_v : 1.0;
            ++n_valid;
        }
        if (n_valid == 0) {
            dst[p] = 0.0f;
            if (out_sup) static_cast<float*>(out_sup->data)[p] = 0.0f;
            if (out_rej) static_cast<float*>(out_rej->data)[p] = 0.0f;
            if (out_valid) static_cast<float*>(out_valid->data)[p] = 0.0f;
            continue;
        }
        P2SampleStackView rv{};
        rv.values = stack.data();
        rv.count = n_valid;
        rv.method = static_cast<int>(method_v);
        rv.sigma_low = lo_v;
        rv.sigma_high = hi_v;
        rv.min_samples = min_samples_v;
        P2RejectionResult rr{};
        rr.accepted = accepted.data();
        if (p2_reject_stack(&rv, &rr) != 0) {
            throw std::runtime_error("mosaic_reject: rejection failed");
        }
        P2PixelStack pi{};
        pi.values = stack.data();
        pi.weights = (sup != nullptr) ? stack_w.data() : nullptr;
        pi.support = (sup != nullptr) ? stack_sup.data() : nullptr;
        pi.accepted = accepted.data();
        pi.count = n_valid;
        pi.weight_mode = 0;
        P2PixelResult pr{};
        if (p2_integrate_pixel(&pi, &pr) != 0) {
            throw std::runtime_error("mosaic_reject: integrate failed");
        }
        dst[p] = (pr.status == 0) ? static_cast<float>(pr.signal) : 0.0f;
        if (out_rej)
            static_cast<float*>(out_rej->data)[p] =
                (float)(rr.rejected_low + rr.rejected_high);
        if (out_valid) static_cast<float*>(out_valid->data)[p] = (float)n_valid;
        if (out_sup) {
            double sup_out = 0.0;
            for (std::uint32_t s = 0; s < n_valid; ++s)
                if (accepted[s]) sup_out = std::max(sup_out, stack_sup[s]);
            static_cast<float*>(out_sup->data)[p] =
                (pr.status == 0) ? static_cast<float>(sup_out) : 0.0f;
        }
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
    const auto ms = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + sizeof(int) +
                         2 * sizeof(double));
    if (!px || !depth || *px == 0 || *depth == 0) {
        throw std::runtime_error("mosaic_reject: missing scalars");
    }
    const BufferBinding* sup = inv.buffers.find(2);
    const BufferBinding* snr = inv.buffers.find(3);
    const BufferBinding* out_sup = inv.buffers.find(4);
    const BufferBinding* out_rej = inv.buffers.find(5);
    const BufferBinding* out_valid = inv.buffers.find(6);
    const int min_samples_v = ms ? *ms : 3;
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
        sup ? static_cast<const float*>(sup->data) : nullptr,
        snr ? static_cast<const float*>(snr->data) : nullptr,
        out_sup ? static_cast<float*>(out_sup->data) : nullptr,
        out_rej ? static_cast<float*>(out_rej->data) : nullptr,
        out_valid ? static_cast<float*>(out_valid->data) : nullptr,
        *depth, *px,
        static_cast<float>(lo ? *lo : -4.0),
        static_cast<float>(hi ? *hi : 3.0),
        8, min_samples_v, &elapsed, &err);
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
        r.args.buffer_count = 7;
        r.args.scalar_bytes = 2 * sizeof(std::size_t) + sizeof(int) +
                              2 * sizeof(double) + sizeof(int);
        r.cpu = &mosaic_reject_legacy;  // CPU 即 legacy reference 语义
        r.legacy_parallel = &mosaic_reject_legacy;
        r.cuda = &mosaic_reject_cuda;
        r.numeric.compute = NumericPolicy::Compute::fp64;
        r.numeric.accumulator = NumericPolicy::Accumulator::fp64;
        global_kernel_registry().register_kernel(r);
    });
}

} // namespace astro::compute::phase2
