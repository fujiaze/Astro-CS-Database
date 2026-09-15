/* v6_p1_integrate_test.cpp — P1-INTEGRATE-001 端到端集成测试
 *
 * 组（ctest 用例）:
 *   positive  <work>  写盘 -> 重开 -> 校验 BUNIT/单位/provenance/权重面
 *   group     <work>  仅由磁盘重开的两个 Phase1 产品组成帧组（Phase2 消费面）
 *   negative  <work>  逐条违反冻结 -> 重开必红（含未篡改正控制）
 *   halfproduct <work> 发布中途失败 -> 无可见半成品 / 无 staging 残留
 *
 * 运行: ./v6_p1_integrate_test <group> <work_root>
 */
#include "astrocs/v6/phase1_product.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace astrocs::v6::phase1;

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

/* ── TAN 像素->天球回调（测试几何 fixture；本层实现不持有 WCS） ── */
struct TanWcs {
  double ra0_deg = 0.0, dec0_deg = 0.0, scale_rad = 1e-3, crpix1 = 0.0,
         crpix2 = 0.0;
};

static bool tan_pix2sky(double px, double py, double& ra, double& dec,
                        void* ud) {
  const TanWcs* w = static_cast<const TanWcs*>(ud);
  const double D2R = 3.14159265358979323846 / 180.0;
  const double xi = (px - w->crpix1) * w->scale_rad;
  const double eta = (py - w->crpix2) * w->scale_rad;
  const double r = std::sqrt(xi * xi + eta * eta);
  const double ra0 = w->ra0_deg * D2R, dec0 = w->dec0_deg * D2R;
  if (r < 1e-15) {
    ra = w->ra0_deg;
    dec = w->dec0_deg;
    return true;
  }
  if (!(r < 1.2)) return false; /* 背面/超适用域显式拒绝 */
  const double c = std::atan(r), sc = std::sin(c), cc = std::cos(c);
  const double dec_r = std::asin(cc * std::sin(dec0) + eta * sc * std::cos(dec0) / r);
  const double ra_r =
      ra0 + std::atan2(xi * sc, r * std::cos(dec0) * cc - eta * std::sin(dec0) * sc);
  ra = ra_r / D2R;
  dec = dec_r / D2R;
  return true;
}

static TanWcs g_wcs{0.0, 0.0, 8.0 * 3.14159265358979323846 / 180.0, 2.0, 2.0};

static Phase1FrameInputs make_inputs(const std::string& frame_id,
                                     double signal_scale,
                                     const std::vector<double>& fhat) {
  Phase1FrameInputs in;
  in.frame_id = frame_id;
  in.run_id = "run-p1-int-001";
  in.software_sha = std::string(40, 'a');
  in.config_hash = "sha256:phase1-config-v1";
  in.calibration.input_frame_sha256 = "sha256:input-frame-" + frame_id;

  using namespace astrocs::calibration::v6;
  DetectorMetadata md;
  md.has_gain = true;
  md.gain = 2.0;
  md.has_read_noise = true;
  md.read_noise_e = 5.0;
  md.quantum_declared = true;
  md.q_adu = 1.0;
  MasterIdentity bias, dark, flat;
  bias.master_id = "bias-master-1";
  bias.normalization_version = "v1";
  bias.units = "ADU";
  bias.combine_rule = "median";
  bias.n_combined = 20;
  bias.v_single_frame = 4.0;
  bias.common_mode_declared = true;
  bias.common_mode_variance = 0.5;
  dark.master_id = "dark-master-1";
  dark.normalization_version = "v1";
  dark.units = "ADU";
  dark.combine_rule = "median";
  dark.n_combined = 15;
  dark.v_single_frame = 9.0;
  dark.common_mode_declared = true;
  dark.common_mode_variance = 0.25;
  flat.master_id = "flat-master-1";
  flat.normalization_version = "v1";
  flat.units = "1";
  flat.combine_rule = "mean";
  flat.n_combined = 30;
  flat.v_single_frame = 1e-4;
  flat.common_mode_declared = false;
  CalConfig cfg;
  cfg.dark_opt = DarkOption::kExplicitBiasDark;
  cfg.alpha = 1.0;
  cfg.detector = md;
  cfg.bias_light_master = bias;
  cfg.bias_dark_master = bias; /* 共享 master -> 系数折叠 (OI-02 精确形式) */
  cfg.dark_master = dark;
  cfg.flat_master = flat;
  /* 共享 master 系统项（FZ-PROV-SHARED-SYSTEMATIC 通道 c）：bias 帧组共享。 */
  cfg.shared.kind = SharedRepresentationKind::kCommonMasterId;
  cfg.shared.common_master.master_id = "bias-master-1";
  cfg.shared.common_master.alpha_m = 0.5;
  cfg.shared.common_master.master_variance = 1.0;
  in.calibration.config = cfg;
  CalPixelInput px;
  px.r = 1000.0;
  px.has_dark = true;
  px.dark = 100.0;
  px.has_flat = true;
  px.flat = 1.0;
  px.has_bias_light = true;
  px.bias_light = 50.0;
  px.has_bias_dark = true;
  px.bias_dark = 50.0;
  in.calibration.pixel = px;

  /* 3x3 归一 PSF（Sum=1）。 */
  const double raw[9] = {0.02, 0.05, 0.02, 0.05, 0.72, 0.05, 0.02, 0.05, 0.02};
  double s = 0.0;
  for (double v : raw) s += v;
  for (double v : raw) in.point_source.psf_profile.push_back(v / s);
  for (int i = 0; i < 9; ++i) {
    in.point_source.sigma2.push_back(9.0);
    in.point_source.data.push_back(signal_scale * in.point_source.psf_profile[i]);
  }
  in.point_source.a = 1.0;
  in.point_source.star_id = "star-" + frame_id;

  in.psfsw.fhat = fhat;
  in.psfsw.background_robust_mean = 10.0;
  in.psfsw.a_nea = 0.0; /* 由 PSF 复算 */
  in.psfsw.a_ref = 0.0;
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

  in.drizzle.nside = 2;
  in.drizzle.nx = 4;
  in.drizzle.ny = 4;
  in.drizzle.pixfrac = 0.8;
  in.drizzle.closure_rel_tol = 1e-6;
  in.drizzle.pixel_to_sky = &tan_pix2sky;
  in.drizzle.user_data = &g_wcs;
  for (int j = 0; j < 16; ++j) {
    in.drizzle.pixel_values.push_back(100.0 + j);
    in.drizzle.pixel_variance.push_back(4.0 + 0.1 * j);
  }
  return in;
}

