// eng/tests/unit/v6_p2_sky/v6_p2_sky_node_adapt_test.cpp
//
// 节点间距「由输入自适应导出」+「进入自适应重试回路」的共址回归测试（能红能绿 + 负例）。
//
// 规范依据：docs/science/PHASE2_UPM.md §7a
//   · 规则 1：节点间距必须由输入自适应导出（不得是标定常数）；
//             节点间距 ≤ min(重叠带宽度, 指向间距) / 2。
//   · 规则 2：节点间距必须进入自适应重试回路，与 roughness_penalty 同级。
//   · 规则 3：节点间距负责表示能力，粗糙度惩罚负责条件数，互不代偿。
//   · 规则 4：κ 上限不得是两个不同的标定值；门控取 H_solve 的条件数，
//             判据按矩阵谱自身定（相对 rank_rtol 口径）。
//   · 规则 5：每条自适应路径都必须有「触发过」的实测记录。
//
// 被测面：lib/algorithms/coverage/src/sky_plane.cpp
#include "astro/phase2/sky_plane.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_total = 0;
void check(bool ok, const std::string& what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
    else std::printf("  ok  : %s\n", what.c_str());
}
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// 真实 M42 产品实测几何（run/UPM-NODE-ADAPT-01/evidence/geometry.json）：
//   重叠带宽度 0.150379 deg、指向间距 0.939887 deg、control 采样间距 0.014400 deg、
//   源像素角尺度 0.989016 arcsec/px。
P2SkyPlaneGeometry m42_geometry() {
    P2SkyPlaneGeometry g{};
    g.overlap_band_width_deg = 0.150379;
    g.pointing_spacing_deg = 0.939887;
    g.sample_pitch_deg = 0.014400;
    g.pixel_scale_arcsec = 0.989016;
    return g;
}

// ---------------------------------------------------------------- 导出函数

void test_derive_positive() {
    std::printf("[derive_positive]\n");
    const P2SkyPlaneGeometry g = m42_geometry();
    P2SkyPlaneNodeSpacing ns{};
    char err[512] = {0};
    const int rc = p2_sky_plane_derive_node_spacing(&g, &ns, err, sizeof(err));
    check(rc == P2_SKY_NODE_SPACING_OK, std::string("derive ok, rc=") + std::to_string(rc) + " " + err);
    if (rc != 0) return;
    // 规则 1：约束尺度 = min(重叠带, 指向间距)；上界 = 该尺度 / 2
    check(near(ns.constraining_scale_deg, g.overlap_band_width_deg, 1e-12),
          "constraining scale = min(overlap, pointing) = overlap");
    check(near(ns.upper_deg, 0.5 * g.overlap_band_width_deg, 1e-12),
          "upper bound = constraining scale / 2");
    check(ns.node_spacing_deg <= ns.upper_deg * (1.0 + 1e-15),
          "rule 1 satisfied: h <= constraining_scale/2");
    check(near(ns.node_spacing_deg, ns.upper_deg, 1e-15), "derived h = rule upper bound");
    check(near(ns.representable_scale_deg, 2.0 * ns.node_spacing_deg, 1e-15),
          "representable scale = 2h");
    // 下界 = 数据自身分辨率极限 = max(采样间距, 像素角尺度)
    check(near(ns.lower_deg, g.sample_pitch_deg, 1e-15),
          "lower bound = max(sample pitch, pixel scale) = sample pitch");
    // 量纲换算：h_px = h_deg*3600/pixel_scale_arcsec
    const double h_px_expect = ns.node_spacing_deg * 3600.0 / g.pixel_scale_arcsec;
    check(near(ns.node_spacing_px, h_px_expect, 1e-9),
          "h_px = h_deg*3600/pixel_scale_arcsec (dimension chain)");
    check(near(ns.node_spacing_px, 273.75, 0.5),
          "M42 derived h in source px ~273.8 (report says 2h ~ 258-547 px)");
    std::printf("  INFO h=%.6f deg = %.3f src px ; 2h=%.6f deg ; bounds [%.6f, %.6f]\n",
                ns.node_spacing_deg, ns.node_spacing_px, ns.representable_scale_deg,
                ns.lower_deg, ns.upper_deg);
}

