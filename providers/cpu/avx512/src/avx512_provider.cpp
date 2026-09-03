// AstroCS CPU AVX-512 provider — providers/cpu/avx512/src/avx512_provider.cpp (CPU-004)
//
// 职责: AMD64 AVX-512 provider (本 TU 单独以 -mavx512f -mavx512cd
// -mavx512bw -mavx512dq -mavx512vl 编译; Windows /arch:AVX512, 15 §6
// 编译隔离)。
//   - 唯一导出 astrocs_provider_query_v1 (include/astrocs/abi/module_api_v1.h
//     冻结; ARC-001 §1.2: provider DLL 不得导出其他符号);
//   - 只迁移实测可能获益的热点 kernel (ISA-004 台账;
//     artifacts/prerelease_v5/ISA-004/MEASUREMENTS.csv):
//       hips-bulk-transform (ALG-P3-002)  avx512 +29.5% vs baseline (≈avx2
//       +28.3% 同档; EVEX 512-bit 向量化获益)  SHIP
//     其余 kernel 不注册 → run_kernel 返回 ACS_ERR_UNSUPPORTED, host 按
//     kernel_id 退回 avx2/baseline (15 §1 "每 kernel 可退回"; CPU-005 路由):
//       calibration-pixel-transform ISA-004 实测 +3.8% (远低 avx2 +11.7%)
//         → NOT_SHIPPED (无额外获益);
//       drizzle-accumulate −22.5% (变体更慢) → NOT_SHIPPED;
//     防 AVX-512 降频使全局性能变差: 只注册实测获益 kernel, 不机械堆砌
//     (ISA_VARIANTS.md §1.6 判定 / 15 §3)。
//   - 参数合同 = baseline provider v1 同一 POD (acs_cpu_baseline_params_v1;
//     CPU-002 冻结, include 复用不复制); 数值公式与 baseline/avx2 kernel
//     实现同式 (ALG-P3-002 离散公式) —— 差异仅编译旗标与注册子集;
//   - query 握手: host_abi 失配 → ACS_ERR_ABI_MISMATCH; host 必填
//     allocator; out_api = 静态 provider 表;
//   - 加载判定 (AVX-512 能力门): acs_cpu_avx512_cap_gate 要求
//     required = F|CD|BW|DQ|VL 五子集 ⊆ os_safe 平面 (CPU-001 classify:
//     OSXSAVE + XGETBV opmask|ZMM_Hi256|Hi16_ZMM=0xE0 + 五子集 hw 全置) ——
//     缺任一子集 / OS 不保存 ZMM state → ACS_ERR_UNSUPPORTED 拒绝加载
//     (15 §2 "不能只看一个 AVX512F=true"; CPUID/XGETBV negative 由 stub
//     注入测试覆盖);
//   - 非法指令保护: 本 TU 只在 run_kernel 执行路径内含 AVX-512 指令;
//     query/self_test/静态初始化零 EVEX 指令 (全局仅 POD/字符串表) →
//     非支持 CPU 上 dlopen + query 安全返回 UNSUPPORTED, 无 #UD;
//     oracle/so_load 测试在真实支持 CPU 证明可执行, gate 负测证明拒绝;
//   - 无全局 SIMD 静态初始化 (12 §7); worker 经 host executor 租借
//     (FORBID-003); 无 executor / 租借失败 → 串行兜底。
//
// FMA/AVX-512 归约顺序记录 (15 §6; 详见 avx512_provider_v1.h 头注释 §6):
//   - hips-bulk: 每输出 = 4 项双线性乘积固定源码序标量累加 (无
//     -ffast-math/重排); AVX-512 只向量化循环/收缩乘加, 不改变项序;
//     输出逐元素独立 → bitwise 不随 worker 数变化;
//   - 结论: AVX-512 不改变归约顺序; 可能每元素 ≤ 数十 ULP 舍入差 →
//     baseline 对照容差 2e-4 相对冻结 (avx512_provider_v1.h 头注释 §6)。
#include "astrocs/cpu/avx512_provider_v1.h"
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
 * 加载判定只使用 os_safe 平面 (15 §2): required = AVX-512 五子集 ⊆ os_safe。
 * 生产链接真实 capability_detect.c; 缺子集/OS 不保存 ZMM 负测由测试 stub
 * 注入 (tests/cpu/avx512/provider_avx512_capability_gate_test.c 链接期替换
 * acs_cap_detect_v1 / acs_cap_os_safe_satisfies_v1)。本函数为 extern "C"
 * 顶层符号 (host/测试可直接判定; 不属 provider 导出白名单)。
 *
 * 非法指令保护 (CPU-004 验收核心): 本 TU 以 -mavx512* 编译, 编译器默认
 * 会用 512-bit EVEX (vmovdqu64 %zmm 等) 内联实现大结构体 (acs_cap_result_v1
 * 约 200B) 的 memset/整拷 —— cap_gate 恰是"判定本 CPU 是否支持 AVX-512"的
 * 函数, 若函数体含 EVEX, 在缺 AVX-512 的 CPU 上**探测自身前**即 #UD
 * (鸡生蛋)。故本函数以 target attribute 强制禁 AVX-512/EVEX 指令生成
 * (退回 SSE2 通用整块拷贝), 保证 query/握手路径 (dlopen 静态 init +
 * query + cap_gate) 在任何不支持 CPU 上可安全执行并返回 UNSUPPORTED。
 * (GCC/Clang 支持函数级 target 覆盖 TU 级 -mavx512*; MSVC 侧由
 * 09_WINDOWS_TOOLCHAIN 另行处理 —— Linux 控制节点验收以本 attribute 为准;
 * 该 attribute 同时把本函数的 memset/copy 内联压回非 EVEX 指令。)
 * 归属与证据: tests/cpu/avx512/check_avx512_illegal_instr.py 反汇编断言
 * cap_gate/query/.init 函数体零 %zmm。 */
