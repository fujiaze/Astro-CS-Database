#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/footprint_provider.hpp"
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
  bool any_missing_error = false;
  bool any_drop_lower_not_positive = false;

  // One source term of a leaf.  Omega_j and A_j are properties of the source
  // (the contribution), a_jp is the only (source, leaf) quantity.
  struct SourceTerm {
    U64 source_row = 0;
    IntervalValue overlap{0.0, 0.0};
    IntervalValue drop{0.0, 0.0};
    IntervalValue omega{0.0, 0.0};
    IntervalValue weight{0.0, 0.0};
    double weight_nominal = 0.0;
    double value = 0.0;
  };

  row_offsets.push_back(0);
  for (const auto& entry : by_leaf) {
    // The cancellation checkpoint of the leaf loop: the leaf that is being
    // assembled is the safety boundary.
    if (context.cancellation.requested()) {
      return fail<DrizzleResult>(Code::cancelled, "cancelled", "build_drizzle:leaves");
    }
    const HealpixLeaf leaf = HealpixLeaf{entry.first.first, entry.first.second};
    leaves.push_back(leaf);

    // --- per source j: w_jp = a_jp / A_j (SPHERICAL_DRIZZLE Eq. (9)) --------
    std::vector<SourceTerm> terms;
    terms.reserve(entry.second.size());
    for (std::size_t index : entry.second) {
      // The cancellation checkpoint of the contribution loop.
      if (context.cancellation.requested()) {
        return fail<DrizzleResult>(Code::cancelled, "cancelled", "build_drizzle:contributions");
      }
      const OverlapContribution& contribution = overlaps->contributions[index];
      SourceTerm term;
      term.source_row = contribution.source.sample;
      term.overlap = IntervalValue{contribution.overlap_area.lower_sr,
                                   contribution.overlap_area.upper_sr};
      term.drop = IntervalValue{contribution.drop_area.lower_sr, contribution.drop_area.upper_sr};
      term.omega = IntervalValue{contribution.native_area.lower_sr,
                                 contribution.native_area.upper_sr};
      if (!(term.overlap.lower <= term.overlap.upper) ||
          !(term.drop.lower <= term.drop.upper) || !(term.omega.lower <= term.omega.upper)) {
        return fail<DrizzleResult>(Code::invalid_model, "invalid_area_interval", "build_drizzle",
                                   "the overlap, drop or native area enclosure is inverted");
      }
      if (term.overlap.lower < 0.0 || term.drop.upper < 0.0 || term.omega.lower < 0.0) {
        return fail<DrizzleResult>(Code::invalid_model, "negative_area", "build_drizzle",
                                   "an area enclosure is negative");
      }
      if (term.source_row >= source_count) {
        // The source row of the contribution is the column of the weight
        // matrix.  A row outside the native values has no value at all: it is
        // an error, never a silent zero (AGENTS: unknown error is not zero).
        return fail<DrizzleResult>(Code::invalid_model, "source_row_out_of_range",
                                   "build_drizzle",
                                   "the source row of an overlap contribution is outside the "
                                   "native values");
      }
      term.value = x[term.source_row];
      // w in [a_L/A_U, min(1, a_U/A_L)].  The true w = a/A satisfies w <= 1
      // because a_jp <= A_j, so the cap is a valid tightening, not a mask.
      if (term.drop.lower > 0.0) {
        term.weight.lower = term.overlap.lower / term.drop.upper;
        term.weight.upper = std::min(1.0, term.overlap.upper / term.drop.lower);
        if (!(term.weight.lower <= term.weight.upper)) {
          return fail<DrizzleResult>(Code::invalid_model, "empty_weight_interval",
                                     "build_drizzle",
                                     "the overlap enclosure does not intersect the drop "
                                     "enclosure");
        }
      } else if (term.drop.upper > 0.0) {
        // Only the positivity of A_j is certified (measure proved A > 0 but its
        // lower bound rounded to zero).  w = a/A is still bounded below by
        // a_L/A_U and above by 1 (a <= A): the lower bound must not be widened
        // to 0, because the representative would then be 0.5 and the published
        // point value wrong (review R3: B = 1.666667 instead of the true 2).
        term.weight.lower = term.overlap.lower / term.drop.upper;
        term.weight.upper = 1.0;
        any_drop_lower_not_positive = true;
      } else {
        // A_j is not even bounded above: w in [0,1] is the only valid
        // enclosure, and the leaf is reported as not positively supported.
        term.weight = IntervalValue{0.0, 1.0};
        any_drop_lower_not_positive = true;
      }
      term.weight_nominal = representative(term.weight);
      terms.push_back(term);
    }

    // --- F_p, N_p and their representative point ---------------------------
    //   F_p = sum_j w_jp x_j,  N_p = sum_j w_jp Omega_j.
    // Both are accumulated per source: the normaliser never uses a hull of the
    // Omega_j or of the A_j across sources (I1).  The point estimate is
    // evaluated at one consistent point of the enclosure box (the midpoints of
    // w and Omega), so the constant field x_j = b0 Omega_j gives B = b0
    // exactly instead of the midpoint of a conservative ratio interval.
    IntervalValue f_interval{0.0, 0.0};
    IntervalValue n_interval{0.0, 0.0};
    double f_nominal = 0.0;
    double n_nominal = 0.0;
    double union_lower = 0.0;
    double union_upper = 0.0;
    for (const SourceTerm& term : terms) {
      f_interval = add(f_interval, multiply(term.weight, IntervalValue{term.value, term.value}));
      // N = sum_j w_jp Omega_j: both factors are non-negative, so the directed
      // product of the two intervals is the outward term.
      n_interval = add(n_interval, multiply(term.weight, term.omega));
      f_nominal += term.weight_nominal * term.value;
      n_nominal += term.weight_nominal * representative(term.omega);
      // The support union S = union_j (D_j cap H_p): a single source is inside
      // the union (lower), the sum of the overlap upper bounds and the leaf
      // area bound it from above.  The sum of the a is not the union.  The sum
      // of the upper bounds is accumulated with the directed addition.
      union_lower = std::max(union_lower, term.overlap.lower);
      union_upper = add(IntervalValue{union_upper, union_upper},
                        IntervalValue{term.overlap.upper, term.overlap.upper})
                        .upper;
    }
    const double omega_leaf = leaf_solid_angle_upper(leaf.order);
    union_upper = std::min(union_upper, omega_leaf);
    area_lower.push_back(union_lower);
    area_upper.push_back(union_upper);

    if (!(n_interval.upper > 0.0)) {
      // N_U = 0: no support.  The signal is unavailable, never a fabricated
      // zero.
      any_absent = true;
      normalizer.push_back(0.0);
      signal.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
      support_states.push_back(0);
      normalization_states.push_back(0);
      observational_states.push_back(1);
      row_offsets.push_back(static_cast<U64>(row_indices.size()));
      continue;
    }
    if (!(n_interval.lower > 0.0)) {
      // N_L <= 0 < N_U: the normaliser may vanish, so no normalised row is
      // constructed (support_unresolved).
      any_unresolved = true;
      normalizer.push_back(std::numeric_limits<double>::quiet_NaN());
      signal.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
      signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
      support_states.push_back(2);
      normalization_states.push_back(2);
      observational_states.push_back(1);
      row_offsets.push_back(static_cast<U64>(row_indices.size()));
      continue;
    }
    any_present = true;
    // B_p = F_p / N_p with N_L > 0; the point value is the same ratio at the
    // representative point, which lies inside the certified interval.
    const IntervalValue brightness = divide(f_interval, n_interval);
    const double brightness_nominal = f_nominal / n_nominal;
    normalizer.push_back(n_nominal);
    signal.push_back(brightness_nominal);
    signal_lower.push_back(brightness.lower);
    signal_upper.push_back(brightness.upper);
    support_states.push_back(1);
    normalization_states.push_back(1);
    observational_states.push_back(1);
    // The row of the same linear operator R: R_pj = w_jp / N_p.  The row is
    // merged per source row (one column per source, SPHERICAL_DRIZZLE Eq. (2)),
    // in source order so that the result is order independent.
    std::map<U64, double> r_by_source;
    for (const SourceTerm& term : terms) {
      r_by_source[term.source_row] += term.weight_nominal / n_nominal;
    }
    for (const auto& [source_row, r] : r_by_source) {
      row_indices.push_back(source_row);
      row_values.push_back(r);
    }
    row_offsets.push_back(static_cast<U64>(row_indices.size()));
  }

  if (any_absent && !any_present) {
    // Every target leaf is without support: there is no certified signal at all.
    return fail<DrizzleResult>(Code::no_support, "no_support_rows_present", "build_drizzle",
                               "no target leaf has a positive support");
  }

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
  support->kind = SupportMap::Kind::primitives;
  support->index = leaf_index;
  support->primitives.push_back(overlaps->primitives ? overlaps->primitives->identity.object_id : "");
  support->states = val(states_array);
  support->area_lower_sr = val(area_lower_array);
  support->area_upper_sr = val(area_upper_array);
  support->geometry_snapshot = overlaps->identity.object_id;
  support->normalization_states = val(normalization_states_array);
  support->observational_states = val(observational_states_array);
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.SupportMap");
    digest.add("leaf_index", leaf_index_id);
    digest.add("geometry", overlaps->identity.object_id);
    digest.add("primitives", overlaps->primitives ? overlaps->primitives->identity.object_id : "");
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
      any_missing_error = true;
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
  const bool overlaps_certified =
      overlaps->status.termination == Termination::tolerance_met &&
      overlaps->status.bound_validity == Validity::certified;
  estimate->assessment.complete = !any_unresolved && !any_missing_error &&
                                  recipe_identity_proven && overlaps_certified;
  estimate->assessment.science =
      (any_unresolved || any_missing_error || !recipe_identity_proven || !overlaps_certified)
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
  if (!recipe_identity_proven) {
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
  op->bounds = overlaps;
  op->normalizer_sr = val(normalizer_array);
  op->support = support;
  op->status = overlaps->status;
  op->assessment = model::make_assessment(Code::ok);
  op->assessment.complete = true;
  // The bounds of the Drizzle operator are the bounds of the overlaps: when
  // the geometry path did not reach its tolerance (a budget exhaustion, an
  // unresolved domain), the operator carries that termination and no bound of
  // the estimate is certified.
  if (overlaps->status.termination != Termination::tolerance_met) {
    op->status.bound_validity = Validity::invalid;
    model::add_issue(op->assessment, Code::nonconverged, "drizzle_bounds_not_certified",
                     "build_drizzle:overlaps",
                     "the overlap bounds of this operator are not certified, so the estimate "
                     "keeps the unresolved mass instead of claiming a certified value",
                     "raise the geometry budget or reduce the source block size");
    model::set_included(op->assessment,
                        std::string("drizzle_termination=") +
                            (overlaps->status.termination == Termination::work ? "work"
                             : overlaps->status.termination == Termination::precision ? "precision"
                             : overlaps->status.termination == Termination::depth ? "depth"
                             : overlaps->status.termination == Termination::unsupported
                                 ? "unsupported"
                                 : "tolerance_met"));
  }
  {
    geometry::ContentDigest digest;
    digest.add("schema", "acsd.DrizzleOperator");
    digest.add_csr("rows", row_offsets, row_indices, row_values);
    digest.add("source_index", observation->native_index ? observation->native_index->identity.object_id : "");
    digest.add("bounds", overlaps->identity.object_id);
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

}  // namespace acsd
