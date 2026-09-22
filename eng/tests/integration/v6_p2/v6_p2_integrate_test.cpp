/* v6_p2_integrate_test.cpp — P2-INTEGRATE-001 端到端集成测试
 *
 * 组（ctest 用例）:
 *   singlepath  单一权重口径门（FZ-WEIGHT-SINGLE-PATH：无任何可接受 token）
 *   write       真实 Phase1 产物 -> Phase2 集成 -> 写盘 -> 重开校验
 *   negative    逐条违反冻结 -> 必红（含未篡改正控制）
 *   halfproduct 发布/重开验证中途失败 -> 无可见半成品 / 无 staging 残留
 *
 * 运行: ./v6_p2_integrate_test <singlepath|write|negative|halfproduct> <work_root>
 */
#include "astrocs/v6/phase2_integrate.h"
#include "astrocs/v6/phase1_product.h"
#include "v6_runtime_contract.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace astrocs::v6;
using namespace astrocs::v6::p2int;

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, msg)                                                    \
  do {                                                                      \
    ++g_checks;                                                             \
    if (!(cond)) {                                                          \
      ++g_fails;                                                            \
      std::fprintf(stderr, "  [FAIL] %s:%d %s\n", __FILE__, __LINE__, msg); \
      return false;                                                         \
    }                                                                       \
  } while (0)

#define CHECK_NEAR(a, b, tol, msg)                                          \
  do {                                                                      \
    ++g_checks;                                                             \
    const double _a = (a), _b = (b);                                        \
    if (!(std::fabs(_a - _b) <= (tol))) {                                   \
      ++g_fails;                                                            \
      std::fprintf(stderr, "  [FAIL] %s:%d %s (%.17g vs %.17g)\n",          \
                   __FILE__, __LINE__, msg, _a, _b);                        \
      return false;                                                         \
    }                                                                       \
  } while (0)

/* ── TAN 像素->天球回调（测试几何 fixture） ── */
struct TanWcs { double ra0_deg=0.0, dec0_deg=0.0, scale_rad=1e-3, crpix1=0.0, crpix2=0.0; };
static bool tan_pix2sky(double px, double py, double& ra, double& dec, void* ud) {
  const TanWcs* w = static_cast<const TanWcs*>(ud);
  const double D2R = 3.14159265358979323846 / 180.0;
  const double xi = (px - w->crpix1) * w->scale_rad;
  const double eta = (py - w->crpix2) * w->scale_rad;
  const double r = std::sqrt(xi * xi + eta * eta);
  const double ra0 = w->ra0_deg * D2R, dec0 = w->dec0_deg * D2R;
  if (r < 1e-15) { ra = w->ra0_deg; dec = w->dec0_deg; return true; }
  if (!(r < 1.2)) return false;
  const double c = std::atan(r), sc = std::sin(c), cc = std::cos(c);
  const double dec_r = std::asin(cc * std::sin(dec0) + eta * sc * std::cos(dec0) / r);
  const double ra_r = ra0 + std::atan2(xi * sc, r * std::cos(dec0) * cc - eta * std::sin(dec0) * sc);
  ra = ra_r / D2R; dec = dec_r / D2R;
  return true;
}
/* nside=4（~14.7° pixel）+ 6° 像素尺度 + 8x8 输入 -> 多 control/overlap。 */
static TanWcs g_wcs{0.0, 0.0, 6.0 * 3.14159265358979323846 / 180.0, 4.0, 4.0};

