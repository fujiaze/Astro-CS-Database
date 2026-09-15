// lib/phase2 tests — tests/unit/v6_p2_samp/v6_p2_samp_test.cpp
//
// IMPL-P2-SAMP-001 共址单元测试：Phase2 采样/覆盖 V6 目标态
//   - coverage/support 区分（FZ-GATE-SUPPORT-COVERAGE）
//   - 权重角色门（FZ-GATE-MEDIAN-SNR / FZ-FIELD-WEIGHTMODE / FZ-MODE-*）
//   - 确定性边界（DESIGN-P2 §8）
//   - 空间模型求值（DESIGN-P2 §7）
//   - 帧级标量降级门（FZ-DEGRADE-SCALAR）
//
// 正例与负例并列；负例断言"违反冻结即失败"。独立 Oracle 见
// run/v6/p2-samp/oracle/check_spec.py（Python 重算 + 结构校验）。
//
// 单位（冻结表）：signal_sb=ADU/px^2、sb_variance_out=ADU^2/px^4、
// W_info=ADU^-2、psfsw_robust_weight=1。support/coverage 不是权重。
#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  FAIL: %s\n", what);
    }
}

bool near(double a, double b, double rtol) {
    if (std::isnan(a) || std::isnan(b)) return false;
    const double d = std::fabs(a - b);
    const double m = std::max(std::fabs(a), std::fabs(b));
    return d <= rtol * (m > 1.0 ? m : 1.0);
}

// 独立参考：long double、代数重排的 bilinear（不调用被测实现）。
long double ref_bilinear(long double v00, long double v10, long double v01,
                         long double v11, long double tx, long double ty) {
    return v00 + tx * (v10 - v00) + ty * (v01 - v00) +
           tx * ty * (v00 - v10 - v01 + v11);
}

// ---------------- 1. coverage/support 分类 ----------------
int case_covsupport() {
    // 5 cells: [covered+3sup], [covered+1sup], [uncovered], [missing support],
    //          [missing coverage]
    const std::uint8_t cov[5] = {1, 1, 0, 1, 0};
    const std::uint8_t cov_known[5] = {1, 1, 1, 1, 0};
    const std::uint32_t sup[5] = {3, 1, 9, 5, 4};
    const std::uint8_t sup_known[5] = {1, 1, 1, 0, 1};
    P2CoverageSupportInput in{};
    in.n_cells = 5;
    in.coverage = cov;
    in.coverage_known = cov_known;
    in.support_frames = sup;
    in.support_known = sup_known;
    in.min_support_frames = 2;
    P2CellState st[5] = {};
    std::uint64_t nsup = 0, nunsup = 0, nunc = 0, nunav = 0;
    char err[256] = {0};
    const int rc = p2_coverage_support_classify(&in, st, &nsup, &nunsup, &nunc,
                                                &nunav, err, sizeof(err));
    check(rc == 0, "classify rc==0");
    check(st[0] == P2_CELL_SUPPORTED, "cell0 supported (3>=2)");
    check(st[1] == P2_CELL_COVERED_UNSUPPORTED, "cell1 covered unsupported (1<2)");
    check(st[2] == P2_CELL_UNCOVERED, "cell2 uncovered");
    check(st[3] == P2_CELL_UNAVAILABLE, "cell3 missing support -> UNAVAILABLE");
    check(st[4] == P2_CELL_UNAVAILABLE, "cell4 missing coverage -> UNAVAILABLE");
    check(nsup == 1 && nunsup == 1 && nunc == 1 && nunav == 2,
          "classify counts exact");
    // coverage_known=nullptr 视为全部已知
    P2CoverageSupportInput in2 = in;
    in2.coverage_known = nullptr;
    P2CellState st2[5] = {};
    const int rc2 = p2_coverage_support_classify(&in2, st2, nullptr, nullptr,
                                                 nullptr, nullptr, nullptr, 0);
    check(rc2 == 0 && st2[1] == P2_CELL_COVERED_UNSUPPORTED &&
              st2[3] == P2_CELL_UNAVAILABLE,
          "coverage_known=null means all known; support still authoritative");
    // 零填充陷阱：min_support_frames=0 时 missing 仍必须是 UNAVAILABLE
    P2CoverageSupportInput in3 = in;
    in3.min_support_frames = 0;
    P2CellState st3[5] = {};
    std::uint64_t nu3 = 0;
    (void)p2_coverage_support_classify(&in3, st3, nullptr, nullptr, nullptr,
                                       &nu3, nullptr, 0);
    check(st3[3] == P2_CELL_UNAVAILABLE && nu3 == 2,
          "missing support never zero-filled even with min_support=0");
    return 0;
}

