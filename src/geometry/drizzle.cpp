#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/footprint_provider.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::Rational;

struct IntervalValue {
  double lower = 0.0;
  double upper = 0.0;
};

IntervalValue hull(const IntervalValue& a, const IntervalValue& b) {
  return IntervalValue{std::min(a.lower, b.lower), std::max(a.upper, b.upper)};
}

IntervalValue multiply(const IntervalValue& a, const IntervalValue& b) {
  const double p[4] = {a.lower * b.lower, a.lower * b.upper, a.upper * b.lower, a.upper * b.upper};
  double lo = p[0];
  double hi = p[0];
  for (double value : p) {
    lo = std::min(lo, value);
    hi = std::max(hi, value);
  }
  return IntervalValue{lo, hi};
}

IntervalValue add(const IntervalValue& a, const IntervalValue& b) {
  return IntervalValue{a.lower + b.lower, a.upper + b.upper};
}

IntervalValue divide(const IntervalValue& a, const IntervalValue& b) {
  if (!(std::fabs(b.lower) > 0.0) || b.lower <= 0.0) {
    return IntervalValue{-std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity()};
  }
  const double p[4] = {a.lower / b.lower, a.lower / b.upper, a.upper / b.lower, a.upper / b.upper};
  double lo = p[0];
  double hi = p[0];
  for (double value : p) {
    lo = std::min(lo, value);
    hi = std::max(hi, value);
  }
  return IntervalValue{lo, hi};
}