static phase1::Phase1FrameInputs make_inputs(const std::string& frame_id,
                                             double signal_scale,
                                             const std::vector<double>& fhat,
                                             const double* raw9) {
  phase1::Phase1FrameInputs in;
  in.frame_id = frame_id;
  in.run_id = "run-p2-int-001";
  in.software_sha = std::string(40, 'a');
  in.config_hash = "sha256:phase2-config-v1";
  in.calibration.input_frame_sha256 = "sha256:input-frame-" + frame_id;

  using namespace astrocs::calibration::v6;
  DetectorMetadata md;
  md.has_gain = true; md.gain = 2.0;
  md.has_read_noise = true; md.read_noise_e = 5.0;
  md.quantum_declared = true; md.q_adu = 1.0;
  MasterIdentity bias, dark, flat;
  bias.master_id = "bias-master-1"; bias.normalization_version = "v1"; bias.units = "ADU";
  bias.combine_rule = "median"; bias.n_combined = 20; bias.v_single_frame = 4.0;
  bias.common_mode_declared = true; bias.common_mode_variance = 0.5;
  dark.master_id = "dark-master-1"; dark.normalization_version = "v1"; dark.units = "ADU";
  dark.combine_rule = "median"; dark.n_combined = 15; dark.v_single_frame = 9.0;
  dark.common_mode_declared = true; dark.common_mode_variance = 0.25;
  flat.master_id = "flat-master-1"; flat.normalization_version = "v1"; flat.units = "1";
  flat.combine_rule = "mean"; flat.n_combined = 30; flat.v_single_frame = 1e-4;
  flat.common_mode_declared = false;
  CalConfig cfg;
  cfg.dark_opt = DarkOption::kExplicitBiasDark;
  cfg.alpha = 1.0; cfg.detector = md;
  cfg.bias_light_master = bias; cfg.bias_dark_master = bias;
  cfg.dark_master = dark; cfg.flat_master = flat;
  cfg.shared.kind = SharedRepresentationKind::kCommonMasterId;
  cfg.shared.common_master.master_id = "bias-master-1";
  cfg.shared.common_master.alpha_m = 0.5;
  cfg.shared.common_master.master_variance = 1.0;
  in.calibration.config = cfg;
  CalPixelInput px;
  px.r = 1000.0; px.has_dark = true; px.dark = 100.0;
  px.has_flat = true; px.flat = 1.0;
  px.has_bias_light = true; px.bias_light = 50.0;
  px.has_bias_dark = true; px.bias_dark = 50.0;
  in.calibration.pixel = px;

  double s = 0.0; for (int i = 0; i < 9; ++i) s += raw9[i];
  for (int i = 0; i < 9; ++i) in.point_source.psf_profile.push_back(raw9[i] / s);
  for (int i = 0; i < 9; ++i) {
    in.point_source.sigma2.push_back(9.0);
    in.point_source.data.push_back(signal_scale * in.point_source.psf_profile[i]);
  }
  in.point_source.a = 1.0;
  in.point_source.star_id = "star-" + frame_id;

  in.psfsw.fhat = fhat;
  in.psfsw.background_robust_mean = 10.0;
  in.psfsw.a_nea = 0.0; in.psfsw.a_ref = 0.0;
  for (double f : fhat) {
    in.psfsw.signal_samples.push_back(f * 5.0);
    in.psfsw.concentration_samples.push_back(f / 3.0);
    in.psfsw.noise_samples.push_back(f * 0.05);
    in.psfsw.background_samples.push_back(10.0);
  }
  in.psfsw.common_star_set_id = "css-" + frame_id;
  in.psfsw.selection_function_id = "sf-ext-catalog-v1";
  in.psfsw.members_hash = "sha256:members-" + frame_id;
  in.psfsw.construction = "external_catalog";
  in.psfsw.independence_proof = "external_reference_catalog";
  in.psfsw.exclusion_flags = "saturated,edge_truncated";

  in.drizzle.nside = 4; in.drizzle.nx = 8; in.drizzle.ny = 8;
  in.drizzle.pixfrac = 0.8; in.drizzle.closure_rel_tol = 1e-6;
  in.drizzle.pixel_to_sky = &tan_pix2sky; in.drizzle.user_data = &g_wcs;
  for (int j = 0; j < 64; ++j) {
    in.drizzle.pixel_values.push_back(100.0 + signal_scale * 0.01 + j);
    in.drizzle.pixel_variance.push_back(4.0 + 0.1 * j);
  }
  return in;
}

static void rm_rf(const fs::path& p) { std::error_code ec; fs::remove_all(p, ec); }

static json load_json(const fs::path& p) {
  std::ifstream in(p); json j; in >> j; return j;
}
static void save_json(const fs::path& p, const json& j) {
  std::ofstream os(p, std::ios::binary | std::ios::trunc); os << j.dump(2);
}
static bool flip_last_byte(const fs::path& p) {
  std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
  if (!f) return false;
  char c = 0;
  f.seekg(-1, std::ios::end); f.read(&c, 1);
  c = static_cast<char>(c ^ 0x01);
  f.seekp(-1, std::ios::end); f.write(&c, 1);
  return static_cast<bool>(f);
}

