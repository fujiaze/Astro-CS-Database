// calibrate_observation: the N1 calibration of a raw detector frame.
//
//   x = (d - b - t*dark) / flat,   t = exposure_seconds / dark_exposure_seconds
//
// The dark map carries the bias when `dark_contains_bias` is set; the bias is
// then removed from the dark before the exposure ratio is applied.  The flat
// map is used through its explicit normalisation.  A non-positive or
// non-finite flat and any overflow are *not* repaired numerically: the data
// quality flag of the sample is kept and the sample is marked bad.
//
// The noise budget is kept separate from the calibrated mean:
//   * a user supplied `raw_noise` is the complete electron expectation plus
//     readout factor and is propagated through the calibration Jacobian
//     d x / d d = 1/flat;
//   * without it the gain/read metadata alone cannot recover the shot
//     variance, so the complete variance is unknown; the user may ask for the
//     explicit Poisson plug-in (max(d-b,0) as the electron expectation), which
//     is reported as approximate;
//   * a master flat root is merged by its shared identity instead of being
//     rebuilt for every frame.
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

using observation::kDqBad;
using observation::kDqDisqualifying;

constexpr const char* kOrigin = "calibrate_observation";

// ---------------------------------------------------------------------------
// Small action helpers built only from the declared contract.
// ---------------------------------------------------------------------------

Result<Ref<LinearAction>> make_identity_action(Context& context, Ref<Index> index,
                                                const Unit& unit) {
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::identity;
  recipe.algorithm = "identity_v1";
  recipe.version = 1;
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  return make_action(context, recipe, std::span<const Ref<LinearAction>>{},
                     std::span<const ArrayRef>{});
}

Result<Ref<LinearAction>> make_diagonal_action(Context& context, const std::vector<double>& weights,
                                                Ref<Index> index, const Unit& in_unit,
                                                const Unit& out_unit) {
  const std::vector<U64> shape{static_cast<U64>(weights.size())};
  Result<ArrayRef> array =
      observation::write_f64_values(context, weights, shape, in_unit,
                                   index ? index->identity.object_id : Id());
  if (!ok(array)) {
    return model::fail<Ref<LinearAction>>(array.code, "the diagonal weights cannot be stored",
                                          kOrigin, "the noise model cannot be built");
  }
  const std::vector<ArrayRef> arrays{val(array)};
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "diagonal_v1";
  recipe.version = 1;
  recipe.arrays.push_back(val(array)->identity.object_id);
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = in_unit;
  recipe.spaces.output = out_unit;
  return make_action(context, recipe, std::span<const Ref<LinearAction>>{},
                     std::span<const ArrayRef>{arrays.data(), arrays.size()});
}

// A noise root whose coordinates are the native samples and whose factor
// scales a standard latent to the raw ADU amplitude.
Result<Ref<NoiseRoot>> make_native_root(Context& context, Ref<Index> index, const Unit& adu_unit,
                                         const SharedId& shared_id, const std::vector<double>& stddev,
                                         const std::string& entity) {
  Result<Ref<LinearAction>> factor =
      observation::make_diagonal_action(context, stddev, index, adu_unit, adu_unit);
  if (!ok(factor)) {
    return model::fail<Ref<NoiseRoot>>(factor.code, "the noise factor cannot be built", kOrigin,
                                        "the noise model cannot be built");
  }
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = shared_id;
  root->shared_id.external_namespace = "acsd.native";
  root->shared_id.entity = entity;
  root->shared_id.model_revision = "1";
  root->shared_id.component = "amplitude";
  root->coordinates = index;
  root->unit = adu_unit;
  root->factor = val(factor);
  root->assessment = model::make_assessment(Code::ok);
  root->identity = model::make_identity("acsd.NoiseRoot", 2, 0,
                                        model::sha256_hex(entity), "acsd");
  return model::ok_result(std::shared_ptr<const NoiseRoot>(root));
}

// ---------------------------------------------------------------------------
// The noise assembly result.
// ---------------------------------------------------------------------------

struct NoiseAssembly {
  Ref<NoiseModel> model;
  NoiseState state = NoiseState::unknown;
  std::vector<std::string> included;
  std::vector<std::string> omitted;
};

}  // namespace

