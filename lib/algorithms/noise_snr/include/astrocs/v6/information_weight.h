/* information_weight.h - Phase1/V6 W_info / Q / F_hat (IMPL-P1-PSFW-001)
 *
 * 合同锚 (docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md §3.2-§3.6;
 *         docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md):
 *   - FZ-FORMULA-WINFO : W_info,k = a_k^2 P_k^T C_k^-1 P_k = 1 / Var(F_hat_k)   [ADU^-2]
 *   - FZ-FORMULA-Q     : Q_k      = a_k P_k^T C_k^-1 d_k                      [ADU^-1]
 *   - FZ-FORMULA-FHAT  : F_hat = Sum Q / Sum W ;  Var(F_hat) = 1 / Sum W      [ADU], [ADU^2]
 *   - FZ-COND-WHITENOISE: C = sigma_pix^2 I  =>  W_info = a^2 / (sigma_pix^2 A_NEA),
 *                         A_NEA = 1 / Sum P^2 ; *仅* C 对角且 sigma_pix 已声明时可用
 *   - FZ-WINFO-DIAG-APPROX: 仅用对角近似 C~ 时必须报告 c~^T C c~ (真实 C) 与偏差比,
 *                         不得直接报告理想 1/W
 *   - FZ-AP1-GLS-QW-RTOL: Q/W == GLS 与 Var = 1/W 相对容差 1e-9
 *   - FZ-UNIT-WINFO/Q/FLUX: 单位 ADU^-2 / ADU^-1 / ADU
 *
 * 纪律: 不接线 session; 无 median(SNR_F)/support/coverage/FWHM 权重来源;
 *       W_info 唯一权威式即 a^2 P^T C^-1 P, 不从任何诊断量反推。
 * 复用: 无 (纯 std + libm)。
 */
#ifndef ASTROCS_V6_P1PSFW_INFORMATION_WEIGHT_H
#define ASTROCS_V6_P1PSFW_INFORMATION_WEIGHT_H

#include <cstddef>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p1psfw {

/* 冻结容差: Q/W == GLS 与 Var(F_hat) == 1/W (FZ-AP1-GLS-QW-RTOL). */
constexpr double kQwRelTol = 1e-9;

/* ------------------------------------------------------------------------- */
/* 协方差视图 (C_k)                                                           */
/* ------------------------------------------------------------------------- */
struct CovarianceView {
    enum class Kind { diagonal, dense_spd, low_rank };
    Kind kind = Kind::diagonal;
    std::size_t m = 0;              /* 支持域像素数 */
    /* diagonal: sigma2[i] = sigma_pix^2 (ADU^2); sigma_declared 必须为真才允许白噪式 */
    const double* sigma2 = nullptr;
    bool sigma_declared = false;
    /* dense_spd: c 为 m*m 行主序对称正定矩阵 (ADU^2) */
    const double* c = nullptr;
    /* low_rank: C = diag(d_diag) + L L^T; d_diag 长度 m, l 为 m*r 行主序 */
    const double* d_diag = nullptr;
    const double* l = nullptr;
    std::size_t r = 0;
};

struct PointEstimate {
    double q = 0.0;        /* ADU^-1 */
    double w_info = 0.0;   /* ADU^-2 */
    bool ok = false;
    const char* reject = nullptr;
};

/* 解 C x = P 后 W = a^2 P^T x, Q = a d^T x (C 对称)。
 * 拒绝原因: "null_input" | "dimension_mismatch" | "non_spd" | "singular". */
PointEstimate w_info_diagonal(const double* p, std::size_t m,
                              const double* sigma2, const double* d, double a);
PointEstimate w_info_dense(const double* p, std::size_t m,
                           const double* c, const double* d, double a);
PointEstimate w_info_low_rank(const double* p, std::size_t m,
                              const double* d_diag, const double* l, std::size_t r,
                              const double* d, double a);
PointEstimate w_info_solve(const CovarianceView& cov,
                           const double* p, const double* d, double a);

/* ------------------------------------------------------------------------- */
/* 白噪声条件式 (FZ-COND-WHITENOISE)                                          */
/* ------------------------------------------------------------------------- */
struct WhiteNoiseGate {
    bool is_diagonal = false;
    bool sigma_declared = false;
    const char* reason = nullptr;   /* "non_diagonal_covariance" | "sigma_pix_undeclared" */
    bool allowed() const { return is_diagonal && sigma_declared; }
};

WhiteNoiseGate white_noise_gate(const CovarianceView& cov);

struct WhiteNoiseResult {
    bool ok = false;
    const char* reject = nullptr;
    double w_info = 0.0;   /* ADU^-2 */
    double a_nea = 0.0;    /* px^2 */
};

/* W_info = a^2 / (sigma_pix^2 * A_NEA); 仅当 gate.allowed() 时 ok。
 * sigma_pix2 <= 0 / 非有限 / P 未归一 -> reject。 */
WhiteNoiseResult w_info_white_noise(const double* p, std::size_t m, double sigma_pix2,
                                    double a, const WhiteNoiseGate& gate);

/* ------------------------------------------------------------------------- */
/* 对角近似误差报告 (FZ-WINFO-DIAG-APPROX)                                    */
/* ------------------------------------------------------------------------- */
struct DiagApproxReport {
    bool ok = false;
    const char* reject = nullptr;
    double c_tilde_C_c_tilde = 0.0;   /* 用真实 C 计算 (ADU^2) */
    double ideal_inv_w = 0.0;         /* 1 / W_true (ADU^2) */
    double variance_ratio = 0.0;      /* c~^T C c~ / (1/W_true) = Var_approx / Var_true */
    bool reported = false;            /* 必须为真才可用于声明 */
};

DiagApproxReport diag_approx_report(const CovarianceView& true_c,
                                    const double* c_tilde,
                                    const double* p, const double* d, double a);

/* ------------------------------------------------------------------------- */
/* 多帧合成 (FZ-FORMULA-FHAT)                                                 */
/* ------------------------------------------------------------------------- */
struct FluxEstimate {
    bool ok = false;
    const char* reject = nullptr;
    double f_hat = 0.0;    /* ADU */
    double var_f = 0.0;    /* ADU^2 */
    double sum_q = 0.0;    /* ADU^-1 */
    double sum_w = 0.0;    /* ADU^-2 */
};

FluxEstimate combine_point_estimates(const std::vector<PointEstimate>& points);

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif /* ASTROCS_V6_P1PSFW_INFORMATION_WEIGHT_H */