static RunMeta make_meta(const std::string& suffix) {
  RunMeta m;
  m.run_id = "p2-int-" + suffix;
  m.software_sha = std::string(40, 'b');
  m.config_hash = "sha256:phase2-config-v1";
  m.generated_utc = "2026-09-16T00:00:00Z";
  return m;
}

/* 写 3 个真实 Phase1 产品（同一几何 -> 有 overlap）。 */
static std::vector<std::string> write_fixture(const fs::path& work) {
  const fs::path ph1 = work / "ph1";
  rm_rf(ph1);
  fs::create_directories(ph1);
  const double scales[3] = {1000.0, 900.0, 800.0};
  const std::vector<std::vector<double>> fhat = {{100,105,95,102,98},{90,94,86,92,88},{80,84,76,82,78}};
  /* 逐帧 PSF 形状不同 -> FWHM(P_eff) != median(FWHM_k) 可判。 */
  const double raw_narrow[9] = {0.02, 0.20, 0.02, 0.20, 0.24, 0.20, 0.02, 0.20, 0.02};
  const double raw_broad[9]  = {0.04, 0.18, 0.04, 0.18, 0.28, 0.18, 0.04, 0.18, 0.04};
  const double* raws[3] = {raw_narrow, raw_broad, raw_broad};
  const char* names[3] = {"frame_a", "frame_b", "frame_c"};
  std::vector<std::string> dirs;
  for (int i = 0; i < 3; ++i) {
    const fs::path p = ph1 / (std::string(names[i]) + ".p1");
    const auto in = make_inputs(names[i], scales[i], fhat[i], raws[i]);
    const phase1::Phase1WriteResult w = phase1::write_phase1_product(in, p.string());
    if (!w.ok) std::fprintf(stderr, "fixture write failed: %s\n", w.error.c_str());
    dirs.push_back(p.string());
  }
  return dirs;
}

/* ── singlepath（单一权重口径：无任何可选择 token） ── */
static bool run_singlepath() {
  /* FZ-WEIGHT-SINGLE-PATH：权重只有一个口径（阶段1 稀疏 SNR 控制点 → 阶段2 重建
   * 稠密 SNR 面 → 逆方差定权 → 叠加），没有可选择项 ⇒ phase2 的 --mode token 面
   * 必须对**所有** token fail-closed（rc=2 = ARGS）。
   * 能红能绿：让任何一个 token 走 kProduction 分支，本块立即转红。 */
  const astrocs::v6runtime::ModeRoute retired =
      astrocs::v6runtime::route_phase2_weight_token("psfsw_robust");
  CHECK(retired.kind == astrocs::v6runtime::RouteKind::kReject,
        "psfsw_robust RETIRED rejected");
  CHECK(retired.reason.find("FZ-MODE-RETIRED") != std::string::npos,
        "retired reject cites FZ-MODE-RETIRED");
  CHECK(retired.reason.find("migration") != std::string::npos,
        "retired reject carries a migration hint");
  for (const char* tok : {"point_information", "surface_gls",
                          "psf_snr_power", "auto", "support_x_snr2", "0", "1", "2",
                          "bogus", ""}) {
    const astrocs::v6runtime::ModeRoute r =
        astrocs::v6runtime::route_phase2_weight_token(tok);
    CHECK(r.kind == astrocs::v6runtime::RouteKind::kReject,
          "no phase2 weight-mode token may be accepted (FZ-WEIGHT-SINGLE-PATH)");
    CHECK(r.rc == 2, "rejected token maps to CLI ARGS rc=2 (fail-closed)");
  }
  /* documented baseline（非科学方差面）：可识别但不作生产口径。 */
  for (const char* tok : {"equal", "pixel_ivar"}) {
    const astrocs::v6runtime::ModeRoute r =
        astrocs::v6runtime::route_phase2_weight_token(tok);
    CHECK(r.kind == astrocs::v6runtime::RouteKind::kBaseline,
          "baseline token is documented non-production (FZ-WEIGHT-SINGLE-PATH)");
    CHECK(r.reason.find("single scientific weight path") != std::string::npos,
          "baseline token reason states the single weight path");
  }
  std::printf("  SINGLEPATH PASS checks=%d\n", g_checks);
  return true;
}

