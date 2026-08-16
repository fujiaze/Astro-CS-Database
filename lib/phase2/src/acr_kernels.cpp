// lib/phase2/src/acr_kernels.cpp — Phase2 × ACR 合成 Operation 注册
//
// W9（ 34A532A2...B2EB308，08_ACR_INTEGRATION）：
// - CPU reference 是权威 science semantics；
// - ACR 只加速热点（block calibration / rejection / weighted reduction）；
// - 首版注册合成 Operation `synthetic.mosaic_reject.fp64acc`：
// legacy_parallel launcher 直接执行 phase2 CPU 语义（逐像素栈 rejection+
// 加权叠加），保证 CPU/ACR 等价；GPU kernel 后续在 profile 后添加。
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
// buffer0 = 输出 signal（独占范围）
// buffer1 = 输入样本栈 values（N 样本 × pixel_count，frame-major）
// buffer2 = 输入 support/weights（可选）
// scalars: [0]=pixel_count, [1]=stack_depth, [2]=rejection method(explicit),
// [3]=underdetermined_n, [4]=sigma_lower, [5]=sigma_upper,
// [6]=max_iterations, [7]=tile 内偏移 p0
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
    const auto und_n = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + sizeof(int));
    const auto lo = read_scalar<double>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int));
    const auto hi = read_scalar<double>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         sizeof(double));
    const auto max_it = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         2 * sizeof(double));
    const auto p0 = read_scalar<std::size_t>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         2 * sizeof(double) + sizeof(int));
    // weight_mode scalar (offset 7); 缺省 0=legacy support×snr²
    const auto wmode = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         2 * sizeof(double) + sizeof(int) + sizeof(std::size_t));
    if (!px || !depth || *px == 0 || *depth == 0) {
        throw std::runtime_error("mosaic_reject: missing scalars");
    }
    const std::size_t n_px = *px;
    const std::size_t n_depth = *depth;
    const std::uint32_t method_v =
        method ? static_cast<std::uint32_t>(*method) : 1u;
    const std::uint32_t und_n_v =
        und_n ? static_cast<std::uint32_t>(*und_n) : 2u;
    const double lo_v = lo ? *lo : -4.0;
    const double hi_v = hi ? *hi : 3.0;
    const int max_it_v = max_it ? *max_it : 8;
    std::vector<double> stack(n_depth);
    std::vector<double> stack_w(n_depth);
    std::vector<double> stack_sup(n_depth);
    std::vector<std::uint64_t> fid_compact(n_depth);
    std::vector<std::uint8_t> reasons(n_depth);
    std::vector<std::uint8_t> accepted(n_depth);
    std::vector<std::uint64_t> frame_seq(n_depth);
    for (std::size_t s = 0; s < n_depth; ++s) frame_seq[s] = s;
    // 显式 plan（ACR 路径仅 robust_mad_clip/sigma）
    P2RejectionPlan plan{};
    plan.method = (int)method_v;
    plan.minimum_n = 3;
    plan.underdetermined_n = und_n_v;
    plan.sigma.lower_sigma = std::fabs(lo_v);
    plan.sigma.upper_sigma = std::fabs(hi_v);
    plan.sigma.max_iterations = max_it_v;
    plan.normalization = P2_NORMALIZE_MEDIAN_CENTER;  // 与 CPU 生产一致
    plan.normalization_floor = 1e-12;
    // 合成 op 首版 FP32 输入（buffer 元素为 float）；科学语义以
    // CPU reference 为准，输入精度由 buffer element_size 声明。
    const float* src = static_cast<const float*>(vals->data);
    float* dst = static_cast<float*>(out->data);

    for (std::size_t p = 0; p < n_px; ++p) {
        // 统一 EligibilityPolicy（与 CPU 生产路径同一 collector）
        P2EligibilityGatherInput gin{};
        gin.values = src;
        gin.value_stride = n_px;
        gin.value_dtype = 0;  // fp32
        gin.support = (sup != nullptr) ? sup->data : nullptr;
        gin.support_stride = n_px;
        gin.frame_ids = frame_seq.data();
        gin.count = (std::uint32_t)n_depth;
        gin.pixel = (std::uint32_t)p;
        gin.support_threshold = 0.0;
        P2EligibilityGatherOutput gout{};
        gout.values = stack.data();
        gout.support = stack_sup.data();
        gout.frame_ids = fid_compact.data();
        std::uint32_t n_valid = 0;
        gout.eligible_count = &n_valid;
        if (p2_collect_candidate_stack(&gin, &gout) != 0) {
            throw std::runtime_error("mosaic_reject: eligibility failed");
        }
        // 权重模式（，ACR-IVAR-001）：wmode=2（cell ivar×support）与
        // CPU 逐像素 ivar 不等价，已从生产路由禁用（stage2 强制 CPU）；
        // kernel 保留 wmode=0（legacy support×snr²，仅 ablation/诊断）。
        if (wmode && *wmode == 2)
            throw std::runtime_error(
                "mosaic_reject: wmode=2 (cell ivar) 已禁用——ivar science "
                "模式必须走 CPU canonical path");
        for (std::uint32_t s = 0; s < n_valid; ++s) {
            double wgt_v = 1.0;
            if (snr != nullptr) {
                const int grid = 8;
                const std::size_t tile_p = p0 ? *p0 + p : p;
                const int px = (int)(tile_p % 512u);
                const int py = (int)(tile_p / 512u);
                const int cell = (py / 64) * grid + (px / 64);
                const std::uint64_t fs = fid_compact[s];
                wgt_v = static_cast<double>(
                    static_cast<const float*>(snr->data)
                        [fs * grid * grid + (std::size_t)cell]);
            }
            stack_w[s] = stack_sup[s] * wgt_v * wgt_v;   // legacy 诊断
        }
        if (n_valid == 0) {
            dst[p] = 0.0f;
            if (out_sup) static_cast<float*>(out_sup->data)[p] = 0.0f;
            if (out_rej) static_cast<float*>(out_rej->data)[p] = 0.0f;
            if (out_valid) static_cast<float*>(out_valid->data)[p] = 0.0f;
            continue;
        }
        P2CandidateStack cstack{};
        cstack.values = stack.data();
        cstack.weights = (sup != nullptr) ? stack_w.data() : nullptr;
        cstack.count = n_valid;
        cstack.data_type = 0;
        P2RejectionDecision rdec{};
        rdec.reasons = reasons.data();
        if (p2_reject_stack_ex(&cstack, &plan, &rdec) != 0) {
            throw std::runtime_error("mosaic_reject: rejection failed");
        }
        // 只有 OK/UNDERDETERMINED 可继续
        if (rdec.status != P2_STATUS_OK &&
            rdec.status != P2_STATUS_UNDERDETERMINED) {
            throw std::runtime_error(
                "mosaic_reject: invalid rejection status " +
                std::to_string(rdec.status));
        }
        for (std::uint32_t s = 0; s < n_valid; ++s) {
            accepted[s] = (rdec.reasons[s] == P2_REASON_ACCEPTED ||
                           rdec.reasons[s] == P2_REASON_UNDERDETERMINED)
                              ? 1 : 0;
        }
        P2PixelStack pi{};
        pi.values = stack.data();
        pi.weights = (sup != nullptr) ? stack_w.data() : nullptr;
        pi.support = (sup != nullptr) ? stack_sup.data() : nullptr;
        pi.accepted = accepted.data();
        pi.count = n_valid;
        P2PixelResult pr{};
        if (p2_integrate_pixel(&pi, &pr) != 0) {
            throw std::runtime_error("mosaic_reject: integrate failed");
        }
        dst[p] = (pr.status == P2_INTEGRATE_OK)
                     ? static_cast<float>(pr.signal) : 0.0f;
        if (out_rej)
            static_cast<float*>(out_rej->data)[p] =
                (float)(rdec.rejected_low + rdec.rejected_high);
        if (out_valid) static_cast<float*>(out_valid->data)[p] = (float)n_valid;
        if (out_sup) {
            static_cast<float*>(out_sup->data)[p] =
                (pr.status == P2_INTEGRATE_OK)
                    ? static_cast<float>(pr.support) : 0.0f;
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
    const auto und_n = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + sizeof(int));
    const auto lo = read_scalar<double>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int));
    const auto hi = read_scalar<double>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         sizeof(double));
    const auto max_it = read_scalar<int>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         2 * sizeof(double));
    const auto p0 = read_scalar<std::size_t>(
        inv.scalars, 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                         2 * sizeof(double) + sizeof(int));
    if (!px || !depth || *px == 0 || *depth == 0) {
        throw std::runtime_error("mosaic_reject: missing scalars");
    }
    const BufferBinding* sup = inv.buffers.find(2);
    const BufferBinding* snr = inv.buffers.find(3);
    const BufferBinding* out_sup = inv.buffers.find(4);
    const BufferBinding* out_rej = inv.buffers.find(5);
    const BufferBinding* out_valid = inv.buffers.find(6);
    const int und_n_v = und_n ? *und_n : 2;
    const int max_it_v = max_it ? *max_it : 8;
    const std::uint32_t method_v =
        method ? static_cast<std::uint32_t>(*method) : 1u;
    if (method_v == P2_REJECT_WINSORIZED_SIGMA) {
        // Winsorized 与 Sigma 算法不同（winsorized mean/std vs
        // median/MAD），CUDA 只实现 Sigma；Winsorized 明确 CPU_ROUTE，
        // 禁止同 kernel 冒充两种 science semantics。
        throw std::runtime_error(
            "mosaic_reject cuda: CPU_ROUTE winsorized not implemented on CUDA");
    }
    if (method_v != P2_REJECT_SIGMA) {
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
        p0 ? *p0 : 0,
        static_cast<float>(-std::fabs(lo ? *lo : 4.0)),  // CUDA kernel 低侧为负
        static_cast<float>(std::fabs(hi ? *hi : 3.0)),
        max_it_v, und_n_v, &elapsed, &err);
    if (rc != 0) {
        throw std::runtime_error(
            std::string("mosaic_reject cuda failed rc=") +
            std::to_string(rc) + " err=" +
            (err ? err : "(null)"));
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
        r.args.scalar_bytes = 2 * sizeof(std::size_t) + 2 * sizeof(int) +
                              2 * sizeof(double) + sizeof(int) +
                              sizeof(std::size_t);
        r.cpu = &mosaic_reject_legacy;  // CPU 即 legacy reference 语义
        r.legacy_parallel = &mosaic_reject_legacy;
        r.cuda = &mosaic_reject_cuda;
        r.numeric.compute = NumericPolicy::Compute::fp64;
        r.numeric.accumulator = NumericPolicy::Accumulator::fp64;
        global_kernel_registry().register_kernel(r);
    });
}

} // namespace astro::compute::phase2
