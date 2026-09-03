/* AstroCS CPU 能力探测实现 — providers/cpu/common/src/capability_detect.c (CPU-001)
 *
 * 职责: acs_cap_* C ABI (capability_v1.h) 的 AMD64 实现。
 *   - CPUID 叶 0/1/7.0/80000000-4 只读探测 (feature/厂商/型号);
 *   - OSXSAVE + XGETBV(0) 实测 XCR0 (仅当 CPUID.1:ECX.OSXSAVE=1 才执行
 *     XGETBV, 保证探测自身永不触发非法指令);
 *   - hw_features (硬件支持) 与 os_safe (硬件 ∩ OS 状态 ∩ 组包含后可安全
 *     执行) 双平面;
 *   - 稳定 JSON 序列化 (键序固定; Windows/Linux 同一 schema; 无第三方依赖)。
 *
 * 并发: 全部 reentrant=yes; threadsafe=yes; internal_parallel=none; 无分配。
 * 编译: 纯 C11; GCC/Clang (cpuid.h + x86intrin.h) 与 MSVC (_M_X64 intrin.h)
 *       同源契约; x86 非 amd64 (__i386__/_M_IX86) 亦可用作编译面, 探测
 *       只在 amd64 语义上权威 (32 位下 OS 状态位仍适用)。
 *
 * 探测自身只用 SSE2 可执行指令 + cpuid/xgetbv; 不触碰任何 AVX* 指令
 * (CPU-002 起的高级 kernel 加载方在调用本探测并确认 os_safe 后才可执行)。
 */
#include "astrocs/cpu/capability_v1.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <x86intrin.h>
#define ACS_CAP_USE_GCC_CPUID 1
#elif defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#define ACS_CAP_USE_MSVC_CPUID 1
#else
#define ACS_CAP_NO_CPUID 1
#endif