// ---------------- 2. coverage/support 负例 ----------------
int case_covsupport_negative() {
    char err[256] = {0};
    check(p2_coverage_support_classify(nullptr, nullptr, nullptr, nullptr,
                                       nullptr, nullptr, err, sizeof(err)) == 1,
          "null input rejected");
    P2CellState st[1] = {};
    P2CoverageSupportInput in{};
    in.n_cells = 1;
    check(p2_coverage_support_classify(&in, st, nullptr, nullptr, nullptr,
                                       nullptr, err, sizeof(err)) == 1,
          "null faces rejected");
    check(p2_coverage_support_classify(&in, nullptr, nullptr, nullptr, nullptr,
                                       nullptr, err, sizeof(err)) == 1,
          "null out rejected");
    // support_known=0 明确不是"已知 0 支撑"
    const std::uint8_t cov[1] = {1};
    const std::uint32_t sup[1] = {0};
    const std::uint8_t known[1] = {0};
    in.coverage = cov;
    in.support_frames = sup;
    in.support_known = known;
    in.min_support_frames = 1;
    std::uint64_t ns = 0, ncu = 0, nun = 0;
    (void)p2_coverage_support_classify(&in, st, &ns, &ncu, nullptr, &nun, err,
                                       sizeof(err));
    check(st[0] == P2_CELL_UNAVAILABLE && nun == 1 && ncu == 0 && ns == 0,
          "support_known=0 is UNAVAILABLE, not COVERED_UNSUPPORTED");
    return 0;
}

// ---------------- 3. 权重角色门 ----------------
int case_weight_gate() {
    // 冻结 forbidden.weight_source_tokens（与 freeze JSON 逐项一致；
    // 结构一致性另由 check_spec.py 机器校验）。
    const char* forbidden[] = {
        "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
        "support", "support_area", "coverage", "coverage_area",
        "fwhm", "psf_fwhm", "median_fwhm", "source_fwhm",
        "residual", "psf_residual", "psf_fit_residual", "fit_residual",
        "psfsw_robust_weight", "psfsw",
    };
    const std::uint64_t nf = sizeof(forbidden) / sizeof(forbidden[0]);
    for (std::uint64_t i = 0; i < nf; ++i) {
        char err[256] = {0};
        const int rc = p2_weight_source_token_reject(&forbidden[i], 1, err,
                                                     sizeof(err));
        if (rc != 1) {
            ++g_fail;
            std::printf("  FAIL: forbidden token '%s' not rejected (rc=%d)\n",
                        forbidden[i], rc);
        } else {
            ++g_pass;
        }
    }
    // 大小写不敏感
    {
        const char* t[1] = {"COVERAGE"};
        char err[256] = {0};
        check(p2_weight_source_token_reject(t, 1, err, sizeof(err)) == 1,
              "case-insensitive forbidden token");
    }
    // 合法权重来源 token 不被误杀
    const char* allowed[] = {"psf", "photometric_response", "noise_covariance",
                             "design_matrix", "upm_scale", "unit_weight",
                             "pixel_ivar", "psfsw.signal"};
    for (std::size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
        char err[256] = {0};
        const int rc = p2_weight_source_token_reject(&allowed[i], 1, err,
                                                     sizeof(err));
        if (rc != 0) {
            ++g_fail;
            std::printf("  FAIL: benign token '%s' rejected (rc=%d)\n",
                        allowed[i], rc);
        } else {
            ++g_pass;
        }
    }
    // 空 token 跳过
    {
        const char* t[2] = {"", nullptr};
        char err[256] = {0};
        check(p2_weight_source_token_reject(t, 2, err, sizeof(err)) == 0,
              "empty/null tokens skipped");
        check(p2_weight_source_token_reject(nullptr, 1, err, sizeof(err)) == 2,
              "null tokens with n>0 -> param error");
    }
    // 生产模式门
    const char* prod[] = {"point_information", "surface_gls", "psfsw_robust"};
    for (std::size_t i = 0; i < 3; ++i) {
        char err[256] = {0};
        check(p2_weight_mode_check(prod[i], err, sizeof(err)) == 0,
              "production mode accepted");
    }
    const char* bad[] = {"psf_snr_power", "auto", "support_x_snr2", "0", "1",
                         "2", "unknown", ""};
    for (std::size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        char err[256] = {0};
        const int rc = p2_weight_mode_check(bad[i], err, sizeof(err));
        if (rc != 1) {
            ++g_fail;
            std::printf("  FAIL: forbidden mode '%s' rc=%d (want 1)\n", bad[i],
                        rc);
        } else {
            ++g_pass;
        }
    }
    {
        char err[256] = {0};
        check(p2_weight_mode_check(nullptr, err, sizeof(err)) == 1,
              "null mode rejected");
        check(p2_weight_mode_check("equal", err, sizeof(err)) == 2,
              "equal is baseline not production");
        check(p2_weight_mode_check("pixel_ivar", err, sizeof(err)) == 2,
              "pixel_ivar is baseline not production");
    }
    return 0;
}

