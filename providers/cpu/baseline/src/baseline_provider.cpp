// AstroCS CPU baseline provider — providers/cpu/baseline/src/baseline_provider.cpp (CPU-002)
//
// 职责: AMD64 baseline provider DLL (仅 SSE2, 编译不带 /arch:AVX* / -mavx* 旗标)。
//   - 唯一导出 astrocs_provider_query_v1 (include/astrocs/abi/module_api_v1.h 冻结;
//     12 §1 / ARC-001 §1.2: provider DLL 不得导出其他符号);
//   - query 握手: host_abi 失配 → ACS_ERR_ABI_MISMATCH (不降级猜测);
//     host 必填 allocator; out_api = 静态 provider 表;
//   - query 期消费 CPU-001 capability 判定 (acs_cap_detect_v1 + os_safe 平面):
//     baseline 只需 SSE2 (amd64 恒备); capability 探测失败 (非 amd64) /
//     SSE2 不在 os_safe → ACS_ERR_UNSUPPORTED (拒绝加载; host 进入无 provider
//     保守路径 — 15 §2 "硬件支持但 OS 不保存寄存器时拒绝");
//   - 12 个注册 kernel 全部有可靠实现 (kernel_list 静态表), 无 unsupported 槽:
//     baseline 是全部 kernel 的最低可用实现 (CPU-003/004 变体仅热点 kernel,
//     其余 kernel 由 host 退回本 provider);
//   - 公式/算术序逐位对齐 ABI-003 冻结实现 (legacy baseline_kernels_impl.inc)
//     — CPU-002 是 legacy baseline backend 的正式 provider 形态迁移, 科学值
//     语义零漂移 (scientific_change=false); 数据面 v1 = float32;
//   - self_test: host 服务往返 + kernel 表自洽 + 冒烟;
//   - 无全局 SIMD 静态初始化: 全局对象仅 POD/字符串表 (静态 init 无 SIMD
//     指令), 加载期不执行任何 SIMD (12 §7 "provider DLL 不在 DllMain 使用 SIMD");
//   - 并行: 输出元素独立 → bitwise 确定性不随 worker 数变化 (ARCH-004 §4);
//     worker 经 host executor 租借 (FORBID-003 禁私有线程池); 无 executor /
//     租借失败 → 串行兜底 (05 §6 保守路线)。
//
// 编译合同: C++17; -fno-exceptions 亦可编译 (导出函数 try/catch 条件包裹);
// 导出 C 函数内捕获 C++ 异常 → ACS_ERR_EXCEPTION (12 §4, 禁异常跨边界)。
//
// NaN/Inf 语义 (对照 ALG 权威; 详见 baseline_provider_v1.h 注释):
//   - 逐元素 op: IEEE-754 传播 (NaN 输入 → NaN 输出);
//   - 排序型 op (noise/rejection): 仅有限帧参与 median/MAD (ALG-CAL §4
//     "输入含 NaN 仅 finite 参与"); 全非有限像素 → med=0/madσ=0/计数=0。
#include "astrocs/cpu/baseline_provider_v1.h"
#include "astrocs/cpu/capability_v1.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if !defined(ASTROCS_NO_EXCEPTIONS)
#include <stdexcept>
#endif

#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif

/* ───────────────────────── 能力门 (query 期) ─────────────────────────
 * baseline 只需 SSE2 (amd64 恒备)。生产链接真实 capability_detect.c; 能力
 * 不足负测由测试以 stub 探测注入 (tests/cpu/baseline/provider_capability_gate_test.c
 * 链接期替换 acs_cap_detect_v1/acs_cap_os_safe_satisfies_v1)。本函数为
 * extern "C" 顶层符号 (host/测试可直接判定; 不属 provider 导出白名单)。 */
extern "C" int acs_cpu_baseline_cap_gate(acs_cap_result_v1* out) {
    if (out == nullptr) return ACS_ERR_PARAM;
    acs_cap_result_v1 c;
    std::memset(&c, 0, sizeof(c));
    c.struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    c.abi_version = ACS_CAP_ABI_VERSION_V1;
    const int rc = acs_cap_detect_v1(&c);
    if (rc != ACS_CAP_OK) return ACS_ERR_UNSUPPORTED;
    if (!acs_cap_os_safe_satisfies_v1(&c, ACS_CAP_FEAT_SSE2))
        return ACS_ERR_UNSUPPORTED;   /* OS/平台不保证 SSE2 → 拒绝 (保守) */
    *out = c;
    return ACS_OK;
}

