/* lib/backend_host/baseline_kernels.h — baseline kernel 参数合同 (ABI-003)
 * v1 通用缓冲合同: 各 kernel op 按 ACS_KOP_* 解释 in/out span(逐字段语义见注释)。
 * 所有 op: 输出元素间独立(无跨线程归约)→固定序确定性随 worker 数不变(ARCH-004 §4)。
 */
#ifndef ASTROCS_BASELINE_KERNELS_H
#define ASTROCS_BASELINE_KERNELS_H

#include "astrocs/common_abi_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* kernel op 标签(v1 冻结) */
enum {
    ACS_KOP_CALIBRATION = 1,        /* out0[i] = (in0[i]-in1[i]-k*in2[i])*in3[i] */
    ACS_KOP_NOISE_REDUCTIONS,       /* 栈式: 每像素 median/out0, MAD*1.4826/out1; aux0=n_frames */
    ACS_KOP_PSF_BATCH,              /* 高斯 PSF 批量: out0[i]=k*exp(-r^2/(2*1^2)) 于网格点 */
    ACS_KOP_DRIZZLE_OVERLAP,        /* out0[i] = wx(i)*wy(i) 双线性覆盖权重 */
    ACS_KOP_DRIZZLE_ACCUMULATE,     /* out0[i] = Σ_f in0[f*N+i]*in1[f*N+i] (固定下标序) */
    ACS_KOP_DRIZZLE_NORMALIZE,      /* out0[i] = w>eps ? val/w : 0 */
    ACS_KOP_UPM_SPMV,               /* CSR: out0[row] = Σ values* x[col] (固定列序) */
    ACS_KOP_UPM_RESIDUAL,           /* out0[i] = in0[i]-in1[i] */
    ACS_KOP_UPM_WEIGHT_UPDATE,      /* out0[i] = max(in0[i], k) (floor 钳制) */
    ACS_KOP_REJECTION_STATS,        /* 每像素 |x-med|>k*MADSIG 计数 → out0 */
    ACS_KOP_INTEGRATION_ACCUM,      /* 每像素固定序加权均值 Σw*x/Σw */
    ACS_KOP_HIPS_BULK               /* 双线性重采样: 源采样位置 (x*k, y*k), 边缘 clamp */
};

typedef struct acs_baseline_params_v1 {
    acs_head head;          /* struct_size/abi_version=ACS_ABI_VERSION_V1 */
    uint32_t op;            /* ACS_KOP_* */
    uint32_t w, h;          /* 2D 域(像素/网格); N=w*h */
    float    k;             /* op 标量(dark 比例/σ 钳制/采样比/clip 倍数/PSF 幅度) */
    uint32_t aux0;          /* op 附加: n_frames(栈类)/nnz(SPMV)/0 */
    uint32_t aux1;          /* op 附加: ncols(SPMV)/0 */
    acs_span_f32 in0;       /* 语义随 op; 所有权=调用方 */
    acs_span_f32 in1;
    acs_span_f32 in2;
    acs_span_f32 in3;
    acs_span_f32 out0;      /* 调用方分配; 元素数≥N(栈类=per pixel) */
    acs_span_f32 out1;      /* 可空(count=0) */
    uint32_t workers_used;  /* out: 实际 worker 数(≥1; 多线程观测点) */
    uint32_t reserved;
} acs_baseline_params_v1;

#ifdef __cplusplus
}
#endif

#endif /* ASTROCS_BASELINE_KERNELS_H */