// ---------------- 4. 确定性边界 ----------------
int case_boundary_determinism() {
    // 跨 tile 边界：同父的叶节点归属唯一 owner，与顺序无关。
    const std::uint64_t leafs[6] = {0, 1, 2, 3, 4, 5};
    const std::uint64_t owners4 = p2_tile_boundary_owner(4, 1);
    const std::uint64_t owners5 = p2_tile_boundary_owner(5, 1);
    const std::uint64_t owners6 = p2_tile_boundary_owner(6, 1);
    const std::uint64_t owners7 = p2_tile_boundary_owner(7, 1);
    check(owners4 == 1 && owners5 == 1 && owners6 == 1 && owners7 == 1,
          "boundary leaves 4..7 share parent owner 1 (deterministic)");
    check(p2_tile_boundary_owner(3, 1) == 0, "leaf 3 owner 0");
    check(p2_tile_boundary_owner(8, 2) == 0, "leaf 8 with shift 2 owner 0");
    check(p2_tile_boundary_owner(0, -1) == UINT64_MAX,
          "negative shift -> invalid sentinel");
    check(p2_tile_boundary_owner(0, 32) == UINT64_MAX,
          "shift 32 -> invalid sentinel");
    (void)leafs;

    // 确定性规约序：打乱输入 → 相同升序去重输出。
    const std::uint64_t a[7] = {7, 3, 1, 7, 5, 3, 9};
    const std::uint64_t b[7] = {9, 5, 7, 1, 3, 3, 7};
    std::uint64_t out_a[7] = {}, out_b[7] = {};
    std::uint64_t na = 0, nb = 0;
    char err[256] = {0};
    check(p2_deterministic_reduction_order(a, 7, out_a, 7, &na, err, sizeof(err)) == 0,
          "reduction order rc=0");
    check(p2_deterministic_reduction_order(b, 7, out_b, 7, &nb, err, sizeof(err)) == 0,
          "reduction order rc=0 (shuffled)");
    check(na == 5 && nb == 5, "dedup count 5");
    check(std::memcmp(out_a, out_b, sizeof(std::uint64_t) * 5) == 0,
          "shuffled input -> identical canonical order");
    const std::uint64_t expect[5] = {1, 3, 5, 7, 9};
    check(std::memcmp(out_a, expect, sizeof(std::uint64_t) * 5) == 0,
          "canonical order ascending");
    // 容量 probe/fill
    std::uint64_t need = 0;
    check(p2_deterministic_reduction_order(b, 7, nullptr, 0, &need, err,
                                           sizeof(err)) == 0 &&
              need == 5,
          "capacity probe reports 5");
    check(p2_deterministic_reduction_order(b, 7, out_b, 4, &need, err,
                                           sizeof(err)) == 1,
          "insufficient capacity rejected");
    check(p2_deterministic_reduction_order(nullptr, 3, out_b, 7, &need, err,
                                           sizeof(err)) == 1,
          "null input with n>0 rejected");
    return 0;
}

// ---------------- 5. 空间模型求值（正例 + 独立 Oracle） ----------------
void make_plane(P2SpatialGridModel* m, std::vector<double>* values,
                std::uint64_t nx, std::uint64_t ny, double a, double b,
                double c, double step) {
    values->assign((std::size_t)(nx * ny), 0.0);
    for (std::uint64_t iy = 0; iy < ny; ++iy) {
        for (std::uint64_t ix = 0; ix < nx; ++ix) {
            (*values)[(std::size_t)(iy * nx + ix)] =
                a * (double)ix + b * (double)iy + c;  // 平面
        }
    }
    m->ra0_deg = 0.0;
    m->dec0_deg = 0.0;
    m->step_deg = step;
    m->nx = nx;
    m->ny = ny;
    m->value = values->data();
    m->valid = nullptr;  // 由调用方设置
}

int case_spatial() {
    const std::uint64_t nx = 5, ny = 4;
    std::vector<double> vals;
    std::vector<std::uint8_t> valid((std::size_t)(nx * ny), 1);
    P2SpatialGridModel m{};
    make_plane(&m, &vals, nx, ny, 3.0, 2.0, 5.0, 1.0);
    m.valid = valid.data();
    // 平面在不同位置的解析值 = 3*ra + 2*dec + 5（step=1）
    double pts[][2] = {{0.0, 0.0},   {0.25, 0.25}, {1.5, 2.25},
                       {3.999, 2.999}, {4.0, 3.0},   {2.0, 1.0}};
    for (std::size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i) {
        P2SpatialEval e{};
        const int rc = p2_spatial_model_eval(&m, pts[i][0], pts[i][1], &e);
        const double expect = 3.0 * pts[i][0] + 2.0 * pts[i][1] + 5.0;
        if (rc != 0 || e.status != P2_SPATIAL_OK || !near(e.value, expect, 1e-12)) {
            ++g_fail;
            std::printf("  FAIL: spatial plane at (%.3f,%.3f) rc=%d val=%.15g "
                        "expect=%.15g\n", pts[i][0], pts[i][1], rc, e.value,
                        expect);
        } else {
            ++g_pass;
        }
        check(e.n_used_nodes == 4 || pts[i][0] == 4.0 || pts[i][1] == 3.0,
              "4 nodes used for interior points");
    }
    // 与独立 long-double 重排参考逐点对拍
    for (int t = 0; t < 40; ++t) {
        const double ra = 0.001 + 0.0999 * (double)t;       // 0..~4
        const double dec = 0.001 + 0.071 * (double)t;       // 0..~2.8
        if (ra > 4.0 || dec > 3.0) continue;
        P2SpatialEval e{};
        if (p2_spatial_model_eval(&m, ra, dec, &e) != 0) {
            ++g_fail;
            std::printf("  FAIL: eval rc!=0 at (%.4f,%.4f)\n", ra, dec);
            continue;
        }
        const std::uint64_t ix0 = (std::uint64_t)std::floor(ra);
        const std::uint64_t iy0 = (std::uint64_t)std::floor(dec);
        const long double tx = (long double)ra - (long double)ix0;
        const long double ty = (long double)dec - (long double)iy0;
        const long double v00 = (long double)vals[(std::size_t)(iy0 * nx + ix0)];
        const long double v10 = (long double)vals[(std::size_t)(iy0 * nx + ix0 + 1)];
        const long double v01 = (long double)vals[(std::size_t)((iy0 + 1) * nx + ix0)];
        const long double v11 =
            (long double)vals[(std::size_t)((iy0 + 1) * nx + ix0 + 1)];
        const long double ref = ref_bilinear(v00, v10, v01, v11, tx, ty);
        check(near(e.value, (double)ref, 1e-12), "matches long-double oracle");
    }
    // 常量场（所有节点同值）→ 处处精确
    std::vector<double> cvals((std::size_t)(nx * ny), 42.5);
    P2SpatialGridModel cm = m;
    cm.value = cvals.data();
    for (int t = 0; t < 10; ++t) {
        P2SpatialEval e{};
        const double ra = 0.01 + 0.43 * (double)t;
        if (ra > 4.0) break;
        (void)p2_spatial_model_eval(&cm, ra, 1.3, &e);
        check(near(e.value, 42.5, 1e-12), "constant field exact (rtol 1e-12)");
    }
    return 0;
}