Result<Ref<NativeObservation>> calibrate_observation(Context& context, Ref<RawObservation> raw,
                                                     const Calibration& request) {
  if (!raw) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request,
                                               "no raw observation supplied", kOrigin,
                                               "the observation cannot be calibrated");
  }
  if (!raw->adu || !raw->dq) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the raw observation has no data or quality array",
                                               kOrigin, "the observation cannot be calibrated");
  }
  if (!raw->source_index) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the raw observation has no source index", kOrigin,
                                               "the observation cannot be calibrated");
  }
  if (raw->width == 0 || raw->height == 0) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model, "the frame has no samples",
                                               kOrigin, "the observation cannot be calibrated");
  }
  if (raw->width > (~0ull) / raw->height) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model, "the frame size overflows",
                                               kOrigin, "the observation cannot be calibrated");
  }
  const U64 count = raw->width * raw->height;
  if (raw->source_index->size != count) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the source index does not cover the frame", kOrigin,
        "the observation cannot be calibrated");
  }
  if (!observation::array_has_count(*raw->adu, count) ||
      !observation::array_has_count(*raw->dq, count)) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the raw arrays do not match the frame shape", kOrigin,
                                               "the observation cannot be calibrated");
  }

  Result<std::vector<double>> adu = observation::read_f64_values(context, raw->adu);
  if (!ok(adu)) {
    return model::fail<Ref<NativeObservation>>(adu.code, "the raw values cannot be read", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<std::vector<std::uint16_t>> dq = observation::read_u16_values(context, raw->dq);
  if (!ok(dq)) {
    return model::fail<Ref<NativeObservation>>(dq.code, "the quality flags cannot be read", kOrigin,
                                               "the observation cannot be calibrated");
  }

  const bool has_bias = static_cast<bool>(request.bias);
  const bool has_dark = static_cast<bool>(request.dark);
  const bool has_flat = static_cast<bool>(request.flat);
  std::vector<double> bias;
  std::vector<double> dark;
  std::vector<double> flat;
  if (has_bias) {
    if (!observation::array_has_count(*request.bias, count)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the bias map does not match the frame", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    Result<std::vector<double>> values = observation::read_f64_values(context, request.bias);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the bias map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    bias = val(values);
  }
  if (has_dark) {
    if (!observation::array_has_count(*request.dark, count)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the dark map does not match the frame", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    Result<std::vector<double>> values = observation::read_f64_values(context, request.dark);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the dark map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    dark = val(values);
  }
  if (has_flat) {
    if (!observation::array_has_count(*request.flat, count)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the flat map does not match the frame", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    Result<std::vector<double>> values = observation::read_f64_values(context, request.flat);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the flat map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    flat = val(values);
  }

  if (!std::isfinite(request.exposure_seconds) || request.exposure_seconds < 0.0) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model, "the exposure time is invalid",
                                               kOrigin, "the observation cannot be calibrated");
  }
  if (!std::isfinite(request.flat_normalization)) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the flat normalisation is not finite", kOrigin,
                                               "the observation cannot be calibrated");
  }
  if (has_flat && !(request.flat_normalization > 0.0)) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the flat normalisation must be positive", kOrigin,
                                               "the observation cannot be calibrated");
  }
  double exposure_ratio = 0.0;
  if (has_dark) {
    if (!std::isfinite(request.dark_exposure_seconds) || !(request.dark_exposure_seconds > 0.0)) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model, "the dark exposure time must be positive", kOrigin,
          "the observation cannot be calibrated");
    }
    exposure_ratio = request.exposure_seconds / request.dark_exposure_seconds;
    if (!std::isfinite(exposure_ratio) || !(exposure_ratio > 0.0)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the exposure ratio must be positive", kOrigin,
                                                 "the observation cannot be calibrated");
    }
  }

  // ---------------------------------------------------------------------
  // The calibrated mean and its numerical enclosure.
  // ---------------------------------------------------------------------
  std::vector<double> values(count, 0.0);
  std::vector<double> lower(count, 0.0);
  std::vector<double> upper(count, 0.0);
  std::vector<std::uint16_t> flags(count, 0);
  U64 bad_flat = 0;
  U64 repaired = 0;

  for (U64 i = 0; i < count; ++i) {
    const double d = val(adu)[i];
    std::uint16_t q = val(dq)[i];
    if ((q & observation::kDqRepaired) != 0u) ++repaired;
    if (!std::isfinite(d) && (q & kDqDisqualifying) == 0u) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model,
          "a non-finite raw value conflicts with the valid data quality bits", kOrigin,
          "the observation cannot be calibrated");
    }
    const double b = has_bias ? bias[i] : 0.0;
    const double dk = has_dark ? dark[i] : 0.0;
    double f = 1.0;
    if (has_flat) f = flat[i] / request.flat_normalization;
    const bool flat_usable = std::isfinite(f) && f > 0.0;

    numerics::Interval d_i = numerics::interval_point(std::isfinite(d) ? d : 0.0);
    numerics::Interval b_i = numerics::interval_point(std::isfinite(b) ? b : 0.0);
    numerics::Interval dk_i = numerics::interval_point(std::isfinite(dk) ? dk : 0.0);
    numerics::Interval f_i = numerics::interval_point(std::isfinite(f) ? f : 0.0);

    numerics::Interval numerator = numerics::interval_sub(d_i, b_i);
    if (has_dark) {
      const numerics::Interval dark_term =
          request.dark_contains_bias ? numerics::interval_sub(dk_i, b_i) : dk_i;
      numerator = numerics::interval_sub(numerator,
                                         numerics::interval_scale(dark_term, exposure_ratio));
    }
    numerics::Interval x_i = numerics::interval_div(numerator, f_i);

    double x = 0.0;
    if (!flat_usable) {
      ++bad_flat;
      x = std::numeric_limits<double>::quiet_NaN();
      if ((q & kDqDisqualifying) == 0u) q = static_cast<std::uint16_t>(q | kDqBad);
    } else {
      x = (d - b - (has_dark ? exposure_ratio * (request.dark_contains_bias ? (dk - b) : dk)
                              : 0.0)) /
          f;
      if (!std::isfinite(x)) {
        x = std::numeric_limits<double>::quiet_NaN();
        if ((q & kDqDisqualifying) == 0u) q = static_cast<std::uint16_t>(q | kDqBad);
        lower[i] = std::numeric_limits<double>::quiet_NaN();
        upper[i] = std::numeric_limits<double>::quiet_NaN();
        values[i] = x;
        flags[i] = q;
        continue;
      }
    }
    values[i] = x;
    lower[i] = x_i.lower;
    upper[i] = x_i.upper;
    flags[i] = q;
  }

  // ---------------------------------------------------------------------
  // The noise budget.
  // ---------------------------------------------------------------------
  const Id index_id = raw->source_index->identity.object_id;
  const std::string identity_text = observation::detector_identity(*raw->source_index);
  const Unit adu_unit = model::unit_adu(identity_text);
  const Unit signal_unit = model::unit_signal(identity_text);
  const Unit variance_unit = model::unit_variance(identity_text);

  std::vector<double> inverse_flat(count, 1.0);
  for (U64 i = 0; i < count; ++i) {
    const double f = has_flat ? flat[i] / request.flat_normalization : 1.0;
    inverse_flat[i] = (std::isfinite(f) && f > 0.0) ? 1.0 / f : 0.0;
  }

  auto noise_model = std::make_shared<NoiseModel>();
  noise_model->variance_unit = variance_unit;
  noise_model->condition = model::make_condition("native_calibrated_noise", "calibration_jacobian");
  noise_model->state = NoiseState::unknown;
  std::vector<std::string> noise_included;
  std::vector<std::string> noise_omitted;
  NoiseState noise_state = NoiseState::unknown;

  if (request.raw_noise) {
    // The user supplied the complete electron expectation and readout factor.
    noise_model->innovations = request.raw_noise->innovations;
    noise_model->shared_factors = request.raw_noise->shared_factors;
    noise_model->influence = request.raw_noise->influence;
    noise_model->unknown_terms = request.raw_noise->unknown_terms;
    noise_state = request.raw_noise->state;
    noise_included.push_back("raw_noise_user_supplied");
    // The calibration Jacobian d x / d d = 1/flat maps the raw amplitudes onto
    // the calibrated samples.
    Result<Ref<LinearAction>> scale =
        observation::make_diagonal_action(context, inverse_flat, raw->source_index, adu_unit, signal_unit);
    if (!ok(scale)) {
      return model::fail<Ref<NativeObservation>>(scale.code, "the calibration Jacobian cannot be built",
                                                 kOrigin, "the observation cannot be calibrated");
    }
    std::vector<Ref<LinearAction>> composed;
    composed.reserve(noise_model->influence.size());
    for (const Ref<LinearAction>& influence : noise_model->influence) {
      Result<Ref<LinearAction>> chained = observation::compose_actions(context, val(scale), influence);
      if (!ok(chained)) {
        return model::fail<Ref<NativeObservation>>(chained.code,
                                                   "the calibration Jacobian cannot be applied",
                                                   kOrigin, "the observation cannot be calibrated");
      }
      composed.push_back(val(chained));
    }
    noise_model->influence = composed;
  } else {
    const bool has_gain = request.gain_e_per_adu.has_value() &&
                          std::isfinite(*request.gain_e_per_adu) && *request.gain_e_per_adu > 0.0;
    const bool has_read = request.read_rms_e.has_value() && std::isfinite(*request.read_rms_e) &&
                          *request.read_rms_e >= 0.0;
    if (has_gain && has_read) {
      // The explicit Poisson plug-in: the electron expectation is taken from
      // the data as max(d-b,0); the shot variance is therefore approximate.
      const double gain = *request.gain_e_per_adu;
      std::vector<double> shot(count, 0.0);
      std::vector<double> read(count, 0.0);
      for (U64 i = 0; i < count; ++i) {
        const double d = val(adu)[i];
        const double b = has_bias ? bias[i] : 0.0;
        const double signal = std::max(d - b, 0.0);
        const double variance_adu = signal / gain;
        shot[i] = std::sqrt(variance_adu > 0.0 ? variance_adu : 0.0);
        read[i] = *request.read_rms_e / gain;
      }
      SharedId shot_id;
      shot_id.external_namespace = "acsd.native";
      shot_id.entity = "shot";
      Result<Ref<NoiseRoot>> shot_root =
          make_native_root(context, raw->source_index, adu_unit, shot_id, shot, "shot_poisson_plugin");
      if (!ok(shot_root)) {
        return model::fail<Ref<NativeObservation>>(shot_root.code, "the shot root cannot be built",
                                                   kOrigin, "the observation cannot be calibrated");
      }
      SharedId read_id;
      read_id.external_namespace = "acsd.native";
      read_id.entity = "read";
      Result<Ref<NoiseRoot>> read_root =
          make_native_root(context, raw->source_index, adu_unit, read_id, read, "readout");
      if (!ok(read_root)) {
        return model::fail<Ref<NativeObservation>>(read_root.code, "the readout root cannot be built",
                                                   kOrigin, "the observation cannot be calibrated");
      }
      Result<Ref<LinearAction>> scale =
          observation::make_diagonal_action(context, inverse_flat, raw->source_index, adu_unit, signal_unit);
      if (!ok(scale)) {
        return model::fail<Ref<NativeObservation>>(scale.code,
                                                   "the calibration Jacobian cannot be built", kOrigin,
                                                   "the observation cannot be calibrated");
      }
      Result<Ref<LinearAction>> shot_influence = observation::compose_actions(context, val(scale), val(shot_root)->factor);
      if (!ok(shot_influence)) {
        return model::fail<Ref<NativeObservation>>(shot_influence.code,
                                                   "the shot influence cannot be built", kOrigin,
                                                   "the observation cannot be calibrated");
      }
      Result<Ref<LinearAction>> read_influence = observation::compose_actions(context, val(scale), val(read_root)->factor);
      if (!ok(read_influence)) {
        return model::fail<Ref<NativeObservation>>(read_influence.code,
                                                   "the readout influence cannot be built", kOrigin,
                                                   "the observation cannot be calibrated");
      }
      noise_model->innovations.push_back(val(shot_root));
      noise_model->innovations.push_back(val(read_root));
      noise_model->influence.push_back(val(shot_influence));
      noise_model->influence.push_back(val(read_influence));
      noise_state = NoiseState::approximate;
      noise_included.push_back("shot_poisson_plugin(max(d-b,0))");
      noise_included.push_back("readout_noise");
      noise_omitted.push_back("exact_shot_variance_not_recovered");
    } else {
      // No electron expectation is available: the complete variance is unknown.
      UnknownTerm term;
      term.id = "shot_variance_unknown";
      term.reason =
          "no non-negative electron expectation is available; the gain and readout metadata "
          "alone do not recover the shot variance";
      Result<Ref<LinearAction>> placeholder =
          observation::make_diagonal_action(context, std::vector<double>(count, 1.0), raw->source_index,
                               signal_unit, signal_unit);
      if (!ok(placeholder)) {
        return model::fail<Ref<NativeObservation>>(placeholder.code,
                                                   "the unknown term cannot be built", kOrigin,
                                                   "the observation cannot be calibrated");
      }
      term.influence = val(placeholder);
      noise_model->unknown_terms.push_back(term);
      noise_state = NoiseState::unknown;
      noise_omitted.push_back("shot_variance_unknown");
      if (has_gain && has_read) {
        // unreachable, kept for clarity of the branch structure
      } else if (has_read && !has_gain) {
        noise_omitted.push_back("readout_noise_without_gain");
      }
    }
  }

  if (request.master_noise) {
    // The master root is merged by its shared identity; it is not rebuilt for
    // every frame.  The flat Jacobian d x / d flat = -x/flat maps the master
    // amplitude onto the calibrated samples.
    std::vector<double> flat_jacobian(count, 0.0);
    for (U64 i = 0; i < count; ++i) {
      const double f = has_flat ? flat[i] / request.flat_normalization : 1.0;
      flat_jacobian[i] = (std::isfinite(f) && f > 0.0 && std::isfinite(values[i]))
                              ? -values[i] / f
                              : 0.0;
    }
    Result<Ref<LinearAction>> jacobian =
        observation::make_diagonal_action(context, flat_jacobian, raw->source_index, signal_unit, signal_unit);
    if (!ok(jacobian)) {
      return model::fail<Ref<NativeObservation>>(jacobian.code,
                                                 "the master Jacobian cannot be built", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    const std::size_t shared_count = request.master_noise->shared_factors.size();
    if (request.master_noise->influence.size() <
        request.master_noise->innovations.size() + shared_count) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model, "the master noise model has an incomplete influence list", kOrigin,
          "the observation cannot be calibrated");
    }
    const std::size_t offset = request.master_noise->influence.size() - shared_count;
    for (std::size_t k = 0; k < shared_count; ++k) {
      const Ref<LinearAction>& influence = request.master_noise->influence[offset + k];
      Result<Ref<LinearAction>> chained = observation::compose_actions(context, val(jacobian), influence);
      if (!ok(chained)) {
        return model::fail<Ref<NativeObservation>>(chained.code,
                                                   "the master root cannot be merged", kOrigin,
                                                   "the observation cannot be calibrated");
      }
      noise_model->shared_factors.push_back(request.master_noise->shared_factors[k]);
      noise_model->influence.push_back(val(chained));
    }
    for (const UnknownTerm& term : request.master_noise->unknown_terms) {
      noise_model->unknown_terms.push_back(term);
    }
    noise_included.push_back("master_flat_root_shared_identity");
    if (!request.master_noise->unknown_terms.empty()) noise_state = NoiseState::unknown;
  }

  noise_model->state = noise_state;
  noise_model->assessment = model::make_assessment(Code::ok);
  noise_model->assessment.noise = noise_state;
  noise_model->identity = model::make_identity(
      "acsd.NoiseModel", 2, 0,
      model::sha256_hex(raw->frame + ":" + std::to_string(count)), "acsd");

  // ---------------------------------------------------------------------
  // The native observation.
  // ---------------------------------------------------------------------
  const std::vector<U64> shape{count};
  Result<ArrayRef> value_array = observation::write_f64_values(context, values, shape, signal_unit,
                                                                index_id);
  if (!ok(value_array)) {
    return model::fail<Ref<NativeObservation>>(value_array.code, "the calibrated values cannot be stored",
                                               kOrigin, "the observation cannot be calibrated");
  }
  Result<ArrayRef> lower_array = observation::write_f64_values(context, lower, shape, signal_unit,
                                                                index_id);
  if (!ok(lower_array)) {
    return model::fail<Ref<NativeObservation>>(lower_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<ArrayRef> upper_array = observation::write_f64_values(context, upper, shape, signal_unit,
                                                                index_id);
  if (!ok(upper_array)) {
    return model::fail<Ref<NativeObservation>>(upper_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<ArrayRef> flag_array = observation::write_u16_values(context, flags, shape,
                                                              model::unit_dimensionless(),
                                                              index_id);
  if (!ok(flag_array)) {
    return model::fail<Ref<NativeObservation>>(flag_array.code, "the quality flags cannot be stored",
                                               kOrigin, "the observation cannot be calibrated");
  }
  Result<Ref<LinearAction>> identity_action =
      make_identity_action(context, raw->source_index, signal_unit);
  if (!ok(identity_action)) {
    return model::fail<Ref<NativeObservation>>(identity_action.code,
                                               "the identity regrouping cannot be built", kOrigin,
                                               "the observation cannot be calibrated");
  }

  // The numerical certificate of the calibrated mean.
  auto error = std::make_shared<ErrorRecord>();
  error->unknown_model_terms.push_back("true_photon_shot_noise_is_not_a_numerical_error");
  error->unknown_model_terms.push_back("real_master_and_flat_deviations_remain_in_the_noise_model");
  error->linearization_selection =
      model::make_condition("native_calibrated_mean", "deterministic_interval");
  error->identity = model::make_identity("acsd.ErrorRecord", 2, 0,
                                         model::sha256_hex("calibrate:" + raw->frame), "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto observation_model = std::make_shared<NativeObservation>();
  observation_model->integrated_values = val(value_array);
  observation_model->dq = val(flag_array);
  observation_model->native_index = raw->source_index;
  observation_model->footprints = raw->footprints;
  observation_model->regrouping = val(identity_action);
  observation_model->scale_applications.clear();
  observation_model->noise = std::shared_ptr<const NoiseModel>(noise_model);
  observation_model->unit = signal_unit;
  observation_model->response = nullptr;
  observation_model->kernel = nullptr;
  observation_model->native_parent = nullptr;
  observation_model->physical_collection = true;
  observation_model->context = raw->context;
  observation_model->frame = raw->frame;
  observation_model->numerical_lower = val(lower_array);
  observation_model->numerical_upper = val(upper_array);
  observation_model->error = std::shared_ptr<const ErrorRecord>(error);

  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  assessment.noise = noise_state;
  assessment.science = ScienceState::bounded;
  observation::note_calibration_jacobian(assessment);
  for (const std::string& item : noise_included) model::set_included(assessment, item);
  for (const std::string& item : noise_omitted) model::set_omitted(assessment, item);
  if (repaired > 0) {
    model::add_issue(assessment, Code::unavailable,
                      "repaired samples need a G/influence before they may be used", kOrigin,
                      "the repaired samples stay flagged in the quality array");
    model::set_omitted(assessment, "repaired_samples_without_influence");
  }
  if (bad_flat > 0) {
    model::add_issue(assessment, Code::invalid_model,
                      "a non-positive or non-finite flat was not repaired numerically", kOrigin,
                      "the affected samples keep their quality flag and carry no value");
  }
  if (!raw->footprints) {
    model::add_issue(assessment, Code::unavailable, "the frame has no detector footprint", kOrigin,
                      "the footprint capability is missing");
  }
  if (noise_state == NoiseState::unknown) {
    model::add_issue(assessment, Code::unavailable,
                      "the complete noise variance is unknown; the signal is still valid", kOrigin,
                      "the variance cannot be published");
  }
  observation_model->assessment = assessment;
  observation_model->identity = model::make_identity(
      "acsd.NativeObservation", 2, 0,
      observation::payload_hash("acsd.NativeObservation", raw->frame,
                                {model::sha256_hex(model::encode_f64(values)),
                                 model::sha256_hex(model::encode_u16(flags))}),
      "acsd");

  return model::ok_result(std::shared_ptr<const NativeObservation>(observation_model));
}

}  // namespace acsd
