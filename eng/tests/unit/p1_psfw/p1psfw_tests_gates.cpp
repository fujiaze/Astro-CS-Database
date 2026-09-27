/* p1psfw_tests_gates.cpp - validity/depth/非均匀/降级门 (正+负) */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

P1PSFW_REGISTER(gates) {
    (void)mode;
    /* ---- n_common 分档 ---- */
    P1_CHECK(n_common_tier(2) == NCommonTier::hard_fail);
    P1_CHECK(n_common_tier(3) == NCommonTier::low);
    P1_CHECK(n_common_tier(9) == NCommonTier::low);
    P1_CHECK(n_common_tier(10) == NCommonTier::standard);
    P1_CHECK(n_common_tier(29) == NCommonTier::standard);
    P1_CHECK(n_common_tier(30) == NCommonTier::preferred);

    /* ---- 深度稳定性门 (PSFSW-T-DEPTH/-K/-SPAN/-RHO) ---- */
    std::vector<DepthScanPoint> stable;
    for (int t = 0; t < 5; ++t) {
        DepthScanPoint s;
        s.mag_limit = 17.0 + 0.3 * t;
        s.n_common = 30;
        s.w_psfsw = {0.5, 1.0, 2.0};   /* 每帧恒定 -> 0 漂移 */
        stable.push_back(s);
    }
    const DepthGateResult gs = depth_stability_gate(stable);
    P1_CHECKF("gates_depth_stable", gs.ok);
    P1_CHECK(gs.max_rel_dev <= kDepthMaxRelDev);

    /* 漂移 20% -> selection_bias_gate_failed (m13) */
    std::vector<DepthScanPoint> drift;
    for (int t = 0; t < 5; ++t) {
        DepthScanPoint s;
        s.mag_limit = 17.0 + 0.3 * t;
        s.n_common = 30;
        const double w = 0.5 + 0.2 * t;   /* 0.5 -> 1.3: rel dev 20% */
        s.w_psfsw = {w, 1.0, 2.0};
        drift.push_back(s);
    }
    const DepthGateResult gd = depth_stability_gate(drift);
    P1_CHECK(!gd.ok && gd.reason == PsfswReason::selection_bias_gate_failed);
    P1_CHECK(gd.max_rel_dev > kDepthMaxRelDev);

    /* 单调漂移但幅度 <5% -> rho 门红 (PSFSW-T-DEPTH-RHO) */
    std::vector<DepthScanPoint> rho;
    for (int t = 0; t < 5; ++t) {
        DepthScanPoint s;
        s.mag_limit = 17.0 + 0.3 * t;
        s.n_common = 30;
        const double w = 1.00 + 0.01 * t;   /* 1.00..1.04, median 1.02, dev<0.05 */
        s.w_psfsw = {w, 1.0, 2.0};
        rho.push_back(s);
    }
    const DepthGateResult gr = depth_stability_gate(rho);
    P1_CHECK(!gr.ok && gr.reason == PsfswReason::selection_bias_gate_failed);
    P1_CHECK(gr.max_abs_rho > kDepthMaxAbsRho);

    /* K<5 -> insufficient_valid_stars (PSFSW-T-DEPTH-K) */
    std::vector<DepthScanPoint> k2(stable.begin(), stable.begin() + 2);
    const DepthGateResult gk = depth_stability_gate(k2);
    P1_CHECK(!gk.ok && gk.reason == PsfswReason::insufficient_valid_stars);
    /* 跨度 <1.0 mag 且 n_common 不变 -> insufficient (PSFSW-T-DEPTH-SPAN) */
    std::vector<DepthScanPoint> span;
    for (int t = 0; t < 5; ++t) {
        DepthScanPoint s;
        s.mag_limit = 17.0 + 0.05 * t;   /* 跨度 0.2 */
        s.n_common = 30;
        s.w_psfsw = {0.5, 1.0, 2.0};
        span.push_back(s);
    }
    const DepthGateResult gsp = depth_stability_gate(span);
    P1_CHECK(!gsp.ok && gsp.reason == PsfswReason::insufficient_valid_stars);
    /* 某扫描点 n_common=2 -> insufficient */
    std::vector<DepthScanPoint> low_n = stable;
    low_n[2].n_common = 2;
    P1_CHECK(!depth_stability_gate(low_n).ok);

    /* ---- 空间非均匀门 (PSFSW-T-NU / -NU-LOW / -TREND) ---- */
    std::vector<ComponentMeasure> flat = fixture::four_components(100.0, 4.0, 2.0, 200.0);
    P1_CHECK(spatial_nonuniformity_gate(flat, NCommonTier::standard).ok);
    /* 40% 散度 -> 红 (m15) */
    std::vector<double> spread40 = {60.0, 100.0, 140.0, 80.0, 120.0};   /* (140-60)/100=0.8 */
    ComponentMeasure sig = fixture::component("signal", "ADU", 100.0, spread40);
    std::vector<ComponentMeasure> nu = fixture::four_components(100.0, 4.0, 2.0, 200.0);
    nu[0] = sig;
    const NonuniformityResult r_nu = spatial_nonuniformity_gate(nu, NCommonTier::standard);
    P1_CHECK(!r_nu.ok && r_nu.reason == PsfswReason::spatial_nonuniformity_gate_failed);
    P1_CHECK(r_nu.spread > kNonuniformityMax);
    /* 25% 散度: standard 档过, LOW 档红 (0.20 收紧) */
    /* 对称排布 -> 线性趋势 ≈ 0, 只检验散度门: (111-89)/100 = 0.22 */
    std::vector<double> spread25 = {89.0, 111.0, 100.0, 111.0, 89.0};
    ComponentMeasure sig25 = fixture::component("signal", "ADU", 100.0, spread25);
    std::vector<ComponentMeasure> nu25 = fixture::four_components(100.0, 4.0, 2.0, 200.0);
    nu25[0] = sig25;
    const double spread_std = quantile_of(spread25, 0.95) - quantile_of(spread25, 0.05);
    P1_CHECK(spread_std / quantile_of(spread25, 0.50) > kNonuniformityMaxLow);   /* >0.20 */
    P1_CHECK(spread_std / quantile_of(spread25, 0.50) <= kNonuniformityMax);     /* <=0.30 */
    P1_CHECK(spatial_nonuniformity_gate(nu25, NCommonTier::standard).ok);
    P1_CHECK(!spatial_nonuniformity_gate(nu25, NCommonTier::low).ok);
    /* 强线性趋势 -> 红 */
    std::vector<double> trend;
    for (int i = 0; i < 10; ++i) trend.push_back(100.0 + 20.0 * i);   /* 斜率 20/步, p50~190, max fit~290 -> 1.5 */
    ComponentMeasure sig_t = fixture::component("signal", "ADU", 100.0, trend);
    std::vector<ComponentMeasure> nu_t = fixture::four_components(100.0, 4.0, 2.0, 200.0);
    nu_t[0] = sig_t;
    const NonuniformityResult r_t = spatial_nonuniformity_gate(nu_t, NCommonTier::standard);
    P1_CHECK(!r_t.ok && r_t.reason == PsfswReason::spatial_nonuniformity_gate_failed);
    P1_CHECK(r_t.trend > kSpatialTrendMax);

    /* ---- 标量降级门 (PSFSW-T-POWERLOSS / -FLUXBIAS) ---- */
    P1_CHECK(scalar_degradation_gate(0.01, 0.001).ok);
    P1_CHECK(!scalar_degradation_gate(0.20, 0.001).ok);
    P1_CHECK(!scalar_degradation_gate(0.01, 0.10).ok);

    /* ---- 权重动态范围 guard (PSFSW-T-WRANGE) ---- */
    const WeightRangeGuard wg1 = weight_dynamic_range_guard({0.001, 1.0});
    P1_CHECK(wg1.exceeded);
    const WeightRangeGuard wg2 = weight_dynamic_range_guard({0.5, 1.0, 2.0});
    P1_CHECK(!wg2.exceeded);
    /* 集中度: sum a^2/(sum a)^2 对 {1,1,1} = 3/9 = 1/3 */
    const WeightRangeGuard wg3 = weight_dynamic_range_guard({1.0, 1.0, 1.0});
    P1_CHECK_NEAR(wg3.concentration, 1.0 / 3.0, 1e-12);
}
