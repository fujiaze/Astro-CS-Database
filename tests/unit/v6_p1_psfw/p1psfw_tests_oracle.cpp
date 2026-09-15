/* p1psfw_tests_oracle.cpp - 独立 Oracle: MC 注入恢复 + covariance MC
 * 说明: 本文件使用的 MC 容差是测试内部统计容差 (ntrials 足够大), 不是冻结阈值;
 *       QF-G-INJ-03 保持 OPEN, 本测试不冒充已冻结容差。 */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <vector>

using namespace astrocs::v6::p1psfw;

P1PSFW_REGISTER(oracle) {
    (void)mode;
    /* ---- MC 1: 注入源 sigma_F = 1/sqrt(Sum W) (FZ-FORMULA-FHAT) ---- */
    const int K = 3;
    const std::size_t m = 15;
    std::vector<std::vector<double>> P_k(K), L_k(K);
    std::vector<double> a_k(K), var_k(K);
    std::vector<double> per_frame_w(K, 0.0), per_frame_q(K, 0.0);
    for (int k = 0; k < K; ++k) {
        P_k[k] = fixture::gaussian_psf(m, 1.5 + 0.5 * k);
        a_k[k] = 0.9 + 0.1 * k;
        var_k[k] = 1.0 + 0.5 * k;
        L_k[k].assign(m * m, 0.0);
        for (std::size_t i = 0; i < m; ++i) L_k[k][i * m + i] = std::sqrt(var_k[k]);
    }
    /* 解析 Sum W (与 trial 无关) */
    for (int k = 0; k < K; ++k) {
        std::vector<double> sig2(m, var_k[k]);
        const PointEstimate e = w_info_diagonal(P_k[k].data(), m, sig2.data(), P_k[k].data(), a_k[k]);
        P1_CHECK(e.ok);
        per_frame_w[k] = e.w_info;
    }
    double sum_w = 0.0;
    for (double w : per_frame_w) sum_w += w;
    P1_CHECK(sum_w > 0.0);

    const double F_true = 50.0;
    const int ntrials = 40000;
    const oracle::McData mc = oracle::o_mc_generate(P_k, L_k, a_k, F_true, ntrials, 20260915ULL);
    std::vector<double> fhats;
    fhats.reserve(ntrials);
    bool gls_match = true;
    for (int t = 0; t < ntrials; ++t) {
        std::vector<PointEstimate> pts(K);
        for (int k = 0; k < K; ++k) {
            const double* dk = mc.d[t].data() + static_cast<std::size_t>(k) * m;
            std::vector<double> sig2(m, var_k[k]);
            pts[k] = w_info_diagonal(P_k[k].data(), m, sig2.data(), dk, a_k[k]);
            if (!pts[k].ok) { gls_match = false; break; }
        }
        const FluxEstimate fe = combine_point_estimates(pts);
        if (!fe.ok) { gls_match = false; break; }
        fhats.push_back(fe.f_hat);
    }
    P1_CHECK(gls_match);
    P1_CHECK(static_cast<int>(fhats.size()) == ntrials);
    double mean = 0.0;
    for (double v : fhats) mean += v;
    mean /= static_cast<double>(fhats.size());
    const double empirical_var = oracle::o_sample_variance(fhats);
    const double analytic_var = 1.0 / sum_w;
    /* 无偏性 (SE = sqrt(var/n) ≈ 0.022 for var~2, n=40000): 2% 容差 */
    P1_CHECK_NEAR(mean, F_true, 0.02);
    /* 散度恢复: 5% 测试内部容差 (ntrials=4e4 时 var 的 RSE ~0.7%) */
    P1_CHECKF("oracle_mc_var", std::fabs(empirical_var - analytic_var) / analytic_var <= 0.05);
    /* 独立 Oracle 的 1/Sum W (long double 显式逆) 与生产一致 */
    long double osum = 0.0L;
    for (int k = 0; k < K; ++k) {
        std::vector<double> sig2(m, var_k[k]);
        std::vector<double> Ck(m * m, 0.0);
        for (std::size_t i = 0; i < m; ++i) Ck[i * m + i] = sig2[i];
        long double w = 0.0L, q = 0.0L;
        oracle::o_qw_dense(P_k[k], Ck, P_k[k], a_k[k], &w, &q);
        osum += w;
    }
    P1_CHECK_NEAR(analytic_var, static_cast<double>(1.0L / osum), 1e-9);

    /* 逐 trial 与显式逆 GLS 的交叉验证 (单帧, 100 trials) */
    {
        const std::size_t mm = m;
        std::vector<double> C(mm * mm, 0.0);
        for (std::size_t i = 0; i < mm; ++i) C[i * mm + i] = var_k[0];
        std::vector<double> sig2(mm, var_k[0]);
        for (int t = 0; t < 100; ++t) {
            const double* dk = mc.d[t].data();
            const PointEstimate e = w_info_diagonal(P_k[0].data(), mm, sig2.data(), dk, a_k[0]);
            const double f_prod = e.q / e.w_info;
            const double f_oracle = static_cast<double>(
                oracle::o_gls_flux_dense(P_k[0], C, std::vector<double>(dk, dk + mm), a_k[0]));
            P1_CHECK_NEAR(f_prod, f_oracle, 1e-9);
        }
    }

    /* ---- MC 2: C_out = R C_in R^T 的 MC 一致性 (FZ-FORMULA-COV-PROP) ---- */
    {
        const int Kc = 4;
        const std::size_t npix = 3;
        std::vector<std::vector<double>> d(Kc, std::vector<double>(npix, 0.0));
        std::vector<std::vector<char>> v(Kc, std::vector<char>(npix, 1));
        std::vector<double> w = {0.4, 1.0, 2.5, 0.8};
        const CoaddResult co = conventional_coadd(d, v, w);
        P1_CHECK(co.ok);
        for (std::size_t p = 0; p < npix; ++p) {
            double s = 0.0;
            for (int k = 0; k < Kc; ++k) s += co.alpha[k][p];
            P1_CHECK_NEAR(s, 1.0, 1e-12);
        }
        std::vector<double> c_in(Kc * Kc, 0.0);
        for (int i = 0; i < Kc; ++i)
            for (int j = 0; j < Kc; ++j)
                c_in[i * Kc + j] = (i == j) ? (1.0 + i) : 0.3 / (1.0 + std::abs(i - j));
        const CovariancePropagation cp = propagate_covariance(co.alpha, c_in);
        P1_CHECK(cp.ok);
        P1_CHECK(cp.method == "propagated_from_composite_coefficients");
        P1_CHECK(!cp.variance_from_weight && !cp.uses_relative_weight_as_ivar);
        const std::vector<double> mc_var = oracle::o_mc_coadd_var(co.alpha, c_in, 40000, 20260916ULL);
        for (std::size_t p = 0; p < npix; ++p) {
            /* 测试内部 5% MC 容差; 同时与解析式独立 (Oracle 只用 alpha/C_in 显式求和) */
            P1_CHECK(std::fabs(cp.var_out[p] - mc_var[p]) / mc_var[p] <= 0.05);
        }
        /* 负向 mutation: Var = 1/W_psfsw 或 Sum W_psfsw 与传播结果不可区分 -> 必须可区分 */
        double sum_w_psfsw = 0.0;
        for (double x : w) sum_w_psfsw += x;
        P1_CHECK(std::fabs(cp.var_out[0] - 1.0 / sum_w_psfsw) / cp.var_out[0] > 1e-3);
        P1_CHECK(std::fabs(cp.var_out[0] - sum_w_psfsw) / cp.var_out[0] > 1e-3);
    }
}
