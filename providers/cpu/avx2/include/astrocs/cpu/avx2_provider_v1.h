/* AstroCS CPU AVX2/FMA provider — 公共 kernel 参数合同 v1 (热点子集)
 * providers/cpu/avx2/include/astrocs/cpu/avx2_provider_v1.h (CPU-003)
 *
 * 角色: CPU-003 冻结的 AMD64 AVX2/FMA provider (target 单独 -mavx2 -mfma;
 * Windows /arch:AVX2) 的 kernel 注册合同。provider 本体导出唯一入口
 * astrocs_provider_query_v1 (include/astrocs/abi/module_api_v1.h 冻结;
 * ARC-001 §1.2 / 15 §1), 消费 CPU-001 capability 判定
 * (providers/cpu/common/include/astrocs/cpu/capability_v1.h) 完成
 * "AVX2/FMA ∈ os_safe" (OSXSAVE + XGETBV XMM|YMM) 平面检查后才提供
 * kernel 服务; 非支持 CPU 一律拒绝加载 (ACS_ERR_UNSUPPORTED), host 回落
 * baseline (02_CURRENT_BASELINE_AUDIT §10.1 / 15 §1)。
 *
 * 冻结合同:
 *   - docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md (DOC-ARCH-CPU-003;
 *     热点 profile 台账 = ISA-001/003 实测, artifacts/prerelease_v5/
 *     ISA-001/MEASUREMENTS.csv);
 *   - include/astrocs/abi/module_api_v1.h (provider ABI: acs_provider_api_v1 /
 *     acs_kernel_desc_v1 / run_kernel 签名; ABI-001);
 *   - include/astrocs/abi/lifecycle_v1.h (self_test 语义 / host_abi 协商; ABI-002);
 *   - providers/cpu/common/include/astrocs/cpu/capability_v1.h (CPU-001 os_safe 平面);
 *   - providers/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h
 *     (CPU-002 冻结参数 POD —— 本 provider 只迁移热点 kernel 的 ISA 变体,
 *     **不复制** baseline 的科学 kernel 实现; 参数/槽位/缓冲合同同源复用);
 *   - ALG-001 calibration-pixel-transform 与 ALG-P3-002 hips-bulk-transform
 *     离散公式 (docs/algorithms/)。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常跨边界; 无第三方依赖。
 *   2) 跨边界结构前两字段 = struct_size + abi_version (同 acs_head 模式); 失配即拒。
 *   3) 本 provider 只注册 profile 指定热点 kernel (函数入口由 provider 表查询,
 *      不复制科学模块 — 15 §1 "高级 provider 只实现已证明热点, 其余返回
 *      unsupported 由 host 使用 baseline"); 其余 kernel 索引 → ACS_ERR_UNSUPPORTED。
 *   4) 加载判定: required_features (AVX|AVX2|FMA) ⊆ cap.os_safe 才可加载;
 *      OS 不保存 YMM (XCR0.0x6 缺失) / 硬件缺 AVX2+FMA → 拒绝
 *      (CPUID/XGETBV negative; 15 §2 / C §C6)。
 *   5) 本 TU 以 -mavx2 -mfma 单独编译 (仅此 target; baseline/主 CLI 零 AVX
 *      污染, 15 §6); 加载期 (query 前) 不执行任何 SIMD 指令 (DllMain 约束
 *      12 §7 —— 全局对象仅 POD/字符串表)。
 *   6) kernel 输出可能含 FMA 舍入差 (vfmadd 融合乘加不中间舍入) → baseline
 *      对照容差 2e-4 相对 (ALG oracle 同规); FMA 是否改变归约顺序逐 kernel
 *      记录于 CPU_003_AVX2_PROVIDER.md §6 (15 §6 "FMA/SIMD/并行归约若改变
 *      结果顺序, 容差必须来自 ALG 且在代码前冻结")。
 *
 * 缓冲合同 (run_kernel 的 in/out 为 acs_span_u8 字节缓冲; 同 baseline v1):
 *   - 参数 POD = acs_cpu_baseline_params_v1 (CPU-002 冻结; 本头只是宿主声明,
 *     不重定义结构, 防止双定义漂移);
 *   - 槽位语义逐 op 同 baseline_provider_v1.h 头注释 (calibration: 4 入 1 出;
 *     hips: 1 入 1 出; 未用槽 off=len=0; 越界 → ACS_ERR_PARAM);
 *   - in/out 不得别名 (aliasing_contract=0); out 由调用方预分配。
 */
