/* p1psfw_tests_negative.cpp - 数值层负向 mutation: 注入违反冻结的实现, 门必红
 * 每条 mutation 都是"先证伪门、再声明门有效": 正确量 -> 绿; 变异量 -> 与独立
 * Oracle 不一致, 对应门/断言确定性变红。 */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

P1PSFW_REGISTER(negative) {
    (void)mode;
    const std::vector<double> P = fixture::gaussian_psf(21, 2.0);
    const std::size_t m = P.size();
    const std::vector<double> d(m, 2.5);
    const double a = 0.9;

    /* 相关 SPD C (指数核) */
    std::vector<double> C(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            const double dist = static_cast<double>(i > j ? i - j : j - i);
            C[i * m + j] = 4.0 * std::pow(0.7, dist);
        }
    const PointEstimate good = w_info_dense(P.data(), m, C.data(), d.data(), a);
    P1_CHECK(good.ok);
    long double ow = 0.0L, oq = 0.0L;
    oracle::o_qw_dense(P, C, d, a, &ow, &oq);
    P1_CHECK_NEAR(good.w_info, static_cast<double>(ow), 1e-9);

    /* M-W1: W 去掉 C^-1 -> W_mut = a^2 Sum P^2; 与 GLS W 必不同 (>1e-3) */
    {
        double sum_p2 = 0.0;
        for (double v : P) sum_p2 += v * v;
        const double w_mut = a * a * sum_p2;
        P1_CHECKF("neg_w1", std::fabs(w_mut - good.w_info) / good.w_info > 1e-3);
    }
    /* M-W2: Var(F_hat) = W (去掉倒数) */
    {
        std::vector<PointEstimate> pts = {good, good};
        const FluxEstimate fe = combine_point_estimates(pts);
        P1_CHECK(fe.ok);
        P1_CHECKF("neg_w2", std::fabs(fe.var_f - fe.sum_w) / fe.var_f > 1e-3);
    }
    /* M-W3: Q 去掉 C^-1 (a P^T d) */
    {
        double ptd = 0.0;
        for (std::size_t i = 0; i < m; ++i) ptd += P[i] * d[i];
        const double q_mut = a * ptd;
        P1_CHECKF("neg_w3", std::fabs(q_mut - good.q) / std::fabs(good.q) > 1e-3);
    }
    /* A_NEA 变异: 1/Sum P (而非 1/Sum P^2) */
    {
        double sum_p = 0.0, sum_p2 = 0.0;
        for (double v : P) { sum_p += v; sum_p2 += v * v; }
        const double anea_true = psf_anea(P.data(), m);
        const double anea_mut = 1.0 / sum_p;
        P1_CHECKF("neg_anea", std::fabs(anea_mut - anea_true) / anea_true > 1e-3);
        P1_CHECK_NEAR(anea_true, static_cast<double>(oracle::o_anea(P.data(), m)), 1e-12);
    }
    /* 白噪误用: 非对角 C 上用 a^2/(sigma^2 A_NEA) 与真 W 显著不同 */
    {
        const double wn_mut = (a * a) / (4.0 * psf_anea(P.data(), m));
        P1_CHECKF("neg_wn", std::fabs(wn_mut - good.w_info) / good.w_info > 1e-3);
    }

    /* ---- PSFSW 复合 ---- */
    const std::vector<ComponentValues> frames = {
        {100.0, 4.0, 2.0, 200.0}, {180.0, 4.5, 1.8, 220.0}, {130.0, 3.8, 2.2, 190.0}};
    const CompositeResult cr = compute_psfsw_weights(frames);
    P1_CHECK(cr.ok);
    /* m21: 去掉组内 med 归一 -> median != 1 */
    {
        auto wt = [](const ComponentValues& f) {
            return std::pow(f.s, kCompositeAlpha) * std::pow(f.conc, kCompositeBeta) /
                   (std::pow(f.n, kCompositeGamma) * std::pow(f.b, kCompositeDelta));
        };
        std::vector<double> raw = {wt(frames[0]), wt(frames[1]), wt(frames[2])};
        P1_CHECK(std::fabs(oracle::o_median(raw) - 1.0) > 1e-9);
        P1_CHECK_NEAR(oracle::o_median(cr.w_psfsw), 1.0, 1e-12);
    }
    /* m22: C_norm 在归一之外作用于 W (伪实现) -> 不满足尺度简并 */
    {
        auto wt = [](const ComponentValues& f) {
            return std::pow(f.s, kCompositeAlpha) * std::pow(f.conc, kCompositeBeta) /
                   (std::pow(f.n, kCompositeGamma) * std::pow(f.b, kCompositeDelta));
        };
        std::vector<double> raw = {wt(frames[0]), wt(frames[1]), wt(frames[2])};
        const double med = oracle::o_median(raw);
        /* 伪实现: W = C_norm * (Wt/med) —— C_norm 未在归一内消去 */
        const double w_pseudo = 1e9 * (raw[0] / med);
        const double w_true = cr.w_psfsw[0];
        P1_CHECKF("neg_cnorm", std::fabs(w_pseudo - w_true) / w_true > 1e-6);
        P1_CHECK(cnorm_invariance_deviation(frames, 1e9) < 1e-9);
    }
    /* 先 floor 后 fail-closed (顺序颠倒) 必被背景门检出 */
    {
        CompositeParams p;
        p.floor = 1e-12;
        std::vector<ComponentValues> fb = {{100.0, 4.0, 2.0, -5.0}};
        const CompositeResult rb = compute_psfsw_weights(fb, p);
        P1_CHECK(!rb.ok && rb.reason == PsfswReason::background_nonpositive_undefined_transform);
    }
    /* 指数方向反转 (alpha=-2) */
    {
        CompositeParams p;
        p.alpha = -2.0;
        P1_CHECK(!composite_exponents_valid(p));
    }

    /* ---- covariance: C_out = R C_in R^T ---- */
    {
        const int K = 3;
        const std::size_t npix = 2;
        std::vector<std::vector<double>> dd(K, std::vector<double>(npix, 0.0));
        std::vector<std::vector<char>> vv(K, std::vector<char>(npix, 1));
        const std::vector<double> w = {1.0, 2.0, 4.0};
        const CoaddResult co = conventional_coadd(dd, vv, w);
        P1_CHECK(co.ok);
        std::vector<double> c_in(K * K, 0.0);
        for (int i = 0; i < K; ++i)
            for (int j = 0; j < K; ++j)
                c_in[i * K + j] = (i == j) ? (1.0 + i) : 0.5;
        const CovariancePropagation cp = propagate_covariance(co.alpha, c_in);
        P1_CHECK(cp.ok);
        /* 伪实现: 忽略非对角项 (仅对角求和) -> 与真 C_out 不同 */
        double diag_only = 0.0;
        for (int i = 0; i < K; ++i) diag_only += co.alpha[i][0] * co.alpha[i][0] * c_in[i * K + i];
        P1_CHECKF("neg_cov_diag", std::fabs(diag_only - cp.var_out[0]) / cp.var_out[0] > 1e-3);
        /* 伪实现: variance = 1/W_psfsw (禁止) */
        double sum_w = 0.0;
        for (double x : w) sum_w += x;
        P1_CHECKF("neg_cov_weight", std::fabs(1.0 / sum_w - cp.var_out[0]) / cp.var_out[0] > 1e-3);
        /* MC 独立复算 */
        const std::vector<double> mc = oracle::o_mc_coadd_var(co.alpha, c_in, 40000, 7ULL);
        P1_CHECK(std::fabs(cp.var_out[0] - mc[0]) / mc[0] <= 0.05);
    }

    /* ---- V8: 参考来源共同星集过深度门; 逐帧样本派生变体不过 (负向控制) ---- */
    {
        std::vector<DepthScanPoint> reference;   /* 参考来源固定: W 不随深度变化 */
        std::vector<DepthScanPoint> sample_derived;   /* 样本派生: W 随深度漂移 13.2% */
        for (int t = 0; t < 6; ++t) {
            DepthScanPoint r, s;
            r.mag_limit = 16.0 + 0.5 * t;
            r.n_common = 40 - 2 * t;
            r.w_psfsw = {0.5, 1.0, 2.0};
            reference.push_back(r);
            s.mag_limit = 16.0 + 0.5 * t;
            s.n_common = 40 - 2 * t;
            const double drift = 1.0 - 0.132 * (t / 5.0);   /* 13.2% 位移 (W1 Oracle K8) */
            s.w_psfsw = {0.5 * drift, 1.0 * drift, 2.0 * drift};
            sample_derived.push_back(s);
        }
        P1_CHECKF("neg_v8_ref", depth_stability_gate(reference).ok);
        const DepthGateResult sd = depth_stability_gate(sample_derived);
        P1_CHECKF("neg_v8_sample", !sd.ok && sd.reason == PsfswReason::selection_bias_gate_failed);
        P1_CHECK(sd.max_rel_dev > kDepthMaxRelDev);
    }
}