/* ── write（单一权重口径端到端） ── */
static bool run_write(const fs::path& work) {
  const std::vector<std::string> dirs = write_fixture(work);
  const fs::path out = work / "out";
  rm_rf(out);
  fs::create_directories(out);

  json report;

  /* --- point_information（独立帧） --- */
  {
    const ProductResult r = run_point_information(dirs, JointCovariance(), make_meta("point"), (out / "point").string());
    CHECK(r.ok, r.error.c_str());
    const Phase2OpenResult o = open_phase2_product((out / "point").string());
    CHECK(o.ok, o.error.c_str());
    CHECK(o.has_flux, "point product has FLUX HDU");
    CHECK(o.has_effective_psf, "point product has EFFECTIVE_PSF HDU");
    CHECK(o.output_sha256 == r.output_sha256, "reopen sha256 == publish sha256");
    CHECK(r.effective_psf.size() == 9, "effective PSF profile present");
    double maxp = 0.0; for (double v : r.effective_psf) maxp = std::max(maxp, v);
    CHECK_NEAR(maxp, 1.0, 1e-9, "peak-normalized effective PSF");
    double asum = 0.0; for (double a : r.combination_coefficients) asum += a;
    CHECK_NEAR(asum, 1.0, 1e-9, "actual combination coefficients sum to 1");
    CHECK_NEAR(r.flux_variance * r.w_info, 1.0, 1e-9, "Var(F_hat)=1/W (FZ-FORMULA-WINFO)");
    CHECK_NEAR(r.flux, r.q / r.w_info, 1e-12, "F_hat = Q/W");
    const json rec = load_json(out / "point" / "phase2_product.json");
    CHECK(rec["phase2_extensions"]["group_normalization_performed"] == false, "point group normalization flag");
    CHECK(rec["weight_mode_record"]["weight"]["kind"] == "W_info", "point weight.kind");
    CHECK(rec["weight_mode"] == "point_information",
          "weight identity constant is canonical (single weight path)");
    CHECK(rec["weight_mode_record"]["weight_mode"] == "point_information",
          "weight record identity constant is canonical (single weight path)");
    CHECK(rec["covariance"]["variance_from"] == "actual_combination_coefficients", "point variance_from");
    report["point"] = {{"q", r.q}, {"w_info", r.w_info}, {"flux", r.flux},
                       {"var_f", r.flux_variance}, {"alpha", r.combination_coefficients},
                       {"peff", r.effective_psf}, {"fwhm", r.effective_psf_fwhm},
                       {"out", (out / "point").string()}};
  }

  /* --- point_information（相关帧 -> 联合 C 必用） --- */
  {
    std::vector<std::string> two = {dirs[0], dirs[1]};
    const FrameSet fs = open_phase2_frame_set(two);
    CHECK(fs.ok, fs.error.c_str());
    const std::size_t K = fs.frames.size();
    const std::size_t m = fs.frames[0].psf_profile.size();
    const std::size_t n = K * m;
    JointCovariance j;
    j.provided = true; j.m = m; j.kernel_id = "phase2_test_shared_systematic_v1";
    /* C = blkdiag(9I_m) + 跨帧同像素相关 9*0.3*I_m -> 估计器有效相关 rho=0.3。 */
    j.c_in.assign(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) j.c_in[i * n + i] = 9.0;
    for (std::size_t k = 0; k < K; ++k)
      for (std::size_t l = 0; l < K; ++l) {
        if (k == l) continue;
        for (std::size_t i = 0; i < m; ++i)
          j.c_in[(k * m + i) * n + (l * m + i)] = 9.0 * 0.3;
      }
    for (std::size_t k = 0; k < K; ++k)
      for (std::size_t i = 0; i < m; ++i) {
        j.a_psf.push_back(fs.frames[k].a * fs.frames[k].psf_profile[i]);
        j.data.push_back(100.0 + static_cast<double>(k * m + i));
      }
    const ProductResult r = run_point_information(two, j, make_meta("point-joint"), (out / "point_joint").string());
    CHECK(r.ok, r.error.c_str());
    CHECK(r.joint_covariance_used, "joint C used for correlated frames");
    CHECK(r.corr_ratio >= kCorrRatioMin, "correlated detection ratio >= 1.05");
    const Phase2OpenResult o = open_phase2_product((out / "point_joint").string());
    CHECK(o.ok, o.error.c_str());
    /* 相关帧朴素 Sum 必须 REJECT（负向，见 negative 组；此处正控制） */
    report["point_joint"] = {{"corr_ratio", r.corr_ratio}, {"naive_var", r.naive_variance},
                             {"joint_var", r.joint_variance}, {"alpha", r.combination_coefficients},
                             {"c_in", j.c_in}, {"a_psf", j.a_psf}, {"data", j.data},
                             {"q", r.q}, {"w_info", r.w_info}, {"flux", r.flux},
                             {"var_f", r.flux_variance}, {"peff", r.effective_psf},
                             {"m", j.m}, {"out", (out / "point_joint").string()}};
  }

  /* --- UPM/REJ/SAMP 接线 --- */
  {
    const FrameSet fs = open_phase2_frame_set(dirs);
    CHECK(fs.ok, fs.error.c_str());
    const UpmRejSampResult u = run_upm_rej_samp_wiring(fs, make_meta("wiring"));
    CHECK(u.ok, u.error.c_str());
    CHECK(u.n_components >= 1, "overlap graph component present");
    CHECK(u.rank == u.n_free, "rank == n_free (FZ-AP2S-RANK-RTOL)");
    CHECK(u.kappa <= 1e6, "kappa <= FZ-AP2S-KAPPA-MAX");
    CHECK(u.sigma_eff2 > u.control_variance, "sigma_eff^2 includes J C_theta J^T (ALG-P2S-REJ.3)");
    CHECK(u.spatial_p05 <= u.spatial_p50 && u.spatial_p50 <= u.spatial_p95, "spatial p05<=p50<=p95");
    CHECK(u.scalar_verdict == 0, "scalar degrade gate ALLOWED (double gate)");
    CHECK(u.coverage_rc == 0, "coverage/support classify rc=0");
    /* 单一口径在位：逆方差权重链是唯一权重来源（无模式门可过/可不过）。 */
    CHECK(u.coverage_rc == 0, "coverage rc=0 alongside the single weight path");
    CHECK(u.reject_status == 0, "reject classify rc=0");
    report["wiring"] = {{"n_components", u.n_components}, {"rank", u.rank}, {"n_free", u.n_free},
                        {"kappa", u.kappa}, {"sigma_eff2", u.sigma_eff2},
                        {"p05", u.spatial_p05}, {"p50", u.spatial_p50}, {"p95", u.spatial_p95},
                        {"coverage", u.spatial_coverage}};
  }

  /* --- 退役对象声明必须被显式拒绝（FZ-MODE-RETIRED 负例证据，供独立 Oracle 复算） --- */
  {
    const fs::path ret = out / "retired";
    rm_rf(ret);
    fs::create_directories(ret);
    json rec = load_json(out / "point" / "phase2_product.json");
    rec["weight_mode"] = "psfsw_robust";
    save_json(ret / "phase2_product.json", rec);
    const Phase2OpenResult o = open_phase2_product(ret.string());
    CHECK(!o.ok, "retired psfsw_robust declaration rejected (no product accepted)");
    CHECK(o.error.find("FZ-MODE-RETIRED") != std::string::npos,
          "retired reject cites FZ-MODE-RETIRED");
    CHECK(o.error.find("psfsw_robust_weight") != std::string::npos,
          "retired reject names the retired object");
    CHECK(o.error.find("migration") != std::string::npos,
          "retired reject carries a migration hint");
    CHECK(o.error.find("SNR^2") != std::string::npos,
          "retired reject states the single weight path (w = SNR^2/F_ref^2)");
    report["psfsw_retired"] = {{"rejected", true}, {"error", o.error}};
  }

  save_json(work / "v6_p2_results.json", report);
  std::printf("  WRITE PASS checks=%d\n", g_checks);
  return true;
}