static void rm_rf(const fs::path& p) {
  std::error_code ec;
  fs::remove_all(p, ec);
}

static json load_json(const fs::path& p) {
  std::ifstream in(p);
  json j;
  in >> j;
  return j;
}

static void save_json(const fs::path& p, const json& j) {
  std::ofstream os(p, std::ios::binary | std::ios::trunc);
  os << j.dump(2);
}

static bool flip_last_byte(const fs::path& p) {
  std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
  if (!f) return false;
  f.seekp(-1, std::ios::end);
  char c = 0;
  f.seekg(-1, std::ios::end);
  f.read(&c, 1);
  c = static_cast<char>(c ^ 0x01);
  f.seekp(-1, std::ios::end);
  f.write(&c, 1);
  return static_cast<bool>(f);
}

/* ── positive ── */
static bool run_positive(const fs::path& work) {
  const fs::path dir = work / "positive";
  rm_rf(dir);
  fs::create_directories(dir);

  const Phase1FrameInputs in_a = make_inputs("frame-a", 1000.0, {100, 105, 95, 102, 98});
  const Phase1FrameInputs in_b = make_inputs("frame-b", 800.0, {50, 52, 49, 51, 48});

  const fs::path pa = dir / "frame_a.p1";
  const fs::path pb = dir / "frame_b.p1";
  {
    const Phase1WriteResult w = write_phase1_product(in_a, pa.string());
    CHECK(w.ok, w.error.c_str());
    CHECK(fs::exists(pa / "science.fits"), "science.fits written");
    CHECK(fs::exists(pa / "phase1_product.json"), "record written");
  }
  {
    const Phase1WriteResult w = write_phase1_product(in_b, pb.string());
    CHECK(w.ok, w.error.c_str());
  }

  /* 重开 + 校验 BUNIT / 单位 / provenance / 权重面语义。 */
  for (const auto& p : {pa, pb}) {
    const Phase1OpenResult r = open_phase1_product(p.string());
    CHECK(r.ok, r.error.c_str());
    const Phase1ProductView& v = r.view;
    CHECK(v.signal_bunit == "ADU/px^2", "signal BUNIT frozen");
    CHECK(v.variance_bunit == "ADU^2/px^4", "variance BUNIT frozen");
    CHECK(v.ivar_bunit == "px^4/ADU^2", "ivar BUNIT frozen");
    CHECK(v.support_bunit == "px^2", "support BUNIT");
    CHECK(v.pixel_semantics == "surface_brightness", "pixel semantics");
    CHECK(v.pixel_area_power == -2, "pixel area power -2");
    CHECK(v.w_info > 0.0 && v.q > 0.0 && v.flux > 0.0, "W_info/Q/flux positive");
    CHECK_NEAR(v.flux_variance * v.w_info, 1.0, 1e-9, "Var(F_hat)=1/W_info");
    CHECK(v.weight_kind == "psfsw_robust_weight", "canonical weight.kind");
    CHECK(v.weight_units == "1", "canonical weight.units");
    CHECK(v.group_normalized == true, "group_normalized true (contract)");
    CHECK(v.norm_scope == "group", "normalization.scope=group");
    CHECK_NEAR(v.norm_median_target, 1.0, 1e-12, "median_target=1");
    CHECK(!v.has_weight_value, "single frame weight_value null (OI-01)");
    CHECK(v.valid && v.reason.empty(), "psfsw validity valid");
    CHECK(v.n_common >= 3, "n_common >= 3");
    CHECK(v.n_components == 4, "four psfsw components");
    CHECK(v.unnormalized_wt > 0.0, "unnormalized composite wt > 0");
    CHECK(v.normalization_deferred_to == "phase2", "group normalization deferred");
    CHECK(!v.target_ipix.empty(), "drizzle targets present");
    CHECK(v.science_bytes > 0, "science bytes > 0");
    CHECK(v.output_hash.size() == 64, "output hash 64 hex");
  }

  /* 未归一 wt 是 scale-degenerate；两帧应不同（证明来自真实四分量）。 */
  const Phase1OpenResult ra = open_phase1_product(pa.string());
  const Phase1OpenResult rb = open_phase1_product(pb.string());
  CHECK(ra.ok && rb.ok, "reopen for wt compare");
  CHECK(std::fabs(ra.view.unnormalized_wt - rb.view.unnormalized_wt) > 1e-12,
        "unnormalized wt frame-dependent");

  /* 记录中的冻结门证据。 */
  const json doc = load_json(pa / "phase1_product.json");
  CHECK(doc["phase1_extensions"]["gates"]["FZ-FORMULA-DRIZZLE-SB"] == true,
        "FZ-FORMULA-DRIZZLE-SB gate");
  CHECK(doc["phase1_extensions"]["gates"]["FZ-FORMULA-DRIZZLE-VAR"] == true,
        "FZ-FORMULA-DRIZZLE-VAR gate");
  CHECK(doc["phase1_extensions"]["gates"]["FZ-COND-FLUX-CONSERV"] == true,
        "FZ-COND-FLUX-CONSERV gate");
  CHECK(doc["phase1_extensions"]["parent_reduction"]["claims_exact"] == false,
        "parent reduction not claimed exact");
  CHECK(doc["phase1_extensions"]["parent_reduction"]["is_lower_bound"] == true,
        "parent reduction lower_bound");
  CHECK(doc["drizzle_covariance"]["diagonal_approximation"]["is_lower_bound"] == true,
        "covariance diagonal lower_bound");
  CHECK(doc["psfsw"]["component_flux_unit"] == "ADU", "component_flux_unit");
  CHECK(doc["psfsw"]["components"]["concentration"]["units"] == "ADU/px^2",
        "concentration units W6");
  CHECK(doc["provenance"]["flux_conservation_factor"] > 0.0, "flux factor present");
  CHECK(doc["provenance"]["sampling"]["pixfrac"] == 0.8, "pixfrac recorded");
  /* 禁诊断来源。 */
  CHECK(doc["provenance"]["correlation_summary"]["representation"] ==
            "correlation_kernel",
        "provenance correlation summary from real operator");
  CHECK(doc["drizzle_covariance"]["representation"] ==
            "diagonal_variance_plus_correlation_kernel",
        "covariance representation carries kernel");

  std::printf("  POSITIVE PASS checks=%d\n", g_checks);
  return true;
}