// ---------------- 6. 空间模型负例（缺失/NaN/越域） ----------------
int case_spatial_negative() {
    const std::uint64_t nx = 3, ny = 3;
    std::vector<double> vals((std::size_t)(nx * ny), 1.0);
    std::vector<std::uint8_t> valid((std::size_t)(nx * ny), 1);
    P2SpatialGridModel m{};
    m.ra0_deg = 0.0;
    m.dec0_deg = 0.0;
    m.step_deg = 1.0;
    m.nx = nx;
    m.ny = ny;
    m.value = vals.data();
    m.valid = valid.data();
    // 缺失节点 (1,1) → 命中该格四角的求值必须 MISSING_NODE
    valid[(std::size_t)(1 * nx + 1)] = 0;
    P2SpatialEval e{};
    int rc = p2_spatial_model_eval(&m, 0.5, 0.5, &e);
    check(rc == 1 && e.status == P2_SPATIAL_MISSING_NODE,
          "missing support node -> MISSING_NODE (no zero-fill)");
    check(std::isnan(e.value), "value is NaN on failure");
    // 恢复但值 NaN
    valid[(std::size_t)(1 * nx + 1)] = 1;
    vals[(std::size_t)(1 * nx + 1)] = std::nan("");
    rc = p2_spatial_model_eval(&m, 0.5, 0.5, &e);
    check(rc == 1 && e.status == P2_SPATIAL_MISSING_NODE,
          "NaN node -> MISSING_NODE (no zero-fill)");
    vals[(std::size_t)(1 * nx + 1)] = 1.0;
    // 越域（不外插）
    rc = p2_spatial_model_eval(&m, -0.001, 0.5, &e);
    check(rc == 1 && e.status == P2_SPATIAL_OUT_OF_DOMAIN, "left of domain rejected");
    rc = p2_spatial_model_eval(&m, 0.5, 2.001, &e);
    check(rc == 1 && e.status == P2_SPATIAL_OUT_OF_DOMAIN, "above domain rejected");
    rc = p2_spatial_model_eval(&m, std::nan(""), 0.5, &e);
    check(rc == 1 && e.status == P2_SPATIAL_OUT_OF_DOMAIN,
          "NaN position rejected as out-of-domain");
    // 非法模型
    P2SpatialGridModel bad = m;
    bad.nx = 1;
    rc = p2_spatial_model_eval(&bad, 0.0, 0.0, &e);
    check(rc == 1 && e.status == P2_SPATIAL_INVALID_MODEL, "nx<2 invalid");
    bad = m;
    bad.step_deg = 0.0;
    rc = p2_spatial_model_eval(&bad, 0.0, 0.0, &e);
    check(rc == 1 && e.status == P2_SPATIAL_INVALID_MODEL, "step<=0 invalid");
    check(p2_spatial_model_eval(nullptr, 0.0, 0.0, &e) == 1,
          "null model rejected");
    check(p2_spatial_model_eval(&m, 0.0, 0.0, nullptr) == 1,
          "null out rejected");
    return 0;
}

