/* AstroCS CPU AVX-512 provider — 公共 kernel 参数合同 v1 (热点子集)
 * providers/cpu/avx512/include/astrocs/cpu/avx512_provider_v1.h (CPU-004)
 *
 * 角色: CPU-004 冻结的 AMD64 AVX-512 provider (target 单独
 * -mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl; Windows
 * /arch:AVX512) 的 kernel 注册合同。provider 本体导出唯一入口
 * astrocs_provider_query_v1 (include/astrocs/abi/module_api_v1.h 冻结;
 * ARC-001 §1.2 / 15 §1), 消费 CPU-001 capability 判定
 * (providers/cpu/common/include/astrocs/cpu/capability_v1.h) 完成
 * "所需 AVX-512 子集 ∈ os_safe" (OSXSAVE + XGETBV XMM|YMM|opmask|ZMM 状态)
 * 平面检查后才提供 kernel 服务; 缺任何子集 / OS 不保存 ZMM state → 拒绝
 * 加载 (ACS_ERR_UNSUPPORTED), host 回落 baseline/AVX2
 * (15 §2 / C §C6 / CPU_PROVIDER_QUALIFICATION_CONTRACT)。
 *
 * 冻结合同:
 *   - 热点 profile 台账 = ISA-004 实测 (docs/architecture/ISA_VARIANTS.md
 *     §1.6 + artifacts/prerelease_v5/ISA-004/MEASUREMENTS.csv; SA-CPU-09
 *     冻结): hips-bulk-transform avx512 +29.5% vs baseline (≈avx2 +28.3%
 *     同档) —— 唯一实测可能获益 kernel; calibration +3.8% (远低 avx2
 *     +11.7%) 与 drizzle-accumulate −22.5% 均 NOT_SHIPPED (防 AVX-512 降频
 *     使全局性能变差 —— 只注册实测获益 kernel, 不机械堆砌);
 *   - include/astrocs/abi/module_api_v1.h (provider ABI: acs_provider_api_v1 /
 *     acs_kernel_desc_v1 / run_kernel 签名; ABI-001);
 *   - include/astrocs/abi/lifecycle_v1.h (self_test 语义 / host_abi 协商; ABI-002);
 *   - providers/cpu/common/include/astrocs/cpu/capability_v1.h (CPU-001 os_safe
 *     平面: AVX-512 组须 F/CD/BW/DQ/VL 五子集 hw 全置 + XCR0.0xE0 全置);
 *   - providers/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h
 *     (CPU-002 冻结参数 POD —— 本 provider 只迁移热点 kernel 的 ISA 变体,
 *     **不复制** baseline 的科学 kernel 实现; 参数/槽位/缓冲合同同源复用);
 *   - ALG-P3-002 hips-bulk-transform 离散公式 (docs/algorithms/)。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常跨边界; 无第三方依赖。
 *   2) 跨边界结构前两字段 = struct_size + abi_version (同 acs_head 模式); 失配即拒。
 *   3) 本 provider 只注册实测可能获益的热点 kernel (函数入口由 provider 表查询,
 *      不复制科学模块 — 15 §1 / CPU_PROVIDER_QUALIFICATION_CONTRACT); 其余
 *      kernel 索引 → ACS_ERR_UNSUPPORTED (host 按 kernel_id 退回 baseline/avx2;
 *      每 kernel 可退回, CPU-005 路由语义)。
 *   4) 加载判定 (AVX-512 门): required = F|CD|BW|DQ|VL 五子集
 *      (ACS_CAP_GROUP_AVX512_SUBSET) ⊆ cap.os_safe —— os_safe 仅在
 *      OSXSAVE=1 且 XCR0.opmask|ZMM_Hi256|Hi16_ZMM (0xE0) 全置 且五子集 hw
 *      全置时含 AVX-512 组; 缺任一子集 / OS 不保存 ZMM state → 拒绝
 *      (CPUID/XGETBV negative; 15 §2 "AVX-512 至少检查所需 F/CD/BW/DQ/VL
 *      子集; 不能只看一个 AVX512F=true")。
 *   5) 本 TU 以 -mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl
 *      单独编译 (仅此 target; baseline/主 CLI 零 AVX 污染, 15 §6);
 *      加载期 (query 前) 不执行任何 SIMD/EVEX 指令 (DllMain 约束 12 §7 ——
 *      全局对象仅 POD/字符串表) → 非法指令保护: 不支持 CPU 上 dlopen 可安全
 *      完成 (query 返回 UNSUPPORTED), 无 #UD 风险。
 *   6) FMA/AVX-512 归约顺序记录 (15 §6; 冻结于本文件下方):
 *      hips-bulk-transform: o[i]=(1-fx)(1-fy)v00 + fx(1-fy)v10 +
 *      (1-fx)fy v01 + fx fy v11 —— 固定 4 项源码序标量累加 (无 -ffast-math/
 *      重排), 每输出独立无跨线程归约 → AVX-512 向量化/FMA 收缩**不改变
 *      归约顺序**; 可能引入每元素 ≤ 数十 ULP 舍入差 (容差 2e-4 相对冻结)。
 *
 * 缓冲合同 (run_kernel 的 in/out 为 acs_span_u8 字节缓冲; 同 baseline v1):
 *   - 参数 POD = acs_cpu_baseline_params_v1 (CPU-002 冻结; 本头只是宿主声明,
 *     不重定义结构, 防止双定义漂移);
 *   - 槽位语义逐 op 同 baseline_provider_v1.h 头注释 (hips: 1 入 1 出;
 *     未用槽 off=len=0; 越界 → ACS_ERR_PARAM);
 *   - in/out 不得别名 (aliasing_contract=0); out 由调用方预分配。
 */
