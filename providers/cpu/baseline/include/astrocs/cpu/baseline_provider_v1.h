/* AstroCS CPU baseline provider — 公共 kernel 参数合同 v1
 * providers/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h (CPU-002)
 *
 * 角色: CPU-002 冻结的 AMD64 baseline provider (仅 SSE2, 不带 /arch:AVX* /
 * -mavx* 编译旗标) 的 kernel 参数 POD 合同。provider 本体导出唯一入口
 * astrocs_provider_query_v1 (include/astrocs/abi/module_api_v1.h 冻结; 12 §1 /
 * ARC-001 §1.2), 消费 CPU-001 capability 判定 (providers/cpu/common/
 * include/astrocs/cpu/capability_v1.h) 完成 "OS 可安全执行" 平面检查后再
 * 提供 kernel 服务。
 *
 * 冻结合同:
 *   - docs/architecture/cpu/CPU_002_BASELINE_PROVIDER.md (DOC-ARCH-CPU-002);
 *   - include/astrocs/abi/module_api_v1.h (provider ABI: acs_provider_api_v1 /
 *     acs_kernel_desc_v1 / run_kernel 签名; ABI-001);
 *   - include/astrocs/abi/lifecycle_v1.h (self_test 语义 / host_abi 协商; ABI-002);
 *   - providers/cpu/common/include/astrocs/cpu/capability_v1.h (CPU-001 os_safe 平面);
 *   - ALG-001/002/004/005/006/008/009/P3-002 离散公式 (docs/algorithms/);
 *   - 12 个已注册 kernel 的算法/序与 lib/backend_host/baseline_kernels_impl.inc
 *     (ABI-003 冻结实现) 逐位一致 —— 本 provider 是 legacy baseline backend
 *     的正式 provider 形态迁移 (CPU-002), 不改变科学值语义。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常/RTTI; 无第三方依赖。
 *   2) 跨边界结构前两字段 = struct_size + abi_version (同 acs_head 模式); 失配即拒。
 *   3) 全部 op 标签/索引/字段语义冻结; 只允许尾部追加。
 *   4) 本 provider 全部 kernel 在任意 AMD64 (SSE2 基线) 上可执行; 无全局 SIMD
 *      静态初始化 (DllMain/LoadLibrary 期不执行任何 SIMD 指令; 12 §7)。
 *
 * 缓冲合同 (run_kernel 的 in/out 为 acs_span_u8 字节缓冲):
 *   - in.data/out.data 均为调用方分配; 元素 = float32 (IEEE-754);
 *   - 每条输入/输出数组以 float32 元素为单位的 (in_off[i], in_len[i]) /
 *     (out_off[j], out_len[j]) 描述自缓冲起点的切片; 数组连续不重叠;
 *   - 各 op 消费/产生的数组槽位见本头 kOpLayout 注释; 实现层按 op 校验
 *     off+len <= 缓冲容量, 越界 → ACS_ERR_PARAM;
 *   - in/out 不得别名 (aliasing_contract=0); out 由调用方预分配;
 *   - 数据指针需 4 字节对齐 (float32); host 建议 64 字节对齐。
 *
 * 确定性/并行 (15 §5 / ARCH-004 §4):
 *   - 每输出元素独立 (无跨线程归约) → 结果 bitwise 不随 worker 数变化;
 *   - worker 数 = min(host->executor.max_workers, N) 的全或无租借
 *     (host executor acquire/release; 禁私有线程池, FORBID-003);
 *   - host->executor 为 NULL 或租借失败逐级减半 → 1 = 串行兜底 (05 §6 保守);
 *   - 取消点 = run_kernel 调用边界 (v1; 同 ABI-003 既有语义)。
 *
 * NaN/Inf 语义 (对照 ALG 权威, 不引入科学变更):
 *   - calibration/psf/overlap/accumulate/normalize/spmv/residual/weight-update/
 *     integration/hips: 逐元素 IEEE 算术传播 (NaN 输入 → NaN 输出; 除零按 IEEE);
 *     normalize/integration 的 w>1e-6f 门: 仅有限且大于阈值才走除法分支
 *     (与 ABI-003 冻结实现同式), NaN 权重 → 0 输出;
 *   - noise/rejection 的 median/MAD (排序型): 仅有限元素参与
 *     (ALG-CAL §4 "输入含 NaN 仅 finite 参与 median/MAD"; 无 finite → 0);
 *   - MAD→σ 系数 1.4826f (与 ABI-003/ALG-NOISE F1 一致)。
 */
