#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/footprint_provider.hpp"
#include "geometry/drizzle_core.hpp"
#include "geometry/drizzle_package.hpp"
#include "geometry/kernel.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/interval.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::Rational;

constexpr double kPi = 3.14159265358979323846;

struct IntervalValue {
  double lower = 0.0;
  double upper = 0.0;
};

// The four rules go through the directed rounding interval primitives of the
// numerics module, so the published B enclosure is outward (the local plain
// double min/max of the four products is not).
numerics::Interval as_interval(const IntervalValue& value) {
  return numerics::Interval(value.lower, value.upper);
}

IntervalValue multiply(const IntervalValue& a, const IntervalValue& b) {
  const numerics::Interval product = numerics::interval_mul(as_interval(a), as_interval(b));
  return IntervalValue{product.lower, product.upper};
}

IntervalValue add(const IntervalValue& a, const IntervalValue& b) {
  const numerics::Interval sum = numerics::interval_add(as_interval(a), as_interval(b));
  return IntervalValue{sum.lower, sum.upper};
}

IntervalValue divide(const IntervalValue& a, const IntervalValue& b) {
  if (!(std::fabs(b.lower) > 0.0) || b.lower <= 0.0) {
    return IntervalValue{-std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity()};
  }
  const numerics::Interval quotient = numerics::interval_div(as_interval(a), as_interval(b));
  return IntervalValue{quotient.lower, quotient.upper};
}

double representative(const IntervalValue& value) { return 0.5 * (value.lower + value.upper); }

// An upper bound of the solid angle of one true leaf: Omega_H = pi / (3 N^2)
// (SPHERICAL_DRIZZLE Eq. (4)).  It bounds every overlap mass a_jp from above,
// so it is rounded upward (the pi literal is the nearest double, and the
// quotient is rounded to nearest: one ulp up encloses the true value).
double leaf_solid_angle_upper(U32 order) {
  const double nside = static_cast<double>(1ull << order);
  const double inf = std::numeric_limits<double>::infinity();
  const double pi_upper = std::nextafter(kPi, inf);
  const double denominator = 3.0 * nside * nside;
  return std::nextafter(pi_upper / denominator, inf);
}

Result<ArrayRef> store_f64(Context& context, const std::vector<double>& values, const Unit& unit,
                           const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

Result<ArrayRef> store_u64(Context& context, const std::vector<U64>& values,
                           const std::vector<U64>& shape, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u64(values);
  return context.storage.copy(context, DType::u64, bytes, shape,
                              model::unit_dimensionless(), index_id);
}

Result<ArrayRef> store_u8(Context& context, const std::vector<std::uint8_t>& values,
                          const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u8(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, DType::u8, bytes, shape, model::unit_dimensionless(), index_id);
}

}  // namespace

// ---------------------------------------------------------------------------
// build_drizzle
// ---------------------------------------------------------------------------

// The model packaging of a finished drizzle accumulation.  build_drizzle and
// drizzle_grid share it: the estimator (DrizzleAccumulator) and the objects
// that expose it are one implementation.