#ifndef ASTROCS_CPU_AVX512_PROVIDER_V1_H
#define ASTROCS_CPU_AVX512_PROVIDER_V1_H

#include "astrocs/abi/module_api_v1.h"   /* acs_head/status/provider 表 (ABI-001/002) */
#include "astrocs/cpu/capability_v1.h"    /* ACS_CAP_FEAT_* (CPU-001; required_features) */
#include "astrocs/cpu/baseline_provider_v1.h" /* 参数 POD 宿主声明 (CPU-002, 只复用不复制) */

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_CPU_AVX512_PARAMS_VERSION 1u   /* params 语义版本 (= baseline v1) */
#define ACS_CPU_AVX512_KERNEL_COUNT 1u     /* 注册热点 kernel 数 (v1 冻结; ISA-004) */
#define ACS_CPU_AVX512_MAX_IN_SLOTS  4u    /* 与 baseline params 槽位上限一致 */
#define ACS_CPU_AVX512_MAX_OUT_SLOTS 2u

/* provider 标识 (query 期 self_test/诊断) */
#define ACS_CPU_AVX512_PROVIDER_ID "astrocs.cpu.avx512"
#define ACS_CPU_AVX512_BUILD_ID     "CPU-004"

/* provider 加载所需能力 (os_safe 平面子集; 15 §2 / C §C6):
 * AVX-512 五子集 F/CD/BW/DQ/VL (CPU-001 classify: hw 全置 + OSXSAVE +
 * XCR0.opmask|ZMM_Hi256|Hi16_ZMM = 0xE0 全置才整体进 os_safe; 不能只看
 * AVX512F)。本 provider TU 含 -mavx512bw/-mavx512dq/-mavx512vl 指令, 故
 * required = 组掩码全五子集。 */
#define ACS_CPU_AVX512_REQUIRED_FEATURES \
    (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET

/* ── 注册热点 kernel 索引 (kernel_list 输出序; v1 冻结) ──
 * 热点 profile (ISA-004 实测, 02 §10.1 / ISA_VARIANTS.md §1.6):
 *   hips-bulk-transform : ALG-P3-002 双线性重采样  avx512 +29.5%
 *     vs baseline (≈avx2 +28.3% 同档; EVEX 512-bit 向量化获益)  SHIP
 *  NOT_SHIPPED (仍落 avx2/baseline; ISA-004 实测无额外获益/更慢):
 *   calibration-pixel-transform +3.8% (远低 avx2 +11.7%),
 *   drizzle-accumulate −22.5% (变体更慢; AVX-512 降频风险 → 不注册防全局
 *   性能变差)。 */
enum acs_cpu_avx512_kernel_index {
    ACS_CPU_AVX512_KIDX_HIPS_BULK = 0   /* ALG-P3-002 hips-bulk-transform */
};

/* 参数 POD: 与 baseline 完全同一结构 (CPU-002 acs_cpu_baseline_params_v1)。
 * 本 provider 不定义第二份 params —— 跨 ISA 变体共享参数合同 (同 avx2). */
typedef acs_cpu_baseline_params_v1 acs_cpu_avx512_params_v1;

ACS_STATIC_ASSERT(sizeof(acs_cpu_avx512_params_v1) ==
                      sizeof(acs_cpu_baseline_params_v1),
                  "avx512 params POD = baseline params POD (不复制科学合同)");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CPU_AVX512_PROVIDER_V1_H */