#ifndef ASTROCS_CPU_AVX2_PROVIDER_V1_H
#define ASTROCS_CPU_AVX2_PROVIDER_V1_H

#include "astrocs/abi/module_api_v1.h"   /* acs_head/status/provider 表 (ABI-001/002) */
#include "astrocs/cpu/capability_v1.h"    /* ACS_CAP_FEAT_* (CPU-001; required_features) */
#include "astrocs/cpu/baseline_provider_v1.h" /* 参数 POD 宿主声明 (CPU-002, 只复用不复制) */

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_CPU_AVX2_PARAMS_VERSION 1u   /* params 语义版本 (= baseline v1; 独立计数) */
#define ACS_CPU_AVX2_KERNEL_COUNT 2u     /* 注册热点 kernel 数 (v1 冻结; ISA-001 profile) */
#define ACS_CPU_AVX2_MAX_IN_SLOTS  4u    /* 与 baseline params 槽位上限一致 */
#define ACS_CPU_AVX2_MAX_OUT_SLOTS 2u

/* provider 标识 (query 期 self_test/诊断) */
#define ACS_CPU_AVX2_PROVIDER_ID "astrocs.cpu.avx2"
#define ACS_CPU_AVX2_BUILD_ID     "CPU-003"

/* provider 加载所需能力 (os_safe 平面子集; 15 §2/C §C6: CPUID + OSXSAVE +
 * XGETBV 后整组进入 os_safe 才可执行; 本 provider 为 -mavx2 -mfma TU,
 * required = AVX | AVX2 | FMA) */
#define ACS_CPU_AVX2_REQUIRED_FEATURES \
    (ACS_CAP_FEAT_AVX | ACS_CAP_FEAT_AVX2 | ACS_CAP_FEAT_FMA)

/* ── 注册热点 kernel 索引 (kernel_list 输出序; 与 baseline 索引无映射义务,
 *  host 以 kernel_id 逐条查询 — CPU-005 路由语义; v1 冻结) ──
 * 热点 profile (ISA-001/003 实测, 02 §10.1 / ISA_VARIANTS.md):
 *   calibration-pixel-transform : ALG-001 逐元素四元乘加 → 自动向量化
 *                                 提升 +20.7%/+11.7% (AVX2+FMA)  SHIP
 *   hips-bulk-transform         : ALG-P3-002 双线性重采样 提升
 *                                 +28.2%/+28.3% (AVX2+FMA)          SHIP
 *  NOT_SHIPPED (仍落 baseline; 实测变体更慢/低收益候选):
 *   drizzle-accumulate −15.2%/−14.0%、noise-snr-reductions (排序型)、
 *   upm-spmv (gather 型)、integration-accumulate。 */
enum acs_cpu_avx2_kernel_index {
    ACS_CPU_AVX2_KIDX_CALIBRATION = 0,   /* ALG-001 calibration-pixel-transform */
    ACS_CPU_AVX2_KIDX_HIPS_BULK = 1      /* ALG-P3-002 hips-bulk-transform */
};

/* 参数 POD: 与 baseline 完全同一结构 (CPU-002 acs_cpu_baseline_params_v1)。
 * 本 provider 不再定义第二份 params 结构 —— 跨 ISA 变体共享参数合同,
 * 防三份科学算法漂移 (02 §10.1 "不要给每段代码机械复制三份")。
 * run_kernel params_bytes 校验仍以 sizeof(acs_cpu_baseline_params_v1)=136
 * (amd64) 为准 (与 baseline 同规)。 */
typedef acs_cpu_baseline_params_v1 acs_cpu_avx2_params_v1;

/* 布局静态断言 (参数结构与 baseline 同一; 对齐/尺寸由 baseline 头保证) */
ACS_STATIC_ASSERT(sizeof(acs_cpu_avx2_params_v1) ==
                      sizeof(acs_cpu_baseline_params_v1),
                  "avx2 params POD = baseline params POD (不复制科学合同)");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CPU_AVX2_PROVIDER_V1_H */