// ---------------- 7. 空间摘要 ----------------
int case_summary() {
    const std::uint64_t nx = 4, ny = 4;
    std::vector<double> vals((std::size_t)(nx * ny), 0.0);
    std::vector<std::uint8_t> valid((std::size_t)(nx * ny), 1);
    for (std::size_t i = 0; i < vals.size(); ++i) vals[i] = (double)i;  // 0..15
    valid[5] = 0;
    valid[15] = 0;  // 两个缺失
    P2SpatialGridModel m{};
    m.ra0_deg = 0.0; m.dec0_deg = 0.0; m.step_deg = 1.0;
    m.nx = nx; m.ny = ny; m.value = vals.data(); m.valid = valid.data();
    P2SpatialSummary s{};
    char err[256] = {0};
    const int rc = p2_spatial_model_summary(&m, &s, err, sizeof(err));
    check(rc == 0, "summary rc==0");
    // 独立重算
    std::vector<double> kept;
    for (std::size_t i = 0; i < vals.size(); ++i)
        if (valid[i] != 0) kept.push_back(vals[i]);
    std::sort(kept.begin(), kept.end());
    const std::size_t n = kept.size();
    auto q = [&](double p) -> double {
        std::size_t i = (std::size_t)std::floor(p * (double)(n - 1));
        if (i >= n) i = n - 1;
        return kept[i];
    };
    double maxdev = 0.0, sse = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = kept[i] - q(0.5);
        maxdev = std::max(maxdev, std::fabs(d));
        sse += d * d;
    }
    check(s.n_nodes == 16 && s.n_valid == 14, "summary counts");
    check(near(s.p05, q(0.05), 1e-12) && near(s.p50, q(0.50), 1e-12) &&
              near(s.p95, q(0.95), 1e-12),
          "summary quantiles match independent recomputation");
    check(near(s.max_systematic_deviation, maxdev, 1e-12), "summary max dev");
    check(near(s.model_error, std::sqrt(sse / (double)n), 1e-12), "summary model error");
    check(near(s.sampling_coverage, (double)n / 16.0, 1e-12), "summary sampling coverage");
    // 负例：全部缺失
    std::vector<std::uint8_t> none((std::size_t)(nx * ny), 0);
    P2SpatialGridModel m2 = m;
    m2.valid = none.data();
    check(p2_spatial_model_summary(&m2, &s, err, sizeof(err)) == 1,
          "all-missing summary rejected");
    // 负例：valid=1 但值非有限
    std::vector<double> bad = vals;
    bad[0] = std::nan("");
    P2SpatialGridModel m3 = m;
    m3.value = bad.data();
    check(p2_spatial_model_summary(&m3, &s, err, sizeof(err)) == 1,
          "non-finite valid node rejected");
    check(p2_spatial_model_summary(nullptr, &s, err, sizeof(err)) == 1,
          "null model summary rejected");
    check(p2_spatial_model_summary(&m, nullptr, err, sizeof(err)) == 1,
          "null out summary rejected");
    return 0;
}

// ---------------- 8. 帧级标量降级门 ----------------
P2ScalarSummaryInput good_summary() {
    P2ScalarSummaryInput s{};
    s.summary_complete = 1;
    s.spatial_residual_p95 = 0.01;
    s.spatial_trend = 0.02;
    s.power_loss = 0.01;
    s.p05 = 0.9;
    s.p50 = 1.0;
    s.p95 = 1.1;
    s.max_systematic_deviation = 0.05;
    s.sampling_coverage = 0.97;
    s.model_error = 0.03;
    std::snprintf(s.applicability_domain, sizeof(s.applicability_domain),
                  "pixfrac in [0.5,1.0], scale in [0.1,0.5] arcsec");
    return s;
}

