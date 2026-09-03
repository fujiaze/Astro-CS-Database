/* AstroCS CPU 能力探测 C ABI v1 — providers/cpu/common/include/astrocs/cpu/capability_v1.h
 *
 * 角色: CPU-001 冻结的 AMD64 CPU 能力探测合同。区分
 *   "硬件支持"(CPUID feature 位) 与 "OS 可安全执行"(OSXSAVE + XGETBV/XCR0
 *   状态位 + 层次包含 + provider required_features 子集判定)。
 * 冻结合同: docs/architecture/cpu/CPU_001_CAPABILITY_PROBE.md (DOC-ARCH-CPU-001)
 *   + providers/cpu/common/schemas/cpu_capability.schema.json (JSON 序列化 schema)。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常/RTTI; 无第三方依赖。
 *   2) 跨边界结构前两字段 = struct_size + abi_version (同 acs_head 模式); 失配即拒。
 *   3) 全部状态位/枚举数值冻结; 只允许尾部追加新位/新成员。
 *   4) 探测只读 CPUID/XGETBV/OS 接口, 无副作用、无分配、可重入; 调用不执行
 *      任何高级 ISA 指令 (探测自身只用 SSE2 基线可用的 mov/cpuid/xgetbv)。
 *   5) 能力判定与硬件支持是两个平面: 探测先收 CPUID 位, 再按 OS 状态位与
 *      层次包含裁剪出 "可安全执行" 的 os_safe 平面; require_* 只针对
 *      os_safe 平面判定 (硬件有而 OS 不保存寄存器 → 拒绝)。
 *   6) 不读取硬编码核心数: 本头不含任何核心计数/线程建议; 逻辑/可用核由
 *      host/ThreadBudget 另行管理 (CPU-008), 探测层不决策。
 *
 * 位语义 (对照 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §2 与
 * include/astrocs/abi/status_codes.h 错误码):
 *   - os_safe 判定规则:
 *       AVX/AVX2/FMA 可安全执行 ⇔ CPUID 置位 且 OSXSAVE=1 且 XCR0.XMM|YMM
 *         (xcr0 位 1|2 = 0x6) 均置;
 *       AVX-512F 可安全执行 ⇔ 且 XCR0.opmask|ZMM_Hi256|Hi16_ZMM (位 5|6|7
 *         = 0xE0) 均置; AVX-512CD/BW/DQ/VL 各自还需对应 CPUID 叶 7 子位。
 *   - 硬件支持但 OS 不保存 → os_safe 平面清除 (provider 加载拒绝路径)。
 */
#ifndef ASTROCS_CPU_CAPABILITY_V1_H
#define ASTROCS_CPU_CAPABILITY_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_CAP_ABI_VERSION_V1 1u
#define ACS_CAP_BRAND_MAX 64    /* brand 字符串 ≤ 63 字符 + NUL */
#define ACS_CAP_VENDOR_MAX 16   /* vendor id ≤ 12 字符 + NUL */
#define ACS_CAP_ERR_TEXT_MAX 160
#define ACS_CAP_SCHEMA_VER 1u   /* JSON schema_version (cpu_capability.schema.json) */

/* ═══════════════════ feature 位 (冻结; 只允许尾部追加) ═══════════════════
 * 本枚举即 C 侧 JSON feature_names 的事实源 (名称映射见 capability_detect.c
 * acs_cap_feature_name_v1); 数值不得复用/重排 (v1 冻结)。 */
enum acs_cap_feature_bit {
    ACS_CAP_FEAT_SSE2    = 1u << 0,  /* amd64 基线 (恒置) */
    ACS_CAP_FEAT_SSE4_1  = 1u << 1,
    ACS_CAP_FEAT_SSE4_2  = 1u << 2,
    ACS_CAP_FEAT_AVX     = 1u << 3,
    ACS_CAP_FEAT_AVX2    = 1u << 4,
    ACS_CAP_FEAT_FMA     = 1u << 5,
    ACS_CAP_FEAT_BMI1    = 1u << 6,
    ACS_CAP_FEAT_BMI2    = 1u << 7,  /* HEALPix 位操作能力, 不要求独立 DLL */
    ACS_CAP_FEAT_AVX512F = 1u << 8,
    ACS_CAP_FEAT_AVX512CD= 1u << 9,
    ACS_CAP_FEAT_AVX512BW= 1u << 10,
    ACS_CAP_FEAT_AVX512DQ= 1u << 11,
    ACS_CAP_FEAT_AVX512VL= 1u << 12
};

/* 逻辑分组 (判定辅助; 非独立 feature 位) */
enum {
    ACS_CAP_GROUP_AVX_FAMILY = ACS_CAP_FEAT_AVX | ACS_CAP_FEAT_AVX2 | ACS_CAP_FEAT_FMA,
    /* AVX-512 全部所需子集: F + CD + BW + DQ + VL (15 §2: 不能只看 AVX512F) */
    ACS_CAP_GROUP_AVX512_SUBSET =
        ACS_CAP_FEAT_AVX512F | ACS_CAP_FEAT_AVX512CD |
        ACS_CAP_FEAT_AVX512BW | ACS_CAP_FEAT_AVX512DQ | ACS_CAP_FEAT_AVX512VL
};

/* ═══════════════════ 探测结果 (POD; 前两字段 acs_head) ═══════════════════
 * 所有权: 全部字段由探测函数填充, 调用方栈上持有; 无指针/无跨边界释放。 */