/* ── group（Phase2 消费面） ── */
static bool run_group(const fs::path& work) {
  const fs::path pos = work / "positive";
  const fs::path dir = work / "group";
  rm_rf(dir);
  fs::create_directories(dir);

  const std::vector<std::string> dirs = {(pos / "frame_a.p1").string(),
                                         (pos / "frame_b.p1").string()};
  const Phase1GroupConsumption g = consume_phase1_group_for_psfsw(dirs);
  CHECK(g.ok, g.error.c_str());
  CHECK(g.n_frames == 2, "two frames consumed from disk");
  CHECK(g.w_psfsw.size() == 2, "two group weights");
  CHECK(g.record_ok, "group record ok");
  const double med = 0.5 * (g.w_psfsw[0] + g.w_psfsw[1]); /* n=2 median */
  CHECK_NEAR(med, 1.0, 1e-9, "group median(w_psfsw)=1");
  CHECK(g.w_psfsw[0] > 0.0 && g.w_psfsw[1] > 0.0, "group weights positive");

  json out;
  out["n_frames"] = g.n_frames;
  out["wt_unnormalized"] = g.wt_unnormalized;
  out["w_psfsw"] = g.w_psfsw;
  out["median_wt"] = g.median_wt;
  save_json(dir / "group_result.json", out);
  std::printf("  GROUP PASS checks=%d w=[%.12g, %.12g]\n", g_checks, g.w_psfsw[0],
              g.w_psfsw[1]);
  return true;
}