int case_scalar_gate() {
    P2ScalarGateThresholds th{};
    th.thresholds_declared = 1;
    th.residual_trend_max = 0.10;
    th.power_loss_max = 0.05;
    const P2ScalarSummaryInput s = good_summary();
    P2ScalarDegradeVerdict v{};
    char err[256] = {0};
    // 正例：双门通过 + 摘要完整 → ALLOWED
    int rc = p2_scalar_degrade_gate(&th, &s, &v, err, sizeof(err));
    check(rc == 0 && v == P2_SCALAR_ALLOWED, "both gates pass -> ALLOWED");
    // 负例：阈值未声明 → UNAVAILABLE（SO-07 未签字，不得自行定值）
    // 阈值数值在场但 declared=0：仍必须 fail-closed（未签字不得启用）
    P2ScalarGateThresholds und{};
    und.thresholds_declared = 0;
    und.residual_trend_max = 0.10;
    und.power_loss_max = 0.05;
    rc = p2_scalar_degrade_gate(&und, &s, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE,
          "undeclared thresholds -> UNAVAILABLE (fail-closed)");
    // 负例：阈值非正/NaN（放宽即违规）
    P2ScalarGateThresholds loose = th;
    loose.residual_trend_max = -1.0;
    rc = p2_scalar_degrade_gate(&loose, &s, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "negative threshold rejected");
    P2ScalarGateThresholds loose2 = th;
    loose2.power_loss_max = std::nan("");
    rc = p2_scalar_degrade_gate(&loose2, &s, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "NaN threshold rejected");
    // 负例：(a) 空间残差/趋势门失败 → RETAIN
    P2ScalarSummaryInput s2 = s;
    s2.spatial_residual_p95 = 0.20;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_RETAIN_SPATIAL,
          "residual gate fail -> RETAIN_SPATIAL");
    s2 = s;
    s2.spatial_trend = 0.30;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_RETAIN_SPATIAL, "trend gate fail -> RETAIN_SPATIAL");
    // 负例：(b) 功率损失门失败 → RETAIN
    s2 = s;
    s2.power_loss = 0.20;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_RETAIN_SPATIAL, "power-loss gate fail -> RETAIN_SPATIAL");
    // 负例：摘要不完整 → UNAVAILABLE
    s2 = s;
    s2.summary_complete = 0;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "incomplete summary -> UNAVAILABLE");
    // 负例：分位数倒挂 / 覆盖越界 / 空域
    s2 = s;
    s2.p05 = 1.2;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "p05>p50 rejected");
    s2 = s;
    s2.p50 = 1.5;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "p50>p95 rejected");
    s2 = s;
    s2.sampling_coverage = 1.5;
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "coverage>1 rejected");
    s2 = s;
    s2.applicability_domain[0] = '\0';
    rc = p2_scalar_degrade_gate(&th, &s2, &v, err, sizeof(err));
    check(rc == 1 && v == P2_SCALAR_UNAVAILABLE, "empty applicability domain rejected");
    // 参数错误
    check(p2_scalar_degrade_gate(nullptr, &s, &v, err, sizeof(err)) == 2,
          "null thresholds -> param error");
    check(p2_scalar_degrade_gate(&th, nullptr, &v, err, sizeof(err)) == 2,
          "null summary -> param error");
    check(p2_scalar_degrade_gate(&th, &s, nullptr, err, sizeof(err)) == 2,
          "null verdict -> param error");
    return 0;
}

// ---------------- 9. 逐位确定性 ----------------
int case_determinism() {
    const std::uint64_t nx = 6, ny = 5;
    std::vector<double> vals((std::size_t)(nx * ny));
    for (std::size_t i = 0; i < vals.size(); ++i)
        vals[i] = std::sin((double)i * 0.37) * 10.0 + 100.0;
    std::vector<std::uint8_t> valid((std::size_t)(nx * ny), 1);
    valid[7] = 0;
    P2SpatialGridModel m{};
    m.ra0_deg = 10.0; m.dec0_deg = -5.0; m.step_deg = 0.25;
    m.nx = nx; m.ny = ny; m.value = vals.data(); m.valid = valid.data();

    std::vector<double> first;
    std::vector<double> first_summary;
    for (int rep = 0; rep < 3; ++rep) {
        std::vector<double> cur;
        for (int t = 0; t < 25; ++t) {
            P2SpatialEval e{};
            const double ra = 10.0 + 0.03 * (double)t;
            const double dec = -5.0 + 0.02 * (double)t;
            if (p2_spatial_model_eval(&m, ra, dec, &e) == 0) cur.push_back(e.value);
            else cur.push_back(std::nan(""));
        }
        P2SpatialSummary s{};
        char err[256] = {0};
        const int src = p2_spatial_model_summary(&m, &s, err, sizeof(err));
        check(src == 0, "determinism summary rc=0");
        std::vector<double> curs = {s.p05, s.p50, s.p95,
                                    s.max_systematic_deviation, s.model_error,
                                    s.sampling_coverage};
        if (rep == 0) {
            first = cur;
            first_summary = curs;
        } else {
            check(cur.size() == first.size() &&
                      std::memcmp(cur.data(), first.data(),
                                  cur.size() * sizeof(double)) == 0,
                  "repeated spatial eval bitwise identical");
            check(std::memcmp(curs.data(), first_summary.data(),
                              curs.size() * sizeof(double)) == 0,
                  "repeated summary bitwise identical");
        }
    }
    // 分类逐位确定
    std::vector<std::uint8_t> cov(20, 1);
    std::vector<std::uint32_t> sup(20);
    std::vector<std::uint8_t> known(20, 1);
    for (std::size_t i = 0; i < 20; ++i) {
        sup[i] = (std::uint32_t)(i % 4);
        if (i == 3) known[i] = 0;
    }
    P2CoverageSupportInput in{};
    in.n_cells = 20; in.coverage = cov.data(); in.coverage_known = nullptr;
    in.support_frames = sup.data(); in.support_known = known.data();
    in.min_support_frames = 2;
    P2CellState a[20] = {}, b[20] = {};
    (void)p2_coverage_support_classify(&in, a, nullptr, nullptr, nullptr, nullptr, nullptr, 0);
    (void)p2_coverage_support_classify(&in, b, nullptr, nullptr, nullptr, nullptr, nullptr, 0);
    check(std::memcmp(a, b, sizeof(a)) == 0, "classify bitwise deterministic");
    return 0;
}

// ---------------- 10. 独立 Oracle 用 dump（JSON） ----------------
// 写出模型定义与实测输出；Python Oracle 从同一模型定义独立重算并比对
// （不调用被测实现求真值）。非有限值写 null。
void jnum(FILE* f, double v) {
    if (std::isfinite(v)) {
        std::fprintf(f, "%.17g", v);
    } else {
        std::fprintf(f, "null");
    }
}

