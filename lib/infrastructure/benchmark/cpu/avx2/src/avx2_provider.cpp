// ACSD CPU AVX2/FMA provider — lib/infrastructure/benchmark/cpu/avx2/src/avx2_provider.cpp (CPU-003)
//
// 职责: AMD64 AVX2/FMA provider (本 TU 单独以 -mavx2 -mfma 编译; Windows
// /arch:AVX2, 15 §6 编译隔离)。
//   - 唯一导出 acsd_provider_query_v1 (lib/include/acsd/abi/module_api_v1.h 冻结;
//     ARC-001 §1.2: provider DLL 不得导出其他符号);
//   - 只迁移 profile 指定的热点 kernel (02 §10.1 / ISA-001/003 实测台账;
//     ISA-001 实测在案，测量工件留档 实验/engineering-evidence/prerelease-v5/):
//       calibration-pixel-transform (ALG-001)  +20.7%/+11.7%  SHIP
//       hips-bulk-transform (ALG-P3-002)        +28.2%/+28.3%  SHIP
//     其余 10 个注册 kernel 不在本 provider 表内 → run_kernel 返回
//     ACS_ERR_UNSUPPORTED, host 按 kernel 逐条回落 baseline (15 §1
//     "高级 provider 只实现已证明热点; 其余返回 unsupported 由 host 使用
//     baseline"; 函数入口由 provider 表查询, 不复制科学模块);
//   - 参数合同 = baseline provider v1 同一 POD (acsd_cpu_baseline_params_v1;
//     CPU-002 冻结, 本 provider include 复用不复制定义); 数值公式与
//     baseline kernel 实现同式 (ALG-001/ALG-P3-002 离散公式; CPU-002 oracle
//     同源对照) —— 差异仅: (a) 编译旗标 -mavx2 -mfma, (b) 注册 kernel 子集;
//   - query 握手: host_abi 失配 → ACS_ERR_ABI_MISMATCH; host 必填 allocator;
//     out_api = 静态 provider 表;
//   - 加载判定 (capability 门): acsd_cpu_avx2_cap_gate 要求
//     required_features (AVX|AVX2|FMA) ⊆ os_safe 平面 (CPUID + OSXSAVE +
//     XGETBV XMM|YMM; CPU-001 classify 组包含) —— 非支持 CPU (硬件缺
//     AVX2/FMA 或 OS 不保存 YMM) → ACS_ERR_UNSUPPORTED 拒绝加载 (15 §2;
//     CPUID/XGETBV negative 路径由 stub 注入测试覆盖);
//   - 无全局 SIMD 静态初始化: 全局对象仅 POD/字符串表 (静态 init 无 SIMD
//     指令), 加载期不执行任何 AVX2/FMA 指令 (12 §7; -mavx2 -mfma 只作用于
//     本 TU 编译, 不改变静态初始化语义);
//   - 并行: 每输出元素独立 → bitwise 确定性不随 worker 数变化 (ARCH-004
//     §4; 对照测试 budget 1 vs 4 证明); worker 经 host executor 租借
//     (FORBID-003); 无 executor / 租借失败 → 串行兜底 (05 §6 保守)。
//
// FMA 归约顺序记录 (15 §6; 详见 CPU_003_AVX2_PROVIDER.md §6):
//   - calibration: 逐元素无归约 (每输出 = (in0-in1-k*in2)*in3 独立表达式);
//     -mfma 仅将乘加收缩为 vfnmadd (一次舍入替代两次), 不改变任何求和顺序;
//   - hips-bulk: 每输出 = 4 项双线性乘积的固定项序标量累加 (源码序
//     (1-fx)(1-fy)v00 + fx(1-fy)v10 + (1-fx)fy v01 + fx fy v11; 无
//     -ffast-math/重排); -mfma 将乘加链收缩 (减少中间舍入), **不改变项序**;
//     输出仍逐元素独立 → bitwise 不随 worker 数变化;
//   - 结论: FMA 不改变归约顺序 (两 kernel 均无跨项/跨线程归约); 可能引入
//     每元素 ≤ 数 ULP 舍入差 → baseline 对照容差 2e-4 相对 (与 ALG oracle
//     同规; 冻结于 CPU_003_AVX2_PROVIDER.md §7)。
#include "acsd/cpu/avx2_provider_v1.h"
#include "acsd/cpu/capability_v1.h"
#include "acsd/cpu/cpuprov_kernels_v1.h"   /* R-60: 计算面跨 TU 桥 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if !defined(ACSD_NO_EXCEPTIONS)
#include <stdexcept>
#endif

#if defined(ACSD_ABI_SHARED) && !defined(ACSD_ABI_EXPORTS)
#define ACSD_ABI_EXPORTS 1
#endif