#ifndef ASTROCS_CPU_BASELINE_PROVIDER_V1_H
#define ASTROCS_CPU_BASELINE_PROVIDER_V1_H

#include "astrocs/abi/module_api_v1.h"   /* acs_head/status/provider 表 (ABI-001/002) */

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_CPU_BASELINE_PARAMS_VERSION 1u  /* params POD 版本 (独立于 ABI v1) */
#define ACS_CPU_BASELINE_KERNEL_COUNT 12u   /* 注册 kernel 数 (v1 冻结) */
#define ACS_CPU_BASELINE_MAX_IN_SLOTS  4u
#define ACS_CPU_BASELINE_MAX_OUT_SLOTS 2u

/* acs_kernel_desc_v1.precision / determinism_class 取值
 * (module_api_v1.h 注释冻结: precision 0=f32 1=f64;
 *  determinism_class 0=bitwise 1=fixed_order 2=threadlocal_merge)。
 * 数据面 v1 全 float32; 命名常量供 provider 表/测试引用, 数值与 ABI 注释一致。 */
enum {
    ACS_CPU_BASELINE_PREC_F32 = 0,
    ACS_CPU_BASELINE_PREC_F64 = 1
};
enum {
    ACS_CPU_BASELINE_DET_BITWISE = 0,
    ACS_CPU_BASELINE_DET_FIXED_ORDER = 1,
    ACS_CPU_BASELINE_DET_THREADLOCAL_MERGE = 2
};

/* provider 标识 (query 期 self_test/诊断; kernel_id 事实见 README §4) */
#define ACS_CPU_BASELINE_PROVIDER_ID "astrocs.cpu.baseline"
#define ACS_CPU_BASELINE_BUILD_ID     "CPU-002-0d32c07"

/* ── kernel 注册索引 (与 kernel_list 输出序一一对应; v1 冻结) ── */
enum acs_cpu_baseline_kernel_index {
    ACS_CPU_KIDX_CALIBRATION = 0,      /* ALG-001 calibration-pixel-transform */
    ACS_CPU_KIDX_NOISE_REDUCTIONS = 1, /* ALG-004 noise-snr-reductions */
    ACS_CPU_KIDX_PSF_BATCH = 2,        /* ALG-002 wcs-psf-batch */
    ACS_CPU_KIDX_DRIZZLE_OVERLAP = 3,  /* ALG-005 drizzle-overlap */
    ACS_CPU_KIDX_DRIZZLE_ACCUMULATE = 4,/* ALG-005 drizzle-accumulate */
    ACS_CPU_KIDX_DRIZZLE_NORMALIZE = 5,/* ALG-005 drizzle-normalize */
    ACS_CPU_KIDX_UPM_SPMV = 6,         /* ALG-006 upm-spmv */
    ACS_CPU_KIDX_UPM_RESIDUAL = 7,     /* ALG-006 upm-residual */
    ACS_CPU_KIDX_UPM_WEIGHT_UPDATE = 8,/* ALG-006 upm-weight-update */
    ACS_CPU_KIDX_REJECTION_STATS = 9,  /* ALG-008 rejection-statistics */
    ACS_CPU_KIDX_INTEGRATION_ACCUM = 10,/* ALG-009 integration-accumulate */
    ACS_CPU_KIDX_HIPS_BULK = 11        /* ALG-P3-002 hips-bulk-transform */
};