/* ── negative ── */
static bool run_negative(const fs::path& work) {
  const std::vector<std::string> dirs = write_fixture(work);
  const fs::path neg = work / "neg";
  rm_rf(neg);
  fs::create_directories(neg);

  /* 正控制：未篡改产品必须重开成功。 */
  {
    const fs::path base = neg / "base";
    const ProductResult r = run_point_information(dirs, JointCovariance(), make_meta("negbase"), base.string());
    CHECK(r.ok, r.error.c_str());
    const Phase2OpenResult o = open_phase2_product(base.string());
    ++g_checks;
    if (!o.ok) { ++g_fails; std::fprintf(stderr, "  [FAIL] positive control reopen: %s\n", o.error.c_str()); return false; }
  }

  const fs::path base = neg / "base";
  int detected = 0, total = 0;

  auto mutate_and_expect_red = [&](const std::string& name,
                                   const std::function<void(json&)>& f) {
    ++total;
    const fs::path d = neg / name;
    rm_rf(d);
    fs::copy(base, d, fs::copy_options::recursive);
    json rec = load_json(d / "phase2_product.json");
    f(rec);
    save_json(d / "phase2_product.json", rec);
    const Phase2OpenResult o = open_phase2_product(d.string());
    ++g_checks;
    if (o.ok) { ++g_fails; std::fprintf(stderr, "  [FAIL] negative NOT detected: %s\n", name.c_str()); }
    else ++detected;
  };

  /* FZ-WEIGHT-SINGLE-PATH：身份常量不是可选项 ⇒ 改成任何其它取值都必红
   * （含曾被当作生产口径的 surface_gls 与退役 token）。 */
  mutate_and_expect_red("weight_mode_identity_not_canonical", [](json& j) { j["weight_mode"] = "surface_gls"; });
  mutate_and_expect_red("weight_mode_identity_retired", [](json& j) { j["weight_mode"] = "psfsw_robust"; });
  mutate_and_expect_red("weight_mode_record_identity_not_canonical", [](json& j) { j["weight_mode_record"]["weight_mode"] = "surface_gls"; });
  mutate_and_expect_red("weight_kind_ivar", [](json& j) { j["weight_mode_record"]["weight"]["kind"] = "ivar"; });
  mutate_and_expect_red("variance_from_weight", [](json& j) { j["covariance"]["variance_from"] = "psfsw_robust_weight"; });
  mutate_and_expect_red("psfsw_boundary_variance_from_weight", [](json& j) {
    j["covariance"]["psfsw_boundary"] = {{"method", "variance_from_weight"}, {"variance_from_weight", true}, {"uses_relative_weight_as_ivar", true}}; });
  mutate_and_expect_red("effective_psf_only_fwhm", [](json& j) {
    j["effective_psf"].erase("values_or_model");
    j["effective_psf"]["values_or_model"] = {{"kind", "values"}}; });
  mutate_and_expect_red("provenance_key_removed", [](json& j) { j["provenance"].erase("flux_conservation_factor"); });
  mutate_and_expect_red("p33_coefficient_reintroduced", [](json& j) { j["phase2_extensions"]["snr_frame_coefficient"] = 3.14; });
  mutate_and_expect_red("support_as_weight_source", [](json& j) { j["weight_mode_record"]["weight"]["sources"].push_back("support"); });
  mutate_and_expect_red("third_vocabulary_token", [](json& j) { j["weight_mode_record"]["normalization_scope"] = "group"; });
  mutate_and_expect_red("bunit_undecidable", [](json& j) {
    j["provenance"]["units"]["bunit"] = "ADU";
    j["provenance"]["units"]["pixel_area_power"] = 0;
    j["provenance"]["pixel_semantics"] = "integrated_flux"; });
  mutate_and_expect_red("output_hash_changed", [](json& j) { j["manifest"]["output_hash"] = std::string(64, 'c'); });
  mutate_and_expect_red("winfo_var_inverse_broken", [](json& j) { j["point_information"]["flux_variance"]["value"] = j["point_information"]["W_info"]["value"].get<double>(); });

  /* 相关帧朴素 Sum -> REJECT（不写盘）。 */
  {
    ++total;
    const FrameSet fs = open_phase2_frame_set({dirs[0], dirs[1]});
    CHECK(fs.ok, fs.error.c_str());
    const std::size_t K = fs.frames.size(), m = fs.frames[0].psf_profile.size(), n = K * m;
    JointCovariance j;
    j.provided = true; j.m = m; j.kernel_id = "k";
    j.c_in.assign(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) j.c_in[i * n + i] = 9.0;
    for (std::size_t i = 0; i < n; ++i) for (std::size_t k = 0; k < n; ++k) j.c_in[i * n + k] += 1.8;
    for (std::size_t k = 0; k < K; ++k)
      for (std::size_t i = 0; i < m; ++i) { j.a_psf.push_back(fs.frames[k].a * fs.frames[k].psf_profile[i]); j.data.push_back(100.0); }
    RunMeta m2 = make_meta("naive");
    m2.force_naive_sum = true;
    const ProductResult r = run_point_information({dirs[0], dirs[1]}, j, m2, (neg / "naive").string());
    ++g_checks;
    if (r.ok) { ++g_fails; std::fprintf(stderr, "  [FAIL] correlated naive Sum NOT rejected\n"); }
    else ++detected;
  }

  /* FITS bit flip -> 重开必红。 */
  {
    ++total;
    const fs::path d = neg / "fits_bitflip";
    rm_rf(d); fs::copy(base, d, fs::copy_options::recursive);
    if (!flip_last_byte(d / "mosaic.fits")) { ++g_fails; std::fprintf(stderr, "  [FAIL] flip failed\n"); }
    const Phase2OpenResult o = open_phase2_product(d.string());
    ++g_checks;
    if (o.ok) { ++g_fails; std::fprintf(stderr, "  [FAIL] FITS bit flip NOT detected\n"); }
    else ++detected;
  }
  /* 单帧不足 -> fail-closed。 */
  {
    ++total;
    const ProductResult r = run_point_information({dirs[0]}, JointCovariance(), make_meta("one"), (neg / "one").string());
    ++g_checks;
    if (r.ok) { ++g_fails; std::fprintf(stderr, "  [FAIL] single-frame group NOT rejected\n"); }
    else ++detected;
  }

  std::printf("  NEGATIVE PASS detected=%d/%d checks=%d\n", detected, total, g_checks);
  return true;
}