/* ───────────────────────── 能力门 (query 期) ─────────────────────────
 * 加载判定只使用 os_safe 平面 (15 §2): required = AVX|AVX2|FMA ⊆ os_safe。
 * 生产链接真实 capability_detect.c; 非支持 CPU (缺 AVX2/FMA / OS 不保存
 * YMM) 负测由测试以 stub 探测注入 (eng/tests/cpu/avx2/
 * provider_avx2_capability_gate_test.c 链接期替换 acsd_cap_detect_v1 /
 * acsd_cap_os_safe_satisfies_v1)。本函数为 extern "C" 顶层符号 (host/测试
 * 可直接判定; 不属 provider 导出白名单)。 */
extern "C" int acsd_cpu_avx2_cap_gate(acsd_cap_result_v1* out) {
    if (out == nullptr) return ACS_ERR_PARAM;
    acsd_cap_result_v1 c;
    std::memset(&c, 0, sizeof(c));
    c.struct_size = (uint32_t)sizeof(acsd_cap_result_v1);
    c.abi_version = ACS_CAP_ABI_VERSION_V1;
    const int rc = acsd_cap_detect_v1(&c);
    if (rc != ACS_CAP_OK) return ACS_ERR_UNSUPPORTED;
    if (!acsd_cap_os_safe_satisfies_v1(&c, ACS_CPU_AVX2_REQUIRED_FEATURES))
        return ACS_ERR_UNSUPPORTED;   /* 非支持 CPU / OS 不保存 YMM → 拒绝 */
    *out = c;
    return ACS_OK;
}

namespace acsd_cpu_avx2 {

/* ───────────────────────── 字符串构造辅助 (静态 init, 无 SIMD) ───────────────────────── */
static acsd_str_v1 mkstr(const char* s) {
    acsd_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acsd_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)std::strlen(s) : 0u;
    return v;
}

/* ───────────────────────── kernel 描述静态表 (2; 序 = 索引) ─────────────────────────
 * kernel_id/sci_contract_id 与 baseline (CPU-002) 同一科学 kernel 身份
 * (avx2 是该 kernel 的已实测更快 ISA 实现; host 以 kernel_id 粒度在 provider
 * 间逐 kernel 选路 — CPU-005 路由语义, 04_CPU_RESOURCE_TASKS CPU-005)。
 * determinism_class: 0=bitwise (逐元素独立; 无跨线程归约)。 */
