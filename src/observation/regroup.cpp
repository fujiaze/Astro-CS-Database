// regroup_observation: the explicit G/M/b regrouping of a calibrated native
// observation.
//
// The regrouping matrix G maps n native samples onto m regrouped samples.
// Only two modes produce a usable physical regrouped sample:
//
//   physical_sum  : every output sample collects a mutually disjoint set of
//                   native photosites with the coefficient one;
//   physical_mean : the same sets with the coefficient one over the member
//                   count, so the value is a mean and the integral has to be
//                   restored by the member count before Drizzle.
//
// Everything else (a general signed, data dependent or demosaicing matrix) is
// a linear view: it may be used for measurement and diagnostics but it does
// not describe a new physical collection, so no new footprint and no new
// physical area may be derived from it.
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/interval.hpp"
#include "observation/observation.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "regroup_observation";

// The comparison tolerance for the structure of a physical regrouping.
constexpr double kCoefficientTolerance = 1e-9;

struct RowAnalysis {
  U64 members = 0;
  double min_coefficient = 0.0;
  double max_coefficient = 0.0;
  bool negative = false;
};

}  // namespace

Result<Ref<NativeObservation>> regroup_observation(Context& context,
                                                   Ref<NativeObservation> observation,
                                                   const RegroupRequest& request) {
  if (!observation) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no observation supplied",
                                               kOrigin, "the regrouping cannot be built");
  }
  if (!request.G) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no regrouping matrix supplied",
                                               kOrigin, "the regrouping cannot be built");
  }
  if (!observation->native_index) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the observation has no native index", kOrigin,
                                               "the regrouping cannot be built");
  }
  if (!request.output_footprints || !request.output_footprints->index) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the regrouping has no output footprint set", kOrigin,
                                               "the regrouping cannot be built");
  }

  const U64 n = observation->native_index->size;
  const U64 m = request.output_footprints->index->size;
  if (m == 0) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model, "the regrouping has no output samples",
                                               kOrigin, "the regrouping cannot be built");
  }
  // The declared domain of the matrix is the public member of the action.
  const DomainUnits& spaces = request.G->spaces;
  if (!spaces.domain || !spaces.codomain) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the regrouping matrix has no declared domain", kOrigin,
                                               "the regrouping cannot be built");
  }
  if (spaces.domain->size != n) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the regrouping matrix does not act on the native samples", kOrigin,
        "the regrouping cannot be built");
  }
  if (spaces.codomain->size != m) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the regrouping matrix does not match the output samples", kOrigin,
        "the regrouping cannot be built");
  }

  // The affine part of the image transform has to be invertible.
  const double determinant = request.old_from_new[0] * request.old_from_new[3] -
                             request.old_from_new[1] * request.old_from_new[2];
  if (!std::isfinite(determinant) || determinant == 0.0) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the image transform is not invertible", kOrigin,
                                               "the regrouping cannot be built");
  }

  // ---------------------------------------------------------------------
  // Read the native samples.
  // ---------------------------------------------------------------------
  Result<std::vector<double>> values = observation::read_f64_values(context, observation->integrated_values);
  if (!ok(values)) {
    return model::fail<Ref<NativeObservation>>(values.code, "the native values cannot be read", kOrigin,
                                               "the regrouping cannot be built");
  }
  Result<std::vector<std::uint16_t>> flags = observation::read_u16_values(context, observation->dq);
  if (!ok(flags)) {
    return model::fail<Ref<NativeObservation>>(flags.code, "the quality flags cannot be read", kOrigin,
                                               "the regrouping cannot be built");
  }
  if (val(values).size() != n || val(flags).size() != n) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the native arrays do not match the index", kOrigin,
                                               "the regrouping cannot be built");
  }

  std::vector<double> lower(n, 0.0);
  std::vector<double> upper(n, 0.0);
  bool have_bounds = false;
  if (observation->numerical_lower && observation->numerical_upper) {
    Result<std::vector<double>> low = observation::read_f64_values(context, observation->numerical_lower);
    Result<std::vector<double>> high = observation::read_f64_values(context, observation->numerical_upper);
    if (ok(low) && ok(high) && val(low).size() == n && val(high).size() == n) {
      lower = val(low);
      upper = val(high);
      have_bounds = true;
    }
  }

  // ---------------------------------------------------------------------
  // Inspect the structure of G.  The columns are obtained by applying the
  // public action to the unit vectors, so the analysis always sees the very
  // same matrix that the regrouping applies.
  // ---------------------------------------------------------------------
  Result<U64> work = context.resources.consume_work(n + 1);
  if (!ok(work)) {
    return model::fail<Ref<NativeObservation>>(work.code, "the regrouping exceeds the work budget",
                                               kOrigin, "the regrouping cannot be built");
  }

  std::vector<RowAnalysis> rows(m);
  std::vector<double> row_lower(m, 0.0);
  std::vector<double> row_upper(m, 0.0);
  std::vector<std::uint16_t> row_flags(m, 0);
  bool overlaps = false;

  std::vector<double> unit(n, 0.0);
  std::vector<double> column(m, 0.0);
  for (U64 j = 0; j < n; ++j) {
    std::fill(unit.begin(), unit.end(), 0.0);
    unit[j] = 1.0;
    const std::vector<U64> shape{n};
    Result<ArrayRef> input = observation::write_f64_values(context, unit, shape, observation->unit,
                                                            observation->native_index->identity.object_id);
    if (!ok(input)) {
      return model::fail<Ref<NativeObservation>>(input.code, "the unit vector cannot be stored", kOrigin,
                                                 "the regrouping cannot be built");
    }
    Result<ArrayRef> mapped = apply(context, request.G, val(input));
    if (!ok(mapped)) {
      return model::fail<Ref<NativeObservation>>(mapped.code,
                                                 "the regrouping matrix cannot be applied", kOrigin,
                                                 "the regrouping cannot be built");
    }
    Result<std::vector<double>> column_values = observation::read_f64_values(context, val(mapped));
    if (!ok(column_values)) {
      return model::fail<Ref<NativeObservation>>(column_values.code,
                                                 "the regrouping matrix cannot be read", kOrigin,
                                                 "the regrouping cannot be built");
    }
    if (val(column_values).size() != m) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model, "the regrouping matrix does not produce the output samples", kOrigin,
          "the regrouping cannot be built");
    }
    U64 touched = 0;
    for (U64 r = 0; r < m; ++r) {
      const double c = val(column_values)[r];
      if (std::fabs(c) <= kCoefficientTolerance) continue;
      ++touched;
      if (c < 0.0) rows[r].negative = true;
      rows[r].members += 1;
      if (rows[r].members == 1) {
        rows[r].min_coefficient = c;
        rows[r].max_coefficient = c;
      } else {
        rows[r].min_coefficient = std::min(rows[r].min_coefficient, c);
        rows[r].max_coefficient = std::max(rows[r].max_coefficient, c);
      }
      // Numerical interval propagation through the signed matrix.
      const double lo = have_bounds ? lower[j] : val(values)[j];
      const double hi = have_bounds ? upper[j] : val(values)[j];
      const double a = c * lo;
      const double b = c * hi;
      row_lower[r] += std::min(a, b);
      row_upper[r] += std::max(a, b);
      row_flags[r] = static_cast<std::uint16_t>(row_flags[r] | val(flags)[j]);
    }
    if (touched > 1) overlaps = true;
  }

  // ---------------------------------------------------------------------
  // Decide whether the matrix is a physical regrouping.
  // ---------------------------------------------------------------------
  const bool physical = request.mode != RegroupRequest::Mode::linear_view;
  double member_count = 1.0;
  if (physical) {
    if (request.mode == RegroupRequest::Mode::physical_mean) {
      if (request.mean_member_count == 0) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model, "the mean regrouping has no member count", kOrigin,
            "the regrouping cannot be built");
      }
      member_count = static_cast<double>(request.mean_member_count);
    }
    for (U64 r = 0; r < m; ++r) {
      if (rows[r].members == 0) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model, "a regrouped sample has no native member", kOrigin,
            "the regrouping cannot be built");
      }
      if (rows[r].negative) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model, "a physical regrouping must not use negative coefficients", kOrigin,
            "the regrouping cannot be built");
      }
      const double expected =
          request.mode == RegroupRequest::Mode::physical_mean ? 1.0 / member_count : 1.0;
      const double tolerance = kCoefficientTolerance * std::max(1.0, expected);
      if (std::fabs(rows[r].min_coefficient - expected) > tolerance ||
          std::fabs(rows[r].max_coefficient - expected) > tolerance) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model,
            "the regrouping matrix does not match the requested physical mode", kOrigin,
            "the regrouping cannot be built");
      }
      if (request.mode == RegroupRequest::Mode::physical_mean &&
          rows[r].members != request.mean_member_count) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model,
            "the member count of a mean regrouping does not match the actual members", kOrigin,
            "the regrouping cannot be built");
      }
    }
    if (overlaps) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model,
          "the physical regrouping reuses a native photosite in more than one output sample", kOrigin,
          "the regrouping cannot be built");
    }
  }

  // ---------------------------------------------------------------------
  // The regrouped values.
  // ---------------------------------------------------------------------
  std::vector<double> input_values = val(values);
  if (request.input_is_mean && request.mode == RegroupRequest::Mode::physical_sum) {
    // A mean input is restored to an integral before the sum is formed.
    const double restore = request.mean_member_count > 0
                               ? static_cast<double>(request.mean_member_count)
                               : 1.0;
    for (double& v : input_values) v *= restore;
  }
  const std::vector<U64> native_shape{n};
  Result<ArrayRef> input_array =
      observation::write_f64_values(context, input_values, native_shape, observation->unit,
                                    observation->native_index->identity.object_id);
  if (!ok(input_array)) {
    return model::fail<Ref<NativeObservation>>(input_array.code, "the native values cannot be stored",
                                               kOrigin, "the regrouping cannot be built");
  }
  Result<ArrayRef> mapped = apply(context, request.G, val(input_array));
  if (!ok(mapped)) {
    return model::fail<Ref<NativeObservation>>(mapped.code, "the regrouping matrix cannot be applied",
                                               kOrigin, "the regrouping cannot be built");
  }
  Result<std::vector<double>> regrouped = observation::read_f64_values(context, val(mapped));
  if (!ok(regrouped)) {
    return model::fail<Ref<NativeObservation>>(regrouped.code, "the regrouped values cannot be read",
                                               kOrigin, "the regrouping cannot be built");
  }
  if (val(regrouped).size() != m) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the regrouping does not produce the output samples", kOrigin,
        "the regrouping cannot be built");
  }

  // The numerical bounds are propagated through the same signed matrix.
  if (!have_bounds) {
    for (U64 r = 0; r < m; ++r) {
      row_lower[r] = val(regrouped)[r];
      row_upper[r] = val(regrouped)[r];
    }
  }

  // ---------------------------------------------------------------------
  // The noise: the same roots, the influence composed with G.
  // ---------------------------------------------------------------------
  Ref<NoiseModel> parent_noise = observation->noise;
  auto noise_model = std::make_shared<NoiseModel>();
  noise_model->variance_unit = parent_noise ? parent_noise->variance_unit
                                             : model::unit_variance("unknown");
  noise_model->condition = parent_noise ? parent_noise->condition
                                        : model::make_condition("unknown", "unknown");
  noise_model->state = parent_noise ? parent_noise->state : NoiseState::unknown;

  Ref<LinearAction> effective_g = request.G;
  if (request.input_is_mean && request.mode == RegroupRequest::Mode::physical_sum) {
    const double restore = request.mean_member_count > 0
                               ? static_cast<double>(request.mean_member_count)
                               : 1.0;
    Result<Ref<LinearAction>> scale =
        observation::make_diagonal_action(context, std::vector<double>(n, restore), observation->native_index,
                             observation->unit, observation->unit);
    if (!ok(scale)) {
      return model::fail<Ref<NativeObservation>>(scale.code, "the mean restoration cannot be built",
                                                 kOrigin, "the regrouping cannot be built");
    }
    Result<Ref<LinearAction>> chained = acsd::compose(context, val(scale), request.G);
    if (!ok(chained)) {
      return model::fail<Ref<NativeObservation>>(chained.code, "the mean restoration cannot be built",
                                                 kOrigin, "the regrouping cannot be built");
    }
    effective_g = val(chained);
  }

  if (parent_noise) {
    noise_model->innovations = parent_noise->innovations;
    noise_model->shared_factors = parent_noise->shared_factors;
    noise_model->unknown_terms = parent_noise->unknown_terms;
    for (const Ref<LinearAction>& influence : parent_noise->influence) {
      Result<Ref<LinearAction>> chained = acsd::compose(context, effective_g, influence);
      if (!ok(chained)) {
        return model::fail<Ref<NativeObservation>>(chained.code,
                                                   "the noise influence cannot be regrouped", kOrigin,
                                                   "the regrouping cannot be built");
      }
      noise_model->influence.push_back(val(chained));
    }
  } else {
    UnknownTerm term;
    term.id = "noise_unknown";
    term.reason = "the parent observation carries no noise model";
    Result<Ref<LinearAction>> placeholder =
        observation::make_diagonal_action(context, std::vector<double>(m, 1.0), request.output_footprints->index,
                             observation->unit, observation->unit);
    if (!ok(placeholder)) {
      return model::fail<Ref<NativeObservation>>(placeholder.code, "the unknown term cannot be built",
                                                 kOrigin, "the regrouping cannot be built");
    }
    term.influence = val(placeholder);
    noise_model->unknown_terms.push_back(term);
  }
  noise_model->assessment = model::make_assessment(Code::ok);
  noise_model->assessment.noise = noise_model->state;
  noise_model->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                               model::sha256_hex("regroup:" + observation->frame),
                                               "acsd");

  // ---------------------------------------------------------------------
  // The regrouped observation.
  // ---------------------------------------------------------------------
  const std::vector<U64> shape{m};
  const Id index_id = request.output_footprints->index->identity.object_id;
  Result<ArrayRef> value_array =
      observation::write_f64_values(context, val(regrouped), shape, observation->unit, index_id);
  if (!ok(value_array)) {
    return model::fail<Ref<NativeObservation>>(value_array.code,
                                               "the regrouped values cannot be stored", kOrigin,
                                               "the regrouping cannot be built");
  }
  Result<ArrayRef> lower_array =
      observation::write_f64_values(context, row_lower, shape, observation->unit, index_id);
  if (!ok(lower_array)) {
    return model::fail<Ref<NativeObservation>>(lower_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the regrouping cannot be built");
  }
  Result<ArrayRef> upper_array =
      observation::write_f64_values(context, row_upper, shape, observation->unit, index_id);
  if (!ok(upper_array)) {
    return model::fail<Ref<NativeObservation>>(upper_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the regrouping cannot be built");
  }
  Result<ArrayRef> flag_array = observation::write_u16_values(
      context, row_flags, shape, model::unit_dimensionless(), index_id);
  if (!ok(flag_array)) {
    return model::fail<Ref<NativeObservation>>(flag_array.code,
                                               "the quality flags cannot be stored", kOrigin,
                                               "the regrouping cannot be built");
  }

  auto error = std::make_shared<ErrorRecord>();
  error->unknown_model_terms.push_back("stochastic_noise_is_not_a_numerical_error");
  error->linearization_selection = model::make_condition(
      request.mode == RegroupRequest::Mode::linear_view ? "regroup_linear_view" : "regroup_physical",
      "exact_linear_map");
  error->identity = model::make_identity("acsd.ErrorRecord", 2, 0,
                                         model::sha256_hex("regroup:" + observation->frame), "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto regrouped_model = std::make_shared<NativeObservation>();
  regrouped_model->integrated_values = val(value_array);
  regrouped_model->dq = val(flag_array);
  regrouped_model->native_index = request.output_footprints->index;
  regrouped_model->footprints =
      request.mode == RegroupRequest::Mode::linear_view ? Ref<FootprintSet>() : request.output_footprints;
  regrouped_model->regrouping = effective_g;
  regrouped_model->scale_applications = observation->scale_applications;
  regrouped_model->noise = std::shared_ptr<const NoiseModel>(noise_model);
  regrouped_model->unit = observation->unit;
  regrouped_model->response = observation->response;
  regrouped_model->kernel = observation->kernel;
  regrouped_model->native_parent = observation;
  regrouped_model->physical_collection = request.mode != RegroupRequest::Mode::linear_view;
  regrouped_model->context = observation->context;
  regrouped_model->frame = observation->frame;
  regrouped_model->numerical_lower = val(lower_array);
  regrouped_model->numerical_upper = val(upper_array);
  regrouped_model->error = std::shared_ptr<const ErrorRecord>(error);

  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  assessment.noise = noise_model->state;
  model::set_included(assessment, "exact_linear_regrouping");
  if (request.mode == RegroupRequest::Mode::physical_mean) {
    model::set_included(assessment, "mean_member_count_" + std::to_string(request.mean_member_count));
    model::set_omitted(assessment, "the_mean_must_be_restored_to_a_sum_before_drizzle");
  }
  if (request.mode == RegroupRequest::Mode::linear_view) {
    model::set_omitted(assessment, "no_physical_collection_domain");
    model::add_issue(assessment, Code::unsupported,
                      "a linear view does not describe a physical collection and cannot feed the "
                      "physical Drizzle path",
                      kOrigin, "the physical drizzle path stays unavailable");
  }
  if (!have_bounds) {
    model::set_omitted(assessment, "the_parent_carries_no_numerical_bounds");
  }
  regrouped_model->assessment = assessment;
  regrouped_model->identity = model::make_identity(
      "acsd.NativeObservation", 2, 0,
      observation::payload_hash("acsd.NativeObservation", observation->frame,
                                {model::sha256_hex(model::encode_f64(val(regrouped))),
                                 model::sha256_hex(model::encode_u16(row_flags))}),
      "acsd");

  return model::ok_result(std::shared_ptr<const NativeObservation>(regrouped_model));
}

}  // namespace acsd