/* ── negative ── */
struct NegCase {
  const char* name;
  std::function<void(const fs::path&)> mutate;
};

static bool run_negative(const fs::path& work) {
  const fs::path dir = work / "negative";
  rm_rf(dir);
  fs::create_directories(dir);

  auto write_case = [&](const std::string& name) -> fs::path {
    const Phase1FrameInputs in =
        make_inputs("neg-" + name, 1000.0, {100, 105, 95, 102, 98});
    const fs::path p = dir / (name + ".p1");
    rm_rf(p);
    const Phase1WriteResult w = write_phase1_product(in, p.string());
    if (!w.ok) {
      std::fprintf(stderr, "  [FAIL] setup write %s: %s\n", name.c_str(),
                   w.error.c_str());
      ++g_fails;
    }
    return p;
  };

  auto rec = [&](const fs::path& p) { return load_json(p / "phase1_product.json"); };
  auto put = [&](const fs::path& p, const json& j) {
    save_json(p / "phase1_product.json", j);
  };

  /* 正控制：未篡改产品必须重开成功（防止负向门恒红）。 */
  {
    const fs::path p = write_case("control");
    const Phase1OpenResult r = open_phase1_product(p.string());
    ++g_checks;
    if (!r.ok) {
      ++g_fails;
      std::fprintf(stderr, "  [FAIL] positive control reopen: %s\n", r.error.c_str());
      return false;
    }
  }

  struct Case {
    std::string name;
    std::function<void(const fs::path&)> f;
  };
  std::vector<Case> cases;

  cases.push_back({"fits_bit_flip", [&](const fs::path& p) {
                     if (!flip_last_byte(p / "science.fits")) {
                       std::fprintf(stderr, "flip failed\n");
                     }
                   }});
  cases.push_back({"weight_kind_ivar", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["weight"]["kind"] = "ivar";
                     put(p, j);
                   }});
  cases.push_back({"group_normalized_false", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["weight"]["group_normalized"] = false;
                     put(p, j);
                   }});
  cases.push_back({"median_target_2", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["weight"]["normalization"]["median_target"] = 2.0;
                     put(p, j);
                   }});
  cases.push_back({"signal_unit_adu", [&](const fs::path& p) {
                     json j = rec(p);
                     j["units"]["signal_sb"] = "ADU";
                     put(p, j);
                   }});
  cases.push_back({"flux_factor_removed", [&](const fs::path& p) {
                     json j = rec(p);
                     j["provenance"].erase("flux_conservation_factor");
                     put(p, j);
                   }});
  cases.push_back({"provenance_output_hash_removed", [&](const fs::path& p) {
                     json j = rec(p);
                     j["provenance"].erase("output_hash");
                     put(p, j);
                   }});
  cases.push_back({"winfo_units_wrong", [&](const fs::path& p) {
                     json j = rec(p);
                     j["point_information"]["W_info"]["units"] = "ADU^-1";
                     put(p, j);
                   }});
  cases.push_back({"valid_false_weight_present", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["validity"]["valid"] = false;
                     j["psfsw"]["validity"]["reason"] = "insufficient_valid_stars";
                     j["psfsw"]["weight"]["weight_value"] = 1.0;
                     put(p, j);
                   }});
  cases.push_back({"variance_from_weight", [&](const fs::path& p) {
                     json j = rec(p);
                     j["drizzle_covariance"]["variance_from"] = "weight";
                     put(p, j);
                   }});
  cases.push_back({"diagonal_claims_exact", [&](const fs::path& p) {
                     json j = rec(p);
                     j["drizzle_covariance"]["diagonal_approximation"]["is_lower_bound"] =
                         false;
                     put(p, j);
                   }});
  cases.push_back({"normalization_not_deferred", [&](const fs::path& p) {
                     json j = rec(p);
                     j["phase1_extensions"]["group_normalization_deferred_to"] =
                         "phase1";
                     put(p, j);
                   }});
  cases.push_back({"manifest_hash_changed", [&](const fs::path& p) {
                     json j = rec(p);
                     j["manifest"]["output_hash"] = std::string(64, 'b');
                     put(p, j);
                   }});
  cases.push_back({"concentration_units_legacy", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["components"]["concentration"]["units"] = "ADU/px";
                     put(p, j);
                   }});
  cases.push_back({"third_vocabulary_token", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["weight"]["normalization_scope"] = "group";
                     put(p, j);
                   }});
  cases.push_back({"correlation_rep_bogus", [&](const fs::path& p) {
                     json j = rec(p);
                     j["provenance"]["correlation_summary"]["representation"] =
                         "median_snr_weight";
                     put(p, j);
                   }});
  cases.push_back({"component_p05_gt_p95", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["components"]["signal"]["spatial_summary"]["p05"] =
                         j["psfsw"]["components"]["signal"]["spatial_summary"]["p95"]
                             .get<double>() +
                         1.0;
                     put(p, j);
                   }});
  cases.push_back({"component_count_3", [&](const fs::path& p) {
                     json j = rec(p);
                     j["psfsw"]["components"].erase("background");
                     put(p, j);
                   }});
  cases.push_back({"kcorr_one", [&](const fs::path& p) {
                     json j = rec(p);
                     j["provenance"]["k_corr"]["value"] = 1.0;
                     put(p, j);
                   }});
  cases.push_back({"software_sha_short", [&](const fs::path& p) {
                     json j = rec(p);
                     j["software_sha"] = "abc";
                     put(p, j);
                   }});
  cases.push_back({"manifest_file_hash_changed", [&](const fs::path& p) {
                     json j = rec(p);
                     j["manifest"]["files"][0]["sha256_hex"] = std::string(64, 'c');
                     put(p, j);
                   }});
  cases.push_back({"p33_coefficient_reintroduced", [&](const fs::path& p) {
                     json j = rec(p);
                     j["phase1_extensions"]["snr_frame_coefficient"] = 3.14;
                     put(p, j);
                   }});
  cases.push_back({"psf_snr_power_reenabled", [&](const fs::path& p) {
                     json j = rec(p);
                     j["phase1_extensions"]["psf_snr_power"] = true;
                     put(p, j);
                   }});
  cases.push_back({"bunit_card_changed", [&](const fs::path& p) {
                     /* 修改记录声明使期望 BUNIT 与实际 FITS 不符（记录篡改）。 */
                     json j = rec(p);
                     j["planes"][1]["bunit"] = "ADU^2";
                     put(p, j);
                   }});

  int detected = 0;
  for (const auto& c : cases) {
    const fs::path p = write_case(c.name);
    c.f(p);
    const Phase1OpenResult r = open_phase1_product(p.string());
    ++g_checks;
    if (r.ok) {
      ++g_fails;
      std::fprintf(stderr, "  [FAIL] negative NOT detected: %s\n", c.name.c_str());
    } else {
      ++detected;
    }
  }
  std::printf("  NEGATIVE PASS detected=%d/%zu checks=%d\n", detected, cases.size(),
              g_checks);
  return true;
}