Result<ArrayRef> store_f64(Context& context, const std::vector<double>& values, const Unit& unit,
                           const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
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

  // Group the contributions by leaf.
  std::map<std::pair<U32, U64>, std::vector<std::size_t>> by_leaf;
  for (std::size_t i = 0; i < overlaps->contributions.size(); ++i) {
    const OverlapContribution& contribution = overlaps->contributions[i];
    by_leaf[{contribution.leaf.order, contribution.leaf.ipix}].push_back(i);
  }
  const std::size_t leaf_count = by_leaf.size();

  // Per leaf: the weight interval, the normaliser, the surface brightness and
  // the row of the drizzle operator.
  std::vector<HealpixLeaf> leaves;
  std::vector<double> normalizer;
  std::vector<double> signal;
  std::vector<double> signal_lower;
  std::vector<double> signal_upper;
  std::vector<std::uint8_t> support_states;
  std::vector<std::uint8_t> normalization_states;
  std::vector<std::uint8_t> observational_states;
  std::vector<double> area_lower;
  std::vector<double> area_upper;

  std::vector<U64> row_offsets;
  std::vector<U64> row_indices;
  std::vector<double> row_values;

  bool any_unresolved = false;
  bool any_absent = false;
  bool any_present = false;

  row_offsets.push_back(0);
  for (const auto& entry : by_leaf) {
    const HealpixLeaf leaf = HealpixLeaf{entry.first.first, entry.first.second};
    leaves.push_back(leaf);
    IntervalValue omega{0.0, 0.0};
    IntervalValue drop{0.0, 0.0};
    IntervalValue weight_sum{0.0, 0.0};
    IntervalValue numerator{0.0, 0.0};
    IntervalValue area{0.0, 0.0};
    bool first_contribution = true;
    std::vector<std::pair<U64, double>> row;
    std::vector<double> nominal_weights;
    for (std::size_t index : entry.second) {
      const OverlapContribution& contribution = overlaps->contributions[index];
      const IntervalValue a{contribution.overlap_area.lower_sr, contribution.overlap_area.upper_sr};
      const IntervalValue a_prim{contribution.native_area.lower_sr, contribution.native_area.upper_sr};
      const IntervalValue drop_prim{contribution.drop_area.lower_sr, contribution.drop_area.upper_sr};
      if (first_contribution) {
        omega = a_prim;
        drop = drop_prim;
        area = a;
        first_contribution = false;
      } else {
        omega = hull(omega, a_prim);
        drop = hull(drop, drop_prim);
        area = hull(area, a);
      }
      if (!(drop.lower > 0.0)) continue;
      // w = a / A with the enclosure of SPHERICAL_DRIZZLE section 5.
      IntervalValue w = divide(a, IntervalValue{drop.upper, drop.lower});
      w.upper = std::min(1.0, w.upper);
      weight_sum = add(weight_sum, w);
      const U64 source_row = 0;  // the single source of this recipe
      const double value = source_row < source_count ? x[source_row] : 0.0;
      const IntervalValue signal_interval = multiply(w, IntervalValue{value, value});
      numerator = add(numerator, signal_interval);
      row.emplace_back(source_row, 0.0);
      nominal_weights.push_back(0.5 * (w.lower + w.upper));
    }
    // The normaliser N = sum_j w_j Omega_j.
    IntervalValue n = multiply(weight_sum, omega);
    if (!(n.upper > 0.0)) {
      any_absent = true;
      normalizer.push_back(0.0);
      signal.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
      support_states.push_back(0);
      normalization_states.push_back(0);
      observational_states.push_back(1);
      area_lower.push_back(area.lower);
      area_upper.push_back(area.upper);
      row_offsets.push_back(static_cast<U64>(row_indices.size()));
      continue;
    }
    if (!(n.lower > 0.0)) {
      any_unresolved = true;
      normalizer.push_back(std::numeric_limits<double>::quiet_NaN());
      signal.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
      support_states.push_back(2);
      normalization_states.push_back(2);
      observational_states.push_back(1);
      area_lower.push_back(area.lower);
      area_upper.push_back(area.upper);
      row_offsets.push_back(static_cast<U64>(row_indices.size()));
      continue;
    }
    any_present = true;
    const IntervalValue brightness = divide(numerator, n);
    normalizer.push_back(0.5 * (n.lower + n.upper));
    signal.push_back(0.5 * (brightness.lower + brightness.upper));
    signal_lower.push_back(brightness.lower);
    signal_upper.push_back(brightness.upper);
    support_states.push_back(1);
    normalization_states.push_back(1);
    observational_states.push_back(1);
    area_lower.push_back(area.lower);
    area_upper.push_back(area.upper);
    for (std::size_t k = 0; k < row.size(); ++k) {
      row_indices.push_back(row[k].first);
      row_values.push_back(k < nominal_weights.size() ? nominal_weights[k] : 0.0);
    }
    row_offsets.push_back(static_cast<U64>(row_indices.size()));
  }

  if (any_absent && !any_present) {
    // Every target leaf is without support: there is no certified signal at all.
    return fail<DrizzleResult>(Code::no_support, "no_support_rows_present", "build_drizzle",
                               "no target leaf has a positive support");
  }

  const Id leaf_index_id = "acsd.drizzle.leaves";
  Result<ArrayRef> normalizer_array =
      store_f64(context, normalizer, model::unit_sr(), leaf_index_id);
  if (!ok(normalizer_array)) {
    return fail<DrizzleResult>(normalizer_array.code, "normalizer_store_failed", "build_drizzle");
  }
  Result<ArrayRef> signal_array =
      store_f64(context, signal, model::unit_sr_per_pixel(), leaf_index_id);
  if (!ok(signal_array)) {
    return fail<DrizzleResult>(signal_array.code, "signal_store_failed", "build_drizzle");
  }
  Result<ArrayRef> signal_lower_array =
      store_f64(context, signal_lower, model::unit_sr_per_pixel(), leaf_index_id);
  if (!ok(signal_lower_array)) {
    return fail<DrizzleResult>(signal_lower_array.code, "signal_store_failed", "build_drizzle");
  }
  Result<ArrayRef> signal_upper_array =
      store_f64(context, signal_upper, model::unit_sr_per_pixel(), leaf_index_id);
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

  // The support map.
  auto support = std::make_shared<SupportMap>();
  support->kind = SupportMap::Kind::primitives;
  support->index = std::make_shared<const Index>(model::make_leaf_index(leaves));
  support->primitives.push_back(overlaps->primitives ? overlaps->primitives->identity.object_id : "");
  support->states = val(states_array);
  support->area_lower_sr = val(area_lower_array);
  support->area_upper_sr = val(area_upper_array);
  support->geometry_snapshot = overlaps->identity.object_id;
  support->normalization_states = val(normalization_states_array);
  support->observational_states = val(observational_states_array);
  support->identity = model::make_identity("acsd.SupportMap", 2, 0, model::sha256_hex(leaf_index_id),
                                           "acsd");
  support->assessment = model::make_assessment(Code::ok);
  support->assessment.complete = true;

  // The error record.
  auto error = std::make_shared<ErrorRecord>();
  error->requested = request.budget.requested_functionals;
  error->achieved.clear();
  for (std::size_t i = 0; i < signal.size(); ++i) {
    ExactInterval interval;
    interval.lower = model::dyadic_from_double(signal_lower[i]);
    interval.upper = model::dyadic_from_double(signal_upper[i]);
    interval.precision_bits = 53;
    error->achieved.push_back(interval);
  }
  error->linearization_selection =
      model::make_condition("drizzle_surface_brightness", "interval_bounded");

  // The effective response: the drizzle operator itself acts on the native
  // samples, so it is the effective response of this frame.
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
  response->identity = model::make_identity("acsd.SpatialResponse", 2, 0,
                                             model::sha256_hex("acsd.drizzle.response"), "acsd");
  response->assessment = model::make_assessment(Code::ok);
  response->assessment.complete = true;

  // The signal estimate.
  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = val(signal_array);
  estimate->unit = observation->unit;
  estimate->estimator.id = "acsd.drizzle.brightness";
  estimate->estimator.quantity = "surface_brightness";
  estimate->estimator.unit = observation->unit;
  estimate->estimator.condition = model::make_condition("drizzle_surface_brightness",
                                                         "interval_bounded");
  estimate->numerical_lower = val(signal_lower_array);
  estimate->numerical_upper = val(signal_upper_array);
  estimate->support = support;
  estimate->effective_response = response;
  // The noise of the observation is a NoiseModel; the covariance action is
  // built by the uncertainty module, so the estimate keeps the noise unknown.
  estimate->noise = nullptr;
  estimate->error = error;
  estimate->identity = model::make_identity("acsd.SignalEstimate", 2, 0,
                                             model::sha256_hex("acsd.drizzle.estimate"), "acsd");
  estimate->assessment = model::make_assessment(Code::ok);
  estimate->assessment.complete = !any_unresolved;
  estimate->assessment.science = any_unresolved ? ScienceState::unconverged : ScienceState::bounded;
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

  // The operator.
  auto op = std::make_shared<DrizzleOperator>();
  op->rows = val(rows);
  op->source_index = observation->native_index;
  op->bounds = overlaps;
  op->normalizer_sr = val(normalizer_array);
  op->support = support;
  op->status = overlaps->status;
  op->identity = model::make_identity("acsd.DrizzleOperator", 2, 0, model::sha256_hex("acsd.drizzle"),
                                       "acsd");
  op->assessment = model::make_assessment(Code::ok);
  op->assessment.complete = true;

  DrizzleResult result;
  result.op = op;
  result.estimate = estimate;
  return ok_result(result);
}

}  // namespace acsd