#if defined(__GNUC__) || defined(__clang__)
#define ACS_CPU_AVX512_CAP_GATE_NOEVEX \
    __attribute__((target("no-avx512f,no-avx512cd,no-avx512bw,no-avx512dq,no-avx512vl")))
#else
#define ACS_CPU_AVX512_CAP_GATE_NOEVEX /* 非 GCC/Clang: 无函数级 target; TU 级旗标由构建隔离 */
#endif

extern "C" ACS_CPU_AVX512_CAP_GATE_NOEVEX
int acs_cpu_avx512_cap_gate(acs_cap_result_v1* out) {
    if (out == nullptr) return ACS_ERR_PARAM;
    acs_cap_result_v1 c;
    std::memset(&c, 0, sizeof(c));
    c.struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    c.abi_version = ACS_CAP_ABI_VERSION_V1;
    const int rc = acs_cap_detect_v1(&c);
    if (rc != ACS_CAP_OK) return ACS_ERR_UNSUPPORTED;
    if (!acs_cap_os_safe_satisfies_v1(&c, ACS_CPU_AVX512_REQUIRED_FEATURES))
        return ACS_ERR_UNSUPPORTED;   /* 缺子集 / OS 不保存 ZMM → 拒绝 */
    *out = c;
    return ACS_OK;
}

namespace astrocs_cpu_avx512 {

/* ───────────────────────── 字符串构造辅助 (静态 init, 无 SIMD) ───────────────────────── */
static acs_str_v1 mkstr(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)std::strlen(s) : 0u;
    return v;
}

/* ───────────────────────── kernel 描述静态表 (1; 序 = 索引) ─────────────────────────
 * kernel_id/sci_contract_id 与 baseline/avx2 (CPU-002/003) 同一科学 kernel
 * 身份 (hips-bulk-transform = ALG-P3-002; host 以 kernel_id 粒度选路)。
 * determinism_class: 0=bitwise (逐元素独立; 无跨线程归约)。 */