int dump_json(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (f == nullptr) {
        std::printf("cannot open %s\n", path);
        return 2;
    }
    const std::uint64_t nx = 5, ny = 4;
    const double ra0 = 1.0, dec0 = -2.0, step = 0.5;
    std::vector<double> vals((std::size_t)(nx * ny), 0.0);
    std::vector<std::uint8_t> valid((std::size_t)(nx * ny), 1);
    for (std::uint64_t iy = 0; iy < ny; ++iy) {
        for (std::uint64_t ix = 0; ix < nx; ++ix) {
            vals[(std::size_t)(iy * nx + ix)] =
                3.0 * (double)ix + 2.0 * (double)iy + 5.0;
        }
    }
    valid[(std::size_t)(1 * nx + 1)] = 0;  // 一个缺失节点
    P2SpatialGridModel m{};
    m.ra0_deg = ra0; m.dec0_deg = dec0; m.step_deg = step;
    m.nx = nx; m.ny = ny; m.value = vals.data(); m.valid = valid.data();

    std::fprintf(f, "{\n  \"grid\": {\"ra0\": %.17g, \"dec0\": %.17g, "
                    "\"step\": %.17g, \"nx\": %llu, \"ny\": %llu,\n",
                 ra0, dec0, step, (unsigned long long)nx, (unsigned long long)ny);
    std::fprintf(f, "    \"value\": [");
    for (std::size_t i = 0; i < vals.size(); ++i) {
        std::fprintf(f, "%s", i ? ", " : "");
        jnum(f, vals[i]);
    }
    std::fprintf(f, "],\n    \"valid\": [");
    for (std::size_t i = 0; i < valid.size(); ++i) {
        std::fprintf(f, "%s%d", i ? ", " : "", (int)valid[i]);
    }
    std::fprintf(f, "]},\n");

    const double pts[][2] = {{1.0, -2.0},  {1.75, -1.25}, {2.2, -1.1},
                             {0.9, -2.0},  {3.0, -0.5},   {1.5, -1.5},
                             {2.4, -1.3},  {2.0, -2.0}};
    std::fprintf(f, "  \"evals\": [");
    for (std::size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i) {
        P2SpatialEval e{};
        const int rc = p2_spatial_model_eval(&m, pts[i][0], pts[i][1], &e);
        std::fprintf(f, "%s{\"ra\": %.17g, \"dec\": %.17g, \"rc\": %d, "
                        "\"status\": %d, \"value\": ",
                     i ? ", " : "", pts[i][0], pts[i][1], rc, (int)e.status);
        jnum(f, e.value);
        std::fprintf(f, "}");
    }
    std::fprintf(f, "],\n");

    P2SpatialSummary s{};
    char err[256] = {0};
    const int src = p2_spatial_model_summary(&m, &s, err, sizeof(err));
    std::fprintf(f, "  \"summary\": {\"rc\": %d, \"n_nodes\": %llu, "
                    "\"n_valid\": %llu, \"p05\": %.17g, \"p50\": %.17g, "
                    "\"p95\": %.17g, \"max_systematic_deviation\": %.17g, "
                    "\"model_error\": %.17g, \"sampling_coverage\": %.17g},\n",
                 src, (unsigned long long)s.n_nodes,
                 (unsigned long long)s.n_valid, s.p05, s.p50, s.p95,
                 s.max_systematic_deviation, s.model_error, s.sampling_coverage);

    const std::uint8_t cov[6] = {1, 1, 1, 0, 1, 1};
    const std::uint8_t covk[6] = {1, 1, 1, 1, 0, 1};
    const std::uint32_t sup[6] = {3, 1, 0, 9, 4, 2};
    const std::uint8_t supk[6] = {1, 1, 1, 1, 1, 0};
    P2CoverageSupportInput in{};
    in.n_cells = 6; in.coverage = cov; in.coverage_known = covk;
    in.support_frames = sup; in.support_known = supk; in.min_support_frames = 2;
    P2CellState st[6] = {};
    std::uint64_t nsup = 0, ncu = 0, nunc = 0, nun = 0;
    const int crc = p2_coverage_support_classify(&in, st, &nsup, &ncu, &nunc,
                                                 &nun, err, sizeof(err));
    std::fprintf(f, "  \"classify\": {\"rc\": %d, \"coverage\": [", crc);
    for (int i = 0; i < 6; ++i) std::fprintf(f, "%s%d", i ? "," : "", (int)cov[i]);
    std::fprintf(f, "], \"coverage_known\": [");
    for (int i = 0; i < 6; ++i) std::fprintf(f, "%s%d", i ? "," : "", (int)covk[i]);
    std::fprintf(f, "], \"support\": [");
    for (int i = 0; i < 6; ++i) std::fprintf(f, "%s%u", i ? "," : "", (unsigned)sup[i]);
    std::fprintf(f, "], \"support_known\": [");
    for (int i = 0; i < 6; ++i) std::fprintf(f, "%s%d", i ? "," : "", (int)supk[i]);
    std::fprintf(f, "], \"min_support\": 2, \"states\": [");
    for (int i = 0; i < 6; ++i) std::fprintf(f, "%s%d", i ? "," : "", (int)st[i]);
    std::fprintf(f, "], \"counts\": {\"supported\": %llu, "
                    "\"covered_unsupported\": %llu, \"uncovered\": %llu, "
                    "\"unavailable\": %llu}},\n",
                 (unsigned long long)nsup, (unsigned long long)ncu,
                 (unsigned long long)nunc, (unsigned long long)nun);

    struct G {
        int declared; double rt, pl; int complete; double res, trend, ploss;
        double p05, p50, p95, cov, merr, maxdev; const char* dom;
    } gs[] = {
        {1, 0.10, 0.05, 1, 0.01, 0.02, 0.01, 0.9, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
        {0, 0.10, 0.05, 1, 0.01, 0.02, 0.01, 0.9, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
        {1, 0.10, 0.05, 1, 0.20, 0.02, 0.01, 0.9, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
        {1, 0.10, 0.05, 1, 0.01, 0.02, 0.20, 0.9, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
        {1, 0.10, 0.05, 0, 0.01, 0.02, 0.01, 0.9, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
        {1, 0.10, 0.05, 1, 0.01, 0.02, 0.01, 1.2, 1.0, 1.1, 0.97, 0.03, 0.05, "dom"},
    };
    std::fprintf(f, "  \"scalar\": [");
    for (std::size_t i = 0; i < sizeof(gs) / sizeof(gs[0]); ++i) {
        P2ScalarGateThresholds th{};
        th.thresholds_declared = gs[i].declared;
        th.residual_trend_max = gs[i].rt;
        th.power_loss_max = gs[i].pl;
        P2ScalarSummaryInput si{};
        si.summary_complete = gs[i].complete;
        si.spatial_residual_p95 = gs[i].res;
        si.spatial_trend = gs[i].trend;
        si.power_loss = gs[i].ploss;
        si.p05 = gs[i].p05; si.p50 = gs[i].p50; si.p95 = gs[i].p95;
        si.sampling_coverage = gs[i].cov; si.model_error = gs[i].merr;
        si.max_systematic_deviation = gs[i].maxdev;
        std::snprintf(si.applicability_domain, sizeof(si.applicability_domain),
                      "%s", gs[i].dom);
        P2ScalarDegradeVerdict v{};
        const int rc = p2_scalar_degrade_gate(&th, &si, &v, err, sizeof(err));
        std::fprintf(f, "%s{\"declared\": %d, \"rt_max\": %.17g, "
                        "\"pl_max\": %.17g, \"complete\": %d, "
                        "\"residual\": %.17g, \"trend\": %.17g, "
                        "\"power_loss\": %.17g, \"p05\": %.17g, \"p50\": %.17g, "
                        "\"p95\": %.17g, \"coverage\": %.17g, "
                        "\"model_error\": %.17g, \"max_dev\": %.17g, "
                        "\"domain\": \"%s\", \"rc\": %d, \"verdict\": %d}",
                     i ? ", " : "", gs[i].declared, gs[i].rt, gs[i].pl,
                     gs[i].complete, gs[i].res, gs[i].trend, gs[i].ploss,
                     gs[i].p05, gs[i].p50, gs[i].p95, gs[i].cov, gs[i].merr,
                     gs[i].maxdev, gs[i].dom, rc, (int)v);
    }
    std::fprintf(f, "]\n}\n");
    std::fclose(f);
    return 0;
}

struct Case {
    const char* name;
    int (*fn)();
};

const Case kCases[] = {
    {"covsupport", case_covsupport},
    {"covsupport_negative", case_covsupport_negative},
    {"weight_gate", case_weight_gate},
    {"boundary_determinism", case_boundary_determinism},
    {"spatial", case_spatial},
    {"spatial_negative", case_spatial_negative},
    {"summary", case_summary},
    {"scalar_gate", case_scalar_gate},
    {"determinism", case_determinism},
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <case|all>\n", argv[0]);
        return 2;
    }
    const std::string arg = argv[1];
    if (arg == "dump") {
        if (argc < 3) {
            std::printf("usage: %s dump <path>\n", argv[0]);
            return 2;
        }
        return dump_json(argv[2]);
    }
    int ran = 0;
    if (arg == "all") {
        for (const auto& c : kCases) {
            const int before = g_fail;
            c.fn();
            std::printf("[%s] %s\n", c.name,
                        g_fail == before ? "PASS" : "FAIL");
            ++ran;
        }
    } else {
        bool found = false;
        for (const auto& c : kCases) {
            if (arg == c.name) {
                c.fn();
                std::printf("[%s] %s\n", c.name, g_fail == 0 ? "PASS" : "FAIL");
                found = true;
                ++ran;
                break;
            }
        }
        if (!found) {
            std::printf("unknown case: %s\n", arg.c_str());
            return 2;
        }
    }
    std::printf("checks: pass=%d fail=%d (cases=%d)\n", g_pass, g_fail, ran);
    return g_fail == 0 ? 0 : 1;
}