/* ── halfproduct ── */
static bool run_halfproduct(const fs::path& work) {
  const fs::path dir = work / "half";
  rm_rf(dir);
  fs::create_directories(dir);
  Phase1FrameInputs in = make_inputs("half", 1000.0, {100, 105, 95, 102, 98});
  in.drizzle.closure_rel_tol = 0.0; /* 几何闭合不可能精确 -> builder 必败 */
  const fs::path target = dir / "half.p1";
  const Phase1WriteResult w = write_phase1_product(in, target.string());
  CHECK(!w.ok, "publish must fail when geometry closure cannot hold");
  CHECK(!fs::exists(target), "no visible half-product target");
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
    std::fprintf(stderr, "usage: %s <positive|group|negative|halfproduct> <work>\n",
                 argv[0]);
    return 2;
  }
  const std::string grp = argv[1];
  const fs::path work = argv[2];
  fs::create_directories(work);

  bool ok = false;
  if (grp == "positive") ok = run_positive(work);
  else if (grp == "group") ok = run_group(work);
  else if (grp == "negative") ok = run_negative(work);
  else if (grp == "halfproduct") ok = run_halfproduct(work);
  else {
    std::fprintf(stderr, "unknown group: %s\n", grp.c_str());
    return 2;
  }
  if (!ok || g_fails != 0) {
    std::fprintf(stderr, "RESULT FAIL group=%s checks=%d fails=%d\n", grp.c_str(),
                 g_checks, g_fails);
    return 1;
  }
  std::printf("RESULT PASS group=%s checks=%d\n", grp.c_str(), g_checks);
  return 0;
}