/* ── kernel 参数 POD (前两字段 acs_head; 只读输入; 无指针字段, 跨边界安全) ──
 * 字段语义逐 op (w*h=N=输出元素数; k/aux0/aux1 按 op):
 *   calibration        : out0[i]=(in0[i]-in1[i]-k*in2[i])*in3[i]   (i<N)
 *   noise-reductions   : in0=帧主序 [frames*N]; out0=med, out1=MADσ (每像素)
 *   psf-batch          : in0={cx,cy}(2); out0[i]=k*exp(-r2/2), r2 像素距心平方
 *   drizzle-overlap    : out0[i]=max(0,1-|in0[i]|)*max(0,1-|in1[i]|)
 *   drizzle-accumulate : in0/in1=帧主序; out0[i]=Σ_f in0[f*N+i]*in1[f*N+i]
 *   drizzle-normalize  : out0[i]= in1[i]>1e-6f ? in0[i]/in1[i] : 0
 *   upm-spmv           : in0=values[nnz], in1=colidx[nnz], in2=rowptr[N+1],
 *                        in3=x[ncols]; out0[row]=Σ_{k∈row} in0[k]*x[colidx[k]]
 *   upm-residual       : out0[i]=in0[i]-in1[i]
 *   upm-weight-update  : out0[i]=max(in0[i], k)
 *   rejection-stats    : in0=帧主序; med/MADσ 同 noise; out0[i]=|v-med|>k*MADσ 计数
 *   integration-accum  : in0=values, in1=weights 帧主序;
 *                        out0[i]= wsum>1e-6f ? Σw*x/Σw : 0
 *   hips-bulk          : in0=源图像 [iw*ih] (iw=aux0, ih=aux1); k=采样比;
 *                        out0[i]=双线性重采样 (目标 w*h; 边缘 clamp)
 * 槽位 (in_off/in_len 索引 0..3; out_off/out_len 索引 0..1):
 *   输入槽位: 0=in0, 1=in1, 2=in2, 3=in3; 输出槽位: 0=out0, 1=out1。
 * 未使用槽位必须 off=len=0 (校验拒绝非零未用槽)。 */
typedef struct acs_cpu_baseline_params_v1 {
    acs_head head;                    /* struct_size=sizeof(本结构); abi=ACS_ABI_VERSION_V1 */
    uint32_t w, h;                    /* 输出域; N=w*h; w>0,h>0 */
    float    k;                       /* op 标量 (dark 比例/σ 倍数/采样比/PSF 幅度/floor) */
    uint32_t aux0;                    /* frames / nnz / 源宽 iw */
    uint32_t aux1;                    /* ncols / 源高 ih */
    uint64_t in_off[ACS_CPU_BASELINE_MAX_IN_SLOTS];   /* f32 元素偏移 (自 in.data) */
    uint64_t in_len[ACS_CPU_BASELINE_MAX_IN_SLOTS];   /* f32 元素数 */
    uint64_t out_off[ACS_CPU_BASELINE_MAX_OUT_SLOTS]; /* f32 元素偏移 (自 out.data) */
    uint64_t out_len[ACS_CPU_BASELINE_MAX_OUT_SLOTS]; /* f32 元素数 */
    uint32_t flags;                   /* v1=0 (保留; 0=无附加语义) */
    uint32_t reserved;                /* 0; 不得依赖内容 */
} acs_cpu_baseline_params_v1;

/* 布局静态断言 (amd64 = 本产品全部平台; uint64 对齐 8 → 136 字节稳定)。
 * 32 位 i386 (非交付面) uint64 对齐 4 → 布局不同, 不做精确断言。 */
ACS_STATIC_ASSERT(offsetof(acs_cpu_baseline_params_v1, head) == 0u,
                  "params head first");
#if ACS_ABI_PTR_BITS == 64
ACS_STATIC_ASSERT(sizeof(acs_cpu_baseline_params_v1) == 136u,
                  "params POD 136 bytes on amd64");
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CPU_BASELINE_PROVIDER_V1_H */
