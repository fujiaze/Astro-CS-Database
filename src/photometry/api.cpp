// The public photometry entry points declared in include/acsd/contracts.hpp.
//
//   Result<Ref<PhotoSolution>> fit_photometry(Context&, measurements, stars, predictions,
//                                              const PhotometryRequest&)
//   Result<Ref<NativeObservation>> apply_scale(Context&, solution, observation, frame)
//
// Scientific contract: docs/science/foundation/GAIA_RESPONSE.md §1-§3,
// docs/engineering/rebuild/SYSTEM.md N3 and
// docs/engineering/rebuild/API.md (fit_photometry / apply_scale rows).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "photometry/joint_fit.hpp"
#include "photometry/spatial_basis.hpp"

namespace acsd {
namespace {

using photometry::FrameInput;
using photometry::FitSettings;
using photometry::StarInput;

constexpr const char* kFitOrigin = "fit_photometry";
constexpr const char* kScaleOrigin = "apply_scale";

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array,
                                            const std::string& what) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " array is missing",
                                             kFitOrigin, "the joint fit cannot be assembled");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model,
                                             "the " + what + " array is not binary64", kFitOrigin,
                                             "the joint fit cannot be assembled");
  }
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) {
      total = 0;
      break;
    }
    if (total > (~0ull) / extent) {
      return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " shape overflows",
                                               kFitOrigin, "the joint fit cannot be assembled");
    }
    total *= extent;
  }
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, total});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the " + what + " array cannot be read",
                                             kFitOrigin, "the joint fit cannot be assembled");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<ArrayRef> copy_f64(Context& context, const std::vector<double>& values,
                          const std::vector<U64>& shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

// Builds a sparse action from a dense matrix (row major).
Result<Ref<LinearAction>> make_dense_action(Context& context, const numerics::Matrix& matrix,
                                             const Id& domain_index, const Id& codomain_index,
                                             const Unit& unit, const Id& application) {
  std::vector<U64> offsets(matrix.rows + 1, 0);
  std::vector<U64> indices;
  std::vector<double> values;
  for (std::size_t row = 0; row < matrix.rows; ++row) {
    for (std::size_t column = 0; column < matrix.cols; ++column) {
      const double value = matrix(row, column);
      if (value != 0.0) {
        indices.push_back(static_cast<U64>(column));
        values.push_back(value);
      }
    }
    offsets[row + 1] = static_cast<U64>(values.size());
  }
  // The offsets and the indices are unsigned 64 bit arrays.
  const std::vector<std::byte> offset_bytes = model::encode_u64(offsets);
  const std::vector<std::byte> index_bytes = model::encode_u64(indices);
  const std::vector<std::byte> value_bytes = model::encode_f64(values);
  const std::vector<U64> offset_shape{static_cast<U64>(offsets.size())};
  const std::vector<U64> index_shape{static_cast<U64>(indices.size())};
  const std::vector<U64> value_shape{static_cast<U64>(values.size())};
  Result<ArrayRef> offset_storage = context.storage.copy(context, DType::u64, offset_bytes,
                                                           offset_shape, unit, domain_index);
  if (!ok(offset_storage)) {
    return model::fail<Ref<LinearAction>>(offset_storage.code, "the sparse offsets cannot be stored",
                                          kFitOrigin, "the action cannot be built");
  }
  Result<ArrayRef> index_storage = context.storage.copy(context, DType::u64, index_bytes, index_shape,
                                                         unit, domain_index);
  if (!ok(index_storage)) {
    return model::fail<Ref<LinearAction>>(index_storage.code, "the sparse indices cannot be stored",
                                          kFitOrigin, "the action cannot be built");
  }
  Result<ArrayRef> value_storage = context.storage.copy(context, DType::f64, value_bytes, value_shape,
                                                         unit, domain_index);
  if (!ok(value_storage)) {
    return model::fail<Ref<LinearAction>>(value_storage.code, "the sparse values cannot be stored",
                                          kFitOrigin, "the action cannot be built");
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::csr;
  recipe.algorithm = "canonical_csr_v1";
  recipe.version = 1;
  // The recipe declares the arrays it consumes; the names are resolved by the
  // product reader, the public entry point only checks the count.
  recipe.arrays = {"offsets", "indices", "values"};
  recipe.spaces.domain = nullptr;
  recipe.spaces.codomain = nullptr;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  const std::vector<ArrayRef> arrays{val(offset_storage), val(index_storage), val(value_storage)};
  Result<Ref<LinearAction>> action =
      make_action(context, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (!ok(action)) {
    return model::fail<Ref<LinearAction>>(action.code, "the sparse action cannot be built", kFitOrigin,
                                          "the action cannot be built");
  }
  (void)domain_index;
  (void)codomain_index;
  (void)application;
  return action;
}

// The identity action on a parameter index.
Result<Ref<LinearAction>> make_identity_action(Context& context, const Index& index) {
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::identity;
  recipe.algorithm = "canonical_identity_v1";
  recipe.version = 1;
  recipe.spaces.domain = std::make_shared<const Index>(index);
  recipe.spaces.codomain = std::make_shared<const Index>(index);
  recipe.spaces.input = model::unit_dimensionless();
  recipe.spaces.output = model::unit_dimensionless();
  Result<Ref<LinearAction>> action =
      make_action(context, recipe, std::span<const Ref<LinearAction>>{},
                  std::span<const ArrayRef>{});
  if (!ok(action)) {
    return model::fail<Ref<LinearAction>>(action.code, "the identity action cannot be built",
                                          kFitOrigin, "the action cannot be built");
  }
  return action;
}

}  // namespace

// ---------------------------------------------------------------------------
// fit_photometry
// ---------------------------------------------------------------------------

Result<Ref<PhotoSolution>> fit_photometry(Context& context,
                                           std::span<const Ref<SourceMeasurements>> measurements,
                                           std::span<const CatalogueStar> stars,
                                           std::span<const Ref<SpectralPrediction>> predictions,
                                           const PhotometryRequest& request) {
  if (measurements.empty() || predictions.empty() || stars.empty()) {
    return model::fail<Ref<PhotoSolution>>(Code::invalid_request,
                                           "the joint fit needs measurements, stars and predictions",
                                           kFitOrigin, "no photometric solution can be produced");
  }
  if (request.frames.size() != measurements.size() || request.frames.size() != predictions.size() ||
      request.responses.size() != measurements.size()) {
    return model::fail<Ref<PhotoSolution>>(
        Code::invalid_request,
        "the frames, the responses, the measurements and the predictions must have the same length",
        kFitOrigin, "no photometric solution can be produced");
  }
  if (!request.likelihood.kind.empty() && request.likelihood.kind != "joint_flux_gaussian") {
    return model::fail<Ref<PhotoSolution>>(
        Code::unsupported, "only the joint flux domain Gaussian likelihood is implemented", kFitOrigin,
        "no photometric solution can be produced");
  }
  if (request.application_id.empty()) {
    return model::fail<Ref<PhotoSolution>>(Code::invalid_request, "an application id is required",
                                           kFitOrigin, "no photometric solution can be produced");
  }
  for (std::size_t k = 0; k < measurements.size(); ++k) {
    if (!measurements[k]) {
      return model::fail<Ref<PhotoSolution>>(Code::invalid_request, "a measurement set is missing",
                                             kFitOrigin, "no photometric solution can be produced");
    }
    if (measurements[k]->frame != request.frames[k]) {
      return model::fail<Ref<PhotoSolution>>(
          Code::invalid_request, "the measurement frames do not follow the requested frame order",
          kFitOrigin, "no photometric solution can be produced");
    }
    if (!predictions[k]) {
      return model::fail<Ref<PhotoSolution>>(Code::invalid_request, "a spectral prediction is missing",
                                             kFitOrigin, "no photometric solution can be produced");
    }
    if (predictions[k]->response->identity.content_hash !=
        request.responses[k]->identity.content_hash) {
      return model::fail<Ref<PhotoSolution>>(
          Code::invalid_request, "the prediction and the requested response are different objects",
          kFitOrigin, "no photometric solution can be produced");
    }
  }
  // The predictions must share one XP basis, and it must be the basis of the
  // measured stars.
  const Id& basis_hash = predictions.front()->basis->identity.content_hash;
  for (const Ref<SpectralPrediction>& prediction : predictions) {
    if (prediction->basis->identity.content_hash != basis_hash) {
      return model::fail<Ref<PhotoSolution>>(
          Code::invalid_request, "the predictions do not share one XP basis", kFitOrigin,
          "no photometric solution can be produced");
    }
  }
  for (const CatalogueStar& star : stars) {
    if (!star.xp_basis || star.xp_basis->identity.content_hash != basis_hash) {
      return model::fail<Ref<PhotoSolution>>(
          Code::invalid_request, "the catalogue stars do not use the XP basis of the predictions",
          kFitOrigin, "no photometric solution can be produced");
    }
  }

  // ---------------------------------------------------------------------
  // Join the stars with the measurements by source id.
  // ---------------------------------------------------------------------
  std::vector<std::size_t> star_of_source;
  {
    // source id -> index in the star table
    std::vector<std::pair<U64, std::size_t>> table;
    for (std::size_t i = 0; i < stars.size(); ++i) {
      table.emplace_back(stars[i].source_id, i);
    }
    std::sort(table.begin(), table.end());
    star_of_source.assign(table.size(), 0);
    for (std::size_t i = 0; i < table.size(); ++i) {
      star_of_source[i] = table[i].second;
    }
  }
  auto lookup_star = [&](U64 source_id) -> std::size_t {
    const auto it = std::lower_bound(
        star_of_source.begin(), star_of_source.end(), source_id,
        [&](std::size_t index, U64 value) { return stars[index].source_id < value; });
    if (it == star_of_source.end() || stars[*it].source_id != source_id) {
      return stars.size();
    }
    return *it;
  };

  // The measured source ids must be exactly the union of the requested stars.
  std::vector<bool> used(stars.size(), false);
  std::vector<std::size_t> star_index_of;
  for (std::size_t k = 0; k < measurements.size(); ++k) {
    for (const StarFlux& flux : measurements[k]->stars) {
      const std::size_t index = lookup_star(flux.gaia_source_id);
      if (index == stars.size()) {
        return model::fail<Ref<PhotoSolution>>(
            Code::invalid_request, "a measured source id is not present in the catalogue stars",
            kFitOrigin, "no photometric solution can be produced");
      }
      if (!used[index]) {
        used[index] = true;
        star_index_of.push_back(index);
      }
    }
  }
  if (star_index_of.size() != stars.size()) {
    return model::fail<Ref<PhotoSolution>>(
        Code::invalid_request, "the catalogue stars are not exactly the measured source ids",
        kFitOrigin, "no photometric solution can be produced");
  }
  std::sort(star_index_of.begin(), star_index_of.end());

  // The XP coefficients of the measured stars.
  std::vector<StarInput> star_inputs;
  for (std::size_t index : star_index_of) {
    StarInput input;
    input.source_id = stars[index].source_id;
    Result<std::vector<double>> coefficients =
        read_f64_array(context, stars[index].xp_coefficients, "XP coefficients");
    if (!ok(coefficients)) {
      return model::fail<Ref<PhotoSolution>>(coefficients.code, "the XP coefficients cannot be read",
                                              kFitOrigin, "no photometric solution can be produced");
    }
    input.gaia = val(coefficients);
    input.noise = stars[index].xp_noise;
    star_inputs.push_back(std::move(input));
  }

  // ---------------------------------------------------------------------
  // Assemble the frames.
  // ---------------------------------------------------------------------
  std::vector<FrameInput> frame_inputs;
  for (std::size_t k = 0; k < measurements.size(); ++k) {
    FrameInput frame;
    frame.frame = request.frames[k];
    frame.measurements = measurements[k].get();
    frame.prediction = predictions[k].get();
    Result<std::vector<double>> integrals =
        read_f64_array(context, predictions[k]->basis_integrals, "basis integrals");
    if (!ok(integrals)) {
      return model::fail<Ref<PhotoSolution>>(integrals.code, "the basis integrals cannot be read",
                                              kFitOrigin, "no photometric solution can be produced");
    }
    frame.v = val(integrals);
    Result<std::vector<double>> errors =
        read_f64_array(context, predictions[k]->absolute_errors, "integration errors");
    if (!ok(errors)) {
      return model::fail<Ref<PhotoSolution>>(errors.code, "the integration errors cannot be read",
                                              kFitOrigin, "no photometric solution can be produced");
    }
    frame.dv = val(errors);

    // The numerical flux enclosures are optional: a missing enclosure is
    // never replaced by a zero radius.  The fit then runs, but the numerical
    // parameter box cannot be certified.
    std::vector<double> low;
    std::vector<double> high;
    bool frame_enclosed = false;
    if (measurements[k]->flux_numerical_lower && measurements[k]->flux_numerical_upper) {
      Result<std::vector<double>> lower =
          read_f64_array(context, measurements[k]->flux_numerical_lower, "flux enclosures");
      Result<std::vector<double>> upper =
          read_f64_array(context, measurements[k]->flux_numerical_upper, "flux enclosures");
      if (!ok(lower) || !ok(upper)) {
        return model::fail<Ref<PhotoSolution>>(
            Code::invalid_model, "the flux enclosures cannot be read", kFitOrigin,
            "no photometric solution can be produced");
      }
      low = val(lower);
      high = val(upper);
      if (low.size() != measurements[k]->stars.size() || high.size() != low.size()) {
        return model::fail<Ref<PhotoSolution>>(
            Code::invalid_model, "the flux enclosures do not match the measured stars", kFitOrigin,
            "no photometric solution can be produced");
      }
      frame_enclosed = true;
    }
    for (std::size_t m = 0; m < measurements[k]->stars.size(); ++m) {
      const StarFlux& flux = measurements[k]->stars[m];
      const std::size_t index = lookup_star(flux.gaia_source_id);
      frame.star.push_back(static_cast<std::size_t>(
          std::find(star_index_of.begin(), star_index_of.end(), index) - star_index_of.begin()));
      frame.counts.push_back(flux.integrated_flux);
      frame.position_x.push_back(flux.position.x);
      frame.position_y.push_back(flux.position.y);
      if (frame_enclosed) {
        frame.flux_low.push_back(low[m]);
        frame.flux_high.push_back(high[m]);
      } else {
        // A missing enclosure is recorded as a degenerate interval that the
        // joint fit refuses to certify; it is never used as a zero radius.
        frame.flux_low.push_back(frame.counts[m]);
        frame.flux_high.push_back(frame.counts[m]);
      }
      if (flux.variance.value.has_value() && *(flux.variance.value) > 0.0) {
        frame.variance.push_back(*(flux.variance.value));
      } else {
        // An unknown variance is never replaced by zero: the fit falls back to
        // equal weights and records the missing term.
        frame.variance.push_back(1.0);
        frame.variance_known = false;
      }
    }
    if (!frame_enclosed) {
      frame.variance_known = frame.variance_known;  // the variance flag is separate
      frame.enclosure_missing = true;
    }
    frame_inputs.push_back(std::move(frame));
  }

  // ---------------------------------------------------------------------
  // The spatial basis: the training weighted mean of the spatial terms.
  // ---------------------------------------------------------------------
  const std::size_t terms = photometry::basis_term_count(request.basis);
  std::vector<double> weighted_mean;
  {
    Result<std::vector<double>> provided =
        read_f64_array(context, request.basis.weighted_mean, "basis weighted mean");
    if (ok(provided) && !val(provided).empty()) {
      weighted_mean = val(provided);
    } else {
      // Compute the training weighted mean over the measured stars.
      weighted_mean.assign(terms, 0.0);
      std::vector<double> weight_sum(terms, 0.0);
      double total_weight = 0.0;
      std::vector<double> buffer;
      for (const FrameInput& frame : frame_inputs) {
        for (std::size_t m = 0; m < frame.counts.size(); ++m) {
          const double weight = 1.0 / frame.variance[m];
          photometry::basis_terms(request.basis, std::vector<double>(terms, 0.0), frame.position_x[m],
                                  frame.position_y[m], buffer);
          for (std::size_t term = 1; term < terms; ++term) {
            weighted_mean[term] += weight * buffer[term];
            weight_sum[term] += weight;
          }
          total_weight += weight;
        }
      }
      for (std::size_t term = 1; term < terms; ++term) {
        weighted_mean[term] = weight_sum[term] > 0.0 ? weighted_mean[term] / weight_sum[term] : 0.0;
      }
      (void)total_weight;
    }
  }

  // ---------------------------------------------------------------------
  // Run the joint fit.
  // ---------------------------------------------------------------------
  FitSettings settings;
  settings.gradient_tolerance =
      request.gradient_tolerance > 0.0 ? request.gradient_tolerance : 1e-8;
  settings.step_tolerance = request.step_tolerance > 0.0 ? request.step_tolerance : 1e-10;
  settings.max_outer_iterations =
      request.max_outer_iterations > 0 ? request.max_outer_iterations : 100;
  settings.rank_relative = request.solve.rank_relative > 0.0 ? request.solve.rank_relative : 1e-12;
  settings.huber = request.solve.huber;
  settings.huber_delta = request.solve.huber_delta > 0.0 ? request.solve.huber_delta : 1.345;

  Result<photometry::FitResult> fit =
      photometry::run_joint_fit(context, frame_inputs, star_inputs, request.basis, weighted_mean,
                                settings);
  if (!ok(fit)) {
    return model::fail<Ref<PhotoSolution>>(fit.code, "the joint photometric fit failed", kFitOrigin,
                                           "no photometric solution can be produced");
  }
  const photometry::FitResult& outcome = val(fit);

  if (!outcome.identifiable) {
    // The rank deficiency is reported with its null space; no scale is
    // published and no ridge is added.
    std::vector<double> nullspace;
    const std::size_t null_dim = outcome.rank.nullspace.size();
    for (const std::vector<double>& vector : outcome.rank.nullspace) {
      nullspace.insert(nullspace.end(), vector.begin(), vector.end());
    }
    const Id index_id = Id();
    Result<ArrayRef> nullspace_array =
        copy_f64(context, nullspace, {static_cast<U64>(null_dim),
                                       static_cast<U64>(null_dim > 0 ? nullspace.size() / null_dim : 0)},
                  model::unit_dimensionless(), index_id);
    if (!ok(nullspace_array)) {
      return model::fail<Ref<PhotoSolution>>(nullspace_array.code, "the null space cannot be stored",
                                              kFitOrigin, "the rank deficiency cannot be reported");
    }
    auto solution = std::make_shared<PhotoSolution>();
    solution->theta = nullptr;
    solution->spatial_basis = request.basis;
    solution->responses = request.responses;
    solution->reference = request.reference;
    solution->rank.columns = outcome.rank.columns;
    solution->rank.rank = outcome.rank.rank;
    solution->rank.nullspace = val(nullspace_array);
    solution->rank.singular_error_bound = outcome.rank.singular_error_bound;
    solution->rank.certified = outcome.rank.certified;
    solution->frames = request.frames;
    solution->application_id = request.application_id;
    solution->parameter_lower = nullptr;
    solution->parameter_upper = nullptr;
    solution->assessment = model::make_assessment(Code::not_identifiable);
    solution->assessment.complete = false;
    solution->assessment.execution = ExecutionState::complete;
    solution->assessment.science = ScienceState::conditional;
    model::add_issue(solution->assessment, Code::not_identifiable,
                     "the joint design matrix is rank deficient in the scale parameters",
                     kFitOrigin, "no photometric scale can be published",
                     "add stars that cover the spatial basis");
    return model::fail<Ref<PhotoSolution>>(Code::not_identifiable,
                                           "the joint design matrix is rank deficient in the scale "
                                           "parameters",
                                           kFitOrigin, "no photometric scale can be published");
  }
  if (!outcome.converged) {
    auto solution = std::make_shared<PhotoSolution>();
    solution->assessment = model::make_assessment(Code::nonconverged);
    solution->assessment.complete = false;
    solution->assessment.execution = ExecutionState::failed;
    model::add_issue(solution->assessment, Code::nonconverged,
                     outcome.failure.empty() ? "the joint photometric fit did not converge"
                                             : outcome.failure,
                     kFitOrigin, "no photometric scale can be published",
                     "relax the tolerances or provide a better conditioned data set");
    return model::fail<Ref<PhotoSolution>>(Code::nonconverged,
                                           outcome.failure.empty()
                                               ? "the joint photometric fit did not converge"
                                               : outcome.failure,
                                           kFitOrigin, "no photometric scale can be published");
  }

  // ---------------------------------------------------------------------
  // Publish the solution.
  // ---------------------------------------------------------------------
  const std::size_t frames = request.frames.size();
  const Id parameter_index_id = std::string("photometry:") + request.application_id;
  Index parameter_index = model::make_parameter_index({parameter_index_id});

  Result<ArrayRef> theta_array =
      copy_f64(context, outcome.theta, {static_cast<U64>(frames), static_cast<U64>(terms)},
               model::unit_dimensionless(), parameter_index.identity.object_id);
  if (!ok(theta_array)) {
    return model::fail<Ref<PhotoSolution>>(theta_array.code, "the parameters cannot be stored",
                                           kFitOrigin, "the solution cannot be published");
  }

  auto solution = std::make_shared<PhotoSolution>();
  solution->theta = val(theta_array);
  solution->spatial_basis = request.basis;
  solution->responses = request.responses;
  solution->reference = request.reference;
  solution->frames = request.frames;
  solution->application_id = request.application_id;

  // The training support is the bounding box of the measured positions.
  {
    Box support;
    support.lower = {std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity()};
    support.upper = {-std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()};
    for (const FrameInput& frame : frame_inputs) {
      for (std::size_t m = 0; m < frame.position_x.size(); ++m) {
        support.lower[0] = std::min(support.lower[0], frame.position_x[m]);
        support.lower[1] = std::min(support.lower[1], frame.position_y[m]);
        support.upper[0] = std::max(support.upper[0], frame.position_x[m]);
        support.upper[1] = std::max(support.upper[1], frame.position_y[m]);
      }
    }
    if (!std::isfinite(support.lower[0])) {
      support.lower = {0.0, 0.0};
      support.upper = {0.0, 0.0};
    }
    solution->support = support;
  }

  // The weighted mean that was actually used is stored with the solution so
  // that apply_scale reduces the spatial terms in exactly the same way.
  {
    Result<ArrayRef> mean_array =
        copy_f64(context, weighted_mean, {static_cast<U64>(weighted_mean.size())},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    if (ok(mean_array)) {
      solution->spatial_basis.weighted_mean = val(mean_array);
    }
  }

  // The rank report.
  {
    std::vector<double> nullspace;
    const std::size_t null_dim = outcome.rank.nullspace.size();
    for (const std::vector<double>& vector : outcome.rank.nullspace) {
      nullspace.insert(nullspace.end(), vector.begin(), vector.end());
    }
    Result<ArrayRef> singular_array =
        copy_f64(context, outcome.rank.singular_values,
                 {static_cast<U64>(outcome.rank.singular_values.size())},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    Result<ArrayRef> nullspace_array =
        copy_f64(context, nullspace,
                 {static_cast<U64>(null_dim),
                  static_cast<U64>(null_dim > 0 ? nullspace.size() / null_dim : 0)},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    if (!ok(singular_array) || !ok(nullspace_array)) {
      return model::fail<Ref<PhotoSolution>>(Code::internal, "the rank report cannot be stored",
                                             kFitOrigin, "the solution cannot be published");
    }
    solution->rank.columns = outcome.rank.columns;
    solution->rank.rank = outcome.rank.rank;
    solution->rank.singular_values = val(singular_array);
    solution->rank.nullspace = val(nullspace_array);
    solution->rank.singular_error_bound = outcome.rank.singular_error_bound;
    solution->rank.certified = outcome.rank.certified;
  }

  // The numerical parameter box.
  if (outcome.parameter_box_certified) {
    Result<ArrayRef> lower_array =
        copy_f64(context, outcome.parameter_lower, {static_cast<U64>(frames), static_cast<U64>(terms)},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    Result<ArrayRef> upper_array =
        copy_f64(context, outcome.parameter_upper, {static_cast<U64>(frames), static_cast<U64>(terms)},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    if (!ok(lower_array) || !ok(upper_array)) {
      return model::fail<Ref<PhotoSolution>>(Code::internal, "the parameter box cannot be stored",
                                             kFitOrigin, "the solution cannot be published");
    }
    solution->parameter_lower = val(lower_array);
    solution->parameter_upper = val(upper_array);
  }

  // The parameter covariance as a covariance action.
  {
    const std::size_t n_theta = outcome.theta.size();
    numerics::Matrix covariance(n_theta, n_theta);
    for (std::size_t i = 0; i < n_theta; ++i) {
      for (std::size_t j = 0; j < n_theta; ++j) {
        covariance(i, j) = outcome.covariance[i * n_theta + j];
      }
    }
    // L with L L^T = covariance: the eigen factor of the covariance.
    numerics::EigenResult eigen = numerics::symmetric_eigen(covariance);
    numerics::Matrix factor(n_theta, n_theta);
    if (eigen.converged) {
      for (std::size_t mode = 0; mode < eigen.eigenvalues.size(); ++mode) {
        if (!(eigen.eigenvalues[mode] > 0.0)) {
          continue;
        }
        const double root = std::sqrt(eigen.eigenvalues[mode]);
        for (std::size_t i = 0; i < n_theta; ++i) {
          for (std::size_t j = 0; j < n_theta; ++j) {
            factor(i, j) += eigen.eigenvectors(i, mode) * eigen.eigenvectors(j, mode) / root;
          }
        }
      }
    }
    Result<Ref<LinearAction>> factor_action = make_dense_action(
        context, factor, parameter_index.identity.object_id, parameter_index.identity.object_id,
        model::unit_dimensionless(), request.application_id);
    if (!ok(factor_action)) {
      return model::fail<Ref<PhotoSolution>>(factor_action.code, "the covariance factor cannot be built",
                                             kFitOrigin, "the solution cannot be published");
    }
    Result<Ref<LinearAction>> identity_action = make_identity_action(context, parameter_index);
    if (!ok(identity_action)) {
      return model::fail<Ref<PhotoSolution>>(identity_action.code, "the covariance influence cannot be built",
                                             kFitOrigin, "the solution cannot be published");
    }
    auto root = std::make_shared<NoiseRoot>();
    root->shared_id = SharedId{"photometry", request.application_id, "1", "parameter"};
    root->coordinates = std::make_shared<const Index>(parameter_index);
    root->unit = model::unit_variance(request.application_id);
    root->factor = val(factor_action);
    auto noise_model = std::make_shared<NoiseModel>();
    noise_model->variance_unit = model::unit_variance(request.application_id);
    noise_model->condition = model::make_condition("photometric_scale", "joint_flux_gaussian");
    noise_model->shared_factors.push_back(root);
    noise_model->influence.push_back(val(identity_action));
    noise_model->state = NoiseState::known;
    Result<Ref<CovarianceAction>> covariance_action = make_covariance(context, noise_model);
    if (!ok(covariance_action)) {
      return model::fail<Ref<PhotoSolution>>(covariance_action.code, "the parameter covariance cannot be built",
                                             kFitOrigin, "the solution cannot be published");
    }
    solution->covariance = val(covariance_action);
  }

  // The influence: the linearised map from a measurement perturbation to the
  // parameter perturbation.
  {
    const std::size_t n_theta = outcome.theta.size();
    Result<ArrayRef> linearization =
        copy_f64(context, outcome.theta, {static_cast<U64>(frames), static_cast<U64>(terms)},
                 model::unit_dimensionless(), parameter_index.identity.object_id);
    if (!ok(linearization)) {
      return model::fail<Ref<PhotoSolution>>(linearization.code, "the linearization point cannot be stored",
                                             kFitOrigin, "the solution cannot be published");
    }
    solution->influence.linearization_point = val(linearization);
    solution->influence.condition =
        model::make_condition("photometric_scale", "joint_flux_gaussian");
    solution->influence.roots.push_back("photometry:" + request.application_id);
  }

  // The error record.
  {
    auto error = std::make_shared<ErrorRecord>();
    error->linearization_selection =
        model::make_condition("photometric_scale", "joint_flux_gaussian");
    error->unknown_model_terms = outcome.unknown_terms;
    for (const std::string& note : outcome.notes) {
      error->unknown_model_terms.push_back(note);
    }
    solution->error = error;
  }

  model::Sha256 hasher;
  hasher.field("application", request.application_id);
  hasher.field("basis", basis_hash);
  for (const Id& frame : request.frames) {
    hasher.field("frame", frame);
  }
  for (double value : outcome.theta) {
    hasher.update_f64(value);
  }
  solution->identity = model::make_identity("acsd.PhotoSolution", 2, 0,
                                            model::content_id(hasher.hex()), "acsd");
  solution->assessment = model::make_assessment(Code::ok);
  const bool unknown_terms = !outcome.unknown_terms.empty();
  solution->assessment.complete = outcome.parameter_box_certified && !unknown_terms;
  solution->assessment.execution = ExecutionState::complete;
  solution->assessment.science =
      (outcome.parameter_box_certified && !unknown_terms) ? ScienceState::bounded
                                                         : ScienceState::conditional;
  solution->assessment.validity = Validity::approximate;
  solution->assessment.condition =
      model::make_condition("photometric_scale", "joint_flux_gaussian");
  model::set_included(solution->assessment, "joint_flux_gaussian_profile_likelihood");
  model::set_included(solution->assessment, "gauss_newton_armijo_backtracking");
  model::set_included(solution->assessment, "shared_latent_across_frames");
  model::set_omitted(solution->assessment, "xp_resolution_model_bias");
  model::set_omitted(solution->assessment, "aperture_and_psf_model_bias");
  if (!outcome.parameter_box_certified) {
    model::add_issue(solution->assessment, Code::unavailable,
                     "the numerical parameter box could not be certified", kFitOrigin,
                     "the scale is conditional and apply_scale cannot give an unconditional numerical "
                     "certificate",
                     "provide finite flux enclosures for every measured star");
  }
  for (const std::string& term : outcome.unknown_terms) {
    model::add_issue(solution->assessment, Code::unavailable, term, kFitOrigin,
                     "the corresponding uncertainty is not quantified", "");
  }
  return model::ok_result(std::shared_ptr<const PhotoSolution>(solution));
}

}  // namespace acsd
