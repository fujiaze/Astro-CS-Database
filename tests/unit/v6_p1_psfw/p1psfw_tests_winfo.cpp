/* p1psfw_tests_winfo.cpp - W_info / Q / F_hat / 白噪条件 / 对角近似门 (正+负) */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

namespace {
/* 构造 SPD 协方差: C = D + rho 相关 (1D, 指数核) */
std::vector<double> cov_matrix(std::size_t m, double var, double rho) {
    std::vector<double> C(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            const double dist = static_cast<double>(i > j ? i - j : j - i);
            C[i * m + j] = var * std::pow(rho, dist);
        }
    return C;
}
}  /* namespace */

P1PSFW_REGISTER(winfo) {
    (void)mode;
    const std::vector<double> P = fixture::gaussian_psf(21, 2.0);
    const std::size_t m = P.size();
    const double a = 0.85;
    const std::vector<double> d(m, 1.0);

    /* ---- 对角 C: W/Q == 独立 Oracle ---- */
    std::vector<double> sigma2(m, 4.0);
    const PointEstimate e1 = w_info_diagonal(P.data(), m, sigma2.data(), d.data(), a);
    P1_CHECKF("winfo_diag", e1.ok);
    P1_CHECK_NEAR(e1.w_info, static_cast<double>(oracle::o_w_diagonal(P, sigma2, a)), 1e-12);
    long double ow = 0.0L, oq = 0.0L;
    std::vector<double> Cdiag(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i) Cdiag[i * m + i] = sigma2[i];
    oracle::o_qw_dense(P, Cdiag, d, a, &ow, &oq);   /* 对角 C 显式展开后走 dense 显式逆 */
    P1_CHECK_NEAR(e1.w_info, static_cast<double>(ow), 1e-9);
    P1_CHECK_NEAR(e1.q, static_cast<double>(oq), 1e-9);

    /* ---- dense SPD: Cholesky 路径 == 显式逆 Oracle (FZ-AP1-GLS-QW-RTOL 1e-9) ---- */
    const std::vector<double> C = cov_matrix(m, 4.0, 0.7);
    const PointEstimate e2 = w_info_dense(P.data(), m, C.data(), d.data(), a);
    P1_CHECKF("winfo_dense", e2.ok);
    long double ow2 = 0.0L, oq2 = 0.0L;
    oracle::o_qw_dense(P, C, d, a, &ow2, &oq2);
    P1_CHECK_NEAR(e2.w_info, static_cast<double>(ow2), 1e-9);
    P1_CHECK_NEAR(e2.q, static_cast<double>(oq2), 1e-9);

    /* ---- 低秩 C = D + L L^T: Woodbury == 显式逆 Oracle ---- */
    const std::size_t r = 3;
    std::vector<double> D(m, 1.5), L(m * r, 0.0);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t alpha = 0; alpha < r; ++alpha)
            L[i * r + alpha] = 0.8 * std::exp(-static_cast<double>(i) / (10.0 + 3.0 * alpha)) *
                               (alpha == 0 ? 1.0 : 0.5);
    const PointEstimate e3 = w_info_low_rank(P.data(), m, D.data(), L.data(), r, d.data(), a);
    P1_CHECKF("winfo_lowrank", e3.ok);
    /* 组完整矩阵 C = D + L L^T 后走 Oracle 显式逆 */
    std::vector<double> Cfull(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i) Cfull[i * m + i] = D[i];
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j)
            for (std::size_t alpha = 0; alpha < r; ++alpha)
                Cfull[i * m + j] += L[i * r + alpha] * L[j * r + alpha];
    long double ow3 = 0.0L, oq3 = 0.0L;
    oracle::o_qw_dense(P, Cfull, d, a, &ow3, &oq3);
    P1_CHECK_NEAR(e3.w_info, static_cast<double>(ow3), 1e-9);
    P1_CHECK_NEAR(e3.q, static_cast<double>(oq3), 1e-9);

    /* ---- 白噪条件式 (FZ-COND-WHITENOISE) ---- */
    CovarianceView diag_cov;
    diag_cov.kind = CovarianceView::Kind::diagonal;
    diag_cov.m = m;
    diag_cov.sigma2 = sigma2.data();
    diag_cov.sigma_declared = true;
    WhiteNoiseGate gate_ok = white_noise_gate(diag_cov);
    P1_CHECK(gate_ok.allowed());
    const WhiteNoiseResult wn = w_info_white_noise(P.data(), m, 4.0, a, gate_ok);
    P1_CHECKF("winfo_wn", wn.ok);
    P1_CHECK_NEAR(wn.w_info, static_cast<double>(oracle::o_w_white_noise(P, 4.0, a)), 1e-12);
    /* 与对角 W_info 一致 (C = sigma^2 I) */
    std::vector<double> sig_const(m, 4.0);
    const PointEstimate ew = w_info_diagonal(P.data(), m, sig_const.data(), d.data(), a);
    P1_CHECK_NEAR(wn.w_info, ew.w_info, 1e-12);

    /* ---- 负向: 白噪条件不成立时必须拒绝 ---- */
    CovarianceView dense_cov;
    dense_cov.kind = CovarianceView::Kind::dense_spd;
    dense_cov.m = m;
    dense_cov.c = C.data();
    dense_cov.sigma_declared = true;
    WhiteNoiseGate gate_bad1 = white_noise_gate(dense_cov);
    P1_CHECK(!gate_bad1.allowed());
    P1_CHECK(std::string(gate_bad1.reason ? gate_bad1.reason : "") == "non_diagonal_covariance");
    const WhiteNoiseResult wn_bad = w_info_white_noise(P.data(), m, 4.0, a, gate_bad1);
    P1_CHECK(!wn_bad.ok);
    CovarianceView diag_undecl;
    diag_undecl.kind = CovarianceView::Kind::diagonal;
    diag_undecl.m = m;
    diag_undecl.sigma2 = sigma2.data();
    diag_undecl.sigma_declared = false;
    const WhiteNoiseGate gate_bad2 = white_noise_gate(diag_undecl);
    P1_CHECK(!gate_bad2.allowed());
    P1_CHECK(std::string(gate_bad2.reason ? gate_bad2.reason : "") == "sigma_pix_undeclared");

    /* 负向: 非 SPD / sigma<=0 / 白噪式在非对角 C 上数值不同于真 W_info
     * (证明白噪条件式不是无条件可用, FZ-COND-WHITENOISE 负向 mutation) */
    std::vector<double> badC = cov_matrix(m, 4.0, 0.9);
    for (std::size_t i = 0; i < m; ++i) badC[i * m + i] = 0.0;   /* 秩亏/非正定 */
    P1_CHECK(!w_info_dense(P.data(), m, badC.data(), d.data(), a).ok);
    std::vector<double> zero_sig(m, 0.0);
    P1_CHECK(!w_info_diagonal(P.data(), m, zero_sig.data(), d.data(), a).ok);

    /* 白噪式 (把非对角 C 当 sigma^2 I) 与真 W_info 的偏差必须 > 容差 —— 因此
     * 无条件白噪近似是可检出错误 (负向 mutation: 非对角 C 用白噪式) */
    const double wn_misuse = (a * a) / (4.0 * psf_anea(P.data(), m));
    P1_CHECK(std::fabs(wn_misuse - e2.w_info) / e2.w_info > 1e-3);

    /* ---- 对角近似误差门 (FZ-WINFO-DIAG-APPROX) ---- */
    std::vector<double> c_tilde = P;   /* 理想匹配滤波系数 */
    const DiagApproxReport rep = diag_approx_report(dense_cov, c_tilde.data(), P.data(), d.data(), a);
    P1_CHECKF("winfo_diagapprox", rep.ok && rep.reported);
    P1_CHECK(rep.variance_ratio > 0.0);
    /* Oracle 复算 c~^T C c~ */
    long double quad = 0.0L;
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j)
            quad += static_cast<long double>(c_tilde[i]) * C[i * m + j] * c_tilde[j];
    P1_CHECK_NEAR(rep.c_tilde_C_c_tilde, static_cast<double>(quad), 1e-12);
    P1_CHECK_NEAR(rep.ideal_inv_w, 1.0 / static_cast<double>(ow2), 1e-9);
    P1_CHECK_NEAR(rep.variance_ratio, static_cast<double>(quad) / (1.0 / static_cast<double>(ow2)), 1e-9);

    /* ---- F_hat / Var(F_hat) (FZ-FORMULA-FHAT) ---- */
    std::vector<PointEstimate> points;
    for (int k = 0; k < 4; ++k) {
        std::vector<double> dk(m);
        for (std::size_t i = 0; i < m; ++i) dk[i] = static_cast<double>(k + 1) * P[i];
        points.push_back(w_info_dense(P.data(), m, C.data(), dk.data(), a));
    }
    const FluxEstimate fe = combine_point_estimates(points);
    P1_CHECKF("winfo_fhat", fe.ok);
    double sumq = 0.0, sumw = 0.0;
    for (const auto& e : points) { sumq += e.q; sumw += e.w_info; }
    P1_CHECK_NEAR(fe.f_hat, sumq / sumw, 1e-9);
    P1_CHECK_NEAR(fe.var_f, 1.0 / sumw, 1e-9);
    /* 负向 mutation: Var(F_hat) = W (去掉倒数) 必被检出 */
    P1_CHECK(std::fabs((sumw) - fe.var_f) / fe.var_f > 1e-3);
}