/* ───────── CPUID 单叶读取 (每平台小包装) ───────── */
static void cap_cpuid(uint32_t leaf, uint32_t subleaf,
                      uint32_t* eax, uint32_t* ebx,
                      uint32_t* ecx, uint32_t* edx) {
#if defined(ACS_CAP_USE_GCC_CPUID)
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
#elif defined(ACS_CAP_USE_MSVC_CPUID)
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    *eax = (uint32_t)regs[0]; *ebx = (uint32_t)regs[1];
    *ecx = (uint32_t)regs[2]; *edx = (uint32_t)regs[3];
#else
    (void)leaf; (void)subleaf; (void)eax; (void)ebx; (void)ecx; (void)edx;
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

/* XGETBV(0): 仅当 OSXSAVE=1 后调用 (调用方保证); GCC/Clang 需 target("xsave")
 * 使 _xgetbv 可编译 (探测函数自身不要求全局 -mxsave)。 */
#if defined(ACS_CAP_USE_GCC_CPUID)
__attribute__((target("xsave")))
static uint64_t cap_xgetbv0_impl(void) {
    return (uint64_t)_xgetbv(0);
}
#elif defined(ACS_CAP_USE_MSVC_CPUID)
static uint64_t cap_xgetbv0_impl(void) {
    return (uint64_t)_xgetbv(0);
}
#else
static uint64_t cap_xgetbv0_impl(void) { return 0; }
#endif

/* ───────── feature 位 → 名称 (事实源; 见 capability_v1.h enum) ───────── */
static const char* cap_bit_name(uint64_t bit) {
    switch (bit) {
    case ACS_CAP_FEAT_SSE2:     return "sse2";
    case ACS_CAP_FEAT_SSE4_1:   return "sse4_1";
    case ACS_CAP_FEAT_SSE4_2:   return "sse4_2";
    case ACS_CAP_FEAT_AVX:      return "avx";
    case ACS_CAP_FEAT_AVX2:     return "avx2";
    case ACS_CAP_FEAT_FMA:      return "fma";
    case ACS_CAP_FEAT_BMI1:     return "bmi1";
    case ACS_CAP_FEAT_BMI2:     return "bmi2";
    case ACS_CAP_FEAT_AVX512F:  return "avx512f";
    case ACS_CAP_FEAT_AVX512CD: return "avx512cd";
    case ACS_CAP_FEAT_AVX512BW: return "avx512bw";
    case ACS_CAP_FEAT_AVX512DQ: return "avx512dq";
    case ACS_CAP_FEAT_AVX512VL: return "avx512vl";
    default:                    return NULL;
    }
}

/* feature 位枚举表 (升序; serialize 与测试共用同一顺序) */
static const uint64_t cap_all_bits[] = {
    ACS_CAP_FEAT_SSE2, ACS_CAP_FEAT_SSE4_1, ACS_CAP_FEAT_SSE4_2,
    ACS_CAP_FEAT_AVX,  ACS_CAP_FEAT_AVX2,   ACS_CAP_FEAT_FMA,
    ACS_CAP_FEAT_BMI1, ACS_CAP_FEAT_BMI2,
    ACS_CAP_FEAT_AVX512F, ACS_CAP_FEAT_AVX512CD,
    ACS_CAP_FEAT_AVX512BW, ACS_CAP_FEAT_AVX512DQ, ACS_CAP_FEAT_AVX512VL
};
#define ACS_CAP_BIT_COUNT (sizeof(cap_all_bits) / sizeof(cap_all_bits[0]))

/* ───────── 判定: os_safe 平面裁剪 ─────────
 * 规则 (15 §2 + capability_v1.h 头注释):
 *   SSE2/SSE4.1/SSE4.2/BMI1/BMI2: 无需 OS 状态, hw 即 safe。
 *   AVX/AVX2/FMA: hw 置位 且 OSXSAVE 且 XCR0.XMM|YMM (0x6)。
 *   AVX-512 五子集 (F/CD/BW/DQ/VL): 组内全部 hw 置位 且 XCR0.opmask|
 *     ZMM_Hi256|Hi16_ZMM (0xE0)。组缺任一子集 → 组全部清除 (缺子集拒绝:
 *     不在 os_safe 平面; 具体缺哪个由 hw/os_safe 位差诊断)。
 * 本机 2 CPU 等线程/核决策不在此 (CPU-008); 本文件不读核心数。 */
static void cap_classify(const acs_cap_result_v1* raw, acs_cap_result_v1* out) {
    *out = *raw; /* 先拷贝全部原始证据/厂商/hw */
    out->hw_features = 0;
    out->os_safe = 0;
    out->osxsave = (raw->leaf1_ecx & (1u << 27)) ? 1u : 0u;

    /* hw 平面: 纯 CPUID (未做 OS 裁剪) */
    uint64_t hw = 0;
    const uint32_t ecx1 = raw->leaf1_ecx;
    const uint32_t edx1 = raw->leaf1_edx;
    if (edx1 & (1u << 26)) hw |= ACS_CAP_FEAT_SSE2;
    if (ecx1 & (1u << 19)) hw |= ACS_CAP_FEAT_SSE4_1;
    if (ecx1 & (1u << 20)) hw |= ACS_CAP_FEAT_SSE4_2;
    if (ecx1 & (1u << 28)) hw |= ACS_CAP_FEAT_AVX;
    if (ecx1 & (1u << 12)) hw |= ACS_CAP_FEAT_FMA;
    const uint32_t ebx7 = raw->leaf7_ebx;
    if (ebx7 & (1u << 3))  hw |= ACS_CAP_FEAT_BMI1;
    if (ebx7 & (1u << 5))  hw |= ACS_CAP_FEAT_AVX2;
    if (ebx7 & (1u << 8))  hw |= ACS_CAP_FEAT_BMI2;
    if (ebx7 & (1u << 16)) hw |= ACS_CAP_FEAT_AVX512F;
    if (ebx7 & (1u << 28)) hw |= ACS_CAP_FEAT_AVX512CD;
    if (ebx7 & (1u << 30)) hw |= ACS_CAP_FEAT_AVX512BW;
    if (ebx7 & (1u << 17)) hw |= ACS_CAP_FEAT_AVX512DQ;
    if (ebx7 & (1u << 31)) hw |= ACS_CAP_FEAT_AVX512VL;
    out->hw_features = hw;

    /* os_safe 平面 */
    uint64_t safe = 0;
    const uint64_t no_os = (uint64_t)(ACS_CAP_FEAT_SSE2 | ACS_CAP_FEAT_SSE4_1 |
                                      ACS_CAP_FEAT_SSE4_2 |
                                      ACS_CAP_FEAT_BMI1 | ACS_CAP_FEAT_BMI2);
    safe |= hw & no_os;

    if (out->osxsave && out->xcr0 != 0) {
        const uint64_t xcr0 = out->xcr0;
        if ((xcr0 & 0x6u) == 0x6u) {
            safe |= hw & (uint64_t)ACS_CAP_GROUP_AVX_FAMILY;
        }
        if ((xcr0 & 0xE0u) == 0xE0u) {
            /* AVX-512 组: 组内五子集全部 hw 置位才整体 safe */
            if ((hw & (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET) ==
                (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET) {
                safe |= hw & (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET;
            }
        }
    }
    out->os_safe = safe;
}

int acs_cap_classify_v1(const acs_cap_result_v1* raw, acs_cap_result_v1* out) {
    if (raw == NULL || out == NULL) return ACS_CAP_ERR_PARAM;
    if (raw->abi_version != ACS_CAP_ABI_VERSION_V1) {
        return ACS_CAP_ERR_ABI_MISMATCH;
    }
    cap_classify(raw, out);
    return ACS_CAP_OK;
}

/* ───────── 探测主流程 ───────── */
int acs_cap_detect_v1(acs_cap_result_v1* out) {
    if (out == NULL) return ACS_CAP_ERR_PARAM;
    acs_cap_result_v1 r;
    memset(&r, 0, sizeof(r));
    r.struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    r.abi_version = ACS_CAP_ABI_VERSION_V1;
    r.schema_version = ACS_CAP_SCHEMA_VER;

    if (out->abi_version != ACS_CAP_ABI_VERSION_V1) {
        *out = r; /* 调用方用旧/错版本头: 拒绝而不猜布局 */
        return ACS_CAP_ERR_ABI_MISMATCH;
    }
#if defined(ACS_CAP_NO_CPUID)
    *out = r;
    return ACS_CAP_ERR_UNSUPPORTED;
#else
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    cap_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    r.max_leaf = eax;
    /* vendor: CPUID.0 EBX:EDX:ECX 拼接 */
    {
        char v[16];
        memcpy(v, &ebx, 4);
        memcpy(v + 4, &edx, 4);
        memcpy(v + 8, &ecx, 4);
        v[12] = '\0';
        memcpy(r.vendor, v, ACS_CAP_VENDOR_MAX);
        r.vendor[ACS_CAP_VENDOR_MAX - 1] = '\0';
    }
    if (r.max_leaf >= 1u) {
        cap_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        r.leaf1_ecx = ecx;
        r.leaf1_edx = edx;
        /* family/model/stepping 解码 (Intel/AMD 通用 AMD64 语义) */
        const uint32_t base_family = (eax >> 8) & 0xFu;
        const uint32_t ext_family  = (eax >> 20) & 0xFFu;
        const uint32_t base_model  = (eax >> 4) & 0xFu;
        const uint32_t ext_model   = (eax >> 16) & 0xFu;
        r.family = (base_family == 0xFu) ? (base_family + ext_family)
                                         : base_family;
        r.model = (base_family == 0xFu || base_family == 0x6u)
                      ? (ext_model << 4) | base_model
                      : base_model;
        r.stepping = eax & 0xFu;
    }
    if (r.max_leaf >= 7u) {
        cap_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        r.leaf7_ebx = ebx;
        r.leaf7_ecx = ecx;
        r.leaf7_edx = edx;
    }
    /* brand: 叶 80000000h 探测后 80000002-4 */
    {
        cap_cpuid(0x80000000u, 0, &eax, &ebx, &ecx, &edx);
        if (eax >= 0x80000004u) {
            char b[ACS_CAP_BRAND_MAX];
            uint32_t part[3][4];
            size_t off = 0;
            for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u && off < sizeof(b);
                 ++leaf) {
                cap_cpuid(leaf, 0, &part[leaf - 0x80000002u][0],
                          &part[leaf - 0x80000002u][1],
                          &part[leaf - 0x80000002u][2],
                          &part[leaf - 0x80000002u][3]);
                for (int i = 0; i < 4 && off < sizeof(b); ++i) {
                    const uint32_t word = part[leaf - 0x80000002u][i];
                    b[off++] = (char)((word >> 0) & 0xFFu);
                    if (off < sizeof(b)) b[off++] = (char)((word >> 8) & 0xFFu);
                    if (off < sizeof(b)) b[off++] = (char)((word >> 16) & 0xFFu);
                    if (off < sizeof(b)) b[off++] = (char)((word >> 24) & 0xFFu);
                }
            }
            b[sizeof(b) - 1] = '\0';
            memcpy(r.brand, b, ACS_CAP_BRAND_MAX);
        }
    }
    /* OS 状态: 仅当 OSXSAVE 置位才执行 XGETBV (安全序: 探测自身无非法指令) */
    if ((r.leaf1_ecx & (1u << 27)) != 0u) {
        r.xcr0 = cap_xgetbv0_impl();
    }
    cap_classify(&r, out);
    return ACS_CAP_OK;
#endif
}

/* ───────── 判定辅助 ───────── */
const char* acs_cap_feature_name_v1(uint64_t bit) { return cap_bit_name(bit); }

int acs_cap_os_safe_satisfies_v1(const acs_cap_result_v1* cap, uint64_t required) {
    if (cap == NULL) return 0;
    if (required == 0) return 1;
    /* 非法位 (非任何已知 feature) → 拒绝 */
    uint64_t known = 0;
    for (size_t i = 0; i < ACS_CAP_BIT_COUNT; ++i) known |= cap_all_bits[i];
    if ((required & ~known) != 0) return 0;
    return (cap->os_safe & required) == required;
}

int acs_cap_hw_satisfies_v1(const acs_cap_result_v1* cap, uint64_t required) {
    if (cap == NULL) return 0;
    if (required == 0) return 1;
    return (cap->hw_features & required) == required;
}

int acs_cap_os_saves_avx512_state_v1(const acs_cap_result_v1* cap) {
    if (cap == NULL) return 0;
    if (cap->osxsave == 0) return 0;
    return (cap->xcr0 & 0xE0u) == 0xE0u;
}

/* ───────── JSON 序列化 (稳定键序; schema 见 providers/cpu/common/schemas) ─────────
 * 输出事实字段逐项: schema_version/kind/architecture/vendor/brand/family/model/
 * stepping/cpuid{...}/os_state{osxsave,xcr0}/features{hw[],os_safe[]}/
 * hw_features_bitmask/os_safe_features_bitmask/judgement{...}。
 * 无浮点 (全部整数/字符串); 键序固定 → 同机同构输出逐字节稳定 (Windows/Linux
 * 同一 schema; vendor/brand 等字符串内容平台无关)。 */
typedef struct cap_appender {
    char*   out;
    size_t  cap;      /* 含 NUL 的可用容量 */
    size_t  need;     /* 已累计所需字节 (含 NUL 的最终长度) */
    int     truncated;
} cap_appender;

static void cap_append(cap_appender* a, const char* fmt, ...) {
    char    tmp[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) { a->truncated = 1; return; }
    const size_t len = (size_t)n;
    a->need += len;
    if (a->cap > 0 && a->out != NULL) {
        const size_t room = a->cap - 1;
        const size_t take = len < room ? len : room;
        if (take > 0) memcpy(a->out, tmp, take);
        a->out += take;
        a->cap -= take;
        *a->out = '\0';
    }
}

/* feature 位集合 → JSON 数组 (升序位序, 稳定) */
static void cap_append_feature_array(cap_appender* a, uint64_t bits) {
    cap_append(a, "[");
    int first = 1;
    for (size_t i = 0; i < ACS_CAP_BIT_COUNT; ++i) {
        const uint64_t bit = cap_all_bits[i];
        if ((bits & bit) == 0) continue;
        const char* name = cap_bit_name(bit);
        if (name == NULL) continue;
        cap_append(a, "%s\"%s\"", first ? "" : ",", name);
        first = 0;
    }
    cap_append(a, "]");
}

static void cap_append_bool(cap_appender* a, int v) {
    cap_append(a, v ? "true" : "false");
}

size_t acs_cap_serialize_json_v1(const acs_cap_result_v1* cap,
                                 char* out_json, size_t out_cap,
                                 char* err, size_t err_cap) {
    if (err != NULL && err_cap > 0) err[0] = '\0';
    if (cap == NULL || cap->abi_version != ACS_CAP_ABI_VERSION_V1) {
        if (err != NULL && err_cap > 0) {
            snprintf(err, err_cap, "acs_cap_serialize_json_v1: bad cap (abi/version)");
        }
        if (out_json != NULL && out_cap > 0) out_json[0] = '\0';
        return 0;
    }
    const uint64_t hw  = cap->hw_features;
    const uint64_t os  = cap->os_safe;
    const int xmm_ymm   = (cap->xcr0 & 0x6u) == 0x6u;
    const int opmask_zm = (cap->xcr0 & 0xE0u) == 0xE0u;

    cap_appender a;
    a.out = out_json;
    a.cap = out_cap;
    a.need = 0;
    a.truncated = 0;
    /* 结构需头 0 长度占位: 逐段 append, 最终 need = 全部字符数 + 1 (NUL) */
    cap_append(&a, "{\"schema_version\":%u,", (unsigned)cap->schema_version);
    cap_append(&a, "\"kind\":\"astrocs_cpu_capability\",");
    cap_append(&a, "\"architecture\":\"amd64\",");
    cap_append(&a, "\"vendor\":\"%s\",", cap->vendor[0] ? cap->vendor : "unknown");
    cap_append(&a, "\"brand\":\"%s\",", cap->brand[0] ? cap->brand : "");
    cap_append(&a, "\"family\":%u,", (unsigned)cap->family);
    cap_append(&a, "\"model\":%u,", (unsigned)cap->model);
    cap_append(&a, "\"stepping\":%u,", (unsigned)cap->stepping);
    cap_append(&a, "\"cpuid\":{\"max_leaf\":%u,\"leaf1_ecx\":%u,\"leaf1_edx\":%u,"
                   "\"leaf7_ebx\":%u,\"leaf7_ecx\":%u,\"leaf7_edx\":%u},",
               (unsigned)cap->max_leaf, (unsigned)cap->leaf1_ecx,
               (unsigned)cap->leaf1_edx, (unsigned)cap->leaf7_ebx,
               (unsigned)cap->leaf7_ecx, (unsigned)cap->leaf7_edx);
    cap_append(&a, "\"os_state\":{\"osxsave\":%u,\"xcr0\":%llu},",
               (unsigned)cap->osxsave,
               (unsigned long long)cap->xcr0);
    cap_append(&a, "\"features\":{\"hw\":");
    cap_append_feature_array(&a, hw);
    cap_append(&a, ",\"os_safe\":");
    cap_append_feature_array(&a, os);
    cap_append(&a, "},");
    cap_append(&a, "\"hw_features_bitmask\":%llu,",
               (unsigned long long)hw);
    cap_append(&a, "\"os_safe_features_bitmask\":%llu,",
               (unsigned long long)os);
    cap_append(&a, "\"judgement\":{");
    cap_append(&a, "\"os_saves_xmm_ymm\":");
    cap_append_bool(&a, cap->osxsave && xmm_ymm);
    cap_append(&a, ",\"os_saves_opmask_zmm\":");
    cap_append_bool(&a, cap->osxsave && opmask_zm);
    cap_append(&a, ",\"avx_family_os_safe\":");
    cap_append_bool(&a, (os & (uint64_t)ACS_CAP_GROUP_AVX_FAMILY) ==
                            (uint64_t)ACS_CAP_GROUP_AVX_FAMILY);
    cap_append(&a, ",\"avx512_subset_os_safe\":");
    cap_append_bool(&a, (os & (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET) ==
                            (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET);
    cap_append(&a, "}}");
    /* need 目前为字符数; 返回含 NUL */
    return a.need + 1;
}
