// measure_science_sources: the measurement of the science sources.
//
// The sources are selected independently from the catalogue predictions; the
// solver detections are never reused as science sources.  The identity of a
// source is frozen after the saturation, neighbour and window masks have been
// applied, and only then the source is fitted.
//
// The template is the pixel integral of the PSF produced by evaluate_psf; the
// flux and the background are a linear problem for a frozen template.  The
// numerical bounds of the flux are constructed from the interval of the
// native input, the interval of the template and the floating point backward
// error of the solve; all three enter the same design matrix.  The data
// residual is a statistical quantity (D^+ r = 0 identically) and is never
// added to the deterministic radius.  When the rank is not certified or the
// enclosure cannot be certified, the nominal flux is kept and the state says
// so; a random variance is never used as a deterministic error.
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "observation/observation.hpp"
#include "operators/linear_action.hpp"
#include "sources/psf_internal.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "measure_science_sources";

using numerics::Matrix;

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is missing", kOrigin,
                                            "the array cannot be read");
  }
  // The block request is a flattened element range, so the count is every
  // element of the array and not the first dimension: a [rows, sources]
  // template is read whole.
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  if (array->shape.empty()) count = 0;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the array cannot be read", kOrigin,
                                            "the array cannot be read");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<ArrayRef> write_f64_array(Context& context, const std::vector<double>& values,
                                  std::span<const U64> shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

Index make_index(U64 size, const Id& namespace_id) {
  Index index;
  index.size = size;
  index.kind = Index::Kind::native;
  index.namespace_id = namespace_id;
  index.identity.object_id = "index:" + namespace_id;
  index.identity.content_hash = "sha256:" + std::string(64, '0');
  index.identity.schema_id = "acsd.Index";
  index.identity.schema_major = 2;
  index.identity.schema_minor = 0;
  index.assessment.complete = true;
  index.assessment.execution = ExecutionState::complete;
  return index;
}

// The native samples of the ancestor of the observation and the transfer from
// those samples to the current output samples.
void psf_transfer(Context& context, Ref<NativeObservation> observation,
                  Ref<FootprintSet>& detector_samples, Ref<LinearAction>& transfer) {
  detector_samples = observation->footprints;
  transfer = observation->regrouping;
  Ref<NativeObservation> node = observation->native_parent;
  while (node) {
    Result<Ref<LinearAction>> chained =
        acsd::compose(context, node->regrouping, transfer);
    if (!ok(chained)) return;
    transfer = val(chained);
    detector_samples = node->footprints;
    node = node->native_parent;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// The numerical enclosure of the linear flux solve.
//
// This is the single implementation of the radius; it is not static because
// the standalone probe calls it (the declaration lives in the probe, the
// definition is the production one).  The returned vector holds one radius per
// flux component; an empty vector means the enclosure is NOT certified.
//
// Perturbation.  The solve is the column scaled one, theta = D^+ d with
// D^+ = S^-1 (D S^-1)^+, so
//   d theta = D^+ (d d - dD theta).
// Every flux component uses the 2-norm of its own row of D^+, which already
// carries the column scaling S^-1 (the audit counterexample C2 is short by
// exactly that factor if it is dropped).  The design perturbation is bounded in
// the Frobenius norm and the non-linearity of the inverse is paid by
// 1/(1 - ||D^+||_2 ||dD||_F).
//
// Two error sources enter:
//   * the declared intervals of the native data and of the template
//     (data_norm, design_norm);
//   * the floating point error of the solve itself.  The computed solution is
//     the exact least squares solution of a perturbed (D+dD, d+dd) with
//     ||dd||_2 <= gamma ||d||_2 and ||dD||_F <= gamma ||D||_F, where
//     gamma = m eps / (1 - m eps) for m rows (Higham, backward error of the
//     least squares solve).  It is therefore bounded by the same formula once
//     those perturbations are added to the declared ones.
//
// The DATA RESIDUAL r = d - D theta is NOT an error term: for the exact least
// squares solution D^T r = 0 and D^+ r = 0 identically, so r contributes
// nothing to the first order perturbation of theta.  Its norm is a statistical
// quantity (noise plus model mismatch, of order sqrt(m) sigma) and putting it
// into a deterministic radius turns a numerical enclosure into a statistical
// interval while still calling it "numerical" (this was defect R1).  The
// normal equation residual ||D^T r|| is reported as a diagnostic: it is the
// floating point defect of the solve and is of order eps ||d||, which the
// gamma bound above already covers.
// ---------------------------------------------------------------------------
std::vector<double> measure_flux_numerical_radius(
    const numerics::Matrix& design, std::size_t sources, const std::vector<double>& measurement,
    const std::vector<double>& solution, const std::vector<double>& row_norm,
    const numerics::SvdResult& svd, const std::vector<double>& column_scale, double data_norm,
    double design_norm, double& amplification, double& spectral_norm,
    double& normal_residual_norm) {
  std::vector<double> radius;
  amplification = 0.0;
  spectral_norm = 0.0;
  normal_residual_norm = 0.0;
  const std::size_t rows = design.rows;
  const std::size_t parameters = design.cols;
  if (rows == 0 || parameters == 0 || sources == 0 || sources > parameters) return radius;
  if (solution.size() != parameters || column_scale.size() != parameters) return radius;
  if (row_norm.size() < sources || measurement.size() != rows) return radius;
  if (svd.rank != parameters || svd.singular_values.size() < parameters) return radius;
  if (!std::isfinite(data_norm) || !std::isfinite(design_norm) || data_norm < 0.0 ||
      design_norm < 0.0) {
    return radius;
  }
  const double eps = std::numeric_limits<double>::epsilon();
  // The rows of the design are the samples of the frame; the backward error
  // constant is the one of an m x p least squares solve.
  const double gamma = (static_cast<double>(rows) * eps) /
                       (1.0 - static_cast<double>(rows) * eps);

  // The spectral norm of D^+.  With D S^-1 = U Sigma V^T,
  //   ||D^+||_2 = ||S^-1 V Sigma^-1 U^T||_2 = ||S^-1 V Sigma^-1||_2
  // because U has orthonormal columns.  The p x p matrix M = S^-1 V Sigma^-1
  // is small (p = sources + background), so its largest singular value is
  // computed; the analytic bound sqrt(p) max_s ||e_s^T D^+||_2 is an upper
  // bound as well, so taking the minimum keeps the certificate valid even if
  // the small SVD is slightly off.
  numerics::Matrix scaled_right(parameters, parameters);
  for (std::size_t c = 0; c < parameters; ++c) {
    const double scale = column_scale[c];
    if (!(scale > 0.0)) continue;
    for (std::size_t i = 0; i < parameters; ++i) {
      const double sigma = svd.singular_values[i];
      if (!(sigma > 0.0)) continue;
      scaled_right(c, i) = svd.v(c, i) / sigma / scale;
    }
  }
  const numerics::SvdResult right_svd = numerics::jacobi_svd(scaled_right, 1.0e-12);
  double svd_norm = 0.0;
  if (!right_svd.singular_values.empty()) svd_norm = right_svd.singular_values.front();
  if (!std::isfinite(svd_norm) || svd_norm < 0.0) return radius;
  // Upward padding of the small SVD, and the analytic sqrt(p) max row bound.
  svd_norm = svd_norm * (1.0 + 64.0 * eps) + 64.0 * eps * svd_norm;
  double largest_row_norm = 0.0;
  for (std::size_t s = 0; s < sources; ++s) {
    if (!std::isfinite(row_norm[s]) || row_norm[s] < 0.0) return radius;
    largest_row_norm = std::max(largest_row_norm, row_norm[s]);
  }
  const double row_bound = std::sqrt(static_cast<double>(parameters)) * largest_row_norm;
  spectral_norm = std::min(svd_norm, row_bound);
  if (!(spectral_norm > 0.0) || !std::isfinite(spectral_norm)) return radius;

  // The data residual is diagnostic only: ||D^T r|| measures the floating
  // point defect of the nominal solve and is far below the statistical norm of
  // r (see above).
  {
    std::vector<double> gradient(parameters, 0.0);
    for (std::size_t k = 0; k < rows; ++k) {
      double model = 0.0;
      for (std::size_t c = 0; c < parameters; ++c) model += design(k, c) * solution[c];
      const double residual = measurement[k] - model;
      for (std::size_t c = 0; c < parameters; ++c) gradient[c] += design(k, c) * residual;
    }
    double sum = 0.0;
    for (double value : gradient) sum += value * value;
    normal_residual_norm = std::sqrt(sum);
    if (!std::isfinite(normal_residual_norm)) return radius;
  }

  double data_vector_norm = 0.0;
  for (double value : measurement) data_vector_norm += value * value;
  data_vector_norm = std::sqrt(data_vector_norm);
  double theta_norm = 0.0;
  for (double value : solution) theta_norm += value * value;
  theta_norm = std::sqrt(theta_norm);
  const double design_frobenius = numerics::frobenius_norm(design);
  if (!std::isfinite(data_vector_norm) || !std::isfinite(theta_norm) ||
      !std::isfinite(design_frobenius)) {
    return radius;
  }
  const double float_data = gamma * data_vector_norm;
  const double float_design = gamma * design_frobenius;
  const double data_total = data_norm + float_data;
  const double design_total = design_norm + float_design;

  const double contraction = spectral_norm * design_total;
  if (!std::isfinite(contraction) || !(contraction < 1.0)) return radius;
  amplification = 1.0 / (1.0 - contraction);
  const double common = data_total + design_total * theta_norm;
  if (!std::isfinite(common) || !std::isfinite(amplification)) return radius;
  radius.assign(sources, 0.0);
  for (std::size_t s = 0; s < sources; ++s) {
    double value = amplification * row_norm[s] * common;
    // The outward rounding of the interval arithmetic: a few ulp of the end
    // points, never a change of the declared tolerance.
    value += 4.0 * eps * (std::fabs(solution[s]) + value);
    if (!std::isfinite(value)) {
      radius.clear();
      return radius;
    }
    radius[s] = value;
  }
  return radius;
}

Result<Ref<SourceMeasurements>> measure_science_sources(Context& context,
                                                        Ref<NativeObservation> observation,
                                                        Ref<WcsModel> wcs,
                                                        std::span<const CatalogueStar> stars,
                                                        Ref<EffectivePSF> psf,
                                                        const PsfRequest& request) {
  if (!observation || !observation->native_index) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_request,
                                                "no observation supplied", kOrigin,
                                                "the measurement cannot be built");
  }
  if (!psf) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_request, "no PSF supplied", kOrigin,
                                                "the measurement cannot be built");
  }
  if (stars.empty()) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_request, "no catalogue star supplied",
                                                kOrigin, "the measurement cannot be built");
  }
  if (!observation->footprints || !observation->footprints->index) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                "the observation has no native footprint", kOrigin,
                                                "the measurement cannot be built");
  }

  const FootprintSet& footprints = *observation->footprints;
  const U64 sample_count = observation->native_index->size;
  Result<std::vector<double>> data = read_f64_array(context, observation->integrated_values);
  if (!ok(data)) {
    return model::fail<Ref<SourceMeasurements>>(data.code, "the native values cannot be read", kOrigin,
                                                "the measurement cannot be built");
  }
  Result<std::vector<std::uint16_t>> flags =
      observation::read_u16_values(context, observation->dq);
  std::vector<std::uint16_t> quality;
  if (ok(flags)) quality = val(flags);

  // The numerical interval of the native input.
  std::vector<double> data_lower = val(data);
  std::vector<double> data_upper = val(data);
  bool have_bounds = false;
  if (observation->numerical_lower && observation->numerical_upper) {
    Result<std::vector<double>> low = read_f64_array(context, observation->numerical_lower);
    Result<std::vector<double>> high = read_f64_array(context, observation->numerical_upper);
    if (ok(low) && ok(high) && val(low).size() == sample_count && val(high).size() == sample_count) {
      data_lower = val(low);
      data_upper = val(high);
      have_bounds = true;
    }
  }

  // ---------------------------------------------------------------------
  // The source selection.  The science sources come from the catalogue
  // predictions; the identity is frozen here, before any fit.
  // ---------------------------------------------------------------------
  struct Candidate {
    U64 source_id = 0;
    PixelCenter0 position;
    Box window;
  };
  std::vector<Candidate> candidates;
  double window_radius = request.window_radius_pixel;
  if (!(window_radius > 0.0)) window_radius = 8.0 * request.initial_width_pixel;

  // The regular grid is the production detector layout: the sample row is
  // y*width+x (the same convention candidate_rows verifies).  The former scan
  // over every sample for every source was both O(stars*native) and fragile
  // when the DQ index did not match the footprint order.
  bool regular_grid = footprints.width > 0 && footprints.height > 0 &&
                      footprints.index->size == footprints.width * footprints.height;
  for (U64 row = 0; regular_grid && row < sample_count && row < 64; ++row) {
    const sources::psfmodel::SampleFootprint footprint =
        sources::psfmodel::sample_footprint(footprints, row);
    if (!footprint.valid) continue;
    const U64 x = row % footprints.width;
    const U64 y = row / footprints.width;
    regular_grid = footprint.x0 == static_cast<double>(x) - 0.5 &&
                   footprint.y0 == static_cast<double>(y) - 0.5 &&
                   footprint.x1 == static_cast<double>(x) + 0.5 &&
                   footprint.y1 == static_cast<double>(y) + 0.5;
    break;
  }
  for (const CatalogueStar& star : stars) {
    Result<PixelCenter0> position = sources::psfmodel::detector_position(
        context, wcs, star, observation->context, request.budget);
    if (!ok(position)) continue;
    const PixelCenter0& point = val(position);
    // The saturation mask: the samples around the source must be usable.
    bool saturated = false;
    if (regular_grid && !quality.empty()) {
      const long long cx = static_cast<long long>(std::llround(point.x));
      const long long cy = static_cast<long long>(std::llround(point.y));
      if (cx >= 0 && cy >= 0 && cx < static_cast<long long>(footprints.width) &&
          cy < static_cast<long long>(footprints.height)) {
        const U64 row = static_cast<U64>(cy) * footprints.width + static_cast<U64>(cx);
        if (row < quality.size() &&
            (quality[static_cast<std::size_t>(row)] & observation::kDqSaturated) != 0u) {
          saturated = true;
        }
      }
    } else {
      for (U64 row = 0; row < sample_count && !saturated; ++row) {
        if ((row & 0x3F) == 0 && context.cancellation.requested()) {
          return model::fail<Ref<SourceMeasurements>>(Code::cancelled, "cancelled", kOrigin,
                                                      "the measurement was cancelled");
        }
        const sources::psfmodel::SampleFootprint footprint =
            sources::psfmodel::sample_footprint(footprints, row);
        if (!footprint.valid) continue;
        if (point.x < footprint.x0 || point.x > footprint.x1) continue;
        if (point.y < footprint.y0 || point.y > footprint.y1) continue;
        if (row < quality.size() && (quality[row] & observation::kDqSaturated) != 0u)
          saturated = true;
      }
    }
    if (saturated) continue;
    Candidate candidate;
    candidate.source_id = star.source_id;
    candidate.position = point;
    candidate.window.lower = {point.x - window_radius, point.y - window_radius};
    candidate.window.upper = {point.x + window_radius, point.y + window_radius};
    candidates.push_back(candidate);
  }
  // The neighbour mask: two sources closer than the fit resolution cannot be
  // separated and are not measured independently.
  std::vector<Candidate> selected;
  for (const Candidate& candidate : candidates) {
    bool neighbour = false;
    for (const Candidate& other : selected) {
      const double dx = other.position.x - candidate.position.x;
      const double dy = other.position.y - candidate.position.y;
      if (std::sqrt(dx * dx + dy * dy) < 2.0 * window_radius) {
        neighbour = true;
        break;
      }
    }
    if (!neighbour) selected.push_back(candidate);
  }
  if (std::getenv("ACSD_FLOW_DEBUG")) {
    std::fprintf(stderr, "measure: candidates=%zu selected=%zu stars=%zu quality=%zu\n",
                 candidates.size(), selected.size(), stars.size(), quality.size());
  }
  if (selected.empty()) {
    return model::fail<Ref<SourceMeasurements>>(Code::no_support,
                                                "no usable science source on the frame", kOrigin,
                                                "the measurement cannot be built");
  }

  // ---------------------------------------------------------------------
  // The measurement window and the template.
  // ---------------------------------------------------------------------
  std::vector<PixelCenter0> positions;
  for (const Candidate& candidate : selected) positions.push_back(candidate.position);

  std::vector<U64> window_rows;
  for (U64 row = 0; row < sample_count; ++row) {
    if ((row & 0x3F) == 0 && context.cancellation.requested()) {
      return model::fail<Ref<SourceMeasurements>>(Code::cancelled, "cancelled", kOrigin,
                                                  "the measurement was cancelled");
    }
    const sources::psfmodel::SampleFootprint footprint =
        sources::psfmodel::sample_footprint(footprints, row);
    if (!footprint.valid) continue;
    bool inside = false;
    for (const Candidate& candidate : selected) {
      if (footprint.x1 < candidate.window.lower[0] || footprint.x0 > candidate.window.upper[0]) continue;
      if (footprint.y1 < candidate.window.lower[1] || footprint.y0 > candidate.window.upper[1]) continue;
      inside = true;
      break;
    }
    if (inside) window_rows.push_back(row);
  }
  if (std::getenv("ACSD_FLOW_DEBUG")) {
    std::fprintf(stderr, "measure: window_rows=%zu positions=%zu\n", window_rows.size(),
                 positions.size());
  }
  if (window_rows.empty()) {
    return model::fail<Ref<SourceMeasurements>>(Code::no_support,
                                                "the measurement window has no sample", kOrigin,
                                                "the measurement cannot be built");
  }

  // The parameters of the PSF kernel.
  Result<std::vector<double>> psf_parameter_values = read_f64_array(context, psf->parameters);
  if (!ok(psf_parameter_values)) {
    return model::fail<Ref<SourceMeasurements>>(psf_parameter_values.code,
                                                "the PSF parameters cannot be read", kOrigin,
                                                "the measurement cannot be built");
  }
  const std::vector<double> psf_parameters = val(psf_parameter_values);

  // The transfer of the PSF decides which output rows carry the window.
  Ref<FootprintSet> detector_samples;
  Ref<LinearAction> transfer;
  psf_transfer(context, observation, detector_samples, transfer);
  // A transfer whose domain and codomain are the same index maps the native
  // samples onto themselves (a diagonal calibration scale): the dependency set
  // of a window row is the same row, so the per-row unit-vector scan is both
  // unnecessary and quadratic in the frame size.  The scientific dependency is
  // the identity of the index, exactly as in evaluate_psf.
  const bool same_index_transfer =
      transfer && transfer->spaces.domain && transfer->spaces.codomain &&
      transfer->spaces.domain == transfer->spaces.codomain;
  const bool identity_transfer = !transfer || same_index_transfer;
  U64 output_count = sample_count;
  if (transfer) {
    if (!transfer->spaces.codomain) {
      return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                  "the transfer has no output index", kOrigin,
                                                  "the measurement cannot be built");
    }
    output_count = transfer->spaces.codomain->size;
  }

  std::vector<U64> measurement_rows;
  if (identity_transfer) {
    measurement_rows = window_rows;
  } else {
    // The output rows that depend on the native samples of the window.  Only
    // the samples that can carry the mass of the sources are scanned, so the
    // cost does not grow with the size of the frame.
    const U64 native_count = detector_samples && detector_samples->index
                                  ? detector_samples->index->size
                                  : 0;
    std::vector<std::vector<double>> locals;
    for (const PixelCenter0& position : positions) {
      const std::vector<double> basis_vector = sources::psfmodel::basis_values(
          psf->spatial_basis, psf->spatial_basis.degree, position.x, position.y);
      locals.push_back(sources::psfmodel::local_parameters(psf->family, psf_parameters, basis_vector));
    }
    const double radius = sources::psfmodel::support_radius(psf->family, locals);
    const std::vector<U64> candidates =
        sources::psfmodel::candidate_rows(footprints, positions, radius);
    std::vector<double> unit(native_count, 0.0);
    for (U64 native : window_rows) {
      if (native >= native_count) continue;
      std::fill(unit.begin(), unit.end(), 0.0);
      unit[native] = 1.0;
      const std::vector<U64> shape{static_cast<U64>(unit.size())};
      Result<ArrayRef> input = write_f64_array(context, unit, shape, model::unit_dimensionless(),
                                                "measurement");
      if (!ok(input)) {
        return model::fail<Ref<SourceMeasurements>>(input.code, "the input cannot be stored", kOrigin,
                                                    "the measurement cannot be built");
      }
      Result<ArrayRef> mapped = apply(context, transfer, val(input));
      if (!ok(mapped)) {
        return model::fail<Ref<SourceMeasurements>>(mapped.code, "the transfer cannot be applied",
                                                    kOrigin, "the measurement cannot be built");
      }
      Result<std::vector<double>> column = read_f64_array(context, val(mapped));
      if (!ok(column)) {
        return model::fail<Ref<SourceMeasurements>>(column.code, "the transfer cannot be read", kOrigin,
                                                    "the measurement cannot be built");
      }
      for (U64 row = 0; row < output_count && row < val(column).size(); ++row) {
        if (val(column)[static_cast<std::size_t>(row)] != 0.0) {
          measurement_rows.push_back(row);
          break;
        }
      }
    }
    std::sort(measurement_rows.begin(), measurement_rows.end());
    measurement_rows.erase(std::unique(measurement_rows.begin(), measurement_rows.end()),
                           measurement_rows.end());
  }
  if (std::getenv("ACSD_FLOW_DEBUG")) {
    std::fprintf(stderr, "measure: measurement_rows=%zu identity=%d\n",
                 measurement_rows.size(), identity_transfer ? 1 : 0);
  }
  if (measurement_rows.empty()) {
    return model::fail<Ref<SourceMeasurements>>(Code::no_support,
                                                "the window does not reach the output samples", kOrigin,
                                                "the measurement cannot be built");
  }

  // The template with its numerical enclosure.
  PsfEvaluationRequest evaluation_request;
  evaluation_request.source_positions = positions;
  evaluation_request.budget = request.budget;
  evaluation_request.output_rows = measurement_rows;
  Result<Ref<PsfEvaluation>> evaluation = evaluate_psf(context, psf, evaluation_request);
  if (!ok(evaluation)) {
    return model::fail<Ref<SourceMeasurements>>(evaluation.code, "the template cannot be built", kOrigin,
                                                "the measurement cannot be built");
  }
  const Ref<PsfEvaluation> template_evaluation = val(evaluation);
  Result<std::vector<double>> template_values = read_f64_array(context, template_evaluation->values);
  Result<std::vector<double>> template_lower =
      read_f64_array(context, template_evaluation->numerical_lower);
  Result<std::vector<double>> template_upper =
      read_f64_array(context, template_evaluation->numerical_upper);
  if (!ok(template_values) || !ok(template_lower) || !ok(template_upper)) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                "the template cannot be read", kOrigin,
                                                "the measurement cannot be built");
  }
  const std::size_t rows = measurement_rows.size();
  const std::size_t sources = selected.size();
  if (val(template_values).size() != rows * sources) {
    return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                "the template does not match the window", kOrigin,
                                                "the measurement cannot be built");
  }

  // ---------------------------------------------------------------------
  // The linear solve for the fluxes and the background.
  // ---------------------------------------------------------------------
  // The background column Z.  Z = 1 holds only for a unit area photosite with
  // a uniform response and an untransformed sample chain (sources.measure
  // "only a unit area, uniform response and untransformed sample give Z=1;
  // after a bin/gain it no longer holds").  The column is built from the same
  // known response chain as the source templates instead of being hard-wired:
  // z_j is the integrated relative response of the native sample j, and a
  // transfer maps it to the measured output samples exactly like the source
  // template.
  // ---------------------------------------------------------------------
  std::vector<double> background_native;
  {
    const FootprintSet* response_samples =
        identity_transfer ? &footprints : (detector_samples ? detector_samples.get() : nullptr);
    const U64 response_count =
        identity_transfer ? sample_count
                          : (detector_samples && detector_samples->index
                                 ? detector_samples->index->size
                                 : 0);
    if (response_samples == nullptr || response_count == 0) {
      return model::fail<Ref<SourceMeasurements>>(
          Code::invalid_model, "the background column has no response samples", kOrigin,
          "the measurement cannot be built");
    }
    background_native.assign(response_count, 1.0);
    std::vector<double> gains;
    if (response_samples->sample_response_gain) {
      Result<std::vector<double>> read =
          read_f64_array(context, response_samples->sample_response_gain);
      if (!ok(read) || val(read).size() != response_count) {
        return model::fail<Ref<SourceMeasurements>>(
            Code::invalid_model, "the sample response gain does not cover the native samples",
            kOrigin, "the measurement cannot be built");
      }
      gains = val(read);
    }
    // The area weighted mean of the in-pixel PRF over the photosite: one for a
    // relative_mean_one response over a full partition, otherwise the
    // response actually collected by that photosite.
    std::vector<double> prf_mean(response_count, 1.0);
    bool have_prf = false;
    if (response_samples->pixel_response && !response_samples->pixel_response->cells.empty()) {
      const PixelResponse& response = *response_samples->pixel_response;
      Result<std::vector<double>> read = read_f64_array(context, response.gains);
      if (!ok(read) || val(read).size() % response.cells.size() != 0) {
        return model::fail<Ref<SourceMeasurements>>(
            Code::invalid_model, "the pixel response gains do not cover its cells", kOrigin,
            "the measurement cannot be built");
      }
      const std::size_t bins = val(read).size() / response.cells.size();
      if (bins != 1) {
        return model::fail<Ref<SourceMeasurements>>(
            Code::unsupported,
            "the pixel response carries more than one wavelength bin while the PSF is achromatic",
            kOrigin, "the background response is not known");
      }
      double area = 0.0;
      double weighted = 0.0;
      bool valid = true;
      for (std::size_t c = 0; c < response.cells.size(); ++c) {
        const double dx = response.cells[c].upper[0] - response.cells[c].lower[0];
        const double dy = response.cells[c].upper[1] - response.cells[c].lower[1];
        if (!(dx > 0.0) || !(dy > 0.0)) {
          valid = false;
          break;
        }
        area += dx * dy;
        weighted += val(read)[c] * dx * dy;
      }
      if (!valid || !(area > 0.0)) {
        return model::fail<Ref<SourceMeasurements>>(
            Code::invalid_model, "the pixel response cells are degenerate", kOrigin,
            "the background response is not known");
      }
      const double mean = weighted / area;
      for (U64 j = 0; j < response_count; ++j) prf_mean[j] = mean;
      have_prf = true;
    }
    for (U64 j = 0; j < response_count; ++j) {
      background_native[j] = prf_mean[j] * (j < gains.size() ? gains[j] : 1.0);
    }
    (void)have_prf;
  }
  std::vector<double> background_column;
  if (identity_transfer) {
    background_column = background_native;
  } else {
    if (!transfer || !transfer->spaces.domain || !transfer->spaces.codomain) {
      return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                  "the transfer has no declared spaces", kOrigin,
                                                  "the measurement cannot be built");
    }
    if (transfer->spaces.domain->size != background_native.size()) {
      return model::fail<Ref<SourceMeasurements>>(
          Code::invalid_model, "the transfer domain does not match the native response samples",
          kOrigin, "the measurement cannot be built");
    }
    const std::vector<U64> shape{static_cast<U64>(background_native.size())};
    Result<ArrayRef> input = write_f64_array(context, background_native, shape,
                                              model::unit_dimensionless(), "measurement");
    if (!ok(input)) {
      return model::fail<Ref<SourceMeasurements>>(input.code, "the response cannot be stored",
                                                  kOrigin, "the measurement cannot be built");
    }
    Result<ArrayRef> mapped = apply(context, transfer, val(input));
    if (!ok(mapped)) {
      return model::fail<Ref<SourceMeasurements>>(mapped.code, "the transfer cannot be applied",
                                                  kOrigin, "the measurement cannot be built");
    }
    Result<std::vector<double>> column = read_f64_array(context, val(mapped));
    if (!ok(column) || val(column).size() != output_count) {
      return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                  "the transferred response has the wrong size",
                                                  kOrigin, "the measurement cannot be built");
    }
    background_column = val(column);
  }
  // The background model of every source: a local plane (constant + two
  // linear terms) over its own window, not one constant shared by the whole
  // frame.  A constant over a 21x21 window in the M42 nebula leaves a
  // background gradient of ~1e3 ADU/px in the residuals and drives the flux
  // of a real star to -1.2e6 ADU; the plane is the minimal model that removes
  // the local tilt while keeping the flux orthogonal to it.  The windows are
  // disjoint, so the background blocks are block-diagonal.
  std::vector<std::size_t> row_source(rows, sources);
  for (std::size_t k = 0; k < rows; ++k) {
    const U64 row = measurement_rows[k];
    const sources::psfmodel::SampleFootprint sample =
        sources::psfmodel::sample_footprint(footprints, row);
    if (!sample.valid) continue;
    const double cx = 0.5 * (sample.x0 + sample.x1);
    const double cy = 0.5 * (sample.y0 + sample.y1);
    for (std::size_t s = 0; s < sources; ++s) {
      const Candidate& candidate = selected[s];
      if (cx < candidate.window.lower[0] || cx > candidate.window.upper[0]) continue;
      if (cy < candidate.window.lower[1] || cy > candidate.window.upper[1]) continue;
      row_source[k] = s;
      break;
    }
  }
  bool background_is_one = true;
  const std::size_t parameters = sources + 3 * sources;
  Matrix design(rows, parameters);
  std::vector<double> measurement(rows, 0.0);
  std::vector<double> measurement_lower(rows, 0.0);
  std::vector<double> measurement_upper(rows, 0.0);
  for (std::size_t k = 0; k < rows; ++k) {
    const U64 row = measurement_rows[k];
    for (std::size_t s = 0; s < sources; ++s) {
      design(k, s) = val(template_values)[k * sources + s];
    }
    const double z = row < background_column.size() ? background_column[row] : 1.0;
    const std::size_t star = row_source[k] < sources ? row_source[k] : 0;
    if (row_source[k] < sources) {
      const sources::psfmodel::SampleFootprint sample =
          sources::psfmodel::sample_footprint(footprints, row);
      const double cx = 0.5 * (sample.x0 + sample.x1);
      const double cy = 0.5 * (sample.y0 + sample.y1);
      design(k, sources + 3 * star) = z;
      design(k, sources + 3 * star + 1) = z * (cx - selected[star].position.x);
      design(k, sources + 3 * star + 2) = z * (cy - selected[star].position.y);
    }
    if (z != 1.0) background_is_one = false;
    measurement[k] = row < val(data).size() ? val(data)[static_cast<std::size_t>(row)] : 0.0;
    measurement_lower[k] =
        row < data_lower.size() ? data_lower[static_cast<std::size_t>(row)] : measurement[k];
    measurement_upper[k] =
        row < data_upper.size() ? data_upper[static_cast<std::size_t>(row)] : measurement[k];
  }
  // The column scaling of the design.
  std::vector<double> column_scale(parameters, 0.0);
  for (std::size_t c = 0; c < parameters; ++c) {
    double norm = 0.0;
    for (std::size_t k = 0; k < rows; ++k) norm += design(k, c) * design(k, c);
    column_scale[c] = std::sqrt(norm);
  }
  Matrix scaled(rows, parameters);
  for (std::size_t k = 0; k < rows; ++k) {
    for (std::size_t c = 0; c < parameters; ++c) {
      scaled(k, c) = column_scale[c] > 0.0 ? design(k, c) / column_scale[c] : 0.0;
    }
  }
  numerics::SvdResult svd = numerics::jacobi_svd(scaled, 1.0e-12);
  const bool rank_certified = svd.rank == parameters;
  std::vector<double> solution(parameters, 0.0);
  std::vector<std::vector<double>> inverse(rows, std::vector<double>(parameters, 0.0));
  if (rank_certified) {
    solution = sources::psfmodel::solve_scaled(svd, measurement, column_scale);
    // The pseudo inverse of the design: the influence of the native samples
    // on the measured fluxes.  The base assembles V S+ U^T of the scaled
    // design; the design itself is the scaled one times the diagonal of the
    // column scales, so the pseudo inverse of the design divides the scaled
    // one by the column scale.
    const Matrix pseudo = numerics::pseudo_inverse(svd, parameters, 1.0e-12);
    for (std::size_t c = 0; c < parameters; ++c) {
      const double scale = column_scale[c] > 0.0 ? column_scale[c] : 0.0;
      for (std::size_t k = 0; k < rows; ++k) {
        inverse[k][c] = scale > 0.0 ? pseudo(c, k) / scale : 0.0;
      }
    }
  }

  // ---------------------------------------------------------------------
  // The numerical bounds of the flux.  The interval of the native input and
  // the interval of the template enter the same design matrix; the floating
  // point error of the solve itself is a third, deterministic perturbation.
  //
  // The enclosure is the shared implementation
  // measure_flux_numerical_radius (see its comment for the math).  The data
  // residual r = d - D theta is NOT part of it: D^+ r = 0 identically, r is a
  // statistical quantity, and adding ||r|| to a deterministic radius turns the
  // "numerical enclosure" into a statistical interval (defect R1).
  // ---------------------------------------------------------------------
  std::vector<double> flux_lower;
  std::vector<double> flux_upper;
  bool flux_certified = false;
  double flux_radius = 0.0;
  double flux_amplification = 0.0;
  double flux_spectral_norm = 0.0;
  double data_norm = 0.0;
  double design_norm = 0.0;
  double residual_enclosure = 0.0;
  double normal_residual_norm = 0.0;
  if (rank_certified) {
    // The interval half width is not enough: the nominal sample has to lie
    // inside its own interval, so the deviation is measured from the nominal
    // value to the farther endpoint.
    std::vector<double> data_error(rows, 0.0);
    for (std::size_t k = 0; k < rows; ++k) {
      data_error[k] =
          std::max(std::fabs(measurement_upper[k] - measurement[k]),
                   std::fabs(measurement[k] - measurement_lower[k]));
      data_norm += data_error[k] * data_error[k];
    }
    data_norm = std::sqrt(data_norm);
    for (std::size_t k = 0; k < rows; ++k) {
      for (std::size_t c = 0; c < sources; ++c) {
        const double lower = val(template_lower)[k * sources + c];
        const double upper = val(template_upper)[k * sources + c];
        const double nominal = design(k, c);
        const double error = std::max(std::fabs(upper - nominal), std::fabs(nominal - lower));
        design_norm += error * error;
      }
    }
    design_norm = std::sqrt(design_norm);
    // The norm of every row of D^+ = S^-1 (D S^-1)^+; the flux columns are
    // the first `sources` columns.
    std::vector<double> row_norm(sources, 0.0);
    for (std::size_t s = 0; s < sources; ++s) {
      double sum = 0.0;
      for (std::size_t k = 0; k < rows; ++k) {
        const double value = inverse[k][s];
        sum += value * value;
      }
      row_norm[s] = std::sqrt(sum);
    }
    // The enclosure residual of the validation solve, reported with the
    // result: the elementwise residual of the nominal model against the data
    // interval and the template interval.
    for (std::size_t k = 0; k < rows; ++k) {
      double model = 0.0;
      for (std::size_t c = 0; c < parameters; ++c) model += design(k, c) * solution[c];
      double error = std::fabs(measurement[k] - model);
      for (std::size_t c = 0; c < sources; ++c) {
        const double lower = val(template_lower)[k * sources + c];
        const double upper = val(template_upper)[k * sources + c];
        error += 0.5 * std::fabs(upper - lower) * std::fabs(solution[c]);
      }
      error += data_error[k];
      residual_enclosure = std::max(residual_enclosure, error);
    }
    const std::vector<double> flux_radii = measure_flux_numerical_radius(
        design, sources, measurement, solution, row_norm, svd, column_scale, data_norm,
        design_norm, flux_amplification, flux_spectral_norm, normal_residual_norm);
    if (flux_radii.size() == sources) {
      flux_lower.assign(sources, 0.0);
      flux_upper.assign(sources, 0.0);
      bool all_finite = true;
      for (std::size_t s = 0; s < sources; ++s) {
        const double radius = flux_radii[s];
        if (!std::isfinite(radius) || !std::isfinite(solution[s])) all_finite = false;
        flux_lower[s] = solution[s] - radius;
        flux_upper[s] = solution[s] + radius;
        if (s == 0) flux_radius = radius;
      }
      // The certificate is only issued when every end point is a finite
      // number: an infinite radius is not an enclosure.
      flux_certified = all_finite && std::isfinite(flux_amplification) &&
                       std::isfinite(flux_spectral_norm) &&
                       std::isfinite(data_norm) && std::isfinite(design_norm);
      if (!flux_certified) {
        flux_lower.clear();
        flux_upper.clear();
      }
    }
  }

  // ---------------------------------------------------------------------
  // The joint noise: the covariance of the measured fluxes.
  // ---------------------------------------------------------------------
  Ref<CovarianceAction> joint_noise;
  std::vector<double> flux_variance(sources, 0.0);
  bool variance_known = false;
  if (observation->noise && rank_certified) {
    std::vector<U64> offsets;
    std::vector<U64> indices;
    std::vector<double> values;
    offsets.push_back(0);
    for (std::size_t c = 0; c < parameters; ++c) {
      for (std::size_t k = 0; k < rows; ++k) {
        const double value = inverse[k][c];
        if (value == 0.0) continue;
        indices.push_back(k);
        values.push_back(value);
      }
      offsets.push_back(indices.size());
    }
    Index flux_index = make_index(parameters, "psf-flux");
    const U64 columns = rows > 0 ? rows : 1;
    Result<Ref<LinearAction>> influence = operators::make_csr_action(
        context, parameters, columns, offsets, indices, values,
        observation->native_index->identity.object_id, flux_index.identity.object_id,
        model::unit_dimensionless());
    if (ok(influence)) {
      auto covariance = std::make_shared<CovarianceAction>();
      covariance->model = observation->noise;
      covariance->influence = val(influence);
      covariance->input_output.domain = observation->native_index;
      covariance->input_output.codomain = std::make_shared<Index>(flux_index);
      covariance->input_output.input = observation->noise->variance_unit;
      covariance->input_output.output = observation->noise->variance_unit;
      covariance->assessment = model::make_assessment(Code::ok);
      covariance->identity = model::make_identity("acsd.CovarianceAction", 2, 0,
                                                  model::sha256_hex("measure_science_sources"), "acsd");
      joint_noise = std::shared_ptr<const CovarianceAction>(covariance);
      // The variance of the measured fluxes is the diagonal of the published
      // covariance I C I^T, where I is the influence of the native samples on
      // the measured parameters.  The base applies the whole chain, so the
      // variance is read from the columns of the applied covariance.  A noise
      // model with an unknown term makes the covariance unavailable: the
      // variance then stays unknown and is never filled with zero.
      for (std::size_t s = 0; s < sources; ++s) {
        std::vector<double> functional(parameters, 0.0);
        functional[s] = 1.0;
        const std::vector<U64> functional_shape{static_cast<U64>(parameters)};
        Result<ArrayRef> functional_array =
            write_f64_array(context, functional, functional_shape, model::unit_dimensionless(),
                            flux_index.identity.object_id);
        if (!ok(functional_array)) {
          variance_known = false;
          break;
        }
        Result<ArrayRef> column = covariance_apply(context, joint_noise, val(functional_array));
        if (!ok(column)) {
          variance_known = false;
          break;
        }
        Result<std::vector<double>> applied = read_f64_array(context, val(column));
        if (!ok(applied) || val(applied).size() != parameters) {
          variance_known = false;
          break;
        }
        flux_variance[s] = val(applied)[s];
        variance_known = true;
      }
    }
  }

  // ---------------------------------------------------------------------
  // The source measurements.
  // ---------------------------------------------------------------------
  std::vector<StarFlux> measurements;
  for (std::size_t s = 0; s < sources; ++s) {
    StarFlux flux;
    flux.gaia_source_id = selected[s].source_id;
    flux.position = selected[s].position;
    flux.integrated_flux = solution[s];
    flux.variance.unit = model::unit_variance(observation->unit.scale);
    if (variance_known) {
      flux.variance.value = flux_variance[s];
      flux.variance.state = observation->noise->state;
    } else {
      flux.variance.value = std::nullopt;
      flux.variance.state = NoiseState::unknown;
      flux.variance.unknown_terms.push_back("the_flux_variance_is_not_certified");
    }
    flux.definition =
        "total_flux_of_the_pixel_integrated_psf_template_in_the_measurement_window";
    flux.window = selected[s].window;
    flux.influence.condition =
        model::make_condition("science_source_flux", "linear_template_fit");
    measurements.push_back(flux);
  }

  // The training pixels keep the connection with the native roots.
  std::vector<U64> training_identities;
  for (U64 row : measurement_rows) {
    training_identities.push_back(
        row < observation->native_index->sources.size()
            ? observation->native_index->sources[static_cast<std::size_t>(row)].sample
            : row);
  }
  const std::vector<U64> training_shape{static_cast<U64>(training_identities.size())};
  Result<ArrayRef> training_u64 = context.storage.copy(
      context, DType::u64, model::encode_u64(training_identities), training_shape,
      model::unit_dimensionless(), observation->native_index->identity.object_id);
  if (!ok(training_u64)) {
    return model::fail<Ref<SourceMeasurements>>(training_u64.code,
                                                "the training pixels cannot be stored", kOrigin,
                                                "the measurement cannot be built");
  }

  // The flux bounds.
  Result<ArrayRef> flux_lower_array;
  Result<ArrayRef> flux_upper_array;
  if (flux_certified) {
    const std::vector<U64> flux_shape{static_cast<U64>(sources)};
    flux_lower_array =
        write_f64_array(context, flux_lower, flux_shape, observation->unit, "psf-flux");
    flux_upper_array =
        write_f64_array(context, flux_upper, flux_shape, observation->unit, "psf-flux");
    if (!ok(flux_lower_array) || !ok(flux_upper_array)) {
      return model::fail<Ref<SourceMeasurements>>(Code::internal,
                                                  "the flux bounds cannot be stored", kOrigin,
                                                  "the measurement cannot be built");
    }
  }

  // The numerical certificate of the measurement.
  auto error = std::make_shared<ErrorRecord>();
  error->linearization_selection =
      model::make_condition("science_source_flux", "linear_template_fit");
  error->unknown_model_terms.push_back("the_real_psf_shape_deviation_is_not_a_numerical_error");
  if (!flux_certified) {
    error->unknown_model_terms.push_back("numerical_flux_uncertified");
  }
  if (!have_bounds) {
    error->unknown_model_terms.push_back("the_native_input_carries_no_numerical_interval");
  }
  error->identity = model::make_identity("acsd.ErrorRecord", 2, 0,
                                         model::sha256_hex("measure_science_sources"), "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto result = std::make_shared<SourceMeasurements>();
  result->frame = observation->frame;
  result->stars = measurements;
  result->psf = psf;
  result->joint_noise = joint_noise;
  result->training_pixel_ids = val(training_u64);
  result->flux_numerical_lower = flux_certified ? val(flux_lower_array) : ArrayRef();
  result->flux_numerical_upper = flux_certified ? val(flux_upper_array) : ArrayRef();
  result->error = std::shared_ptr<const ErrorRecord>(error);
  result->observation = observation;
  result->assessment = model::make_assessment(Code::ok);
  result->assessment.complete = true;
  result->assessment.noise = variance_known ? observation->noise->state : NoiseState::unknown;
  result->assessment.science = flux_certified ? ScienceState::bounded : ScienceState::conditional;
  model::set_included(result->assessment, "psf_template_pixel_integral");
  model::set_omitted(result->assessment, "the_window_is_not_renormalised_to_the_total_flux");
  // P1-8: the condition under which the background column is one, or the fact
  // that it was built from the same response chain as the templates.
  if (background_is_one) {
    model::set_included(result->assessment,
                        "background_column_Z_equals_one: unit area samples, uniform relative "
                        "response and an identity transfer");
  } else {
    model::set_included(result->assessment,
                        "background_column_Z_is_built_from_the_same_PRF/sample_gain_and_transfer_"
                        "chain_as_the_templates; Z_is_not_one");
  }
  // P1-7: the model identity of the measured position.  fit_psf fits continuous
  // source positions while this entry evaluates the template at the catalogue
  // position inverted through the WCS; that difference is recorded instead of
  // being silent (a position offset enters the flux only at second order for a
  // symmetric PSF, but it is a model difference and not a numerical error).
  model::set_omitted(result->assessment,
                     "the_template_is_evaluated_at_the_wcs_position_while_fit_psf_fits_a_free_"
                     "continuous_position");
  error->unknown_model_terms.push_back(
      "the_source_position_used_here_is_the_catalogue_position_inverted_through_the_wcs_and_not_"
      "the_free_position_estimated_by_fit_psf");
  model::set_omitted(result->assessment,
                     "the_flux_interval_is_a_numerical_enclosure_of_the_linear_solve_and_not_a_"
                     "statistical_or_model_error");
  if (flux_certified) {
    // The composition of the enclosure is kept with the result: the radius of
    // every component is
    //   1/(1-||D^+||_2 ||dD||_F) * ||D^+_s||_2 * (||dd|| + ||dD||_F ||theta||),
    // where the data and design norms include the floating point backward
    // error of the solve (gamma ||d|| and gamma ||D||_F) and ||D^+||_2 is the
    // spectral norm (bounded by the smaller of the computed spectral norm and
    // sqrt(p) max row norm).  The data residual is not a term: D^+ r = 0.
    model::set_included(
        result->assessment,
        "the_flux_radius_includes_the_column_scaling_because_every_component_uses_the_row_norm_of_"
        "the_pseudo_inverse_D^+_=_S^-1_(D_S^-1)^+, and the_floating_point_backward_error_of_the_"
        "solve_(gamma*||d||, gamma*||D||_F); amplification=" +
            std::to_string(flux_amplification) + " spectral_norm_D+=" +
            std::to_string(flux_spectral_norm) + " data_norm=" + std::to_string(data_norm) +
            " design_norm=" + std::to_string(design_norm) +
            " normal_equation_residual_D^T_r=" + std::to_string(normal_residual_norm) +
            " residual_enclosure=" + std::to_string(residual_enclosure) +
            " (the_data_residual_itself_is_statistical_and_is_not_in_the_radius)");
  }
  if (!flux_certified) {
    model::add_issue(result->assessment, Code::not_identifiable,
                     "the flux bounds could not be certified; the nominal flux is kept", kOrigin,
                     "the numerical flux interval is missing");
  }
  result->identity = model::make_identity(
      "acsd.SourceMeasurements", 2, 0,
      model::sha256_hex(observation->frame + ":" + std::to_string(sources)), "acsd");

  return model::ok_result(std::shared_ptr<const SourceMeasurements>(result));
}

}  // namespace acsd