Result<DrizzleResult> package_drizzle(Context& context,
                                      const geometry::DrizzleLeafOutput& leaf_output,
                                      Ref<NativeObservation> observation,
                                      const DrizzleRequest& request, U64 source_count,
                                      const DrizzleGeometrySource& source) {
  using model::fail;
  using model::ok_result;
  if (!leaf_output.any_present) {
    // No leaf has a certified normaliser: the frame has no signal at all.
    return fail<DrizzleResult>(Code::no_support, "no_support_rows_present", "build_drizzle",
                               "no target leaf has a positive support");
  }
  const std::size_t leaf_count = leaf_output.leaves.size();
  const std::vector<HealpixLeaf>& leaves = leaf_output.leaves;
  const std::vector<double>& normalizer = leaf_output.normalizer;
  const std::vector<double>& signal = leaf_output.signal;
  const std::vector<double>& signal_lower = leaf_output.signal_lower;
  const std::vector<double>& signal_upper = leaf_output.signal_upper;
  const std::vector<std::uint8_t>& support_states = leaf_output.support_states;
  const std::vector<std::uint8_t>& normalization_states = leaf_output.normalization_states;
  const std::vector<std::uint8_t>& observational_states = leaf_output.observational_states;
  const std::vector<double>& area_lower = leaf_output.area_lower;
  const std::vector<double>& area_upper = leaf_output.area_upper;
  const std::vector<U64>& row_offsets = leaf_output.row_offsets;
  const std::vector<U64>& row_indices = leaf_output.row_indices;
  const std::vector<double>& row_values = leaf_output.row_values;
  const bool any_unresolved = leaf_output.any_unresolved;
  const bool any_absent = leaf_output.any_absent;
  const bool any_missing_error = leaf_output.any_missing_error;
  const bool any_drop_lower_not_positive = leaf_output.any_drop_lower_not_positive;
  // The leaf index owns its [N,2] (order,ipix) identifiers (OBJECTS:104) and
  // the arrays it orders reference that one identity; a hard-coded index id
  // made the product unusable for mosaic/export.
  auto leaf_index =
      std::make_shared<Index>(model::make_leaf_index(leaves));
  std::vector<U64> leaf_ids;
  leaf_ids.reserve(leaves.size() * 2);
  for (const HealpixLeaf& leaf : leaves) {
    leaf_ids.push_back(leaf.order);
    leaf_ids.push_back(leaf.ipix);
  }
  const Id leaf_index_id = leaf_index->identity.content_hash;
  Result<ArrayRef> ids_array = store_u64(
      context, leaf_ids,
      {static_cast<U64>(leaves.size()), static_cast<U64>(2)}, leaf_index_id);
  if (!ok(ids_array)) {
    return fail<DrizzleResult>(ids_array.code, "leaf_ids_store_failed",
                               "build_drizzle");
  }
  leaf_index->ids = val(ids_array);
  Result<ArrayRef> normalizer_array =
      store_f64(context, normalizer, model::unit_sr(), leaf_index_id);
  if (!ok(normalizer_array)) {
    return fail<DrizzleResult>(normalizer_array.code, "normalizer_store_failed", "build_drizzle");
  }
  // The signal is F/N: a native integrated value (the observation unit) per
  // steradian, so the unit of the array and of the estimator is the
  // observation unit divided by sr, not sr/pixel.
  const Unit signal_unit = model::unit_multiply(observation->unit, model::unit_per_sr());
  Result<ArrayRef> signal_array = store_f64(context, signal, signal_unit, leaf_index_id);
  if (!ok(signal_array)) {
    return fail<DrizzleResult>(signal_array.code, "signal_store_failed", "build_drizzle");
  }
  Result<ArrayRef> signal_lower_array =
      store_f64(context, signal_lower, signal_unit, leaf_index_id);
  if (!ok(signal_lower_array)) {
    return fail<DrizzleResult>(signal_lower_array.code, "signal_store_failed", "build_drizzle");
  }
  Result<ArrayRef> signal_upper_array =
      store_f64(context, signal_upper, signal_unit, leaf_index_id);
  if (!ok(signal_upper_array)) {
    return fail<DrizzleResult>(signal_upper_array.code, "signal_store_failed", "build_drizzle");
  }
  Result<ArrayRef> states_array = store_u8(context, support_states, leaf_index_id);
  if (!ok(states_array)) {
    return fail<DrizzleResult>(states_array.code, "support_store_failed", "build_drizzle");
  }
  Result<ArrayRef> area_lower_array = store_f64(context, area_lower, model::unit_sr(), leaf_index_id);
  if (!ok(area_lower_array)) {
    return fail<DrizzleResult>(area_lower_array.code, "support_store_failed", "build_drizzle");
  }
  Result<ArrayRef> area_upper_array = store_f64(context, area_upper, model::unit_sr(), leaf_index_id);
  if (!ok(area_upper_array)) {
    return fail<DrizzleResult>(area_upper_array.code, "support_store_failed", "build_drizzle");
  }
  Result<ArrayRef> normalization_states_array = store_u8(context, normalization_states, leaf_index_id);
  if (!ok(normalization_states_array)) {
    return fail<DrizzleResult>(normalization_states_array.code, "support_store_failed", "build_drizzle");
  }
  Result<ArrayRef> observational_states_array = store_u8(context, observational_states, leaf_index_id);
  if (!ok(observational_states_array)) {
    return fail<DrizzleResult>(observational_states_array.code, "support_store_failed", "build_drizzle");
  }

  Result<Ref<LinearAction>> rows = operators::make_csr_action(
      context, static_cast<U64>(leaf_count), source_count, row_offsets, row_indices, row_values,
      leaf_index_id, "acsd.drizzle.native", model::unit_per_sr());
  if (!ok(rows)) {
    return fail<DrizzleResult>(rows.code, "drizzle_action_failed", "build_drizzle");
  }

  // The support map.  Its area is the independent union enclosure, never the
  // sum of the a (I9).
  auto support = std::make_shared<SupportMap>();
  // The support is the union enclosure of the per-source overlap masses.  The
  // streaming pixel grid does not materialise a per-source primitive table,
  // so the support kind names that fact instead of publishing an empty
  // primitive string (the old product carried primitives = [""], which no
  // reader can resolve).
  support->kind = source.primitives_id.empty() ? SupportMap::Kind::effective_area_approx
                                               : SupportMap::Kind::primitives;
  support->index = leaf_index;
  if (!source.primitives_id.empty()) support->primitives.push_back(source.primitives_id);
  support->states = val(states_array);
  support->area_lower_sr = val(area_lower_array);
  support->area_upper_sr = val(area_upper_array);
  support->geometry_snapshot = source.geometry_id;
  support->normalization_states = val(normalization_states_array);
  support->observational_states = val(observational_states_array);
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.SupportMap");
    digest.add("leaf_index", leaf_index_id);
    digest.add("geometry", source.geometry_id);
    digest.add("primitives", source.primitives_id);
    digest.add("states", val(states_array)->identity.object_id);
    digest.add("area_lower", val(area_lower_array)->identity.object_id);
    digest.add("area_upper", val(area_upper_array)->identity.object_id);
    digest.add("normalization_states", val(normalization_states_array)->identity.object_id);
    digest.add("observational_states", val(observational_states_array)->identity.object_id);
    support->identity =
        model::make_identity("acsd.SupportMap", 2, 0, digest.hex(), "acsd");
  }
  support->assessment = model::make_assessment(Code::ok);
  support->assessment.complete = true;
  // The support geometry is a deterministic statement, not a noise model.
  support->assessment.noise = NoiseState::known;
  support->assessment.science = ScienceState::bounded;
  if (source.primitives_id.empty()) {
    model::set_included(support->assessment,
                        "support_kind=effective_area_approx:the per-source primitive table is "
                        "not materialised by the streaming drizzle; states and the union "
                        "area enclosure are published");
  }

  // The error record.  The leaves without a certified interval keep no
  // achieved entry: an unknown error is not a zero error.
  auto error = std::make_shared<ErrorRecord>();
  error->requested = request.budget.requested_functionals;
  error->achieved.clear();
  // The ErrorRecord has no index column (model type), so the map from the
  // achieved entries back to the leaf index is recorded in the assessment.
  std::string achieved_index_map;
  for (std::size_t i = 0; i < signal.size(); ++i) {
    if (!std::isfinite(signal_lower[i]) || !std::isfinite(signal_upper[i])) {
      continue;
    }
    ExactInterval interval;
    interval.lower = model::dyadic_from_double(signal_lower[i]);
    interval.upper = model::dyadic_from_double(signal_upper[i]);
    interval.precision_bits = 53;
    error->achieved.push_back(interval);
    if (!achieved_index_map.empty()) achieved_index_map += ",";
    achieved_index_map += std::to_string(i);
  }
  error->linearization_selection =
      model::make_condition("drizzle_surface_brightness", "interval_bounded");

  // The effective response: the drizzle operator itself acts on the native
  // samples, so it is the effective response of this frame.  Its coefficients
  // are the final R = w/N, the same operator that produced the signal and the
  // noise (I2), not w.
  auto response = std::make_shared<SpatialResponse>();
  response->spatial_actions.push_back(val(rows));
  Result<ArrayRef> weights_array = store_f64(context, {1.0}, model::unit_dimensionless(),
                                              "acsd.drizzle.weights");
  if (!ok(weights_array)) {
    return fail<DrizzleResult>(weights_array.code, "response_store_failed", "build_drizzle");
  }
  Result<ArrayRef> factors_array = store_f64(context, {1.0}, model::unit_dimensionless(),
                                              "acsd.drizzle.factors");
  if (!ok(factors_array)) {
    return fail<DrizzleResult>(factors_array.code, "response_store_failed", "build_drizzle");
  }
  response->weights = val(weights_array);
  response->reference_factors = val(factors_array);
  response->signed_response = false;
  response->condition = model::make_condition("drizzle_kernel", "same_R_for_signal_and_noise");
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.SpatialResponse");
    digest.add_csr("operator", row_offsets, row_indices, row_values);
    digest.add("weights", val(weights_array)->identity.object_id);
    digest.add("reference_factors", val(factors_array)->identity.object_id);
    digest.add_u64("signed_response", response->signed_response ? 1 : 0);
    response->identity = model::make_identity("acsd.SpatialResponse", 2, 0, digest.hex(), "acsd");
  }
  response->assessment = model::make_assessment(Code::ok);
  response->assessment.complete = true;

  // The signal estimate.
  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = val(signal_array);
  estimate->unit = signal_unit;
  estimate->estimator.id = "acsd.drizzle.brightness";
  estimate->estimator.quantity = "surface_brightness";
  estimate->estimator.unit = signal_unit;
  estimate->estimator.condition = model::make_condition("drizzle_surface_brightness",
                                                         "interval_bounded");
  estimate->numerical_lower = val(signal_lower_array);
  estimate->numerical_upper = val(signal_upper_array);
  estimate->support = support;
  estimate->effective_response = response;
  // R transforms signal, PSF and the noise roots together: the native noise
  // covariance C becomes R C R^T on the leaf index through the same operator
  // that produced the signal.  Without an input noise model the output stays
  // unknown (null), never zero.
  if (observation->noise) {
    Result<Ref<CovarianceAction>> native_covariance =
        make_covariance(context, observation->noise);
    if (ok(native_covariance)) {
      Result<Ref<CovarianceAction>> leaf_covariance =
          control_covariance(context, val(native_covariance), val(rows));
      if (ok(leaf_covariance)) estimate->noise = val(leaf_covariance);
    }
  }
  estimate->error = error;
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.SignalEstimate");
    digest.add("signal", val(signal_array)->identity.object_id);
    digest.add("lower", val(signal_lower_array)->identity.object_id);
    digest.add("upper", val(signal_upper_array)->identity.object_id);
    digest.add("unit", model::unit_to_string(signal_unit));
    digest.add("support", support->identity.object_id);
    digest.add("response", response->identity.object_id);
    digest.add("noise", estimate->noise ? estimate->noise->identity.object_id : "");
    digest.add("condition", estimate->estimator.condition.kind);
    estimate->identity = model::make_identity("acsd.SignalEstimate", 2, 0, digest.hex(), "acsd");
  }
  estimate->assessment = model::make_assessment(Code::ok);
  // The estimate inherits the certification state of the overlap it was built
  // from: an invalid or unconverged geometry must not be reported as a bounded
  // estimate (review R3d: complete=1/bounded while op.validity=invalid).
  const bool overlaps_certified = source.overlaps_certified;
  estimate->assessment.complete = !any_unresolved && !any_missing_error &&
                                  source.recipe_identity_proven && overlaps_certified;
  estimate->assessment.science =
      (any_unresolved || any_missing_error || !source.recipe_identity_proven || !overlaps_certified)
          ? ScienceState::unconverged
          : ScienceState::bounded;
  estimate->assessment.validity =
      overlaps_certified ? Validity::certified : Validity::invalid;
  if (any_absent) {
    model::add_issue(estimate->assessment, Code::no_support, "leaves_without_positive_support",
                     "build_drizzle",
                     "the leaves without support keep a NaN signal and a named support state",
                     "refine the geometry");
  }
  if (any_unresolved) {
    model::add_issue(estimate->assessment, Code::support_unresolved,
                     "normalizer_lower_bound_is_zero", "build_drizzle",
                     "the certified normalization row is not constructed for these leaves",
                     "refine the geometry");
  }
  if (any_missing_error) {
    model::set_included(estimate->assessment,
                        "leaves_without_a_certified_signal_interval_have_no_achieved_error_entry");
    model::set_included(estimate->assessment,
                        "achieved_error_leaf_indices=" + achieved_index_map);
  }
  if (any_drop_lower_not_positive) {
    model::set_included(estimate->assessment,
                        "drop_area_lower_bound_not_positive:weight_enclosed_by_[0,1]");
  }
  if (!source.recipe_identity_proven) {
    model::add_issue(estimate->assessment, Code::identity_conflict,
                     "overlaps_without_recipe_identity", "build_drizzle:overlaps",
                     "the order and pixfrac of the request cannot be checked against the "
                     "geometry that produced the contributions",
                     "carry the GeometryRecipe of the overlap through the merge");
  }

  // The operator.
  auto op = std::make_shared<DrizzleOperator>();
  op->rows = val(rows);
  op->source_index = observation->native_index;
  op->bounds = source.overlaps;
  op->normalizer_sr = val(normalizer_array);
  op->support = support;
  if (source.overlaps) {
    op->status = source.overlaps->status;
  } else {
    op->status.termination = source.geometry_termination_met ? Termination::tolerance_met
                                                             : Termination::unsupported;
    op->status.bound_validity =
        source.overlaps_certified ? Validity::certified : Validity::invalid;
    op->status.support = SupportState::positive;
  }
  op->assessment = model::make_assessment(Code::ok);
  op->assessment.complete = true;
  // The bounds of the Drizzle operator are the bounds of the overlaps: when
  // the geometry path did not reach its tolerance (a budget exhaustion, an
  // unresolved domain), the operator carries that termination and no bound of
  // the estimate is certified.
  if (source.overlaps && source.overlaps->status.termination != Termination::tolerance_met) {
    op->status.bound_validity = Validity::invalid;
    model::add_issue(op->assessment, Code::nonconverged, "drizzle_bounds_not_certified",
                     "build_drizzle:overlaps",
                     "the overlap bounds of this operator are not certified, so the estimate "
                     "keeps the unresolved mass instead of claiming a certified value",
                     "raise the geometry budget or reduce the source block size");
    model::set_included(op->assessment,
                        std::string("drizzle_termination=") +
                            (source.overlaps->status.termination == Termination::work ? "work"
                             : source.overlaps->status.termination == Termination::precision ? "precision"
                             : source.overlaps->status.termination == Termination::depth ? "depth"
                             : source.overlaps->status.termination == Termination::unsupported
                                 ? "unsupported"
                                 : "tolerance_met"));
  }
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.DrizzleOperator");
    digest.add_csr("rows", row_offsets, row_indices, row_values);
    digest.add("source_index", observation->native_index ? observation->native_index->identity.object_id : "");
    digest.add("bounds", source.geometry_id);
    digest.add("normalizer", val(normalizer_array)->identity.object_id);
    digest.add("support", support->identity.object_id);
    digest.add_u64("termination", static_cast<U64>(op->status.termination));
    digest.add_u64("bound_validity", static_cast<U64>(op->status.bound_validity));
    op->identity = model::make_identity("acsd.DrizzleOperator", 2, 0, digest.hex(), "acsd");
  }

  DrizzleResult result;
  result.op = op;
  result.estimate = estimate;
  return ok_result(result);
}


