// lib/phase3_rsmp/p3_rsmp_propagation.cpp
// 三模式传播与输出帧 Q/W 重算（ALG-P3-002/004/005/006/007）。
#include "p3_rsmp.h"

#include <algorithm>
#include <cmath>

namespace astrocs {
namespace p3rsmp {

namespace {
GateResult aggregate(const std::vector<GateResult>& all) {
  if (all.empty()) return GateResult{Status::Ok, "", ""};
  // Reject 优先于 Unavailable；保持 fail-closed 语义。
  for (const GateResult& g : all) {
    if (g.status == Status::Reject) return g;
  }
  return all.front();
}

ProductRecord base_record(P3Mode mode) {
  ProductRecord rec;
  rec.mode = mode;
  rec.mode_declared = true;
  return rec;
}
}  // namespace

VisualizationResult propagate_visualization(const VisualizationInput& in) {
  ProductRecord rec = base_record(P3Mode::Visualization);
  rec.measurement_capable = in.measurement_capable;
  rec.writes_variance = in.writes_variance;
  rec.writes_ivar = in.writes_ivar;
  rec.writes_point_information = in.writes_point_information;
  VisualizationResult out;
  const std::vector<GateResult> all = check_failclosed_all(rec, GateConfig{});
  const GateResult g = aggregate(all);
  out.status = g.status;
  out.code = g.code;
  out.reason = g.reason;
  return out;
}

SurfaceBrightnessResult propagate_surface_brightness(const SparseOperator& r_op,
                                                     const SurfaceBrightnessInput& in,
                                                     const GateConfig& cfg) {
  SurfaceBrightnessResult out;
  ProductRecord rec = base_record(P3Mode::SurfaceBrightness);
  rec.flux_conversion = in.flux_conversion_requested;
  rec.omega_per_pixel = in.omega_per_pixel_present;
  rec.measurement_capable = in.measurement_capable;
  rec.uncertainty_available = in.uncertainty_available;
  rec.uncertainty_unavailable_reason = in.uncertainty_unavailable_reason;
  rec.signal_bunit = units::signal_sb;
  rec.variance_bunit = units::sb_variance_out;
  rec.ivar_bunit = units::sb_ivar_out;
  rec.bunit_quadratic_law_ok =
      is_quadratic_variance(rec.signal_bunit, rec.variance_bunit) &&
      is_inverse_pair(rec.variance_bunit, rec.ivar_bunit);
  rec.covariance = in.covariance;
  rec.variance_diagonal_only = (in.covariance == CovarianceRepresentation::DiagonalOnly);
  rec.correlation_kernel_present =
      (in.covariance == CovarianceRepresentation::ExactFull ||
       in.covariance == CovarianceRepresentation::ExactCorrelationKernel);
  rec.correlation_approx_error_available = in.correlation_approx_error_available;
  rec.correlation_approx_error = in.correlation_approx_error;
  rec.variance_from = in.variance_from;
  rec.weight_sources = in.weight_sources;
  rec.uses_relative_weight_as_ivar = in.uses_relative_weight_as_ivar;
  rec.variance_from_weight =
      (in.variance_from == "weight" || in.variance_from == "psfsw_robust_weight");
  rec.provenance_complete = in.provenance_complete;
  rec.provenance_missing_keys = in.provenance_missing_keys;

  const GateResult g = aggregate(check_failclosed_all(rec, cfg));
  if (!g.ok()) {
    out.status = g.status;
    out.code = g.code;
    out.reason = g.reason;
    return out;
  }

  if (!r_op.row_normalized) {
    out.status = Status::Reject;
    out.code = "G-P3-SB-00";
    out.reason = "operator_is_not_row_normalized";
    return out;
  }
  if (static_cast<int>(in.x.size()) != r_op.n_in || in.c_in.rows != r_op.n_in ||
      in.c_in.cols != r_op.n_in) {
    out.status = Status::InvalidArgument;
    out.code = "G-P3-SB-DIM";
    out.reason = "input_dimension_mismatch";
    return out;
  }

  out.y = apply_operator(r_op, in.x);
  out.c_y = propagate_covariance(r_op, in.c_in);
  if (out.c_y.rows != r_op.n_out || out.c_y.cols != r_op.n_out) {
    out.status = Status::Reject;
    out.code = "G-P3-COV-00";
    out.reason = "covariance_propagation_failed";
    return out;
  }
  out.variance = matrix_diagonal(out.c_y);
  out.valid = r_op.valid_out;
  out.coverage = r_op.coverage;
  out.status = Status::Ok;
  return out;
}

PointSourceResult propagate_point_source_flux(const PointSourceInput& in, const GateConfig& cfg) {
  PointSourceResult out;
  ProductRecord rec = base_record(P3Mode::PointSourceFlux);
  rec.psf_present = in.psf_present;
  double psf_sum = 0.0;
  for (double p : in.psf_p) psf_sum += p;
  rec.psf_sum = in.psf_p.empty() ? kNaN : psf_sum;
  rec.point_information_present = in.point_information_present;
  rec.rebuildable_from_frames = in.rebuildable_from_frames;
  rec.photometric_scale_present = in.photometric_scale_present;
  rec.photometric_scale = in.a;
  rec.effective_psf_present = in.effective_psf_present;
  rec.effective_psf_fwhm_only = in.effective_psf_fwhm_only;
  rec.effective_psf_normalization_declared = in.effective_psf_normalization_declared;
  rec.input_qw_resampled = in.input_qw_resampled;
  rec.w_from_sum_input = in.w_from_sum_input;
  rec.upstream_w_recomputed = in.upstream_w_recomputed;
  rec.covariance = in.covariance;
  rec.variance_diagonal_only = (in.covariance == CovarianceRepresentation::DiagonalOnly);
  rec.correlation_kernel_present =
      (in.covariance == CovarianceRepresentation::ExactFull ||
       in.covariance == CovarianceRepresentation::ExactCorrelationKernel);
  rec.correlation_approx_error_available = in.correlation_approx_error_available;
  rec.optimism_audit_performed = in.optimism_audit_performed;
  rec.diagonal_optimism_detected = in.diagonal_optimism_detected;
  rec.variance_from = in.variance_from;
  rec.weight_sources = in.weight_sources;
  rec.uses_relative_weight_as_ivar = in.uses_relative_weight_as_ivar;
  rec.variance_from_weight = in.variance_from_weight;
  rec.provenance_complete = in.provenance_complete;
  rec.provenance_missing_keys = in.provenance_missing_keys;

  const GateResult g = aggregate(check_failclosed_all(rec, cfg));
  if (!g.ok()) {
    out.status = g.status;
    out.code = g.code;
    out.reason = g.reason;
    return out;
  }

  const SparseOperator& s = in.s_op;
  if (!s.column_normalized) {
    out.status = Status::Reject;
    out.code = "G-P3-QW-00";
    out.reason = "operator_is_not_column_normalized";
    return out;
  }
  if (static_cast<int>(in.x.size()) != s.n_in || in.c_x.rows != s.n_in ||
      in.c_x.cols != s.n_in || static_cast<int>(in.psf_p.size()) != s.n_in) {
    out.status = Status::InvalidArgument;
    out.code = "G-P3-QW-DIM";
    out.reason = "input_dimension_mismatch";
    return out;
  }
  for (int i = 0; i < s.n_out; ++i) {
    if (!s.valid_out[static_cast<std::size_t>(i)]) {
      out.status = Status::Reject;
      out.code = "G-P3-QW-FRAME";
      out.reason = "output_frame_has_invalid_pixels_missing_tile";
      return out;
    }
  }

  const int ni = s.n_in;
  // d_j = x_j * Omega_j（SB → 积分通量）；C_d = diag(Omega) C_x diag(Omega)
  std::vector<double> d(static_cast<std::size_t>(ni), 0.0);
  DenseMatrix c_d;
  if (in.x_is_surface_brightness) {
    if (static_cast<int>(in.geom.omega_in_sr.size()) != ni) {
      out.status = Status::InvalidArgument;
      out.code = "G-P3-QW-DIM";
      out.reason = "omega_in_size_mismatch";
      return out;
    }
    for (int j = 0; j < ni; ++j) {
      d[static_cast<std::size_t>(j)] = in.x[static_cast<std::size_t>(j)] *
                                       in.geom.omega_in_sr[static_cast<std::size_t>(j)];
    }
    c_d = DenseMatrix(ni, ni);
    for (int r = 0; r < ni; ++r) {
      for (int c = 0; c < ni; ++c) {
        c_d(r, c) = in.geom.omega_in_sr[static_cast<std::size_t>(r)] * in.c_x(r, c) *
                    in.geom.omega_in_sr[static_cast<std::size_t>(c)];
      }
    }
  } else {
    d = in.x;
    c_d = in.c_x;
  }

  out.f = apply_operator(s, d);
  out.pi = apply_operator(s, in.psf_p);
  double pi_sum = 0.0;
  for (double p : out.pi) pi_sum += p;
  if (!(std::fabs(pi_sum - 1.0) <= cfg.psf_sum_tol)) {
    out.status = Status::Reject;
    out.code = "G-P3-PSF-02";
    out.reason = "effective_psf_not_normalised_sum_ne_1";
    return out;
  }
  out.c_y = propagate_covariance(s, c_d);
  if (out.c_y.rows != s.n_out || out.c_y.cols != s.n_out) {
    out.status = Status::Reject;
    out.code = "G-P3-COV-00";
    out.reason = "covariance_propagation_failed";
    return out;
  }

  std::vector<double> z;
  std::string why;
  Status st = cholesky_solve(out.c_y, out.pi, &z, &why);
  if (st != Status::Ok) {
    out.status = st;
    out.code = "G-P3-QW-04";
    out.reason = "full_c_y_solve_failed:" + why;
    return out;
  }
  std::vector<double> ysol;
  st = cholesky_solve(out.c_y, out.f, &ysol, &why);
  if (st != Status::Ok) {
    out.status = st;
    out.code = "G-P3-QW-04";
    out.reason = "full_c_y_solve_failed:" + why;
    return out;
  }
  double pi_cinv_pi = 0.0;
  double pi_cinv_f = 0.0;
  for (int i = 0; i < s.n_out; ++i) {
    pi_cinv_pi += out.pi[static_cast<std::size_t>(i)] * z[static_cast<std::size_t>(i)];
    pi_cinv_f += out.pi[static_cast<std::size_t>(i)] * ysol[static_cast<std::size_t>(i)];
  }
  out.W = in.a * in.a * pi_cinv_pi;
  out.Q = in.a * pi_cinv_f;
  out.F_hat = (out.W != 0.0) ? (out.Q / out.W) : kNaN;
  out.var_F_hat = (out.W != 0.0) ? (1.0 / out.W) : kNaN;
  out.frame_is_output_recompute = true;
  out.status = Status::Ok;
  return out;
}

}  // namespace p3rsmp
}  // namespace astrocs
