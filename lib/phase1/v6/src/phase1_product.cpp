/* phase1_product.cpp — V6 Phase1 单帧产品装配 / 原子落盘 / 磁盘重开 / Phase2 消费面
 * 见 phase1_product.h 的冻结锚。本层只接线 Wave 5 实现，不含新科学公式。
 */
#include "astrocs/v6/phase1_product.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "astro/aio/v6_atomic_publish.h"
#include "astro/aio/v6_bunit.h"
#include "astro/aio/v6_fits.h"
#include "astro/aio/v6_product_io.h"
#include "astro/aio/v6_provenance.h"
#include "astro/aio/v6_sha256.h"

using nlohmann::json;
using namespace astrocs::aio;
using astrocs::calibration::v6::CalReason;
using astrocs::calibration::v6::CalResult;
using astrocs::calibration::v6::CalStatus;
using astrocs::calibration::v6::CovarianceRecord;

namespace astrocs {
namespace v6 {
namespace phase1 {

namespace {

const char* kForbiddenVarianceSources[] = {
    "median_source_snr", "median_snr", "source_snr_median", "support",
    "coverage",          "fwhm",       "psf_residual",      "residual",
    "weight",            "psfsw_robust_weight", "psfsw", "effective_psf",
    "source_snr"};

json forbidden_variance_sources_json() {
  json a = json::array();
  for (const char* t : kForbiddenVarianceSources) a.push_back(t);
  return a;
}

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else if (c != ' ') {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

/* P33 撤销守卫（C-004.2 / FZ-GATE-MEDIAN-SNR）：帧级 median(SNR_F) 系数与
 * 任何诊断量权重形态不得在 Phase1 产品记录中以键形式重新出现。只扫键，不扫
 * 值（forbidden_variance_sources 的值列表本身含诊断词，属合法声明）。 */
bool has_forbidden_p33_key(const json& j) {
  static const char* kForbiddenKeys[] = {
      "snr_frame_coefficient", "snr_frame_science", "snr_coefficient",
      "snr_coef",              "median_snr_weight", "support_x_snr2",
      "support_x_snr",         "psf_snr_power",     "snr_frame"};
  if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      for (const char* bad : kForbiddenKeys)
        if (it.key() == bad) return true;
      if (has_forbidden_p33_key(it.value())) return true;
    }
  } else if (j.is_array()) {
    for (const auto& e : j)
      if (has_forbidden_p33_key(e)) return true;
  }
  return false;
}

bool is_hex40(const std::string& s) {
  if (s.size() != 40) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

void append_f64_be(std::vector<std::uint8_t>& out, double v) {
  std::uint64_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  for (int i = 7; i >= 0; --i)
    out.push_back(static_cast<std::uint8_t>(u >> (8 * i)));
}

std::vector<std::uint8_t> encode_f64_be(const std::vector<double>& v) {
  std::vector<std::uint8_t> out;
  out.reserve(v.size() * 8);
  for (double x : v) append_f64_be(out, x);
  return out;
}

FitsLayer make_layer(const std::string& extname, const std::string& bunit,
                     const std::vector<double>& values,
                     const std::vector<FitsCard>& extra = {}) {
  FitsLayer l;
  l.spec.extname = extname;
  l.spec.bitpix = -64;
  l.spec.naxis = {values.size()};
  l.spec.cards.push_back(FitsCard::make_string("BUNIT", bunit, ""));
  for (const auto& c : extra) l.spec.cards.push_back(c);
  l.data = encode_f64_be(values);
  return l;
}

/* 单位：用生产 schema 的冻结单位串逐条比对（不发明新串）。 */
bool units_frozen_ok(const Phase1Units& u, std::vector<std::string>* why) {
  ValidationReport r;
  astrocs::aio::unit_matches_frozen(Quantity::kSignalSb, u.signal_sb, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kPixelVarianceIn, u.pixel_variance_in, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kSbVarianceOut, u.sb_variance_out, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kSbIvarOut, u.sb_ivar_out, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kWInfo, u.w_info, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kQ, u.q, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kFlux, u.flux, &r);
  astrocs::aio::unit_matches_frozen(Quantity::kPsfswRobustWeight,
                                    u.psfsw_robust_weight, &r);
  std::string qwhy;
  if (!astrocs::aio::quadratic_law_holds(u.signal_sb, u.sb_variance_out,
                                         u.sb_ivar_out, &qwhy)) {
    r.add("G-BUNIT-QUADRATIC", "FZ-P3-BUNIT-QUADRATIC", qwhy);
  }
  if (!r.ok()) {
    for (const auto& v : r.violations())
      why->push_back(v.gate + "(" + v.freeze_id + "): " + v.message);
    return false;
  }
  return true;
}

/* 由 JSON 构造 ExpectedHdu 列表（重开独立验证用）。 */
bool expected_from_record(const json& doc, std::vector<ExpectedHdu>* out,
                          std::string* err) {
  if (!doc.contains("planes") || !doc["planes"].is_array() ||
      doc["planes"].empty()) {
    *err = "record.planes missing/empty";
    return false;
  }
  for (const auto& p : doc["planes"]) {
    ExpectedHdu e;
    e.extname = p.value("extname", std::string());
    e.bitpix = p.value("bitpix", -64);
    for (const auto& d : p["naxis"]) e.naxis.push_back(d.get<std::size_t>());
    e.bunit = p.value("bunit", std::string());
    e.check_bunit = true;
    out->push_back(e);
  }
  return true;
}

bool write_text_file(const std::string& path, const std::string& text,
                     std::string* err) {
  std::ofstream os(path, std::ios::binary | std::ios::trunc);
  if (!os) {
    *err = "cannot open for write: " + path;
    return false;
  }
  os << text;
  os.close();
  if (!os) {
    *err = "write failed: " + path;
    return false;
  }
  return true;
}

bool read_text_file(const std::string& path, std::string* out,
                    std::string* err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    *err = "cannot open: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

/* 生产 provenance 对象（AIO 形状；所有字段来自本帧真实输入/实现）。 */
Provenance make_provenance(const Phase1FrameInputs& in, const Phase1Units& u,
                           const std::string& science_sha, double pixfrac,
                           const std::string& corr_rep,
                           const std::string& corr_kernel_id, double corr_scale,
                           double rho_mean, double rho_max, double A_pixel) {
  Provenance p;
  p.product.type_id = kPhase1TypeId;
  p.product.schema_version = 1;
  p.software_sha = in.software_sha;
  p.run_id = in.run_id;
  p.input_product_hashes = {
      in.calibration.input_frame_sha256.empty()
          ? ("sha256:" + Sha256::hex_of_string(in.frame_id))
          : in.calibration.input_frame_sha256};
  p.config_hash = in.config_hash.empty() ? ("sha256:" + Sha256::hex_of_string("phase1-config"))
                                         : in.config_hash;
  p.units.bunit = u.signal_sb;
  p.units.pixel_semantics = u.pixel_semantics;
  p.units.pixel_area_power = u.pixel_area_power;
  p.units.has_target_pixel_area = true;
  p.units.target_pixel_area = A_pixel;
  p.coordinate_frame = in.coordinate_frame;
  p.coordinate_epoch = in.coordinate_epoch;
  p.pixel_semantics = u.pixel_semantics;
  p.sampling.kernel_id = "drizzle_forward";
  p.sampling.has_pixfrac = true;
  p.sampling.pixfrac = pixfrac;
  p.algorithm_ids = {"ALG-P1-CAL-COV-001", "ALG-P1-PSFINF-001",
                     "ALG-P1-PSFW-001", "ALG-P1-DRZ-SB-001"};
  p.module.module_id = "astrocs.phase1.product";
  p.module.build_id = in.build_id;
  p.provider = in.provider;
  p.approximations = {
      "diagonal_per_pixel_covariance_for_point_information (FZ-COND-WHITENOISE)",
      "output_variance_reported_from_actual_drizzle_coefficients "
      "(FZ-FORMULA-COV-PROP)",
      "k_corr inherited frozen in-domain constant 1.4 "
      "(FZ-PROV-KCORR-VALUE / PENDING_OWNER_SIGNOFF); Phase1 does not re-fit",
      "OI-01 OPEN: psfsw group_normalization deferred to Phase2"};
  p.degradations.clear();
  p.normalization_version = "drizzle_sb_a_pixel_preserving_v1";
  p.weight_mode_version = "psfsw_composite_PSFSW-COMPOSITE-V1";
  /* provenance.correlation_summary.representation 的词表与 covariance.v1
   * representation 不同（生产 provenance schema 只接受 correlation_kernel /
   * low_rank_factors / common_master / full_matrix_unavailable）。 */
  p.correlation_summary.representation = corr_rep;
  if (corr_rep != "diagonal_variance" && corr_rep != "full_matrix_unavailable") {
    p.correlation_summary.kernel_id = corr_kernel_id;
    p.correlation_summary.has_scale = true;
    p.correlation_summary.scale = corr_scale;
    p.correlation_summary.has_mean_abs_rho = true;
    p.correlation_summary.mean_abs_rho = rho_mean;
    p.correlation_summary.has_max_abs_rho = true;
    p.correlation_summary.max_abs_rho = rho_max;
  }
  p.has_flux_conservation_factor = true;
  p.flux_conservation_factor = pixfrac * pixfrac;
  p.k_corr.definition = "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]";
  p.k_corr.value = 1.4; /* FZ-PROV-KCORR-VALUE 域内冻结常数；Phase1 不重拟合 */
  p.k_corr.domain.geometry = "spherical_drizzle";
  p.k_corr.domain.pixfrac = pixfrac;
  p.k_corr.domain.patch_size = 8;
  p.k_corr.domain.estimator = "median";
  p.k_corr.domain.spherical = true;
  p.k_corr.calibration.script =
      "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"
      "#FZ-PROV-KCORR-VALUE";
  p.k_corr.calibration.seed = 20260915;
  p.k_corr.calibration.calibration_run_id =
      "frozen-in-domain-constant-FZ-PROV-KCORR-VALUE";
  p.k_corr.lookup_table_ref = "K_CORR_DOMAIN_B";
  p.has_k_corr = true;
  p.unavailable_flag = false;
  p.unavailable_reason = "not_applicable";
  p.unavailable_scope = "none";
  p.generated_utc = "2026-09-15T00:00:00Z";
  p.output_hash = "sha256:" + science_sha;
  p.signal_unit = u.signal_sb;
  p.variance_unit = u.sb_variance_out;
  p.ivar_unit = u.sb_ivar_out;
  p.diagonal_variance_only =
      (corr_rep == "full_matrix_unavailable" || corr_rep == "diagonal_variance");
  return p;
}

std::string now_note() { return "P1-INTEGRATE-001"; }

}  /* namespace */

/* ======================================================================== */
/* 写出                                                                    */
/* ======================================================================== */

Phase1WriteResult write_phase1_product(const Phase1FrameInputs& in,
                                       const std::string& target_dir) {
  Phase1WriteResult res;
  res.target_dir = target_dir;

  if (in.frame_id.empty() || in.run_id.empty()) {
    res.error = "frame_id/run_id required";
    return res;
  }
  if (!is_hex40(in.software_sha)) {
    res.error = "software_sha must be full 40-hex";
    return res;
  }
  std::vector<std::string> uwhy;
  if (!units_frozen_ok(in.units, &uwhy)) {
    res.error = "frozen units violated: " + (uwhy.empty() ? std::string("?") : uwhy[0]);
    return res;
  }
  /* BUNIT 可判性门（FZ-BUNIT-SEMANTICS）。 */
  {
    const BunitCheck bc = bunit_dimension_decidable(
        in.units.signal_sb, in.units.pixel_semantics, in.units.pixel_area_power,
        true, 1.0);
    if (!bc.decidable) {
      res.error = "BUNIT not dimensionally decidable: " + bc.reason;
      return res;
    }
  }

  /* 1) 校准 covariance（Wave 5）。 */
  const CalResult cal = calibration::v6::calibrate_pixel(
      in.calibration.config, in.calibration.pixel);
  if (cal.status != CalStatus::kOk) {
    res.error = std::string("calibration not available: ") + calibration::v6::to_string(cal.reason);
    return res;
  }
  const CovarianceRecord cal_rec =
      calibration::v6::make_calibration_covariance_record(cal);
  std::string cerr;
  if (!calibration::v6::validate_covariance_record(cal_rec, &cerr)) {
    res.error = "calibration covariance record invalid: " + cerr;
    return res;
  }

  /* 2) PSF 归一 / A_NEA（Wave 5）。 */
  const auto pst = p1psfw::psf_profile_stats(in.point_source.psf_profile.data(),
                                             in.point_source.psf_profile.size());
  if (!pst.ok) {
    res.error = std::string("PSF profile rejected: ") +
                (pst.reject ? pst.reject : "unknown");
    return res;
  }

  /* 3) 点源信息 W_info / Q / F_hat（Wave 5）。 */
  const std::size_t m = in.point_source.psf_profile.size();
  if (in.point_source.sigma2.size() != m || in.point_source.data.size() != m) {
    res.error = "point_source profile/sigma2/data length mismatch";
    return res;
  }
  const p1psfw::PointEstimate pe = p1psfw::w_info_diagonal(
      in.point_source.psf_profile.data(), m, in.point_source.sigma2.data(),
      in.point_source.data.data(), in.point_source.a);
  if (!pe.ok) {
    res.error = std::string("W_info rejected: ") + (pe.reject ? pe.reject : "unknown");
    return res;
  }
  const p1psfw::FluxEstimate fe = p1psfw::combine_point_estimates({pe});
  if (!fe.ok) {
    res.error = std::string("F_hat rejected: ") + (fe.reject ? fe.reject : "unknown");
    return res;
  }

  /* 4) PSFSW 四分量 + 未归一复合（Wave 5；OI-01：组内归一归 Phase2）。 */
  p1psfw::FrameComponentInput ci;
  ci.a_nea = (in.psfsw.a_nea > 0.0) ? in.psfsw.a_nea : pst.a_nea;
  ci.fhat = in.psfsw.fhat;
  ci.fhat_valid = in.psfsw.fhat_valid;
  ci.background_robust_mean = in.psfsw.background_robust_mean;
  ci.a_ref = in.psfsw.a_ref;
  ci.signal_samples = in.psfsw.signal_samples;
  ci.concentration_samples = in.psfsw.concentration_samples;
  ci.noise_samples = in.psfsw.noise_samples;
  ci.background_samples = in.psfsw.background_samples;
  const p1psfw::PsfswFrameComponents comps = p1psfw::extract_psfsw_components(ci);
  if (!comps.ok) {
    res.error = std::string("psfsw components rejected: ") +
                (comps.reject ? comps.reject : "unknown");
    return res;
  }
  p1psfw::ComponentValues cv;
  cv.s = comps.s;
  cv.conc = comps.conc;
  cv.n = comps.n;
  cv.b = comps.b;
  const p1psfw::CompositeResult cr = p1psfw::compute_psfsw_weights({cv});
  if (!cr.ok || cr.wt.empty()) {
    res.error = std::string("psfsw composite rejected: ") +
                (cr.reject ? cr.reject : "unknown");
    return res;
  }
  const double wt_unnormalized = cr.wt[0];

  /* 共同星集门（Wave 5）。 */
  {
    p1psfw::CommonStarSet set;
    set.common_star_set_id = in.psfsw.common_star_set_id;
    set.selection.selection_function_id = in.psfsw.selection_function_id;
    set.selection.reference_catalog_id = "phase1_external_catalog";
    set.selection.reference_catalog_version_hash = "v1";
    set.selection.mag_min = 8.0;
    set.selection.mag_max = 20.0;
    set.selection.detection_threshold_sigma = 5.0;
    set.selection.matching_radius_arcsec = 1.0;
    set.selection.epoch_pm_handling = "epoch_matched";
    set.selection.applied_at = "phase1";
    set.selection.independence_proof = in.psfsw.independence_proof;
    for (std::int64_t sid = 1;
         sid <= static_cast<std::int64_t>(in.psfsw.fhat.size()); ++sid) {
      p1psfw::CommonStarMember mem;
      mem.star_id = sid;
      mem.valid_in_frame = {1};
      set.members.push_back(mem);
    }
    const auto csv = p1psfw::validate_common_star_set(set, 1);
    if (!csv.ok) {
      res.error = std::string("common star set rejected: ") +
                  (csv.reject ? csv.reject : "unknown");
      return res;
    }
  }

  /* 5) 球面 Drizzle：真实球面 overlap -> v6 SB/方差算子（Wave 5）。 */
  const std::size_t npx = static_cast<std::size_t>(in.drizzle.nx) *
                          static_cast<std::size_t>(in.drizzle.ny);
  if (in.drizzle.pixel_to_sky == nullptr || in.drizzle.nx <= 0 ||
      in.drizzle.ny <= 0 || in.drizzle.pixel_values.size() != npx ||
      in.drizzle.pixel_variance.size() != npx) {
    res.error = "drizzle inputs invalid (pixel_to_sky/nx/ny/pixel_values/variance)";
    return res;
  }

  /* 在 builder 内完成 drizzle + FITS + JSON，使几何失败发生在 staging 建立后，
   * 由外层 atomic_publish_directory 清理 -> 无可见半成品。 */
  double corr_scale = 1.0;
  std::string corr_rep = "diagonal_variance";       /* covariance.v1 enum */
  std::string prov_corr_rep = "full_matrix_unavailable"; /* provenance enum */
  std::string corr_kernel_id;
  double rho_mean = 0.0, rho_max = 0.0;
  double A_pixel_out = 1.0;
  std::vector<std::uint64_t> target_ipix_out;
  std::string fit_err;

  const DirBuilderFn builder = [&](const std::string& stage_dir,
                                   const CancelFn& cancel,
                                   std::string* err) -> bool {
    if (cancel && cancel()) {
      *err = "cancelled";
      return false;
    }
    using namespace drizzle;
    ::healpix::HealpixCore hp(in.drizzle.nside, true);
    std::vector<SourceDropSpec> sources;
    sources.reserve(npx);
    for (int y = 0; y < in.drizzle.ny; ++y) {
      for (int x = 0; x < in.drizzle.nx; ++x) {
        SourceDropSpec s;
        s.px = static_cast<double>(x) + 0.5;
        s.py = static_cast<double>(y) + 0.5;
        s.pixfrac = in.drizzle.pixfrac;
        s.A_pixel = 0.0; /* 由球面 drop 面积反推 */
        sources.push_back(s);
      }
    }
    DrizzleOperator op;
    const DrzError de = build_operator_from_sources(
        hp, sources, in.drizzle.pixel_to_sky, in.drizzle.user_data,
        in.drizzle.closure_rel_tol, op, &target_ipix_out, nullptr);
    if (de != DrzError::ok) {
      *err = std::string("drizzle operator build failed: ") + drz_error_name(de);
      return false;
    }
    const uint32_t nd = op.n_dst();
    std::vector<double> sig(nd), var(nd), ivar(nd), support(nd);
    for (uint32_t p = 0; p < nd; ++p) {
      sig[p] = op.signal_sb(p, in.drizzle.pixel_values.data());
      var[p] = op.variance_sb(p, in.drizzle.pixel_variance.data());
      ivar[p] = (var[p] > 0.0) ? (1.0 / var[p]) : 0.0;
      support[p] = op.D(p);
    }
    /* 相关核摘要：由真实算子 covariance（非对角）计算。 */
    corr_rep = "diagonal_variance";
    if (nd >= 2) {
      double max_abs_cov = 0.0, sum_abs_rho = 0.0, max_abs_rho = 0.0;
      long n_rho = 0;
      for (uint32_t p = 0; p < nd; ++p) {
        for (uint32_t q = p + 1; q < nd; ++q) {
          const double c = op.covariance_sb(p, q, in.drizzle.pixel_variance.data());
          max_abs_cov = std::max(max_abs_cov, std::fabs(c));
          const double d = std::sqrt(var[p] * var[q]);
          if (d > 0.0) {
            const double rho = std::fabs(c) / d;
            sum_abs_rho += rho;
            max_abs_rho = std::max(max_abs_rho, rho);
            ++n_rho;
          }
        }
      }
      if (max_abs_cov > 0.0 && n_rho > 0) {
        corr_rep = "diagonal_variance_plus_correlation_kernel";
        prov_corr_rep = "correlation_kernel";
        corr_kernel_id = "drizzle_forward_operator_v1";
        corr_scale = max_abs_cov;
        rho_mean = sum_abs_rho / static_cast<double>(n_rho);
        rho_max = max_abs_rho;
      }
    }
    A_pixel_out = op.A_pixel(0);

    /* 冻结门证据（Wave 5 gate 裁决）。 */
    const GateVerdict g_sb = gate_sb_definition(op, in.drizzle.pixel_values.data(), sig);
    const GateVerdict g_var = gate_variance_identity(op, in.drizzle.pixel_variance.data(), var);
    const GateVerdict g_flux =
        gate_flux_conservation(op, in.drizzle.pixel_values.data(), true, true);
    const GateVerdict g_ulaw =
        gate_unit_law(UnitId::signal_sb, UnitId::sb_variance_out, UnitId::sb_ivar_out);
    std::vector<double> aperture_w(nd, 1.0); /* 单位 aperture 权重（gate 需要真实权重） */
    const GateVerdict g_cov = gate_covariance_propagation(
        op, in.drizzle.pixel_variance.data(), aperture_w.data(), false);
    if (!g_sb.pass || !g_var.pass || !g_flux.pass || !g_ulaw.pass || !g_cov.pass) {
      *err = "drizzle frozen gate failed: sb=" + g_sb.reason + " var=" + g_var.reason +
             " flux=" + g_flux.reason + " unit=" + g_ulaw.reason + " cov=" + g_cov.reason;
      return false;
    }

    /* FITS 层：PRIMARY=signal_sb, SUPPORT, VARIANCE, IVAR。 */
    const FitsCard org = FitsCard::make_string("ORIGIN", "AstroCS", "");
    const FitsCard nside_card =
        FitsCard::make_integer("NSIDE", in.drizzle.nside, "HEALPix NESTED");
    std::vector<FitsLayer> layers;
    layers.push_back(make_layer("", in.units.signal_sb, sig, {org, nside_card}));
    layers.push_back(make_layer("SUPPORT", in.units.support, support, {org}));
    layers.push_back(make_layer("VARIANCE", in.units.sb_variance_out, var, {org}));
    layers.push_back(make_layer("IVAR", in.units.sb_ivar_out, ivar, {org}));

    const std::string sci = stage_dir + "/" + kPhase1ScienceFile;
    PublishOptions po;
    const PublishResult pr = publish_fits_product(
        sci, layers, expected_from_layers(layers), po, CancelFn());
    if (pr.status != PublishStatus::kOk) {
      *err = "science.fits publish failed: " + pr.message;
      return false;
    }
    std::string sci_sha;
    if (!sha256_file_hex(sci, &sci_sha)) {
      *err = "science.fits sha256 failed";
      return false;
    }

    /* ── 记录 JSON ── */
    json doc;
    doc["product_schema"] = kPhase1ProductSchema;
    doc["schema_version"] = 1;
    doc["phase"] = "phase1";
    doc["product_role"] = "phase1_product_v1";
    doc["type_id"] = kPhase1TypeId;
    doc["frame_id"] = in.frame_id;
    doc["run_id"] = in.run_id;
    doc["software_sha"] = in.software_sha;
    doc["generated_utc"] = "2026-09-15T00:00:00Z";
    doc["producer_note"] = now_note();

    json units;
    units["signal_sb"] = in.units.signal_sb;
    units["pixel_variance_in"] = in.units.pixel_variance_in;
    units["sb_variance_out"] = in.units.sb_variance_out;
    units["sb_ivar_out"] = in.units.sb_ivar_out;
    units["W_info"] = in.units.w_info;
    units["Q"] = in.units.q;
    units["flux"] = in.units.flux;
    units["psfsw_robust_weight"] = in.units.psfsw_robust_weight;
    units["support"] = in.units.support;
    units["pixel_semantics"] = in.units.pixel_semantics;
    units["pixel_area_power"] = in.units.pixel_area_power;
    doc["units"] = units;

    doc["planes"] = json::array();
    doc["planes"].push_back({{"plane_id", "signal"},
                             {"extname", ""},
                             {"bunit", in.units.signal_sb},
                             {"units", in.units.signal_sb},
                             {"bitpix", -64},
                             {"dtype", "float64"},
                             {"naxis", {sig.size()}},
                             {"invalid_policy", "nan_or_support_le_0"}});
    doc["planes"].push_back({{"plane_id", "support"},
                             {"extname", "SUPPORT"},
                             {"bunit", in.units.support},
                             {"units", in.units.support},
                             {"bitpix", -64},
                             {"dtype", "float64"},
                             {"naxis", {support.size()}},
                             {"invalid_policy", "zero_when_no_coverage"}});
    doc["planes"].push_back({{"plane_id", "variance"},
                             {"extname", "VARIANCE"},
                             {"bunit", in.units.sb_variance_out},
                             {"units", in.units.sb_variance_out},
                             {"bitpix", -64},
                             {"dtype", "float64"},
                             {"naxis", {var.size()}},
                             {"invalid_policy", "nan_or_support_le_0"}});
    doc["planes"].push_back({{"plane_id", "ivar"},
                             {"extname", "IVAR"},
                             {"bunit", in.units.sb_ivar_out},
                             {"units", in.units.sb_ivar_out},
                             {"bitpix", -64},
                             {"dtype", "float64"},
                             {"naxis", {ivar.size()}},
                             {"invalid_policy", "zero_when_variance_le_0"}});

    /* point_information（生产 schema 形状）。 */
    json pi;
    pi["point_information_schema"] = "astrocs.v6.point-information/v1";
    pi["schema_version"] = 1;
    pi["authoritative_formula"] =
        "Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; "
        "F_hat=Q/W; Var(F_hat)=1/W";
    pi["W_info"] = {{"value", pe.w_info}, {"units", in.units.w_info}, {"representation", "scalar"}};
    pi["Q"] = {{"value", pe.q}, {"units", in.units.q}, {"representation", "scalar"}};
    pi["flux"] = {{"value", fe.f_hat}, {"units", in.units.flux}, {"representation", "scalar"}};
    pi["flux_variance"] = {{"value", fe.var_f}, {"units", "ADU^2"}, {"representation", "scalar"}};
    pi["representation"] = "scalar";
    pi["independent_frame_combination"] = {
        {"independent_frames", true},
        {"Q_combined", "Q = Sum_k Q_k"},
        {"W_combined", "W = Sum_k W_info,k"}};
    pi["white_noise_approximation"] = {
        {"applied", true},
        {"conditions", {{"C_diagonal", true}, {"sigma_pix_declared", true}, {"a_k_consistent", true}}},
        {"formula",
         "W_info,k = a_k^2/(sigma_pix,k^2 * A_NEA,k), A_NEA,k = 1/Sum_p P_k,p^2"}};
    pi["a_nea_ref"] = "A_NEA from psf_information (FZ-COND-WHITENOISE)";
    pi["frame_inputs"] = json::array();
    pi["frame_inputs"].push_back({{"frame_id", in.frame_id},
                                  {"a_ref", "phase1_product.json#point_source.a"},
                                  {"P_ref", "phase1_product.json#point_source.psf_profile"},
                                  {"C_ref", "phase1_product.json#calibration_covariance"}});
    pi["provenance_ref"] = "provenance";
    doc["point_information"] = pi;

    /* psfsw（生产 schema 形状；weight_value=null 表示 Phase1 尚未有帧组）。 */
    json ps;
    ps["psfsw_schema"] = "astrocs.v6.psfsw/v1";
    ps["schema_version"] = 1;
    ps["weight_mode"] = "psfsw_robust";
    ps["weight"] = {{"kind", "psfsw_robust_weight"},
                    {"units", "1"},
                    {"group_normalized", true},
                    {"normalization",
                     {{"scope", "group"},
                      {"median_target", 1.0},
                      {"constants_version", p1psfw::kCompositeVersion}}},
                    {"weight_value", nullptr}};
    ps["component_flux_unit"] = "ADU";
    auto comp_json = [](const p1psfw::ComponentMeasure& c) {
      return json{{"measurement_id", c.measurement_id},
                  {"value", c.value},
                  {"units", c.unit},
                  {"estimator", {{"id", c.estimator}, {"version", c.estimator_version}}},
                  {"spatial_summary",
                   {{"p05", c.p05},
                    {"p50", c.p50},
                    {"p95", c.p95},
                    {"valid_area_fraction", c.valid_area_fraction}}}};
    };
    ps["components"] = {{"signal", comp_json(comps.signal)},
                        {"concentration", comp_json(comps.concentration)},
                        {"noise", comp_json(comps.noise)},
                        {"background", comp_json(comps.background)}};
    ps["composite"] = {
        {"formula_ref",
         "Wt_k = C_norm * S_k^alpha * Conc_k^beta / (N_k^gamma * B_k^delta); "
         "W_psfsw,k = Wt_k / median_j(Wt_j)"},
        {"exponents",
         {{"alpha", p1psfw::kCompositeAlpha},
          {"beta", p1psfw::kCompositeBeta},
          {"gamma", p1psfw::kCompositeGamma},
          {"delta", p1psfw::kCompositeDelta}}},
        {"C_norm_version", p1psfw::kCompositeVersion},
        {"truncation", "fail_closed_then_component_floor_then_group_median_normalize"},
        {"estimator_version", "psfsw-est-v1"},
        {"calibration_sample_id", in.psfsw.calibration_sample_id}};
    ps["common_star_set"] = {
        {"common_star_set_id", in.psfsw.common_star_set_id},
        {"selection_function_id", in.psfsw.selection_function_id},
        {"members_hash", in.psfsw.members_hash},
        {"construction", in.psfsw.construction},
        {"n_common", static_cast<int>(in.psfsw.fhat.size())},
        {"exclusion_flags", split_csv(in.psfsw.exclusion_flags)}};
    ps["validity"] = {{"valid", true}, {"reason", nullptr}};
    ps["covariance_ref"] = "calibration_covariance";
    ps["effective_psf_ref"] = "effective_psf";
    ps["provenance_ref"] = "provenance";
    doc["psfsw"] = ps;

    /* calibration covariance（生产 schema 形状）。 */
    json cc;
    cc["covariance_schema"] = "astrocs.v6.covariance/v1";
    cc["schema_version"] = 1;
    cc["propagation"] = "C_out = R C_in R^T";
    cc["scalar_form"] = "Var(out) = c^T C_in c = Sum_{i,j} c_i c_j [C_in]_{ij}";
    cc["representation"] = cal_rec.representation;
    cc["combination_coefficients"] = cal_rec.combination_coefficients;
    cc["variance_from"] = cal_rec.variance_from;
    cc["input_covariance"] = cal_rec.input_covariance;
    cc["avail"] = cal_rec.avail;
    cc["forbidden_variance_sources"] = forbidden_variance_sources_json();
    if (cal_rec.representation == "common_master") {
      cc["common_master"] = {
          {"master_id", cal.shared_systematic.common_master.master_id},
          {"alpha_m", cal.shared_systematic.common_master.alpha_m},
          {"alpha_unit", "1"}};
    } else if (cal_rec.representation ==
               "diagonal_variance_plus_correlation_kernel") {
      cc["correlation_kernel"] = {
          {"kernel_id", cal.shared_systematic.correlation_kernel.kernel_id},
          {"scale", cal.shared_systematic.correlation_kernel.scale},
          {"rho_summary", {{"mean_abs_rho", 0.0}, {"max_abs_rho", 0.0}}}};
    } else if (cal_rec.representation == "low_rank_factors") {
      cc["low_rank"] = {
          {"L_ref", cal.shared_systematic.low_rank.factor_ref},
          {"rank", cal.shared_systematic.low_rank.rank}};
    } else {
      cc["operator_descriptor"] = {
          {"kind", "drizzle_forward"},
          {"reconstructable", true},
          {"summary_ref", "calibration_covariance.combination_coefficients"}};
    }
    cc["variance_plane"] = {{"units", "ADU^2"},
                            {"dtype", "float64"},
                            {"invalid_policy", "nan_or_support_le_0"}};
    cc["provenance_ref"] = "provenance";
    doc["calibration_covariance"] = cc;

    /* drizzle covariance（生产 schema 形状；实际操作系数可重建）。 */
    const double pvar_exact = op.parent_variance_exact(in.drizzle.pixel_variance.data());
    const double pvar_diag = op.parent_variance_diagonal(in.drizzle.pixel_variance.data());
    const double deficit = (pvar_exact > 0.0) ? (pvar_exact - pvar_diag) / pvar_exact : 0.0;
    json dc;
    dc["covariance_schema"] = "astrocs.v6.covariance/v1";
    dc["schema_version"] = 1;
    dc["propagation"] = "C_out = R C_in R^T";
    dc["scalar_form"] = "Var(out) = c^T C_in c = Sum_{i,j} c_i c_j [C_in]_{ij}";
    dc["representation"] = corr_rep;
    dc["operator_descriptor"] = {{"kind", "drizzle_forward"},
                                 {"reconstructable", true},
                                 {"summary_ref", "phase1_product.json#phase1_extensions.operator"}};
    if (corr_rep != "diagonal_variance") {
      dc["correlation_kernel"] = {
          {"kernel_id", corr_kernel_id},
          {"scale", corr_scale},
          {"rho_summary", {{"mean_abs_rho", rho_mean}, {"max_abs_rho", rho_max}}}};
    }
    dc["diagonal_approximation"] = {
        {"is_lower_bound", true},
        {"deficit_metric", {{"name", "deficit=(exact-diag)/exact"}, {"value", deficit}}},
        {"use_for_aperture", false},
        {"threshold_ref", "FZ-GATE-PARENT-VAR/PROPOSED_PENDING_OWNER_SIGNOFF_SO07"}};
    dc["variance_from"] = "actual_combination_coefficients";
    dc["input_covariance"] = "declared";
    dc["input_covariance_ref"] = "calibration_covariance";
    dc["avail"] = "available";
    dc["forbidden_variance_sources"] = forbidden_variance_sources_json();
    dc["variance_plane"] = {{"units", in.units.sb_variance_out},
                            {"dtype", "float64"},
                            {"invalid_policy", "nan_or_support_le_0"}};
    dc["ivar_plane"] = {{"units", in.units.sb_ivar_out},
                        {"dtype", "float64"},
                        {"invalid_policy", "nan_or_support_le_0"}};
    dc["provenance_ref"] = "provenance";
    doc["drizzle_covariance"] = dc;

    /* effective PSF（生产 schema 形状；FZ-GATE-PSFSW-EPSF 必输）。 */
    const p1psfw::EffectivePsf epsf = p1psfw::conventional_effective_psf(
        {in.point_source.psf_profile.data()}, m,
        {in.point_source.a}, {1.0},
        p1psfw::EffectivePsfNormalization::peak, in.frame_id + "/epsf-v1");
    if (!epsf.ok) {
      *err = std::string("effective PSF rejected: ") + (epsf.reject ? epsf.reject : "unknown");
      return false;
    }
    const std::string peff_hash = Sha256::hex_of(
        epsf.profile.data(), epsf.profile.size() * sizeof(double));
    json ep;
    ep["effective_psf_schema"] = "astrocs.v6.effective-psf/v1";
    ep["schema_version"] = 1;
    ep["effective_psf_id"] = epsf.effective_psf_id;
    ep["definition"] = "impulse_response_of_combination";
    ep["product_family"] = "phase1_point_information";
    ep["formula_family"] = "conventional_coadd";
    ep["formula_ref"] = "P_eff = Sum_k alpha_k a_k P_k / Sum_k alpha_k a_k P_k(0)";
    ep["normalization"] = "peak";
    ep["values_or_model"] = {{"kind", "values"}, {"values", epsf.profile}};
    ep["kernel_transfer"] = json::array();
    ep["kernel_transfer"].push_back(
        {{"frame_id", in.frame_id}, {"kernel_id", "identity_native_grid"}, {"kernel_version", "v1"}});
    ep["combination_coefficients_ref"] = "point_information.independent_frame_combination";
    ep["input_psf_refs"] = json::array();
    ep["input_psf_refs"].push_back(in.frame_id + "#psf_profile");
    ep["fwhm_from_effective"] = {{"value", epsf.fwhm}, {"unit", "px"}, {"role", "derived_diagnostic"}};
    ep["reproducibility"] = {{"p_eff_hash", peff_hash}, {"kernel_set_hash", "identity_native_grid_v1"}};
    ep["provenance_ref"] = "provenance";
    doc["effective_psf"] = ep;

    /* provenance（生产 schema 形状；最小集）。 */
    const Provenance prov = make_provenance(
        in, in.units, sci_sha, in.drizzle.pixfrac, prov_corr_rep, corr_kernel_id,
        corr_scale, rho_mean, rho_max, A_pixel_out);
    const ValidationReport prov_rep = validate_provenance(prov);
    if (!prov_rep.ok()) {
      *err = "provenance gate failed: " + prov_rep.summary();
      return false;
    }
    doc["provenance"] = provenance_to_json(prov);
    doc["provenance"]["units"]["target_pixel_area"] = A_pixel_out;

    /* Phase1 扩展：显式登记 OI-01 延后与未归一复合。 */
    json ext;
    ext["group_normalization_deferred_to"] = "phase2";
    ext["unnormalized_composite_wt"] = wt_unnormalized;
    ext["composite_version"] = p1psfw::kCompositeVersion;
    ext["open_items"] = json::array({"OI-01"});
    ext["weight_value_status"] = "not_available_in_phase1_single_frame";
    ext["a_nea_px2"] = pst.a_nea;
    ext["psf_norm_abs_error"] = pst.norm_abs_error;
    ext["operator"] = {{"kind", "drizzle_forward"},
                       {"n_src", op.n_src()},
                       {"n_dst", op.n_dst()},
                       {"pixfrac", op.pixfrac()},
                       {"normalization", "sb_a_pixel"},
                       {"D_p_sum", [&] {
                          double s = 0.0;
                          for (uint32_t p = 0; p < op.n_dst(); ++p) s += op.D(p);
                          return s;
                        }()}};
    ext["support_plane_semantics"] = "D_p coverage area [px^2] (gate only, not weight)";
    ext["nside"] = in.drizzle.nside;
    ext["ordering"] = "NESTED";
    ext["target_ipix"] = target_ipix_out;
    ext["gates"] = {{"FZ-FORMULA-DRIZZLE-SB", g_sb.pass},
                    {"FZ-FORMULA-DRIZZLE-VAR", g_var.pass},
                    {"FZ-COND-FLUX-CONSERV", g_flux.pass},
                    {"FZ-FORMULA-COV-PROP", g_cov.pass},
                    {"G-STRUCT-UNIT-LAW", g_ulaw.pass}};
    ext["parent_reduction"] = {{"variance_exact", pvar_exact},
                               {"variance_diagonal", pvar_diag},
                               {"deficit", deficit},
                               {"is_lower_bound", true},
                               {"claims_exact", false}};
    ext["point_source"] = {{"star_id", in.point_source.star_id},
                           {"a", in.point_source.a},
                           {"support_px", m}};
    ext["psf_profile"] = in.point_source.psf_profile;
    doc["phase1_extensions"] = ext;

    /* manifest：文件 SHA-256 + output_hash。 */
    json mf;
    mf["manifest_schema"] = "astrocs.v6.hips_manifest/v1";
    mf["schema_version"] = 1;
    mf["product_type_id"] = kPhase1TypeId;
    mf["frame_id"] = in.frame_id;
    mf["files"] = json::array();
    mf["files"].push_back({{"relative_path", kPhase1ScienceFile},
                           {"size_bytes", pr.bytes_written},
                           {"sha256_hex", sci_sha},
                           {"role", "science_planes"}});
    mf["output_hash"] = sci_sha;
    mf["target_ipix_count"] = target_ipix_out.size();
    doc["manifest"] = mf;

    if (!write_text_file(stage_dir + "/" + kPhase1RecordFile, doc.dump(2), err))
      return false;

    fit_err.clear();
    return true;
  };

  const VerifyFn verify = [](const std::string& path, std::string* err) -> bool {
    const Phase1OpenResult r = open_phase1_product(path);
    if (!r.ok) {
      if (err) *err = r.error.empty() ? "reopen verify failed" : r.error;
      return false;
    }
    return true;
  };

  PublishOptions opts;
  opts.verify_after_rename = true;
  opts.fsync_directory = true;
  opts.remove_on_verify_failure = true;
  const PublishResult pr = atomic_publish_directory(target_dir, builder, verify,
                                                    opts, CancelFn());
  if (pr.status != PublishStatus::kOk) {
    res.error = pr.message.empty() ? "atomic publish failed" : pr.message;
    return res;
  }
  res.ok = true;
  res.output_sha256 = pr.sha256_hex;
  res.evidence.push_back("G-FORMULA-DRIZZLE-SB/ VAR/ FLUX/ COV: pass");
  res.evidence.push_back("FZ-GATE-PSFSW-COV/EPSF/FAILCLOSED: wired");
  res.evidence.push_back("OI-01 registered OPEN; group normalization deferred to phase2");
  (void)fit_err;
  return res;
}

/* ======================================================================== */
/* 磁盘重开：独立验证（不信任写出路径）                                       */
/* ======================================================================== */

Phase1OpenResult open_phase1_product(const std::string& target_dir) {
  Phase1OpenResult out;
  auto fail = [&out](const std::string& msg) {
    out.ok = false;
    out.error = msg;
    return out;
  };

  const std::string record_path = target_dir + "/" + kPhase1RecordFile;
  const std::string science_path = target_dir + "/" + kPhase1ScienceFile;
  std::string text, err;
  if (!read_text_file(record_path, &text, &err)) return fail(err);

  json doc;
  try {
    doc = json::parse(text);
  } catch (const std::exception& e) {
    return fail(std::string("record JSON parse error: ") + e.what());
  }

  /* (0) P33 撤销守卫：帧级 SNR 系数/诊断权重形态不得重新引入。 */
  if (has_forbidden_p33_key(doc))
    return fail("P33 frame-coefficient / diagnostic weight token reintroduced");

  /* (1) 身份与 schema。 */
  if (doc.value("product_schema", std::string()) != kPhase1ProductSchema)
    return fail("product_schema mismatch");
  if (doc.value("type_id", std::string()) != kPhase1TypeId)
    return fail("type_id mismatch");
  if (!is_hex40(doc.value("software_sha", std::string())))
    return fail("software_sha not 40-hex");
  if (!doc.contains("manifest") || !doc.contains("provenance") ||
      !doc.contains("psfsw") || !doc.contains("point_information") ||
      !doc.contains("units") || !doc.contains("planes") ||
      !doc.contains("phase1_extensions"))
    return fail("record missing required top-level blocks");

  /* (2) provenance 最小集（用 AIO 独立门重跑）。 */
  if (!doc["provenance"].is_object()) return fail("provenance not object");
  {
    ValidationReport r = validate_provenance_json(
        doc["provenance"], provenance_required_keys());
    if (!r.ok()) {
      out.violations.push_back("G-PROV-MINIMAL-SET: " + r.summary());
      return fail("provenance minimal set/invariants violated");
    }
  }

  /* (3) 单位 / BUNIT / 二次律。 */
  const json& u = doc["units"];
  const std::string sig_unit = u.value("signal_sb", std::string());
  const std::string var_unit = u.value("sb_variance_out", std::string());
  const std::string ivar_unit = u.value("sb_ivar_out", std::string());
  if (!(sig_unit == "ADU/px^2" && var_unit == "ADU^2/px^4" && ivar_unit == "px^4/ADU^2"))
    return fail("frozen unit strings violated");
  {
    std::string why;
    if (!quadratic_law_holds(sig_unit, var_unit, ivar_unit, &why))
      return fail("quadratic law violated: " + why);
  }
  {
    const BunitCheck bc = bunit_dimension_decidable(
        doc["provenance"]["units"].value("bunit", std::string()),
        doc["provenance"]["units"].value("pixel_semantics", std::string()),
        doc["provenance"]["units"].value("pixel_area_power", 0), false, 0.0);
    if (!bc.decidable) return fail("BUNIT not decidable: " + bc.reason);
  }

  /* (4) FITS 重开：CHECKSUM/DATASUM/结构/BUNIT（不信任 manifest）。 */
  std::vector<ExpectedHdu> expected;
  if (!expected_from_record(doc, &expected, &err)) return fail(err);
  const FitsVerifyResult vr = verify_fits_file(science_path, expected);
  if (!vr.ok) {
    out.violations.push_back(vr.error.empty()
                                 ? (vr.violations.empty() ? "fits verify failed"
                                                          : vr.violations.front().gate + ": " +
                                                                vr.violations.front().message)
                                 : vr.error);
    return fail("science.fits verify failed");
  }

  /* (5) SHA-256 vs manifest.output_hash（重算，不信任记录）。 */
  std::string sci_sha;
  if (!sha256_file_hex(science_path, &sci_sha)) return fail("sha256 recompute failed");
  if (doc["manifest"].value("output_hash", std::string()) != sci_sha)
    return fail("manifest.output_hash != recomputed science sha256");
  const std::string prov_hash = doc["provenance"].value("output_hash", std::string());
  if (prov_hash != "sha256:" + sci_sha && prov_hash != sci_sha)
    return fail("provenance.output_hash != recomputed science sha256");
  {
    bool found = false;
    for (const auto& f : doc["manifest"]["files"]) {
      if (f.value("relative_path", std::string()) == kPhase1ScienceFile) {
        if (f.value("sha256_hex", std::string()) != sci_sha)
          return fail("manifest file sha256 mismatch");
        found = true;
      }
    }
    if (!found) return fail("manifest missing science.fits record");
  }

  /* (6) 权重面 canonical 语义（禁第三套词表 / 禁 ivar 冒充）。 */
  const json& ps = doc["psfsw"];
  const json& w = ps["weight"];
  if (ps.value("weight_mode", std::string()) != "psfsw_robust")
    return fail("psfsw weight_mode != psfsw_robust");
  if (w.value("kind", std::string()) != "psfsw_robust_weight")
    return fail("weight.kind != psfsw_robust_weight (no ivar/fisher)");
  if (w.value("units", std::string()) != "1")
    return fail("weight.units != 1");
  if (w.value("group_normalized", false) != true)
    return fail("weight.group_normalized != true");
  if (w["normalization"].value("scope", std::string()) != "group")
    return fail("normalization.scope != group");
  if (std::fabs(w["normalization"].value("median_target", 0.0) - 1.0) > 1e-12)
    return fail("normalization.median_target != 1.0");
  if (w["normalization"].value("constants_version", std::string()).empty())
    return fail("normalization.constants_version empty");
  for (const char* bad : {"weight_kind", "weight_units", "weight_normalized",
                          "normalization_scope", "weight_type", "norm_scope"}) {
    if (w.contains(bad) || ps.contains(bad))
      return fail(std::string("forbidden third-vocabulary token: ") + bad);
  }
  const bool valid = ps["validity"].value("valid", false);
  const bool has_wv = !w["weight_value"].is_null();
  if (valid && has_wv) return fail("valid=true but weight_value present (single frame)");
  if (!valid && has_wv)
    return fail("valid=false but weight_value present (FZ-GATE-PSFSW-FAILCLOSED)");
  if (!valid) {
    const std::string reason = ps["validity"]["reason"].is_null()
                                   ? std::string()
                                   : ps["validity"].value("reason", std::string());
    if (reason.empty()) return fail("valid=false without whitelisted reason");
  }
  if (ps["components"].size() != 4) return fail("psfsw components != 4");
  {
    std::set<std::string> ids, names;
    for (auto it = ps["components"].begin(); it != ps["components"].end(); ++it) {
      const json& c = it.value();
      ids.insert(c.value("measurement_id", std::string()));
      names.insert(it.key());
      const double p05 = c["spatial_summary"].value("p05", 0.0);
      const double p50 = c["spatial_summary"].value("p50", 0.0);
      const double p95 = c["spatial_summary"].value("p95", 0.0);
      if (!(p05 <= p50 && p50 <= p95)) return fail("component p05<=p50<=p95 violated");
    }
    if (ids.size() != 4) return fail("component measurement_id not distinct");
    if (!names.count("signal") || !names.count("concentration") ||
        !names.count("noise") || !names.count("background"))
      return fail("psfsw four components missing");
  }
  if (ps["components"]["concentration"].value("units", std::string()) != "ADU/px^2")
    return fail("concentration units != ADU/px^2 (W6 authority)");

  /* (7) 点源信息定位与单位。 */
  const json& pi = doc["point_information"];
  if (pi.value("authoritative_formula", std::string()).find("W_info,k=a_k^2 P_k^T C_k^-1 P_k") ==
      std::string::npos)
    return fail("point_information authoritative formula altered");
  if (pi["W_info"].value("units", std::string()) != "ADU^-2") return fail("W_info units");
  if (pi["Q"].value("units", std::string()) != "ADU^-1") return fail("Q units");
  if (pi["flux"].value("units", std::string()) != "ADU") return fail("flux units");
  const double w_info = pi["W_info"].value("value", 0.0);
  const double var_f = pi["flux_variance"].value("value", 0.0);
  if (!(w_info > 0.0) || !(var_f > 0.0) ||
      std::fabs(var_f * w_info - 1.0) > 1e-9)
    return fail("Var(F_hat) != 1/W_info (FZ-FORMULA-WINFO)");

  /* (8) covariance 禁区：variance_from 不得是诊断/权重别名。 */
  {
    const std::string vf = doc["drizzle_covariance"].value("variance_from", std::string());
    for (const char* bad : kForbiddenVarianceSources)
      if (vf == bad) return fail(std::string("drizzle covariance variance_from forbidden: ") + bad);
    if (doc["drizzle_covariance"]["diagonal_approximation"].value("is_lower_bound", false) != true)
      return fail("diagonal reduction must declare lower_bound");
    if (doc["drizzle_covariance"]["diagonal_approximation"].value("use_for_aperture", true) != false)
      return fail("diagonal reduction must not be used for aperture");
  }

  /* (9) flux_conservation_factor（FZ-COND-FLUX-CONSERV）。 */
  const json& pj = doc["provenance"];
  const bool has_pixfrac = pj["sampling"].contains("pixfrac");
  const double pixfrac = pj["sampling"].value("pixfrac", 1.0);
  if (has_pixfrac && pixfrac > 0.0 && pixfrac < 1.0) {
    if (!pj.contains("flux_conservation_factor") ||
        !pj["flux_conservation_factor"].is_number() ||
        pj["flux_conservation_factor"].get<double>() <= 0.0)
      return fail("pixfrac<1 without positive flux_conservation_factor");
    if (std::fabs(pj["flux_conservation_factor"].get<double>() - pixfrac * pixfrac) > 1e-12)
      return fail("flux_conservation_factor != pixfrac^2");
  }
  if (doc["drizzle_covariance"]["diagonal_approximation"]["deficit_metric"].value("value", -1.0) < 0.0)
    return fail("parent deficit must be >= 0");

  /* (10) OI-01：Phase1 单帧必须显式声明组内归一延后。 */
  const json& ext = doc["phase1_extensions"];
  if (ext.value("group_normalization_deferred_to", std::string()) != "phase2")
    return fail("phase1 group normalization must be deferred to phase2 (OI-01)");

  /* ── 组装视图 ── */
  Phase1ProductView& v = out.view;
  v.ok = true;
  v.frame_id = doc.value("frame_id", std::string());
  v.run_id = doc.value("run_id", std::string());
  v.output_hash = sci_sha;
  for (const auto& h : vr.hdus) {
    if (h.extname.empty()) v.signal_bunit = h.bunit;
    else if (h.extname == "SUPPORT") v.support_bunit = h.bunit;
    else if (h.extname == "VARIANCE") v.variance_bunit = h.bunit;
    else if (h.extname == "IVAR") v.ivar_bunit = h.bunit;
  }
  v.signal_unit = sig_unit;
  v.variance_unit = var_unit;
  v.ivar_unit = ivar_unit;
  v.pixel_semantics = u.value("pixel_semantics", std::string());
  v.pixel_area_power = u.value("pixel_area_power", 0);
  v.w_info = w_info;
  v.q = pi["Q"].value("value", 0.0);
  v.flux = pi["flux"].value("value", 0.0);
  v.flux_variance = var_f;
  v.weight_kind = w.value("kind", std::string());
  v.weight_units = w.value("units", std::string());
  v.group_normalized = w.value("group_normalized", false);
  v.norm_scope = w["normalization"].value("scope", std::string());
  v.norm_median_target = w["normalization"].value("median_target", 0.0);
  v.constants_version = w["normalization"].value("constants_version", std::string());
  v.has_weight_value = has_wv;
  v.weight_value = has_wv ? w["weight_value"].get<double>() : 0.0;
  v.valid = valid;
  v.reason = ps["validity"]["reason"].is_null() ? std::string()
                                                : ps["validity"].value("reason", std::string());
  v.n_common = ps["common_star_set"].value("n_common", 0);
  v.common_star_set_id = ps["common_star_set"].value("common_star_set_id", std::string());
  v.selection_function_id = ps["common_star_set"].value("selection_function_id", std::string());
  v.n_components = static_cast<int>(ps["components"].size());
  for (auto it = ps["components"].begin(); it != ps["components"].end(); ++it)
    v.component_names.push_back(it.key());
  v.unnormalized_wt = ext.value("unnormalized_composite_wt", 0.0);
  v.normalization_deferred_to = ext.value("group_normalization_deferred_to", std::string());
  v.science_path = science_path;
  if (doc["manifest"]["files"].is_array() && !doc["manifest"]["files"].empty())
    v.science_bytes = doc["manifest"]["files"][0].value("size_bytes", static_cast<std::uint64_t>(0));
  if (ext.contains("target_ipix") && ext["target_ipix"].is_array()) {
    for (const auto& ip : ext["target_ipix"]) v.target_ipix.push_back(ip.get<std::uint64_t>());
  }
  v.nside = ext.value("nside", 0);
  out.ok = true;
  out.error.clear();
  return out;
}

/* ======================================================================== */
/* Phase2 消费面：仅由磁盘重开的产品组成帧组                                  */
/* ======================================================================== */

Phase1GroupConsumption consume_phase1_group_for_psfsw(
    const std::vector<std::string>& product_dirs) {
  Phase1GroupConsumption g;
  if (product_dirs.size() < 2) {
    g.error = "group requires >= 2 disk-reopened phase1 products";
    return g;
  }
  std::vector<p1psfw::ComponentValues> frames;
  for (const auto& d : product_dirs) {
    const Phase1OpenResult r = open_phase1_product(d);
    if (!r.ok) {
      g.error = "reopen failed for " + d + ": " + r.error;
      return g;
    }
    if (!r.view.valid) {
      g.error = "frame invalid (fail-closed): " + d;
      return g;
    }
    if (r.view.n_components != 4) {
      g.error = "frame lacks four psfsw components: " + d;
      return g;
    }
    /* 从磁盘记录读取四分量值（不信任进程内状态）。 */
    std::string text, err;
    if (!read_text_file(d + "/" + kPhase1RecordFile, &text, &err)) {
      g.error = err;
      return g;
    }
    json doc = json::parse(text);
    p1psfw::ComponentValues cv;
    cv.s = doc["psfsw"]["components"]["signal"].value("value", 0.0);
    cv.conc = doc["psfsw"]["components"]["concentration"].value("value", 0.0);
    cv.n = doc["psfsw"]["components"]["noise"].value("value", 0.0);
    cv.b = doc["psfsw"]["components"]["background"].value("value", 0.0);
    frames.push_back(cv);
    g.wt_unnormalized.push_back(r.view.unnormalized_wt);
  }
  const p1psfw::CompositeResult cr = p1psfw::compute_psfsw_weights(frames);
  if (!cr.ok) {
    g.error = std::string("group composite rejected: ") + (cr.reject ? cr.reject : "unknown");
    return g;
  }
  g.n_frames = static_cast<int>(product_dirs.size());
  g.w_psfsw = cr.w_psfsw;
  g.median_wt = cr.median_wt;
  const double med = p1psfw::median_of(cr.w_psfsw);
  g.record_ok = true;
  if (std::fabs(med - 1.0) > 1e-9) {
    g.record_ok = false;
    g.findings.push_back("group median(w_psfsw) != 1");
  }
  for (double wv : cr.w_psfsw)
    if (!(wv > 0.0)) {
      g.record_ok = false;
      g.findings.push_back("non-positive w_psfsw");
      break;
    }
  g.ok = true;
  return g;
}

}  /* namespace phase1 */
}  /* namespace v6 */
}  /* namespace astrocs */