namespace astrocs_cpu_baseline {

/* ───────────────────────── 字符串构造辅助 (静态 init) ───────────────────────── */
static acs_str_v1 mkstr(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)std::strlen(s) : 0u;
    return v;
}

/* ───────────────────────── kernel 描述静态表 (12; 序 = 索引) ─────────────────────────
 * kernel_id/algorithm_id/precision/determinism 与 ABI-003 backend_table.inc
 * 语义一致 (kernel_id = legacy algorithm_id; desc.precision 以实际 f32 数据面
 * 为准 — legacy 部分条目 F64 标注与其 f32 实现不一致, 见 README §4)。
 * determinism_class: 0=bitwise (逐元素独立); 1=fixed_order (帧内固定序累加)。 */
static const acs_kernel_desc_v1 kKernels[ACS_CPU_BASELINE_KERNEL_COUNT] = {
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("calibration-pixel-transform"), mkstr("ALG-001"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("noise-snr-reductions"), mkstr("ALG-004"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("wcs-psf-batch"), mkstr("ALG-002"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("drizzle-overlap"), mkstr("ALG-005"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_FIXED_ORDER },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("drizzle-accumulate"), mkstr("ALG-005"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_FIXED_ORDER },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("drizzle-normalize"), mkstr("ALG-005"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_FIXED_ORDER },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("upm-spmv"), mkstr("ALG-006"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("upm-residual"), mkstr("ALG-006"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("upm-weight-update"), mkstr("ALG-006"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("rejection-statistics"), mkstr("ALG-008"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_FIXED_ORDER },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("integration-accumulate"), mkstr("ALG-009"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_FIXED_ORDER },
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("hips-bulk-transform"), mkstr("ALG-P3-002"), ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE }
};

/* ───────────────────────── 槽位解析 / 参数校验 ───────────────────────── */

/* 校验 P->in_off/len 相对缓冲起点 (元素) 越界; 填 S 槽指针。 */
static int validate_slots(const acs_cpu_baseline_params_v1* P,
                          const acs_span_u8& in, const acs_span_u8& out) {
    if (in.data == nullptr || out.data == nullptr) return ACS_ERR_PARAM;
    if (P->head.struct_size < sizeof(*P) ||
        P->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    const uint64_t in_cap  = in.count >> 2;   /* f32 元素容量 (调用方保证 4 整除) */
    const uint64_t out_cap = out.count >> 2;
    if ((in.count & 3u) != 0 || (out.count & 3u) != 0) return ACS_ERR_PARAM;
    for (uint32_t s = 0; s < ACS_CPU_BASELINE_MAX_IN_SLOTS; ++s) {
        if (P->in_len[s] == 0) {
            if (P->in_off[s] != 0) return ACS_ERR_PARAM;  /* 未用槽 must 0 */
            continue;
        }
        if (P->in_off[s] > in_cap || P->in_len[s] > in_cap - P->in_off[s])
            return ACS_ERR_PARAM;                          /* off+len 越界 */
    }
    for (uint32_t s = 0; s < ACS_CPU_BASELINE_MAX_OUT_SLOTS; ++s) {
        if (P->out_len[s] == 0) {
            if (P->out_off[s] != 0) return ACS_ERR_PARAM;
            continue;
        }
        if (P->out_off[s] > out_cap || P->out_len[s] > out_cap - P->out_off[s])
            return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

/* 元素级长度需求表 (各 op 要求; 返回 ACS_OK 或 ACS_ERR_PARAM)。
 * frames_out: 栈类 op 的帧数 (accumulate/noise/rejection/integration), 成功写回。 */
static int check_lengths(const acs_cpu_baseline_params_v1* P, uint32_t kidx) {
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    uint64_t frames = 0;
    /* 槽启用以 len>0 为准 (validate_slots 已保证 len=0 → off=0) */
    auto req_in = [&](unsigned s, uint64_t need) -> bool {
        return P->in_len[s] >= need;
    };
    auto req_out = [&](unsigned s, uint64_t need) -> bool {
        return P->out_len[s] >= need;
    };

    switch (kidx) {
    case ACS_CPU_KIDX_CALIBRATION:
        return (req_in(0, N) && req_in(1, N) && req_in(2, N) && req_in(3, N) &&
                req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_PSF_BATCH:
        return (req_in(0, 2) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_DRIZZLE_OVERLAP:
        return (req_in(0, N) && req_in(1, N) && req_out(0, N)) ? ACS_OK
                                                               : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_DRIZZLE_ACCUMULATE:
    case ACS_CPU_KIDX_INTEGRATION_ACCUM:
        frames = P->aux0;
        if (frames == 0 || frames > (UINT64_MAX / N)) return ACS_ERR_PARAM;
        return (req_in(0, frames * N) && req_in(1, frames * N) &&
                req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_DRIZZLE_NORMALIZE:
    case ACS_CPU_KIDX_UPM_RESIDUAL:
        return (req_in(0, N) && req_in(1, N) && req_out(0, N)) ? ACS_OK
                                                               : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_UPM_WEIGHT_UPDATE:
        return (req_in(0, N) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_UPM_SPMV: {
        const uint64_t nnz = P->aux0, ncols = P->aux1;
        if (nnz == 0 || ncols == 0) return ACS_ERR_PARAM;
        return (req_in(0, nnz) && req_in(1, nnz) && req_in(2, N + 1) &&
                req_in(3, ncols) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    }
    case ACS_CPU_KIDX_NOISE_REDUCTIONS:
        frames = P->aux0;
        if (frames == 0 || frames > (UINT64_MAX / N)) return ACS_ERR_PARAM;
        return (req_in(0, frames * N) && req_out(0, N) &&
                req_out(1, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_REJECTION_STATS:
        frames = P->aux0;
        if (frames == 0 || frames > (UINT64_MAX / N)) return ACS_ERR_PARAM;
        return (req_in(0, frames * N) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_KIDX_HIPS_BULK: {
        const uint64_t iw = P->aux0, ih = P->aux1;
        if (iw < 2 || ih < 2 || iw > UINT64_MAX / ih) return ACS_ERR_PARAM;
        return (req_in(0, iw * ih) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    }
    default:
        return ACS_ERR_UNSUPPORTED;
    }
}

/* ───────────────────────── kernel 数值实现 ─────────────────────────
 * 公式与算术序逐位对齐 ABI-003 baseline_kernels_impl.inc (同式迁移);
 * 排序型 op 显式"仅有限帧"语义 (legacy std::sort 对 NaN 无定义, 此处按
 * ALG-CAL §4 冻结规则显式化 — 有限输入与 legacy 逐位一致)。
 * 全部 kernel: 每输出元素独立; 无跨线程归约 (确定性关键)。 */

/* 仅有限帧值收集到 scratch; 返回有限帧数 (帧主序: src[f*N+i]) */
static size_t gather_finite_frames(const float* src, uint64_t N, uint64_t frames,
                                   uint64_t i, float* scratch) {
    size_t m = 0;
    for (uint64_t f = 0; f < frames; ++f) {
        const float v = src[f * N + i];
        if (std::isfinite(v)) scratch[m++] = v;
    }
    return m;
}

static float median_of_sorted(float* v, size_t n) {
    if (n == 0) return 0.0f;
    if (n % 2 == 1) return v[n / 2];
    return 0.5f * (v[n / 2 - 1] + v[n / 2]);   /* 与 ABI-003 median_inplace 同式 */
}

/* 单输出像素带 [i0,i1) 的 kernel 计算。 */
static void kernel_pixel_range(const acs_cpu_baseline_params_v1* P, uint32_t kidx,
                               const float* const* in, float* const* out,
                               float* scratch, uint64_t i0, uint64_t i1) {
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    const float kf = P->k;
    switch (kidx) {
    case ACS_CPU_KIDX_CALIBRATION: {
        const float* a = in[0]; const float* b = in[1];
        const float* c = in[2]; const float* d = in[3];
        float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i)
            o[i] = (a[i] - b[i] - kf * c[i]) * d[i];
        break;
    }
    case ACS_CPU_KIDX_PSF_BATCH: {
        const float cx = in[0][0], cy = in[0][1];
        float* o = out[0];
        const uint32_t w = P->w;
        for (uint64_t i = i0; i < i1; ++i) {
            const float x = static_cast<float>(i % w);
            const float y = static_cast<float>(i / w);
            const float dx = x - cx, dy = y - cy;
            const float r2 = dx * dx + dy * dy;
            o[i] = kf * std::exp(-r2 * 0.5f);
        }
        break;
    }
    case ACS_CPU_KIDX_DRIZZLE_OVERLAP: {
        const float* a = in[0]; const float* b = in[1]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) {
            const float wx = std::max(0.0f, 1.0f - std::fabs(a[i]));
            const float wy = std::max(0.0f, 1.0f - std::fabs(b[i]));
            o[i] = wx * wy;
        }
        break;
    }
    case ACS_CPU_KIDX_DRIZZLE_ACCUMULATE: {
        const uint64_t frames = P->aux0;
        const float* a = in[0]; const float* b = in[1]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) {
            float acc = 0.0f;
            for (uint64_t f = 0; f < frames; ++f)      /* 固定帧序 (f32 累加) */
                acc += a[f * N + i] * b[f * N + i];
            o[i] = acc;
        }
        break;
    }
    case ACS_CPU_KIDX_DRIZZLE_NORMALIZE: {
        const float* a = in[0]; const float* b = in[1]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i)
            o[i] = (b[i] > 1e-6f) ? (a[i] / b[i]) : 0.0f;
        break;
    }
    case ACS_CPU_KIDX_UPM_SPMV: {
        const float* vals = in[0]; const float* cols = in[1];
        const float* rowptr = in[2]; const float* x = in[3];
        float* o = out[0];
        for (uint64_t r = i0; r < i1; ++r) {
            const uint64_t a = static_cast<uint64_t>(rowptr[r]);
            const uint64_t b = static_cast<uint64_t>(rowptr[r + 1]);
            float acc = 0.0f;
            for (uint64_t k = a; k < b; ++k) {         /* 固定列序 */
                const uint64_t col = static_cast<uint64_t>(cols[k]);
                if (col >= P->aux1) { o[r] = 0.0f; break; }  /* 防御 (应被校验) */
                acc += vals[k] * x[col];
            }
            o[r] = acc;
        }
        break;
    }
    case ACS_CPU_KIDX_UPM_RESIDUAL: {
        const float* a = in[0]; const float* b = in[1]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) o[i] = a[i] - b[i];
        break;
    }
    case ACS_CPU_KIDX_UPM_WEIGHT_UPDATE: {
        const float* a = in[0]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) o[i] = std::max(a[i], kf);
        break;
    }
    case ACS_CPU_KIDX_NOISE_REDUCTIONS: {
        const uint64_t frames = P->aux0;
        const float* a = in[0]; float* om = out[0]; float* os = out[1];
        for (uint64_t i = i0; i < i1; ++i) {
            const size_t m = gather_finite_frames(a, N, frames, i, scratch);
            if (m == 0) { om[i] = 0.0f; os[i] = 0.0f; continue; }
            std::sort(scratch, scratch + m);           /* 就地; 多重集同 */
            const float med = median_of_sorted(scratch, m);
            for (size_t f = 0; f < m; ++f) scratch[f] = std::fabs(scratch[f] - med);
            std::sort(scratch, scratch + m);
            const float mad = median_of_sorted(scratch, m);
            om[i] = med;
            os[i] = mad * 1.4826f;                     /* MAD→σ (ALG-NOISE F1) */
        }
        break;
    }
    case ACS_CPU_KIDX_REJECTION_STATS: {
        const uint64_t frames = P->aux0;
        const float* a = in[0]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) {
            const size_t m = gather_finite_frames(a, N, frames, i, scratch);
            if (m == 0) { o[i] = 0.0f; continue; }
            std::sort(scratch, scratch + m);
            const float med = median_of_sorted(scratch, m);
            for (size_t f = 0; f < m; ++f) scratch[f] = std::fabs(scratch[f] - med);
            std::sort(scratch, scratch + m);
            const float mad = median_of_sorted(scratch, m);
            const float th = kf * (mad * 1.4826f);
            uint32_t cnt = 0;
            for (uint64_t f = 0; f < frames; ++f) {    /* 原帧值 (含 NaN: 比较恒假) */
                const float v = a[f * N + i];
                if (std::fabs(v - med) > th) ++cnt;
            }
            o[i] = static_cast<float>(cnt);
        }
        break;
    }
    case ACS_CPU_KIDX_INTEGRATION_ACCUM: {
        const uint64_t frames = P->aux0;
        const float* x = in[0]; const float* w = in[1]; float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i) {
            float acc = 0.0f, wsum = 0.0f;
            for (uint64_t f = 0; f < frames; ++f) {    /* 固定帧序 */
                acc += w[f * N + i] * x[f * N + i];
                wsum += w[f * N + i];
            }
            o[i] = (wsum > 1e-6f) ? (acc / wsum) : 0.0f;
        }
        break;
    }
    case ACS_CPU_KIDX_HIPS_BULK: {
        const uint64_t iw = P->aux0, ih = P->aux1;
        const float s = kf;
        const float* src = in[0]; float* o = out[0];
        const uint32_t w = P->w;
        for (uint64_t i = i0; i < i1; ++i) {
            const float x = static_cast<float>(i % w) * s;
            const float y = static_cast<float>(i / w) * s;
            const float flx = std::floor(x), fly = std::floor(y);
            int x0 = static_cast<int>(flx);
            int y0 = static_cast<int>(fly);
            float fx = x - flx, fy = y - fly;
            x0 = std::min(std::max(x0, 0), static_cast<int>(iw) - 2);
            y0 = std::min(std::max(y0, 0), static_cast<int>(ih) - 2);
            fx = std::min(std::max(fx, 0.0f), 1.0f);
            fy = std::min(std::max(fy, 0.0f), 1.0f);
            const uint64_t r0 = static_cast<uint64_t>(y0) * iw;
            const uint64_t r1 = r0 + iw;
            const uint64_t x0s = static_cast<uint64_t>(x0);
            const float v00 = src[r0 + x0s], v10 = src[r0 + x0s + 1];
            const float v01 = src[r1 + x0s], v11 = src[r1 + x0s + 1];
            o[i] = (1.0f - fx) * (1.0f - fy) * v00 + fx * (1.0f - fy) * v10 +
                   (1.0f - fx) * fy * v01 + fx * fy * v11;
        }
        break;
    }
    default:
        break;   /* 索引在前置校验保证范围内; 不可达 */
    }
}

/* 从 span 槽位取 f32 指针 (元素级偏移已由 validate_slots 校验) */
static int gather_ptrs(const acs_cpu_baseline_params_v1* P,
                       const acs_span_u8& in, const acs_span_u8& out,
                       const float** ip, float** op) {
    const uint8_t* ib = in.data;
    uint8_t* ob = out.data;
    for (uint32_t s = 0; s < ACS_CPU_BASELINE_MAX_IN_SLOTS; ++s)
        ip[s] = P->in_len[s] ? reinterpret_cast<const float*>(ib + P->in_off[s] * 4)
                             : nullptr;
    for (uint32_t s = 0; s < ACS_CPU_BASELINE_MAX_OUT_SLOTS; ++s)
        op[s] = P->out_len[s] ? reinterpret_cast<float*>(ob + P->out_off[s] * 4)
                              : nullptr;
    return ACS_OK;
}

/* ───────────────────────── 并行执行 (host executor 租借) ───────────────────────── */

/* 输出带 [0,N) 按 workers 均分行带; 每线程独立 scratch。返回实际 worker 数。 */
static uint32_t run_banded(const acs_host_api_v1* host,
                           const acs_cpu_baseline_params_v1* P, uint32_t kidx,
                           const float* const* ip, float* const* op,
                           uint64_t scratch_needed) {
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    uint32_t cap = 1;
    if (host != nullptr && host->executor != nullptr) {
        const uint32_t e = host->executor->max_workers;
        if (e >= 1) cap = e;
    }
    /* 全或无租借; 失败减半; 1 = 串行兜底 */
    uint32_t workers = 1;
    if (cap > 1 && N > 1) {
        while (workers < cap && workers < N) workers <<= 1;
        if (workers > cap) workers = cap;
        if (workers > N) workers = (uint32_t)N;
        if (host != nullptr && host->executor != nullptr &&
            host->executor->acquire != nullptr && host->executor->release != nullptr) {
            while (workers > 1 &&
                   host->executor->acquire(host->executor->user_data, workers) != 0)
                workers >>= 1;
            if (workers == 1)
                (void)host->executor->acquire(host->executor->user_data, 1);
        } else {
            workers = 1;   /* 无 executor → 串行 */
        }
    }

    const uint32_t base = (uint32_t)(N / workers), rem = (uint32_t)(N % workers);
    std::vector<uint32_t> start(workers + 1, 0);
    for (uint32_t t = 0; t < workers; ++t)
        start[t + 1] = start[t] + base + (t < rem ? 1u : 0u);

    std::vector<std::vector<float>> sc(workers);
    if (scratch_needed > 0)
        for (auto& v : sc) v.resize(scratch_needed);

    /* 行带连续; tid0 主线程, 其余租借 worker 线程 */
    std::vector<std::thread> ths;
    ths.reserve(workers > 1 ? workers - 1 : 0);
    for (uint32_t t = 1; t < workers; ++t)
        ths.emplace_back([&, t]() {
            kernel_pixel_range(P, kidx, ip, op, sc[t].data(),
                               start[t], start[t + 1]);
        });
    kernel_pixel_range(P, kidx, ip, op, sc[0].data(), start[0], start[1]);
    for (auto& th : ths) th.join();

    if (host != nullptr && host->executor != nullptr &&
        host->executor->release != nullptr && workers > 0)
        host->executor->release(host->executor->user_data, workers);
    return workers;
}

/* ───────────────────────── provider vtable 函数 ───────────────────────── */

static acs_status baseline_self_test(const acs_host_api_v1* host) {
    if (host == nullptr) return ACS_ERR_PARAM;
    if (host->head.struct_size < sizeof(acs_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (host->allocator == nullptr || host->allocator->alloc == nullptr ||
        host->allocator->free == nullptr)
        return ACS_ERR_PARAM;   /* 必填 (module_api_v1.h) */
    /* allocator 往返 (对齐 64) */
    void* p = host->allocator->alloc(host->allocator->user_data, 128, 64);
    if (p == nullptr) return ACS_ERR_NOMEM;
    if ((reinterpret_cast<uintptr_t>(p) & 63u) != 0) {
        host->allocator->free(host->allocator->user_data, p);
        return ACS_ERR_SELFTEST;
    }
    std::memset(p, 0xAB, 128);
    host->allocator->free(host->allocator->user_data, p);
    /* executor 租借往返 */
    if (host->executor != nullptr && host->executor->acquire != nullptr &&
        host->executor->release != nullptr) {
        if (host->executor->acquire(host->executor->user_data, 1) != 0)
            return ACS_ERR_BUDGET;
        host->executor->release(host->executor->user_data, 1);
    }
    /* kernel 表自洽 */
    for (uint32_t i = 0; i < ACS_CPU_BASELINE_KERNEL_COUNT; ++i) {
        const acs_kernel_desc_v1& k = kKernels[i];
        if (k.head.abi_version != ACS_ABI_VERSION_V1 ||
            k.kernel_id.data == nullptr || k.kernel_id.size == 0 ||
            k.sci_contract_id.data == nullptr || k.sci_contract_id.size == 0)
            return ACS_ERR_SELFTEST;
    }
    return ACS_OK;
}

static acs_status baseline_kernel_list(const acs_host_api_v1* host,
                                       uint32_t* out_count,
                                       const acs_kernel_desc_v1** out_kernels) {
    (void)host;
    if (out_count == nullptr || out_kernels == nullptr) return ACS_ERR_PARAM;
    *out_count = ACS_CPU_BASELINE_KERNEL_COUNT;
    *out_kernels = kKernels;
    return ACS_OK;
}

static acs_status baseline_run_kernel(uint32_t kernel_index,
                                      const acs_host_api_v1* host,
                                      const void* params, uint32_t params_bytes,
                                      acs_span_u8 in, acs_span_u8 out) {
    if (kernel_index >= ACS_CPU_BASELINE_KERNEL_COUNT)
        return ACS_ERR_UNSUPPORTED;   /* host 应退其它 provider / 失败路径 */
    if (host == nullptr || params == nullptr) return ACS_ERR_PARAM;
    if (host->head.struct_size < sizeof(acs_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    acs_cpu_baseline_params_v1 P;
    if (params_bytes < sizeof(P)) return ACS_ERR_PARAM;
    std::memcpy(&P, params, sizeof(P));
    if (P.head.abi_version != ACS_ABI_VERSION_V1 ||
        P.head.struct_size < sizeof(P))
        return ACS_ERR_ABI_MISMATCH;
    if (P.w == 0 || P.h == 0) return ACS_ERR_PARAM;
    const uint64_t N = (uint64_t)P.w * (uint64_t)P.h;
    if (N == 0 || N > UINT32_MAX) return ACS_ERR_PARAM;
    if (P.flags != 0) return ACS_ERR_PARAM;
    /* cancel 点 = 调用边界 (v1) */
    if (host->cancel != nullptr && host->cancel->is_cancelled != nullptr &&
        host->cancel->is_cancelled(host->cancel->user_data))
        return ACS_ERR_CANCELLED;

    int rc = validate_slots(&P, in, out);
    if (rc != ACS_OK) return (acs_status)rc;
    rc = check_lengths(&P, kernel_index);
    if (rc != ACS_OK) return (acs_status)rc;

    const float* ip[ACS_CPU_BASELINE_MAX_IN_SLOTS] = { nullptr };
    float* op[ACS_CPU_BASELINE_MAX_OUT_SLOTS] = { nullptr };
    rc = gather_ptrs(&P, in, out, ip, op);
    if (rc != ACS_OK) return (acs_status)rc;

    /* UPM-SPMV 数据安全: rowptr 单调非减且 ≤ nnz (越界读防护) */
    if (kernel_index == ACS_CPU_KIDX_UPM_SPMV) {
        const uint64_t nnz = P.aux0;
        const float* rowptr = ip[2];
        if (rowptr == nullptr) return ACS_ERR_PARAM;
        uint64_t prev = 0;
        for (uint64_t r = 0; r <= N; ++r) {
            const float rv = rowptr[r];
            if (!(rv >= 0.0f) || rv > static_cast<float>(nnz)) return ACS_ERR_PARAM;
            const uint64_t cur = static_cast<uint64_t>(rv);
            if (cur < prev) return ACS_ERR_PARAM;
            prev = cur;
        }
    }
    /* HIPS-BULK / SPMV 索引列安全: col < ncols */
    if (kernel_index == ACS_CPU_KIDX_UPM_SPMV) {
        const uint64_t nnz = P.aux0, ncols = P.aux1;
        const float* cols = ip[1];
        for (uint64_t k = 0; k < nnz; ++k) {
            const float cv = cols[k];
            if (!(cv >= 0.0f) || cv >= static_cast<float>(ncols))
                return ACS_ERR_PARAM;
        }
    }

    const uint64_t scratch_needed =
        (kernel_index == ACS_CPU_KIDX_NOISE_REDUCTIONS ||
         kernel_index == ACS_CPU_KIDX_REJECTION_STATS)
            ? (P.aux0 > 0 ? P.aux0 : 0u) : 0u;
    (void)run_banded(host, &P, kernel_index, ip, op, scratch_needed);
    return ACS_OK;
}

/* ───────────────────────── provider 静态表 ───────────────────────── */
static const acs_provider_api_v1 g_provider_api = {
    { (uint32_t)sizeof(acs_provider_api_v1), ACS_ABI_VERSION_V1 },
    &baseline_self_test,
    &baseline_kernel_list,
    &baseline_run_kernel
};

}  // namespace astrocs_cpu_baseline

/* ───────────────────────── 唯一导出 (12 §1) ─────────────────────────
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH (不降级猜测); host 必填 allocator;
 * 能力门 (os_safe ∩ SSE2) 失败 → ACS_ERR_UNSUPPORTED (拒绝加载)。
 * 异常边界: 全部 C++ 异常捕获转 ACS_ERR_EXCEPTION (12 §4)。 */
extern "C" {

#if !defined(ASTROCS_NO_EXCEPTIONS)
#define ACS_TRY try
#define ACS_CATCH \
    catch (...) { return ACS_ERR_EXCEPTION; }
#else
#define ACS_TRY
#define ACS_CATCH
#endif

ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_provider_query_v1(uint32_t host_abi,
                          const acs_host_api_v1* host,
                          const acs_provider_api_v1** out_api) ACS_TRY {
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host == nullptr || host->allocator == nullptr) return ACS_ERR_ABI_MISMATCH;
    if (host->head.struct_size < sizeof(acs_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (out_api == nullptr) return ACS_ERR_PARAM;
    using namespace astrocs_cpu_baseline;
    acs_cap_result_v1 cap;
    const int grc = acs_cpu_baseline_cap_gate(&cap);
    if (grc != ACS_OK) return ACS_ERR_UNSUPPORTED;   /* 能力不足 → 拒绝加载 */
    *out_api = &g_provider_api;
    return ACS_OK;
} ACS_CATCH

}  // extern "C"