static const acs_kernel_desc_v1 kKernels[ACS_CPU_AVX512_KERNEL_COUNT] = {
    { { (uint32_t)sizeof(acs_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("hips-bulk-transform"), mkstr("ALG-P3-002"),
      ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE }
};

/* ───────────────────────── 参数校验 (仅注册热点; 合同同 baseline v1) ─────────────────────────
 * hips-bulk (AVX512_KIDX_HIPS_BULK): 1 入 (≥ iw*ih, iw=aux0, ih=aux1 ≥2)
 *                                     1 出 (≥ N=w*h);
 * 未用槽 off=len=0; 越界 → ACS_ERR_PARAM; 未知索引 → ACS_ERR_UNSUPPORTED。 */
static int avx512_validate(const acs_cpu_baseline_params_v1* P,
                           uint32_t kidx,
                           const acs_span_u8& in, const acs_span_u8& out) {
    if (in.data == nullptr || out.data == nullptr) return ACS_ERR_PARAM;
    if (P->head.struct_size < sizeof(*P) ||
        P->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    const uint64_t in_cap  = in.count >> 2;
    const uint64_t out_cap = out.count >> 2;
    if ((in.count & 3u) != 0 || (out.count & 3u) != 0) return ACS_ERR_PARAM;
    for (uint32_t s = 0; s < ACS_CPU_AVX512_MAX_IN_SLOTS; ++s) {
        if (P->in_len[s] == 0) {
            if (P->in_off[s] != 0) return ACS_ERR_PARAM;
            continue;
        }
        if (P->in_off[s] > in_cap || P->in_len[s] > in_cap - P->in_off[s])
            return ACS_ERR_PARAM;
    }
    for (uint32_t s = 0; s < ACS_CPU_AVX512_MAX_OUT_SLOTS; ++s) {
        if (P->out_len[s] == 0) {
            if (P->out_off[s] != 0) return ACS_ERR_PARAM;
            continue;
        }
        if (P->out_off[s] > out_cap || P->out_len[s] > out_cap - P->out_off[s])
            return ACS_ERR_PARAM;
    }
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    switch (kidx) {
    case ACS_CPU_AVX512_KIDX_HIPS_BULK: {
        const uint64_t iw = P->aux0, ih = P->aux1;
        if (iw < 2 || ih < 2 || iw > UINT64_MAX / ih) return ACS_ERR_PARAM;
        return (P->in_len[0] >= iw * ih && P->out_len[0] >= N)
                   ? ACS_OK : ACS_ERR_PARAM;
    }
    default:
        return ACS_ERR_UNSUPPORTED;   /* 非热点 kernel → host 退回 avx2/baseline */
    }
}

static void gather_ptrs(const acs_cpu_baseline_params_v1* P,
                        const acs_span_u8& in, const acs_span_u8& out,
                        const float** ip, float** op) {
    const uint8_t* ib = in.data;
    uint8_t* ob = out.data;
    for (uint32_t s = 0; s < ACS_CPU_AVX512_MAX_IN_SLOTS; ++s)
        ip[s] = P->in_len[s] ? reinterpret_cast<const float*>(ib + P->in_off[s] * 4)
                             : nullptr;
    for (uint32_t s = 0; s < ACS_CPU_AVX512_MAX_OUT_SLOTS; ++s)
        op[s] = P->out_len[s] ? reinterpret_cast<float*>(ob + P->out_off[s] * 4)
                              : nullptr;
}

/* ───────────────────────── 热点 kernel 数值实现 ─────────────────────────
 * 公式与算术序与 baseline/avx2 provider (CPU-002/003) 同式 (ALG-P3-002 离散
 * 公式) —— 只迁移实测获益热点 (hips); 数值语义零变更
 * (scientific_change=false), 差异仅编译旗标 (-mavx512* 自动向量化 + FMA
 * 收缩; 可能每元素 ≤ 数十 ULP 舍入差, 容差 2e-4 冻结)。
 * 标量源码 + TU 局部旗标 (同 avx2/legacy ISA-004 策略), 不手写 intrinsic。 */
static void kernel_pixel_range(const acs_cpu_baseline_params_v1* P, uint32_t kidx,
                               const float* const* in, float* const* out,
                               uint64_t i0, uint64_t i1) {
    const float kf = P->k;
    switch (kidx) {
    case ACS_CPU_AVX512_KIDX_HIPS_BULK: {
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
            /* 固定项序: v00 → v10 → v01 → v11 (与 baseline/avx2 同式;
             * AVX-512/FMA 只收缩乘加, 不重排项序) */
            o[i] = (1.0f - fx) * (1.0f - fy) * v00 + fx * (1.0f - fy) * v10 +
                   (1.0f - fx) * fy * v01 + fx * fy * v11;
        }
        break;
    }
    default:
        break;   /* 不可达 (avx512_validate 已挡) */
    }
}

/* ───────────────────────── 并行执行 (host executor 租借) ─────────────────────────
 * 同 baseline/avx2 语义: 行带划分, 每输出独立 → bitwise 确定; 全或无租借。 */
static uint32_t run_banded(const acs_host_api_v1* host,
                           const acs_cpu_baseline_params_v1* P, uint32_t kidx,
                           const float* const* ip, float* const* op) {
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    uint32_t cap = 1;
    if (host != nullptr && host->executor != nullptr) {
        const uint32_t e = host->executor->max_workers;
        if (e >= 1) cap = e;
    }
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
            workers = 1;
        }
    }

    const uint32_t base = (uint32_t)(N / workers), rem = (uint32_t)(N % workers);
    std::vector<uint32_t> start(workers + 1, 0);
    for (uint32_t t = 0; t < workers; ++t)
        start[t + 1] = start[t] + base + (t < rem ? 1u : 0u);

    std::vector<std::thread> ths;
    ths.reserve(workers > 1 ? workers - 1 : 0);
    for (uint32_t t = 1; t < workers; ++t)
        ths.emplace_back([&, t]() {
            kernel_pixel_range(P, kidx, ip, op, start[t], start[t + 1]);
        });
    kernel_pixel_range(P, kidx, ip, op, start[0], start[1]);
    for (auto& th : ths) th.join();

    if (host != nullptr && host->executor != nullptr &&
        host->executor->release != nullptr && workers > 0)
        host->executor->release(host->executor->user_data, workers);
    return workers;
}