void test_derive_pixel_scale_invariance() {
    std::printf("[derive_pixel_scale_invariance]\n");
    // 换仪器：角几何不变、像素角尺度减半 ⇒ h 的**度数不变**、像素数翻倍。
    P2SkyPlaneGeometry g1 = m42_geometry();
    P2SkyPlaneGeometry g2 = m42_geometry();
    g2.pixel_scale_arcsec = 0.5 * g1.pixel_scale_arcsec;
    P2SkyPlaneNodeSpacing a{}, b{};
    check(p2_sky_plane_derive_node_spacing(&g1, &a, nullptr, 0) == 0, "derive g1 ok");
    check(p2_sky_plane_derive_node_spacing(&g2, &b, nullptr, 0) == 0, "derive g2 ok");
    check(a.node_spacing_deg == b.node_spacing_deg,
          "h_deg invariant under pixel-scale change (degree-domain quantity)");
    check(near(b.node_spacing_px, 2.0 * a.node_spacing_px, 1e-9),
          "h_px inversely proportional to pixel scale (doubles when scale halves)");
    check(a.pixel_scale_deg != b.pixel_scale_deg, "pixel_scale_deg follows the input");
}

void test_derive_min_and_bounds() {
    std::printf("[derive_min_and_bounds]\n");
    // 指向间距比重叠带更窄 ⇒ 约束尺度取指向间距
    P2SkyPlaneGeometry g = m42_geometry();
    g.pointing_spacing_deg = 0.05;   // < overlap 0.150379
    P2SkyPlaneNodeSpacing ns{};
    check(p2_sky_plane_derive_node_spacing(&g, &ns, nullptr, 0) == 0, "derive ok");
    check(near(ns.constraining_scale_deg, 0.05, 1e-15), "constraining scale = pointing spacing");
    check(near(ns.upper_deg, 0.025, 1e-15), "upper = pointing spacing / 2");
    // 像素角尺度比采样间距更粗 ⇒ 下界取像素角尺度（仍细于规则上界，故几何相容）
    P2SkyPlaneGeometry g2 = m42_geometry();
    g2.pixel_scale_arcsec = 3600.0 * 0.02;   // 0.02 deg/px > 采样间距 0.0144 deg
    P2SkyPlaneNodeSpacing ns2{};
    check(p2_sky_plane_derive_node_spacing(&g2, &ns2, nullptr, 0) == 0, "derive g2 ok");
    check(near(ns2.lower_deg, 0.02, 1e-15), "lower = max(sample pitch, pixel scale) = pixel scale");
    check(near(ns2.lower_px, 1.0, 1e-12), "lower_px == 1 source pixel (0.02 deg at 72 arcsec/px)");
}