static const acsd_kernel_desc_v1 kKernels[ACS_CPU_AVX2_KERNEL_COUNT] = {
    { { (uint32_t)sizeof(acsd_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("calibration-pixel-transform"), mkstr("ALG-001"),
      ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE },
    { { (uint32_t)sizeof(acsd_kernel_desc_v1), ACS_ABI_VERSION_V1 },
      mkstr("hips-bulk-transform"), mkstr("ALG-P3-002"),
      ACS_CPU_BASELINE_PREC_F32, ACS_CPU_BASELINE_DET_BITWISE }
};

/* ───────────────────────── 参数校验 (仅注册热点; 合同同 baseline v1) ─────────────────────────
 * 槽位语义逐 op (同 baseline_provider_v1.h 头注释; 参数 POD = baseline 同型):
 *   calibration (AVX2_KIDX_CALIBRATION): 4 入 1 出, 各 ≥ N=w*h;
 *   hips-bulk   (AVX2_KIDX_HIPS_BULK):   1 入 (≥ iw*ih, iw=aux0, ih=aux1 ≥2)
 *                                         1 出 (≥ N);
 * 未使用槽必须 off=len=0; 越界 → ACS_ERR_PARAM; 未知 kernel 索引 →
 * ACS_ERR_UNSUPPORTED (host 回落 baseline)。 */
static int avx2_validate(const acsd_cpu_baseline_params_v1* P,
                         uint32_t kidx,
                         const acsd_span_u8& in, const acsd_span_u8& out) {
    if (in.data == nullptr || out.data == nullptr) return ACS_ERR_PARAM;
    if (P->head.struct_size < sizeof(*P) ||
        P->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    const uint64_t in_cap  = in.count >> 2;
    const uint64_t out_cap = out.count >> 2;
    if ((in.count & 3u) != 0 || (out.count & 3u) != 0) return ACS_ERR_PARAM;
    for (uint32_t s = 0; s < ACS_CPU_AVX2_MAX_IN_SLOTS; ++s) {
        if (P->in_len[s] == 0) {
            if (P->in_off[s] != 0) return ACS_ERR_PARAM;
            continue;
        }
        if (P->in_off[s] > in_cap || P->in_len[s] > in_cap - P->in_off[s])
            return ACS_ERR_PARAM;
    }
    for (uint32_t s = 0; s < ACS_CPU_AVX2_MAX_OUT_SLOTS; ++s) {
        if (P->out_len[s] == 0) {
            if (P->out_off[s] != 0) return ACS_ERR_PARAM;
            continue;
        }
        if (P->out_off[s] > out_cap || P->out_len[s] > out_cap - P->out_off[s])
            return ACS_ERR_PARAM;
    }
    const uint64_t N = (uint64_t)P->w * (uint64_t)P->h;
    auto req_in = [&](unsigned s, uint64_t need) -> bool {
        return P->in_len[s] >= need;
    };
    auto req_out = [&](unsigned s, uint64_t need) -> bool {
        return P->out_len[s] >= need;
    };
    switch (kidx) {
    case ACS_CPU_AVX2_KIDX_CALIBRATION:
        return (req_in(0, N) && req_in(1, N) && req_in(2, N) && req_in(3, N) &&
                req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    case ACS_CPU_AVX2_KIDX_HIPS_BULK: {
        const uint64_t iw = P->aux0, ih = P->aux1;
        if (iw < 2 || ih < 2 || iw > UINT64_MAX / ih) return ACS_ERR_PARAM;
        return (req_in(0, iw * ih) && req_out(0, N)) ? ACS_OK : ACS_ERR_PARAM;
    }
    default:
        return ACS_ERR_UNSUPPORTED;   /* 非热点 kernel → host 回落 baseline */
    }
}

/* 从 span 槽位取 f32 指针 (越界已由 avx2_validate 校验) */
static void gather_ptrs(const acsd_cpu_baseline_params_v1* P,
                        const acsd_span_u8& in, const acsd_span_u8& out,
                        const float** ip, float** op) {
    const uint8_t* ib = in.data;
    uint8_t* ob = out.data;
    for (uint32_t s = 0; s < ACS_CPU_AVX2_MAX_IN_SLOTS; ++s)
        ip[s] = P->in_len[s] ? reinterpret_cast<const float*>(ib + P->in_off[s] * 4)
                             : nullptr;
    for (uint32_t s = 0; s < ACS_CPU_AVX2_MAX_OUT_SLOTS; ++s)
        op[s] = P->out_len[s] ? reinterpret_cast<float*>(ob + P->out_off[s] * 4)
                              : nullptr;
}

/* ───────────────────────── 热点 kernel 数值实现 ─────────────────────────
 * 公式与算术序与 baseline provider (CPU-002) 同式 (ALG-001 / ALG-P3-002 离散
 * 公式; CPU-002 oracle 同源) —— 本 provider 只迁移经 profile 证明的热点
 * (calibration/hips), 不复制其余 10 个科学 kernel; 数值语义零变更
 * (scientific_change=false), 差异仅编译旗标 (-mavx2 -mfma 自动向量化 +
 * FMA 收缩; 可能每元素 ≤ 数 ULP 舍入差, 容差 2e-4 冻结)。
 * 本实现写为标量源码 (与 legacy ISA-001 avx2 变体同策略: 共享同式源 +
 * TU 局部旗标), 由编译器按 -mavx2 -mfma 自动向量化; 不手写 intrinsic
 * (防科学漂移, 02 §10.1)。 */
/* ───────────────────────── 热点 kernel 数值实现（已移出本 TU） ─────────────────────────
 * R-60 配方: 非 GCC 工具链没有函数级指令集覆盖（pragma 全表无 target、
 * __declspec 全表无 cpu_specific/cpu_dispatch、/arch: 的作用域是整个 TU）
 * ⇒ 若计算循环与 query/self_test/cap_gate 同 TU，握手入口在高 ISA 主机上会与
 * 变体同码（改前产物自证: cap_gate 函数体含 18 处 VEX/EVEX）—— 能力预检不过
 * 则应"干净拒绝"，同码则退化成本不该发生的指令集要求。
 * 现计算面在 lib/infrastructure/benchmark/cpu/avx2/src/avx2_kernels.cpp
 * （唯一带 ISA 旗标的 TU），本 TU（门面）零 ISA 旗标，两者只经唯一跨 TU 桥
 * acsd_cpuprov_kernel_range_v1 相连（见 acsd/cpu/cpuprov_kernels_v1.h）。
 * 数值源码逐字符搬移: 公式/项序/容差零变更。 */
/* ───────────────────────── 并行执行 (host executor 租借) ─────────────────────────
 * 同 baseline 语义: 输出带 [0,N) 按 workers 均分行带 (每输出元素独立 →
 * bitwise 确定不随 worker 数变化); 全或无租借, 失败减半, 1=串行兜底。 */
static uint32_t run_banded(const acsd_host_api_v1* host,
                           const acsd_cpu_baseline_params_v1* P, uint32_t kidx,
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
            acsd_cpuprov_kernel_range_v1(P, kidx, ip, op, start[t], start[t + 1]);
        });
    acsd_cpuprov_kernel_range_v1(P, kidx, ip, op, start[0], start[1]);
    for (auto& th : ths) th.join();

    if (host != nullptr && host->executor != nullptr &&
        host->executor->release != nullptr && workers > 0)
        host->executor->release(host->executor->user_data, workers);
    return workers;
}

/* ───────────────────────── provider vtable 函数 ───────────────────────── */

static acsd_status avx2_self_test(const acsd_host_api_v1* host) {
    if (host == nullptr) return ACS_ERR_PARAM;
    if (host->head.struct_size < sizeof(acsd_host_api_v1) ||
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
    /* kernel 表自洽: 2 条 desc 头/串非空 */
    for (uint32_t i = 0; i < ACS_CPU_AVX2_KERNEL_COUNT; ++i) {
        const acsd_kernel_desc_v1& k = kKernels[i];
        if (k.head.abi_version != ACS_ABI_VERSION_V1 ||
            k.kernel_id.data == nullptr || k.kernel_id.size == 0 ||
            k.sci_contract_id.data == nullptr || k.sci_contract_id.size == 0)
            return ACS_ERR_SELFTEST;
    }
    return ACS_OK;
}

static acsd_status avx2_kernel_list(const acsd_host_api_v1* host,
                                   uint32_t* out_count,
                                   const acsd_kernel_desc_v1** out_kernels) {
    (void)host;
    if (out_count == nullptr || out_kernels == nullptr) return ACS_ERR_PARAM;
    *out_count = ACS_CPU_AVX2_KERNEL_COUNT;
    *out_kernels = kKernels;
    return ACS_OK;
}

static acsd_status avx2_run_kernel(uint32_t kernel_index,
                                  const acsd_host_api_v1* host,
                                  const void* params, uint32_t params_bytes,
                                  acsd_span_u8 in, acsd_span_u8 out) {
    if (kernel_index >= ACS_CPU_AVX2_KERNEL_COUNT)
        return ACS_ERR_UNSUPPORTED;   /* 非热点 kernel → host 回落 baseline */
    if (host == nullptr || params == nullptr) return ACS_ERR_PARAM;
    if (host->head.struct_size < sizeof(acsd_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    acsd_cpu_baseline_params_v1 P;
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

    int rc = avx2_validate(&P, kernel_index, in, out);
    if (rc != ACS_OK) return (acsd_status)rc;

    const float* ip[ACS_CPU_AVX2_MAX_IN_SLOTS] = { nullptr };
    float* op[ACS_CPU_AVX2_MAX_OUT_SLOTS] = { nullptr };
    gather_ptrs(&P, in, out, ip, op);
    (void)run_banded(host, &P, kernel_index, ip, op);
    return ACS_OK;
}

/* ───────────────────────── provider 静态表 ───────────────────────── */
static const acsd_provider_api_v1 g_provider_api = {
    { (uint32_t)sizeof(acsd_provider_api_v1), ACS_ABI_VERSION_V1 },
    &avx2_self_test,
    &avx2_kernel_list,
    &avx2_run_kernel
};

}  // namespace acsd_cpu_avx2

/* ───────────────────────── 唯一导出 (12 §1) ─────────────────────────
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH; host 必填 allocator; 能力门
 * (AVX|AVX2|FMA ⊆ os_safe) 失败 → ACS_ERR_UNSUPPORTED (非支持 CPU 不加载;
 * host 回落 baseline)。异常边界: C++ 异常捕获转 ACS_ERR_EXCEPTION (12 §4)。 */
extern "C" {

#if !defined(ACSD_NO_EXCEPTIONS)
#define ACS_TRY try
#define ACS_CATCH \
    catch (...) { return ACS_ERR_EXCEPTION; }
#else
#define ACS_TRY
#define ACS_CATCH
#endif

ACSD_EXPORT acsd_status ACSD_CALL
acsd_provider_query_v1(uint32_t host_abi,
                          const acsd_host_api_v1* host,
                          const acsd_provider_api_v1** out_api) ACS_TRY {
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host == nullptr || host->allocator == nullptr) return ACS_ERR_ABI_MISMATCH;
    if (host->head.struct_size < sizeof(acsd_host_api_v1) ||
        host->head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (out_api == nullptr) return ACS_ERR_PARAM;
    using namespace acsd_cpu_avx2;
    acsd_cap_result_v1 cap;
    const int grc = acsd_cpu_avx2_cap_gate(&cap);
    if (grc != ACS_OK) return ACS_ERR_UNSUPPORTED;   /* 非支持 CPU → 拒绝加载 */
    *out_api = &g_provider_api;
    return ACS_OK;
} ACS_CATCH

}  // extern "C"