/* ───────────────────────── provider vtable 函数 ───────────────────────── */

static acs_status avx512_self_test(const acs_host_api_v1* host) {
    if (host == nullptr) return ACS_ERR_PARAM;
    if (host->head.struct_size < sizeof(acs_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (host->allocator == nullptr || host->allocator->alloc == nullptr ||
        host->allocator->free == nullptr)
        return ACS_ERR_PARAM;   /* 必填 (module_api_v1.h) */
    void* p = host->allocator->alloc(host->allocator->user_data, 128, 64);
    if (p == nullptr) return ACS_ERR_NOMEM;
    if ((reinterpret_cast<uintptr_t>(p) & 63u) != 0) {
        host->allocator->free(host->allocator->user_data, p);
        return ACS_ERR_SELFTEST;
    }
    std::memset(p, 0xAB, 128);
    host->allocator->free(host->allocator->user_data, p);
    if (host->executor != nullptr && host->executor->acquire != nullptr &&
        host->executor->release != nullptr) {
        if (host->executor->acquire(host->executor->user_data, 1) != 0)
            return ACS_ERR_BUDGET;
        host->executor->release(host->executor->user_data, 1);
    }
    /* kernel 表自洽 */
    for (uint32_t i = 0; i < ACS_CPU_AVX512_KERNEL_COUNT; ++i) {
        const acs_kernel_desc_v1& k = kKernels[i];
        if (k.head.abi_version != ACS_ABI_VERSION_V1 ||
            k.kernel_id.data == nullptr || k.kernel_id.size == 0 ||
            k.sci_contract_id.data == nullptr || k.sci_contract_id.size == 0)
            return ACS_ERR_SELFTEST;
    }
    return ACS_OK;
}

static acs_status avx512_kernel_list(const acs_host_api_v1* host,
                                     uint32_t* out_count,
                                     const acs_kernel_desc_v1** out_kernels) {
    (void)host;
    if (out_count == nullptr || out_kernels == nullptr) return ACS_ERR_PARAM;
    *out_count = ACS_CPU_AVX512_KERNEL_COUNT;
    *out_kernels = kKernels;
    return ACS_OK;
}

static acs_status avx512_run_kernel(uint32_t kernel_index,
                                    const acs_host_api_v1* host,
                                    const void* params, uint32_t params_bytes,
                                    acs_span_u8 in, acs_span_u8 out) {
    if (kernel_index >= ACS_CPU_AVX512_KERNEL_COUNT)
        return ACS_ERR_UNSUPPORTED;   /* 非热点 kernel → host 退回 avx2/baseline */
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
    if (host->cancel != nullptr && host->cancel->is_cancelled != nullptr &&
        host->cancel->is_cancelled(host->cancel->user_data))
        return ACS_ERR_CANCELLED;   /* cancel 点 = 调用边界 (v1) */

    int rc = avx512_validate(&P, kernel_index, in, out);
    if (rc != ACS_OK) return (acs_status)rc;

    const float* ip[ACS_CPU_AVX512_MAX_IN_SLOTS] = { nullptr };
    float* op[ACS_CPU_AVX512_MAX_OUT_SLOTS] = { nullptr };
    gather_ptrs(&P, in, out, ip, op);
    (void)run_banded(host, &P, kernel_index, ip, op);
    return ACS_OK;
}

/* ───────────────────────── provider 静态表 ───────────────────────── */
static const acs_provider_api_v1 g_provider_api = {
    { (uint32_t)sizeof(acs_provider_api_v1), ACS_ABI_VERSION_V1 },
    &avx512_self_test,
    &avx512_kernel_list,
    &avx512_run_kernel
};

}  // namespace astrocs_cpu_avx512

/* ───────────────────────── 唯一导出 (12 §1) ─────────────────────────
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH; host 必填 allocator; AVX-512 能力门
 * (五子集 ⊆ os_safe) 失败 → ACS_ERR_UNSUPPORTED (缺子集/OS 不保存 ZMM →
 * 不加载; host 退回 avx2/baseline)。异常边界: C++ 异常捕获转
 * ACS_ERR_EXCEPTION (12 §4)。 */
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
    using namespace astrocs_cpu_avx512;
    acs_cap_result_v1 cap;
    const int grc = acs_cpu_avx512_cap_gate(&cap);
    if (grc != ACS_OK) return ACS_ERR_UNSUPPORTED;   /* 缺子集/ZMM state → 拒绝 */
    *out_api = &g_provider_api;
    return ACS_OK;
} ACS_CATCH

}  // extern "C"