void test_derive_negative() {
    std::printf("[derive_negative]\n");
    char err[512] = {0};
    P2SkyPlaneNodeSpacing ns{};
    // 几何整体缺失
    check(p2_sky_plane_derive_node_spacing(nullptr, &ns, err, sizeof(err)) ==
              P2_SKY_NODE_SPACING_INVALID_ARGS, "null geometry -> INVALID_ARGS (no constant fallback)");
    check(std::strstr(err, "geometry not provided") != nullptr, "err names the missing quantity");
    // 逐个量纲缺失（0 / 非有限）
    const char* names[4] = {"overlap_band_width_deg", "pointing_spacing_deg",
                            "sample_pitch_deg", "pixel_scale_arcsec"};
    for (int k = 0; k < 4; ++k) {
        P2SkyPlaneGeometry g = m42_geometry();
        double* f[4] = {&g.overlap_band_width_deg, &g.pointing_spacing_deg,
                        &g.sample_pitch_deg, &g.pixel_scale_arcsec};
        *f[k] = 0.0;
        err[0] = '\0';
        check(p2_sky_plane_derive_node_spacing(&g, &ns, err, sizeof(err)) ==
                  P2_SKY_NODE_SPACING_INVALID_ARGS,
              std::string("missing ") + names[k] + " -> INVALID_ARGS");
        check(std::strstr(err, names[k]) != nullptr,
              std::string("err names ") + names[k]);
        *f[k] = std::numeric_limits<double>::quiet_NaN();
        check(p2_sky_plane_derive_node_spacing(&g, &ns, err, sizeof(err)) ==
                  P2_SKY_NODE_SPACING_INVALID_ARGS,
              std::string("NaN ") + names[k] + " -> INVALID_ARGS");
    }
    // 规则上界低于数据分辨率极限 ⇒ 几何本身不相容，显式失败（不得静默取其一）
    P2SkyPlaneGeometry bad = m42_geometry();
    bad.overlap_band_width_deg = 0.02;   // upper = 0.01 < lower = 0.0144
    err[0] = '\0';
    check(p2_sky_plane_derive_node_spacing(&bad, &ns, err, sizeof(err)) ==
              P2_SKY_NODE_SPACING_GEOMETRY_UNSUPPORTED,
          "rule upper < resolution limit -> GEOMETRY_UNSUPPORTED (fail-closed)");
    check(std::strstr(err, "unsupported") != nullptr, "err names the unsupported geometry");
}

// ------------------------------------------------------- build 侧的导出/门控

void test_default_config_has_no_calibrated_default() {
    std::printf("[default_config]\n");
    const P2SkyPlaneConfig c = p2_sky_plane_default_config();
    check(c.node_spacing_deg == 0.0,
          "default node_spacing_deg == 0.0 (sentinel 'derive from input', not a constant)");
    // UPM-KAPPA-UNIFY-01：kappa_max / roughness_penalty 已从配置面**删除**
    // （编译期事实：写它们不再能编译）。判据阈值只剩 rank_rtol 一个符号。
    check(c.rank_rtol == 1e-10,
          "default rank_rtol == 1e-10 (FZ-AP2S-RANK-RTOL: the single criterion threshold)");
    check(c.retain_normal_matrices == 0,
          "default retain_normal_matrices == 0 (audit-only retention, off by default)");
    check(c.geometry.overlap_band_width_deg == 0.0 && c.geometry.pointing_spacing_deg == 0.0 &&
              c.geometry.sample_pitch_deg == 0.0 && c.geometry.pixel_scale_arcsec == 0.0,
          "default geometry is empty (caller must supply it)");
}

std::vector<P2SkySample> smooth_samples(int nf, double sigma, unsigned seed) {
    std::vector<P2SkySample> s;
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> g(0.0, sigma);
    for (int k = 0; k < nf; ++k)
        for (int i = 0; i <= 40; ++i)
            for (int j = 0; j <= 40; ++j) {
                const double u = -1.0 + 2.0 * i / 40.0, v = -1.0 + 2.0 * j / 40.0;
                P2SkySample sm{};
                sm.frame_id = 1000 + static_cast<std::uint64_t>(k);
                sm.control_id = static_cast<std::uint64_t>(i * 41 + j);
                sm.ra_deg = u; sm.dec_deg = v;
                sm.value = 100.0 + 3.0 * u - 2.0 * v + 0.5 * u * v + 0.7 * k + g(rng);
                sm.variance = sigma * sigma;
                sm.snr = 1.0 / sigma;
                sm.flags = P2_SKY_FLAG_NONE;
                s.push_back(sm);
            }
    return s;
}

void test_build_requires_geometry_or_explicit_h() {
    std::printf("[build_geometry_required]\n");
    const std::vector<P2SkySample> s = smooth_samples(3, 0.02, 4242);
    char err[1024] = {0};
    void* m = nullptr;
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();   // node_spacing=0, geometry empty
    const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    check(rc == P2_SKY_PLANE_GEOMETRY_REQUIRED,
          std::string("no h and no geometry -> GEOMETRY_REQUIRED (rc=") + std::to_string(rc) + ")");
    check(m == nullptr, "fail-closed: no model produced");
    check(std::strstr(err, "refusing to fall back") != nullptr,
          "err states the refusal to fall back to a calibrated constant");
    std::printf("  INFO err=%s\n", err);
}