Result<DrizzleResult> build_drizzle(Context& context, Ref<Overlaps> overlaps,
                                     Ref<NativeObservation> observation, const DrizzleRequest& request) {
  using model::fail;
  using model::ok_result;
  if (!overlaps) {
    return fail<DrizzleResult>(Code::invalid_model, "null_overlaps", "build_drizzle");
  }
  if (!observation) {
    return fail<DrizzleResult>(Code::invalid_model, "null_observation", "build_drizzle");
  }
  if (context.cancellation.requested()) {
    return fail<DrizzleResult>(Code::cancelled, "cancelled", "build_drizzle");
  }
  // The identity of the geometry that produced the contributions must be the
  // identity of the requested drizzle: same order, same pixfrac.  A merged
  // Overlaps that does not carry its recipe cannot prove this; the check is
  // then named instead of silently skipped (the operator is not certified).
  bool recipe_identity_proven = false;
  if (overlaps->recipe) {
    const GeometryRecipe& recipe = *overlaps->recipe;
    if (recipe.order != request.order) {
      return fail<DrizzleResult>(Code::identity_conflict, "drizzle_order_differs_from_overlap",
                                 "build_drizzle");
    }
    if (std::fabs(recipe.pixfrac - request.pixfrac) > 0.0) {
      return fail<DrizzleResult>(Code::identity_conflict, "drizzle_pixfrac_differs_from_overlap",
                                 "build_drizzle");
    }
    recipe_identity_proven = true;
  }

  // The source values and their index.
  Result<std::vector<double>> values =
      operators::read_f64(context, observation->integrated_values, 0,
                             observation->integrated_values->shape[0]);
  if (!ok(values)) {
    return fail<DrizzleResult>(values.code, "native_values_unavailable", "build_drizzle");
  }
  const std::vector<double>& x = val(values);
  const U64 source_count = static_cast<U64>(x.size());

  // The single accumulation implementation of the spherical drizzle.
  geometry::DrizzleAccumulator accumulator;
  for (const OverlapContribution& contribution : overlaps->contributions) {
    if (contribution.source.sample >= source_count) {
      // A source row outside the native values has no value at all: it is an
      // error, never a silent zero (AGENTS: unknown error is not zero).
      return fail<DrizzleResult>(Code::invalid_model, "source_row_out_of_range",
                                 "build_drizzle",
                                 "the source row of an overlap contribution is outside the "
                                 "native values");
    }
    geometry::DrizzleTerm term;
    term.source_row = contribution.source.sample;
    term.order = contribution.leaf.order;
    term.ipix = contribution.leaf.ipix;
    term.overlap_lower = contribution.overlap_area.lower_sr;
    term.overlap_upper = contribution.overlap_area.upper_sr;
    term.drop_lower = contribution.drop_area.lower_sr;
    term.drop_upper = contribution.drop_area.upper_sr;
    term.omega_lower = contribution.native_area.lower_sr;
    term.omega_upper = contribution.native_area.upper_sr;
    term.value = x[term.source_row];
    Result<rt::Done> added = accumulator.add(term);
    if (!ok(added)) {
      return fail<DrizzleResult>(added.code, "invalid_contribution", "build_drizzle",
                                 added.assessment.issues.empty()
                                     ? "an overlap contribution is invalid"
                                     : added.assessment.issues.front().reason);
    }
  }
  const geometry::DrizzleLeafOutput leaf_output = accumulator.finish();
  if (leaf_output.any_absent && !leaf_output.any_present) {
    // Every target leaf is without support: there is no certified signal at all.
    return fail<DrizzleResult>(Code::no_support, "no_support_rows_present", "build_drizzle",
                               "no target leaf has a positive support");
  }

  DrizzleGeometrySource source;
  source.geometry_id = overlaps->identity.object_id;
  source.primitives_id = overlaps->primitives ? overlaps->primitives->identity.object_id : "";
  source.overlaps = overlaps;
  source.recipe_identity_proven = recipe_identity_proven;
  source.overlaps_certified =
      overlaps->status.termination == Termination::tolerance_met &&
      overlaps->status.bound_validity == Validity::certified;
  source.geometry_termination_met =
      overlaps->status.termination == Termination::tolerance_met;
  return package_drizzle(context, leaf_output, observation, request, source_count, source);
}

}  // namespace acsd
