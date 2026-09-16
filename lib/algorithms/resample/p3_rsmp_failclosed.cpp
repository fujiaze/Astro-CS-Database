// lib/algorithms/resample/p3_rsmp_failclosed.cpp
// 三模式 fail-closed 12 门 + kernel/covariance/QW/epsf/provenance 扩展门。
// 冻结锚：FZ-P3-FAILCLOSED、FZ-P3-QW-RECOMPUTE、FZ-P3-KERNEL-REGISTRY、
//         FZ-P3-BUNIT-QUADRATIC、FZ-FORMULA-COV-PROP、FZ-GATE-MEDIAN-SNR、
//         FZ-GATE-SUPPORT-COVERAGE、FZ-GATE-PSFSW-COV、FZ-PROV-MINIMAL-SET。
#include "p3_rsmp.h"

#include <algorithm>

namespace astrocs {
namespace p3rsmp {

const std::vector<std::string>& forbidden_weight_source_tokens() {
  static const std::vector<std::string> v = {
      "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
      "support",           "support_area", "coverage",          "coverage_area",
      "fwhm",              "psf_fwhm",     "median_fwhm",       "source_fwhm",
      "residual",          "psf_residual", "psf_fit_residual",  "fit_residual",
      "psfsw_robust_weight", "psfsw",      "weight",
  };
  return v;
}

const std::vector<std::string>& forbidden_psfsw_product_keys() {
  // FZ-GATE-PSFSW-COV §3（任何层命中即 REJECT）+ 扩展守卫。
  static const std::vector<std::string> v = {
      "ivar", "inverse_variance", "variance", "var", "sigma", "sigma2",
      "fisher", "fisher_information", "information", "w_info", "w_psf",
      "snr", "snr2", "support", "coverage",
  };
  return v;
}

bool token_is_forbidden_weight_source(const std::string& token) {
  const auto& v = forbidden_weight_source_tokens();
  return std::find(v.begin(), v.end(), token) != v.end();
}

namespace {
void add_gate(std::vector<GateResult>* out, Status s, const char* code, std::string reason) {
  out->push_back(GateResult{s, code, std::move(reason)});
}

bool contains_forbidden(const std::vector<std::string>& sources, std::string* hit) {
  for (const std::string& s : sources) {
    if (token_is_forbidden_weight_source(s)) {
      if (hit) *hit = s;
      return true;
    }
  }
  return false;
}
}  // namespace

std::vector<GateResult> check_failclosed_all(const ProductRecord& rec, const GateConfig& cfg) {
  std::vector<GateResult> v;

  // --- 模式声明（FZ-P3-MODES）---
  if (!rec.mode_declared) {
    add_gate(&v, Status::Reject, "G-P3-MODE-01", "phase3_mode_not_declared");
  }

  // --- 全局：上游相对复合权重不得写成 ivar/variance（G-P3-GLB-01）---
  {
    std::string hit;
    const bool ws_forbidden = contains_forbidden(rec.weight_sources, &hit);
    const bool vf_forbidden =
        !rec.variance_from.empty() && token_is_forbidden_weight_source(rec.variance_from);
    if (ws_forbidden || vf_forbidden || rec.uses_relative_weight_as_ivar ||
        rec.variance_from_weight) {
      std::string why = "relative_composite_weight_used_as_variance";
      if (ws_forbidden) why += ":weight_sources=" + hit;
      if (vf_forbidden) why += ":variance_from=" + rec.variance_from;
      if (rec.uses_relative_weight_as_ivar) why += ":uses_relative_weight_as_ivar";
      if (rec.variance_from_weight) why += ":variance_from_weight";
      add_gate(&v, Status::Reject, "G-P3-GLB-01", why);
    }
  }

  // --- surface_brightness 4 门 ---
  if (rec.mode == P3Mode::SurfaceBrightness) {
    if (rec.flux_conversion && !rec.omega_per_pixel) {
      add_gate(&v, Status::Reject, "G-P3-SB-01",
               "surface_brightness_flux_conversion_without_per_pixel_omega");
    }
    if (rec.measurement_capable && !rec.uncertainty_available &&
        rec.uncertainty_unavailable_reason.empty()) {
      add_gate(&v, Status::Reject, "G-P3-SB-02",
               "measurement_declared_but_uncertainty_unavailable_without_reason");
    }
    if (!rec.bunit_quadratic_law_ok ||
        !is_quadratic_variance(rec.signal_bunit, rec.variance_bunit) ||
        !is_inverse_pair(rec.variance_bunit, rec.ivar_bunit)) {
      add_gate(&v, Status::Reject, "G-P3-SB-03",
               "variance_bunit_not_signal_bunit_squared_or_ivar_not_inverse");
    }
    if (rec.variance_diagonal_only && !rec.correlation_kernel_present) {
      add_gate(&v, Status::Reject, "G-P3-SB-04",
               "diagonal_only_variance_without_correlation_kernel");
    }
  }

  // --- point_source_flux 6 门 ---
  if (rec.mode == P3Mode::PointSourceFlux) {
    if (!rec.psf_present) {
      add_gate(&v, Status::Reject, "G-P3-PSF-01", "point_source_flux_missing_psf");
    } else if (!(std::fabs(rec.psf_sum - 1.0) <= cfg.psf_sum_tol)) {
      add_gate(&v, Status::Reject, "G-P3-PSF-02",
               "psf_not_normalised_sum_ne_1");
    }
    if (!rec.point_information_present && !rec.rebuildable_from_frames) {
      add_gate(&v, Status::Reject, "G-P3-PSF-03",
               "missing_point_information_and_not_rebuildable");
    }
    if (rec.variance_diagonal_only && !rec.correlation_kernel_present) {
      add_gate(&v, Status::Reject, "G-P3-PSF-04",
               "diagonal_only_covariance_without_correlation_kernel");
    }
    if (!rec.photometric_scale_present || !(rec.photometric_scale > 0.0) ||
        std::isnan(rec.photometric_scale)) {
      add_gate(&v, Status::Reject, "G-P3-PSF-05", "missing_photometric_scale_a");
    }
    if (!rec.effective_psf_present || rec.effective_psf_fwhm_only) {
      add_gate(&v, Status::Reject, "G-P3-PSF-06",
               "effective_psf_missing_or_fwhm_scalar_only");
    }
  }

  // --- visualization（G-P3-VIS-01）---
  if (rec.mode == P3Mode::Visualization) {
    if (rec.measurement_capable || rec.writes_variance || rec.writes_ivar ||
        rec.writes_point_information) {
      add_gate(&v, Status::Reject, "G-P3-VIS-01",
               "visualization_declared_measurement_or_wrote_measurement_layer");
    }
  }

  // --- 扩展门：covariance ---
  if (rec.variance_diagonal_only && !rec.correlation_kernel_present &&
      !rec.correlation_approx_error_available) {
    add_gate(&v, Status::Reject, "G-P3-COV-01",
             "diagonal_only_without_correlation_kernel_or_approximation_error");
  }
  if (rec.covariance == CovarianceRepresentation::WeightDerived) {
    add_gate(&v, Status::Reject, "G-P3-COV-02",
             "covariance_derived_from_weight_scalar");
  }
  if (rec.uses_relative_weight_as_ivar || rec.variance_from_weight) {
    add_gate(&v, Status::Reject, "G-P3-COV-03",
             "relative_weight_used_as_ivar");
  }
  if (rec.covariance == CovarianceRepresentation::ApproximateCorrelation &&
      !cfg.epsilon_corr_ratified) {
    // CF-T-P3-CORR-EPSILON 为 OPEN（SO-07）：未签字前该面 fail-closed（不得自定值）。
    add_gate(&v, Status::Unavailable, "CF-T-P3-CORR-EPSILON",
             "epsilon_corr_open_pending_owner_signoff");
  }
  if (rec.correlation_approx_error_available && !cfg.epsilon_corr_ratified) {
    add_gate(&v, Status::Unavailable, "CF-T-P3-CORR-EPSILON",
             "correlation_approximation_error_threshold_open");
  }

  // --- 扩展门：Q/W 输出帧（FZ-P3-QW-RECOMPUTE）---
  if (rec.input_qw_resampled) {
    add_gate(&v, Status::Reject, "G-P3-QW-01", "input_qw_resampled_forbidden");
  }
  if (rec.w_from_sum_input) {
    add_gate(&v, Status::Reject, "G-P3-QW-02", "w_equals_sum_input_w_forbidden");
  }
  if (rec.upstream_w_recomputed) {
    add_gate(&v, Status::Reject, "G-P3-QW-03",
             "upstream_w_info_recomputed_or_replaced_forbidden");
  }
  if (rec.covariance == CovarianceRepresentation::DiagonalOnly &&
      (!rec.optimism_audit_performed || rec.diagonal_optimism_detected)) {
    add_gate(&v, Status::Reject, "G-P3-QW-04",
             "diagonalisation_optimism_not_detected_or_detected_invalid_product");
  }

  // --- 扩展门：effective PSF 标量冒充 ---
  if (rec.effective_psf_fwhm_only) {
    add_gate(&v, Status::Reject, "G-P3-EPSF-01",
             "fwhm_scalar_does_not_constitute_effective_psf");
  }

  // --- provenance 最小集（FZ-PROV-MINIMAL-SET）---
  if (!rec.provenance_complete || !rec.provenance_missing_keys.empty()) {
    std::string why = "provenance_minimal_set_incomplete";
    for (const std::string& k : rec.provenance_missing_keys) {
      why += ":" + k;
    }
    add_gate(&v, Status::Reject, "G-P3-PROV-01", why);
  }

  return v;
}

GateResult check_failclosed(const ProductRecord& rec, const GateConfig& cfg) {
  const std::vector<GateResult> all = check_failclosed_all(rec, cfg);
  if (all.empty()) return GateResult{Status::Ok, "", ""};
  return all.front();
}

const char* to_string(CovarianceRepresentation v) {
  switch (v) {
    case CovarianceRepresentation::ExactFull: return "exact_full";
    case CovarianceRepresentation::ExactCorrelationKernel: return "exact_correlation_kernel";
    case CovarianceRepresentation::ApproximateCorrelation: return "approximate_correlation";
    case CovarianceRepresentation::DiagonalOnly: return "diagonal_only";
    case CovarianceRepresentation::WeightDerived: return "weight_derived";
  }
  return "unknown";
}

}  // namespace p3rsmp
}  // namespace astrocs