void test_build_derives_from_geometry() {
    std::printf("[build_derives_from_geometry]\n");
    const std::vector<P2SkySample> s = smooth_samples(3, 0.02, 777);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.geometry = m42_geometry();
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, std::string("derive-and-build ok rc=") + std::to_string(rc) + " " + err);
    if (rc != 0) return;
    P2SkyPlaneInfo info{};
    p2_sky_plane_info(m, &info);
    P2SkyPlaneNodeSpacing ns{};
    p2_sky_plane_derive_node_spacing(&cfg.geometry, &ns, nullptr, 0);
    check(info.node_spacing_deg == ns.upper_deg,
          "info.node_spacing_deg == derived upper bound (no constant anywhere)");
    check(info.node_spacing_source == 0, "info.node_spacing_source == 0 (derived from geometry)");
    check(info.node_spacing_upper_deg == ns.upper_deg &&
              info.node_spacing_lower_deg == ns.lower_deg,
          "provenance records the derived search bounds");
    std::printf("  INFO h=%.6f upper=%.6f lower=%.6f n_nodes=%llu\n", info.node_spacing_deg,
                info.node_spacing_upper_deg, info.node_spacing_lower_deg,
                (unsigned long long)info.n_nodes);
    p2_sky_plane_close(m);
}

void test_kappa_gate_unified() {
    std::printf("[criterion_unified]\n");
    // 唯一判据：identifiable ⟺ r_eff(H_red) == n_params ⟺ κ(H_red) < 1/τ。
    // 「欠定」与「病态」是同一条不等式的两种读法 ⇒ 只有一个判决位。
    const std::vector<P2SkySample> s = smooth_samples(3, 0.02, 31337);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.geometry = m42_geometry();
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "build ok");
    if (rc != 0) return;
    P2SkyPlaneInfo info{};
    p2_sky_plane_info(m, &info);
    check(info.identifiable == 1, "green side: identifiable == 1 on well-posed input");
    check(info.rank == info.n_params && info.n_unidentified == 0,
          "green side: r_eff == n_params (count), 0 unidentified directions");
    check(info.kappa_data == info.kappa, "kappa_data kept as a bit-exact alias of kappa");
    check(info.rank_rtol_effective >= cfg.rank_rtol,
          "tau_eff >= requested tau (precision floor may tighten it)");
    check(info.lambda_numerical > 0.0, "derived numerical ridge is reported");
    // 等价性（同一件事的两种读数）：identifiable ⟺ κ < 1/τ
    check((info.identifiable == 1) ==
              (info.kappa < 1.0 / info.rank_rtol_effective),
          "verdict == (kappa(H_red) < 1/tau): one inequality, one verdict bit");
    std::printf("  INFO kappa=%.6g kappa_data=%.6g tau=%.3g rank=%llu/%llu rank_solve=%llu "
                "kappa_solve=%.6g\n",
                info.kappa, info.kappa_data, info.rank_rtol_effective,
                (unsigned long long)info.rank, (unsigned long long)info.n_params,
                (unsigned long long)info.rank_solve, info.kappa_solve);
    p2_sky_plane_close(m);

    // 红侧（同一条判据，同一份数据，只把 τ 收紧到 κ(H_red) 之下）：
    // 门必须能被触发，否则它是恒真门。阈值由**实测 κ** 反推，不写死。
    P2SkyPlaneConfig cfg2 = cfg;
    cfg2.rank_rtol = 2.0 / info.kappa;   // ⇒ 1/τ = κ/2 < κ ⇒ 必红
    void* m2 = nullptr;
    err[0] = '\0';
    const int rc2 = p2_sky_plane_build(s.data(), s.size(), &cfg2, &m2, err, sizeof(err));
    check(rc2 == P2_SKY_PLANE_NOT_IDENTIFIABLE,
          std::string("red side: tau tightened below the measured gap => NOT_IDENTIFIABLE (rc=") +
              std::to_string(rc2) + ") " + err);
    check(m2 == nullptr, "red side is fail-closed: no model produced");
    check(std::strstr(err, "rank_eff") != nullptr && std::strstr(err, "kappa(H_red)") != nullptr,
          "red-side error names the criterion readings, not an absolute constant");
}