typedef struct acs_cap_result_v1 {
    uint32_t struct_size;    /* sizeof(acs_cap_result_v1) */
    uint32_t abi_version;    /* ACS_CAP_ABI_VERSION_V1 */
    uint32_t schema_version; /* ACS_CAP_SCHEMA_VER (JSON schema_version 对齐) */
    uint32_t reserved;       /* 0; 供尾部对齐, 不得依赖内容 */
    /* CPUID 叶 0/1/7 原始证据 (探测事实; 供审计/交叉核对, 非判定依据) */
    uint32_t max_leaf;       /* CPUID 最大基础叶 (eax=0) */
    uint32_t leaf1_ecx;      /* CPUID 1 ECX */
    uint32_t leaf1_edx;      /* CPUID 1 EDX */
    uint32_t leaf7_ebx;      /* CPUID 7.0 EBX (结构化扩展) */
    uint32_t leaf7_ecx;      /* CPUID 7.0 ECX */
    uint32_t leaf7_edx;      /* CPUID 7.0 EDX */
    /* 厂商/型号 (探测事实) */
    char     vendor[ACS_CAP_VENDOR_MAX]; /* CPUID 0 EBX:EDX:ECX ("GenuineIntel"/"AuthenticAMD") */
    char     brand[ACS_CAP_BRAND_MAX];   /* CPUID 80000002-4 brand 串 (截断 NUL) */
    uint32_t family;         /* base family (含 extended, 叶 1 EAX 解码) */
    uint32_t model;          /* base model (含 extended) */
    uint32_t stepping;
    /* OS 状态 */
    uint32_t osxsave;        /* CPUID 1 ECX.OSXSAVE (bit27); 0/1 */
    uint64_t xcr0;           /* XGETBV(0) 实测; OSXSAVE=0 时为 0 */
    /* 判定平面 */
    uint64_t hw_features;    /* 硬件支持 (CPUID, 未做 OS 裁剪) */
    uint64_t os_safe;        /* 硬件 ∩ OS 状态 ∩ 层次包含 后可安全执行 */
} acs_cap_result_v1;

/* ═══════════════════ 探测/判定 API ═══════════════════
 * 全部 reentrant=yes; threadsafe=yes; internal_parallel=none; 无分配。
 * 失败 (非 x86/未知) 返回 ACS_CAP_ERR_UNSUPPORTED (数值 5, 与
 * acs_status ACS_ERR_UNSUPPORTED 一致) 并写空结果。 */

/* 错误码 (数值与 include/astrocs/abi/status_codes.h acs_status 对齐;
 * 本接口不需要 3/4/6..10, 不扩展) */
enum acs_cap_status {
    ACS_CAP_OK = 0,
    ACS_CAP_ERR_PARAM = 1,
    ACS_CAP_ERR_ABI_MISMATCH = 2,
    ACS_CAP_ERR_UNSUPPORTED = 5
};

/* 探测本机 AMD64 CPU: 填 *out (非 NULL)。out->abi_version != V1 时先写
 * *out->struct_size 后返回 ACS_CAP_ERR_ABI_MISMATCH。非 x86/探测失败 →
 * ACS_CAP_ERR_UNSUPPORTED。 */
int acs_cap_detect_v1(acs_cap_result_v1* out);

/* 纯判定: 以调用方填写的原始 CPUID/OS 证据 (leaf1_ecx/edx, leaf7_ebx,
 * xcr0; osxsave 由 leaf1_ecx.bit27 权威派生) 计算 hw_features/os_safe 平面。
 * 供 feature matrix 模拟/审计注入合成证据 (模拟测试与生产共用同一判定引擎);
 * 不清除调用方已填的 vendor/brand/family 等事实字段 (原样拷贝)。
 * raw/out 非 NULL 且 raw->abi_version==V1; 失败返回 ACS_CAP_ERR_PARAM/
 * ACS_CAP_ERR_ABI_MISMATCH。reentrant=yes; threadsafe=yes; 无分配。 */
int acs_cap_classify_v1(const acs_cap_result_v1* raw, acs_cap_result_v1* out);

/* feature 名称 (feature_names 的事实源; 静态 NUL 串, 永不 NULL)。
 * bit 必须恰为单个 ACS_CAP_FEAT_* 位 (组合/非法 → 返回 NULL)。 */
const char* acs_cap_feature_name_v1(uint64_t bit);

/* 判定: required 中的全部位都在 os_safe 平面 (OS 可安全执行)。
 * 任一 required 位非法/未被 os_safe 包含 → 0 (拒绝);
 * required==0 (无要求) → 1 (通过)。 */
int acs_cap_os_safe_satisfies_v1(const acs_cap_result_v1* cap, uint64_t required);

/* 判定: required 中的全部位都在 hw_features 平面 (硬件支持, 含 OS 不可执行)。
 * 用于诊断 "硬件有但 OS 拒绝" 的精确原因; 加载判定一律用 os_safe_satisfies。 */
int acs_cap_hw_satisfies_v1(const acs_cap_result_v1* cap, uint64_t required);

/* 判定: OS 是否保存 AVX-512 ZMM/opmask 状态 (xcr0 0xE0)。
 * os_safe 平面已含该判定的聚合结果; 本查询供诊断/报告细分。 */
int acs_cap_os_saves_avx512_state_v1(const acs_cap_result_v1* cap);

/* JSON 序列化: 稳定 schema (Windows/Linux 同一 schema; 键序固定, 逐字段
 * 见 cpu_capability.schema.json)。err 可 NULL; err_cap 含 NUL。
 * 返回需要字节数 (含 NUL)。buffer 不足 → 写已可用前缀 + NUL,
 * 返回所需总长 (调用方可重试); out_json==NULL → 只求长度。
 * 本函数只使用标准库 snprintf, 无第三方 JSON 依赖。 */
size_t acs_cap_serialize_json_v1(const acs_cap_result_v1* cap,
                                 char* out_json, size_t out_cap,
                                 char* err, size_t err_cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CPU_CAPABILITY_V1_H */