/* ── halfproduct ── */
static bool run_halfproduct(const fs::path& work) {
  const std::vector<std::string> dirs = write_fixture(work);
  const fs::path dir = work / "half";
  rm_rf(dir);
  fs::create_directories(dir);

  /* 重开验证注入失败 -> 撤销发布，无可见产物/无 staging 残留。 */
  {
    RunMeta m = make_meta("half");
    m.force_publish_verify_fail = true;
    const ProductResult r = run_point_information(dirs, JointCovariance(), m, (dir / "half_verify").string());
    CHECK(!r.ok, "publish must fail on injected reopen verify failure");
    CHECK(!fs::exists(dir / "half_verify"), "no visible half-product (verify failure)");
  }
  /* BUNIT 不可判 -> 重开验证必红 -> 撤销发布。 */
  {
    RunMeta m = make_meta("halfbunit");
    m.force_bunit_undecidable = true;
    const ProductResult r = run_point_information(dirs, JointCovariance(), m, (dir / "half_bunit").string());
    CHECK(!r.ok, "publish must fail on undecidable BUNIT");
    CHECK(!fs::exists(dir / "half_bunit"), "no visible half-product (BUNIT)");
  }
  int residue = 0;
  for (const auto& e : fs::directory_iterator(dir)) {
    const std::string n = e.path().filename().string();
    if (n.find(".staging.tmp-") != std::string::npos) ++residue;
  }
  CHECK(residue == 0, "no staging tmp residue after failed publish");
  std::printf("  HALFPRODUCT PASS checks=%d\n", g_checks);
  return true;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <singlepath|write|negative|halfproduct> <work>\n", argv[0]);
    return 2;
  }
  const std::string grp = argv[1];
  const fs::path work = argv[2];
  fs::create_directories(work);

  bool ok = false;
  if (grp == "singlepath") ok = run_singlepath();
  else if (grp == "write") ok = run_write(work);
  else if (grp == "negative") ok = run_negative(work);
  else if (grp == "halfproduct") ok = run_halfproduct(work);
  else { std::fprintf(stderr, "unknown group: %s\n", grp.c_str()); return 2; }

  if (!ok || g_fails != 0) {
    std::fprintf(stderr, "RESULT FAIL group=%s checks=%d fails=%d\n", grp.c_str(), g_checks, g_fails);
    return 1;
  }
  std::printf("RESULT PASS group=%s checks=%d\n", grp.c_str(), g_checks);
  return 0;
}
