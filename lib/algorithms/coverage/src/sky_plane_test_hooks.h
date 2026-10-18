// lib/algorithms/coverage/src/sky_plane_test_hooks.h
//
// 内部**测试接缝**（不是公开 C ABI 面，不在 lib/algorithms/coverage/include 下）：
// 让 eng/tests 直接驱动 sky_plane.cpp 内匿名命名空间里的罚法方程求解器
// （sky_penalized_solve：直接 Cholesky → 惩罚零空间极小锚 → 精确 Woodbury
// deflation → 原方程后向误差门），与生产**同一编译产物、同一实现**，不复制算法。
//
// 依据 #80 P0-A1：「使用独立 pivoted QR / gelsd SVD / Woodbury 解同一个导出的
// 惩罚问题……包含完整 7×6、缺整行、双 3×3 岛和当前矩阵的单元/正式 C++ 测试」。
// 与既有接缝同款（lib/algorithms/coverage/src/sampler.cpp
// `acsd_phase2_kcorr_lookup_for_test`）：只暴露匿名命名空间实现，不新增产品入口。
#pragma once

#include <cstdint>

// 求解读数（与 SkyPenalizedSolveResult 逐字段对应）。
struct AcsdSkyDeflationDiag {
    int status;              // 0=OK_DIRECT 1=OK_WOODBURY 2=ANCHOR_NOT_SPD
                             // 3=NOT_RESOLVABLE 4=BAD_ARGS
    int branch;              // 0=直接 Cholesky；1=锚定 + deflation
    int n;                   // 方程阶
    int mq;                  // 锚基列数
    int chol_fail_col;       // 失败列（-1 = 无失败）
    double alpha;            // 锚系数
    double chol_fail_pivot;  // 失败列当时的未开方主元
    double residual_rel;     // ‖b − A B‖∞ / (‖A‖∞·‖B‖∞ + ‖b‖∞)
    double residual_gate;    // τ_floor = max(n_obs, n_free)·ε
    double a_norm_inf;
    double b_norm_inf;
    double x_norm_inf;
};

#ifdef __cplusplus
extern "C" {
#endif

// 解 A B = b（A: n×n row-major 对称；Q: n×mq row-major 列正交候选锚基）。
// force_anchor != 0：**确定性分支注入**（只由测试使用，生产恒 0）——跳过直接
// Cholesky，强制走「锚定 + 精确 deflation」分支，用于在同一 A,b 上与直接解对拍
// deflation 的精确性（#80 P0-A1「复用确定性分支注入在同一 A,b 上比较纠正后残差和
// QR/SVD 真解」）。它不是 env 旋钮，也不改变 A,b。
// B_out 长度 ≥ n；diag 可空。返回 0 = 求解成功（diag->status ∈ {0,1}），
// 1 = 未产生可信解（diag->status 给出具名原因），-1 = 参数非法。
int acsd_sky_plane_penalized_solve_for_test(const double* A, int n, const double* b,
                                            const double* Q, int mq, double alpha,
                                            double residual_gate, int force_anchor,
                                            double* B_out, AcsdSkyDeflationDiag* diag);

#ifdef __cplusplus
}
#endif