// ------------------------------------------------------------ 自适应重试回路

// 病态合成输入：B_ref 含 0.30 deg 波长的正弦结构，而规则 1 上界给的节点间距
// （h=0.2 deg，2h=0.4 deg）表示不了它 ⇒ 细化节点必须能实质降低 chi2_red。
std::vector<P2SkySample> fine_structure_samples(double wavelength_deg, int nf, double sigma,
                                                unsigned seed) {
    // 采样间距 0.05 deg（= 用例几何的下界），41x41/帧，节点数最多 ~1849 ⇒ 求解快。
    std::vector<P2SkySample> s;
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> g(0.0, sigma);
    const double k2pi = 6.28318530717958647692;
    for (int k = 0; k < nf; ++k)
        for (int i = 0; i <= 40; ++i)
            for (int j = 0; j <= 40; ++j) {
                const double u = -1.0 + 2.0 * i / 40.0, v = -1.0 + 2.0 * j / 40.0;
                const double b = 100.0 + 2.0 * u + 5.0 * std::sin(k2pi * u / wavelength_deg);
                P2SkySample sm{};
                sm.frame_id = 1000 + static_cast<std::uint64_t>(k);
                sm.control_id = static_cast<std::uint64_t>(i * 41 + j);
                sm.ra_deg = u; sm.dec_deg = v;
                sm.value = b + 0.4 * k + g(rng);
                sm.variance = sigma * sigma;
                sm.snr = 1.0 / sigma;
                sm.flags = P2_SKY_FLAG_NONE;
                s.push_back(sm);
            }
    return s;
}

P2SkyPlaneGeometry coarse_geometry(double overlap_deg, double pitch_deg) {
    P2SkyPlaneGeometry g{};
    g.overlap_band_width_deg = overlap_deg;
    g.pointing_spacing_deg = 2.0 * overlap_deg;   // 重叠带才是 min
    g.sample_pitch_deg = pitch_deg;
    g.pixel_scale_arcsec = 1.0;
    return g;
}

