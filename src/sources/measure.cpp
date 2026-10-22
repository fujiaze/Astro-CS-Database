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
// native input and the interval of the template, which enter the same design
// matrix: the inverse of the design matrix bounds the perturbation and the
// residual is enclosed.  When the rank is not certified or the enclosure
// cannot be certified, the nominal flux is kept and the state says so; a
// random variance is never used as a deterministic error.
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
  const U64 count = array->shape.empty() ? 0 : array->shape[0];
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
        observation::compose_actions(context, node->regrouping, transfer);
    if (!ok(chained)) return;
    transfer = val(chained);
    detector_samples = node->footprints;
    node = node->native_parent;
  }
}

}  // namespace

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

  for (const CatalogueStar& star : stars) {
    Result<PixelCenter0> position = sources::psfmodel::detector_position(
        context, wcs, star, observation->context, request.budget);
    if (!ok(position)) continue;
    const PixelCenter0& point = val(position);
    // The saturation mask: the samples around the source must be usable.
    bool saturated = false;
    for (U64 row = 0; row < sample_count && !saturated; ++row) {
      const sources::psfmodel::SampleFootprint footprint =
          sources::psfmodel::sample_footprint(footprints, row);
      if (!footprint.valid) continue;
      if (point.x < footprint.x0 || point.x > footprint.x1) continue;
      if (point.y < footprint.y0 || point.y > footprint.y1) continue;
      if (row < quality.size() && (quality[row] & observation::kDqSaturated) != 0u) saturated = true;
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
      if (std::sqrt(dx * dx + dy * dy) < 0.5 * window_radius) {
        neighbour = true;
        break;
      }
    }
    if (!neighbour) selected.push_back(candidate);
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
  Result<ActionRecipe> transfer_recipe =
      transfer ? transfer->serialize_recipe() : model::ok_result(ActionRecipe());
  if (!ok(transfer_recipe)) {
    return model::fail<Ref<SourceMeasurements>>(transfer_recipe.code,
                                                "the transfer cannot be described", kOrigin,
                                                "the measurement cannot be built");
  }
  const bool identity_transfer = !transfer || val(transfer_recipe).kind == ActionRecipe::Kind::identity;
  U64 output_count = sample_count;
  if (transfer) {
    if (!val(transfer_recipe).spaces.codomain) {
      return model::fail<Ref<SourceMeasurements>>(Code::invalid_model,
                                                  "the transfer has no output index", kOrigin,
                                                  "the measurement cannot be built");
    }
    output_count = val(transfer_recipe).spaces.codomain->size;
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
  const std::size_t parameters = sources + 1;
  Matrix design(rows, parameters);
  std::vector<double> measurement(rows, 0.0);
  std::vector<double> measurement_lower(rows, 0.0);
  std::vector<double> measurement_upper(rows, 0.0);
  for (std::size_t k = 0; k < rows; ++k) {
    const U64 row = measurement_rows[k];
    for (std::size_t s = 0; s < sources; ++s) {
      design(k, s) = val(template_values)[k * sources + s];
    }
    design(k, sources) = 1.0;
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
    // on the measured fluxes.
    for (std::size_t c = 0; c < parameters; ++c) {
      const double scale = column_scale[c] > 0.0 ? column_scale[c] : 0.0;
      for (std::size_t i = 0; i < parameters; ++i) {
        const double sigma = svd.singular_values[i];
        if (!(sigma > 0.0)) continue;
        for (std::size_t k = 0; k < rows; ++k) {
          inverse[k][c] += svd.u(k, i) * svd.v(c, i) / sigma * scale;
        }
      }
    }
  }

  // ---------------------------------------------------------------------
  // The numerical bounds of the flux.  The interval of the native input and
  // the interval of the template enter the same design matrix.
  // ---------------------------------------------------------------------
  std::vector<double> flux_lower;
  std::vector<double> flux_upper;
  bool flux_certified = false;
  double flux_radius = 0.0;
  double residual_enclosure = 0.0;
  if (rank_certified) {
    std::vector<double> data_error(rows, 0.0);
    double data_norm = 0.0;
    for (std::size_t k = 0; k < rows; ++k) {
      data_error[k] = 0.5 * std::fabs(measurement_upper[k] - measurement_lower[k]);
      data_norm += data_error[k] * data_error[k];
    }
    data_norm = std::sqrt(data_norm);
    double design_norm = 0.0;
    for (std::size_t k = 0; k < rows; ++k) {
      for (std::size_t c = 0; c < sources; ++c) {
        const double lower = val(template_lower)[k * sources + c];
        const double upper = val(template_upper)[k * sources + c];
        const double error = 0.5 * std::fabs(upper - lower);
        design_norm += error * error;
      }
    }
    design_norm = std::sqrt(design_norm);
    double solution_norm = 0.0;
    for (double value : solution) solution_norm += value * value;
    solution_norm = std::sqrt(solution_norm);
    double inverse_norm = 0.0;
    for (double sigma : svd.singular_values) {
      if (!(sigma > 0.0)) continue;
      inverse_norm = std::max(inverse_norm, 1.0 / sigma);
    }
    // The perturbation of the solution: d theta = A^-1 (d data - d A theta).
    flux_radius = inverse_norm * (data_norm + design_norm * solution_norm);
    flux_lower.assign(sources, 0.0);
    flux_upper.assign(sources, 0.0);
    for (std::size_t s = 0; s < sources; ++s) {
      flux_lower[s] = solution[s] - flux_radius;
      flux_upper[s] = solution[s] + flux_radius;
    }
    // The residual enclosure of the validation solve.
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
    flux_certified = true;
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
      // The variance of the measured fluxes is the covariance of the native
      // samples contracted with the influence of the samples on the fluxes:
      // C_flux = sum_j (T e_j)(T e_j)^T C_native.  The contraction is only
      // carried out for a moderate number of samples; otherwise the variance
      // stays unknown instead of being approximated.
      const std::size_t support_columns = indices.size();
      if (support_columns <= 2048) {
        auto native_covariance = std::make_shared<CovarianceAction>();
        native_covariance->model = observation->noise;
        native_covariance->input_output.domain = observation->native_index;
        native_covariance->input_output.codomain = std::make_shared<Index>(flux_index);
        native_covariance->input_output.input = observation->noise->variance_unit;
        native_covariance->input_output.output = observation->noise->variance_unit;
        native_covariance->assessment = model::make_assessment(Code::ok);
        std::vector<double> matrix(parameters * parameters, 0.0);
        bool complete = true;
        for (std::size_t column : indices) {
          std::vector<double> functional(parameters, 0.0);
          for (std::size_t c = 0; c < parameters; ++c) {
            // The influence of one native sample on the measured parameters.
            for (std::size_t k = 0; k < rows; ++k) {
              if (static_cast<U64>(k) != column) continue;
              functional[c] = inverse[k][c];
            }
          }
          const std::vector<U64> functional_shape{static_cast<U64>(parameters)};
          Result<ArrayRef> functional_array =
              write_f64_array(context, functional, functional_shape,
                              model::unit_dimensionless(), flux_index.identity.object_id);
          if (!ok(functional_array)) {
            complete = false;
            break;
          }
          Result<acsd::Variance> quadratic =
              quadratic_form(context, native_covariance, val(functional_array));
          // An unknown noise term makes the quadratic form unavailable: the
          // variance stays unknown and is never filled with zero.
          if (!ok(quadratic) || !val(quadratic).value.has_value()) {
            complete = false;
            break;
          }
          for (std::size_t a = 0; a < parameters; ++a) {
            for (std::size_t b = 0; b < parameters; ++b) {
              matrix[a * parameters + b] +=
                  *val(quadratic).value * functional[a] * functional[b];
            }
          }
        }
        if (complete) {
          for (std::size_t s = 0; s < sources; ++s) {
            flux_variance[s] = matrix[s * parameters + s];
          }
          variance_known = true;
        }
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