void test_adaptive_triggers_node_refinement() {
    std::printf("[adaptive_triggers_node_refinement]\n");
    // 几何：重叠带 0.40 deg ⇒ 规则上界 h_hi = 0.20 deg；采样间距 0.05 deg ⇒ 下界 0.05 deg。
    const std::vector<P2SkySample> s = fine_structure_samples(0.30, 4, 0.02, 20260924);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.geometry = coarse_geometry(0.40, 0.05);
    cfg.max_nodes = 5000;
    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    P2SkyPlaneAdaptiveReport rep{};
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, std::string("adaptive build ok rc=") + std::to_string(rc) + " " + err);
    if (rc != 0) return;
    std::printf("  INFO attempts=%d refinements=%d coarsenings=%d final_h=%.6f "
                "upper=%.6f lower=%.6f kappa=%.4g chi2_red=%.4g\n",
                rep.n_attempts, rep.n_node_refinements, rep.n_node_coarsenings,
                rep.node_spacing_deg, rep.node_spacing_upper_deg,
                rep.node_spacing_lower_deg, rep.kappa, rep.chi2_red);
    for (int i = 0; i < rep.n_attempts; ++i) {
        const P2SkyPlaneAttempt& a = rep.attempts[i];
        std::printf("    #%d h=%.6f lambda=%.4g rc=%d action=%d adopted=%d kappa=%.4g "
                    "chi2_red=%.4g rank=%llu/%llu rank_solve=%llu identifiable=%d\n",
                    i, a.node_spacing_deg, a.lambda_numerical, a.rc, a.action, a.adopted,
                    a.kappa, a.chi2_red, (unsigned long long)a.rank,
                    (unsigned long long)a.n_params, (unsigned long long)a.rank_solve,
                    a.identifiable);
    }
    check(rep.n_node_refinements >= 1,
          "rule 5: node-spacing adaptive path was ACTUALLY triggered (n_node_refinements>=1)");
    check(rep.node_adaptive_used == 1, "node_adaptive_used flag set");
    check(rep.node_spacing_deg < rep.node_spacing_upper_deg,
          "final node spacing refined below the rule upper bound");
    check(rep.node_spacing_deg >= rep.node_spacing_lower_deg,
          "final node spacing not below the data resolution limit");
    check(rep.n_attempts >= 2, "every attempt recorded (>=2)");
    // 逐次尝试都带判据读数（provenance 要求）：求解成功的尝试必须给出 κ/r_eff/n_params；
    // 求解失败的尝试没有 κ 可报（record() 只在 rc==OK 时取 Info），但必须带 rc 与判决位。
    bool ok_have_kappa = true, bad_have_rc = true, any_red = false;
    for (int i = 0; i < rep.n_attempts; ++i) {
        const P2SkyPlaneAttempt& a = rep.attempts[i];
        if (a.rc == P2_SKY_PLANE_OK) {
            if (!(a.kappa > 0.0) || a.n_params == 0) ok_have_kappa = false;
        } else {
            if (a.rc == 0) bad_have_rc = false;
            if (a.identifiable == 0) any_red = true;
        }
    }
    check(ok_have_kappa, "every SOLVED attempt records kappa + n_params (provenance)");
    check(bad_have_rc, "every FAILED attempt records its rc");
    // 判据在细化探针上真的判过红（细到数据支撑不住 ⇒ 秩亏）——非退化证据
    check(any_red, "the criterion actually went RED on the over-refined probe (non-degenerate)");
    // 细化确实降低了残差（判据非退化）
    double first_ok_chi2 = -1.0;
    for (int i = 0; i < rep.n_attempts; ++i)
        if (rep.attempts[i].rc == P2_SKY_PLANE_OK) { first_ok_chi2 = rep.attempts[i].chi2_red; break; }
    check(first_ok_chi2 > 0.0 && rep.chi2_red < first_ok_chi2,
          "refinement materially reduced chi2_red (representation-limited input)");
    p2_sky_plane_close(m);
}

void test_adaptive_no_trigger_on_benign_input() {
    std::printf("[adaptive_no_trigger_on_benign]\n");
    // 良性输入：B_ref 是低阶多项式，规则上界已经能表示 ⇒ 细化不应被触发。
    const std::vector<P2SkySample> s = smooth_samples(4, 0.02, 555);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.geometry = coarse_geometry(0.40, 0.02);
    cfg.max_nodes = 20000;
    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    P2SkyPlaneAdaptiveReport rep{};
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, std::string("adaptive build ok rc=") + std::to_string(rc) + " " + err);
    if (rc != 0) return;
    std::printf("  INFO attempts=%d refinements=%d final_h=%.6f chi2_red=%.4g\n",
                rep.n_attempts, rep.n_node_refinements, rep.node_spacing_deg, rep.chi2_red);
    check(rep.n_node_refinements == 0,
          "non-degenerate: benign input does NOT trigger refinement (criterion can be green)");
    check(rep.node_spacing_deg == rep.node_spacing_upper_deg,
          "benign input keeps the rule upper bound (best conditioned)");
    check(rep.node_adaptive_used == 0, "node_adaptive_used == 0 on benign input");
    p2_sky_plane_close(m);
}

void test_adaptive_red_side_is_fail_closed() {
    std::printf("[adaptive_red_side_fail_closed]\n");
    // UPM-KAPPA-UNIFY-01：**已退休**「提高 roughness_penalty 救红」分支——λ 不再是
    // 自由参数（λ_eff = τ·mean(diag(H_red))），故自适应回路里不存在这条路径。
    // 判红时的唯一动作是在规则区间内**放粗节点间距**；起点已是规则上界（最粗）
    // ⇒ 无路可走时必须诚实 fail-closed，而不是靠调 λ 把红买成绿。
    const std::vector<P2SkySample> s = smooth_samples(2, 0.02, 90210);
    P2SkyPlaneConfig base = p2_sky_plane_default_config();
    base.geometry = coarse_geometry(0.60, 0.02);   // upper = 0.30
    base.max_nodes = 20000;
    base.frame_gradient_order = 0;
    char e0[512] = {0};
    void* m0 = nullptr;
    const int rc0 = p2_sky_plane_build(s.data(), s.size(), &base, &m0, e0, sizeof(e0));
    check(rc0 == P2_SKY_PLANE_OK, "baseline build ok (green at the frozen tau)");
    if (rc0 != P2_SKY_PLANE_OK || !m0) return;
    P2SkyPlaneInfo i0{};
    p2_sky_plane_info(m0, &i0);
    p2_sky_plane_close(m0);
    check(i0.identifiable == 1, "baseline verdict green at tau = 1e-10");

    P2SkyPlaneConfig cfg = base;
    cfg.rank_rtol = 2.0 / i0.kappa;   // ⇒ 1/τ = κ/2 < κ ⇒ 判红（阈值由实测 κ 反推）
    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    P2SkyPlaneAdaptiveReport rep{};
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep, err,
                                               sizeof(err));
    std::printf("  INFO rc=%d attempts=%d coarsenings=%d final_lambda=%.6g final_kappa=%s\n",
                rc, rep.n_attempts, rep.n_node_coarsenings, rep.lambda_numerical,
                std::isfinite(rep.kappa) ? std::to_string(rep.kappa).c_str() : "+inf");
    for (int i = 0; i < rep.n_attempts; ++i) {
        const P2SkyPlaneAttempt& a = rep.attempts[i];
        std::printf("    #%d h=%.6f lambda=%.4g rc=%d action=%d adopted=%d kappa=%.4g "
                    "identifiable=%d\n",
                    i, a.node_spacing_deg, a.lambda_numerical, a.rc, a.action, a.adopted,
                    a.kappa, a.identifiable);
    }
    check(rep.n_attempts >= 1 && rep.attempts[0].rc == P2_SKY_PLANE_NOT_IDENTIFIABLE,
          "first attempt rejected by the single criterion (red side)");
    check(rep.attempts[0].identifiable == 0, "attempt log records the red verdict");
    check(rep.attempts[0].action == P2_SKY_ADAPT_BUILD_FAILED,
          "at the rule upper bound there is nowhere to coarsen => BUILD_FAILED (honest)");
    check(rc == P2_SKY_PLANE_NOT_IDENTIFIABLE && m == nullptr,
          "fail-closed: no product from a red verdict, and no lambda trick to buy green");
}

void test_adaptive_requires_geometry() {
    std::printf("[adaptive_requires_geometry]\n");
    const std::vector<P2SkySample> s = smooth_samples(3, 0.02, 1);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();   // 无几何
    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    P2SkyPlaneAdaptiveReport rep{};
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep, err, sizeof(err));
    check(rc == P2_SKY_PLANE_GEOMETRY_REQUIRED,
          "adaptive without geometry -> GEOMETRY_REQUIRED (search bounds must come from input)");
    check(m == nullptr, "fail-closed: no model");
    check(rep.n_attempts == 0, "no attempts recorded on argument failure");
}

void test_adaptive_provenance_roundtrip() {
    std::printf("[adaptive_provenance_roundtrip]\n");
    const std::vector<P2SkySample> s = fine_structure_samples(0.30, 3, 0.02, 24680);
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.geometry = coarse_geometry(0.40, 0.05);
    cfg.max_nodes = 5000;
    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    P2SkyPlaneAdaptiveReport rep{};
    char err[1024] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "adaptive build ok");
    if (rc != 0) return;
    P2SkyPlaneAdaptiveReport got{};
    check(p2_sky_plane_adaptive_report(m, &got) == 0, "adaptive report accessor ok");
    check(got.n_attempts == rep.n_attempts && got.node_spacing_deg == rep.node_spacing_deg,
          "model carries the adaptive provenance");
    const std::string path = std::string(SKY_TEST_OUTDIR) + "/sky_plane_node_adapt_roundtrip.json";
    check(p2_sky_plane_save(m, path.c_str()) == P2_SKY_PLANE_OK, "save ok");
    void* m2 = nullptr;
    check(p2_sky_plane_open(path.c_str(), &m2) == P2_SKY_PLANE_OK, "open ok");
    if (m2) {
        P2SkyPlaneAdaptiveReport g2{};
        p2_sky_plane_adaptive_report(m2, &g2);
        check(g2.n_attempts == rep.n_attempts, "roundtrip preserves n_attempts");
        check(g2.node_spacing_deg == rep.node_spacing_deg, "roundtrip preserves final h");
        check(g2.n_node_refinements == rep.n_node_refinements, "roundtrip preserves refinements");
        bool same = true;
        for (int i = 0; i < rep.n_attempts; ++i)
            if (g2.attempts[i].node_spacing_deg != rep.attempts[i].node_spacing_deg ||
                g2.attempts[i].lambda_numerical != rep.attempts[i].lambda_numerical ||
                g2.attempts[i].rc != rep.attempts[i].rc ||
                g2.attempts[i].action != rep.attempts[i].action)
                same = false;
        check(same, "roundtrip preserves per-attempt (h, lambda_numerical, rc, action)");
        P2SkyPlaneInfo i2{};
        p2_sky_plane_info(m2, &i2);
        check(i2.node_spacing_source == 0 && i2.identifiable == 1 &&
                  i2.rank == i2.n_params && i2.n_unidentified == 0,
              "roundtrip preserves the criterion verdict + node-spacing source");
        check(i2.lambda_numerical > 0.0 && i2.rank_rtol_effective > 0.0,
              "roundtrip preserves lambda_numerical + tau");
        p2_sky_plane_close(m2);
    }
    p2_sky_plane_close(m);
}

}  // namespace

int main(int argc, char** argv) {
    // 输出目录由 CMake 指到 run/ 下（gitignore + 会被 round 回收）⇒ 测试自己建，
    // 否则目录被回收后测试会以"save ok"失败伪装成代码故障。
    std::error_code ec;
    std::filesystem::create_directories(SKY_TEST_OUTDIR, ec);
    const std::string t = argc > 1 ? argv[1] : "all";
    if (t == "derive_positive" || t == "all") test_derive_positive();
    if (t == "derive_pixel_scale" || t == "all") test_derive_pixel_scale_invariance();
    if (t == "derive_min_bounds" || t == "all") test_derive_min_and_bounds();
    if (t == "derive_negative" || t == "all") test_derive_negative();
    if (t == "default_config" || t == "all") test_default_config_has_no_calibrated_default();
    if (t == "build_geometry_required" || t == "all") test_build_requires_geometry_or_explicit_h();
    if (t == "build_derives" || t == "all") test_build_derives_from_geometry();
    if (t == "kappa_gate" || t == "all") test_kappa_gate_unified();
    if (t == "adaptive_refine" || t == "all") test_adaptive_triggers_node_refinement();
    if (t == "adaptive_benign" || t == "all") test_adaptive_no_trigger_on_benign_input();
    if (t == "adaptive_red" || t == "all") test_adaptive_red_side_is_fail_closed();
    if (t == "adaptive_geometry" || t == "all") test_adaptive_requires_geometry();
    if (t == "adaptive_roundtrip" || t == "all") test_adaptive_provenance_roundtrip();
    if (g_fail) { std::printf("RESULT: FAIL (%d/%d checks)\n", g_fail, g_total); return 1; }
    std::printf("RESULT: PASS (%d checks)\n", g_total);
    return 0;
}
