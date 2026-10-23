// The unique per-sample calibration kernel and the shared finish assembly of
// observation.calibrate.
//
// The arithmetic, the outward interval primitives and the data quality rules
// live here exactly once.  The synchronous public entry point, the block
// adapter and the runtime finish assembly all call these functions; no other
// translation unit repeats the pixel formula.
#include "observation/calibration_kernel.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "numerics/interval.hpp"
#include "observation/observation.hpp"

namespace acsd::observation {

using rt::Done;

namespace {

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

Result<Ref<LinearAction>> make_calibration_diagonal(Context& context,
                                                const std::vector<double>& weights,
                                                Ref<Index> index, const Unit& in_unit,
                                                const Unit& out_unit) {
  const std::vector<U64> shape{static_cast<U64>(weights.size())};
  Result<ArrayRef> array =
      write_f64_values(context, weights, shape, in_unit,
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
      make_calibration_diagonal(context, stddev, index, adu_unit, adu_unit);
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

}  // namespace

CalibrationSampleParameters calibration_parameters(const Calibration& request) {
  CalibrationSampleParameters parameters;
  parameters.has_bias = static_cast<bool>(request.bias);
  parameters.has_dark = static_cast<bool>(request.dark);
  parameters.has_flat = static_cast<bool>(request.flat);
  parameters.dark_contains_bias = request.dark_contains_bias;
  parameters.flat_normalization = request.flat_normalization;
  parameters.exposure_ratio = 0.0;
  if (parameters.has_dark) {
    parameters.exposure_ratio = request.exposure_seconds / request.dark_exposure_seconds;
  }
  return parameters;
}

Result<CalibratedSample> calibrate_sample(const CalibrationSampleInputs& inputs) {
  const CalibrationSampleParameters& parameters = inputs.parameters;
  CalibratedSample sample;
  sample.dq = inputs.dq;
  sample.repaired = (inputs.dq & kDqRepaired) != 0u;

  const double d = inputs.raw;
  if (!std::isfinite(d) && (inputs.dq & kDqDisqualifying) == 0u) {
    return model::fail<CalibratedSample>(
        Code::invalid_model,
        "a non-finite raw value conflicts with the valid data quality bits", kOrigin,
        "the observation cannot be calibrated");
  }

  const double b = parameters.has_bias ? inputs.bias : 0.0;
  const double dk = parameters.has_dark ? inputs.dark : 0.0;
  double f = 1.0;
  if (parameters.has_flat) f = inputs.flat / parameters.flat_normalization;
  const bool flat_usable = std::isfinite(f) && f > 0.0;
  sample.flat_usable = flat_usable;

  const numerics::Interval d_i = numerics::interval_point(std::isfinite(d) ? d : 0.0);
  const numerics::Interval b_i = numerics::interval_point(std::isfinite(b) ? b : 0.0);
  const numerics::Interval dk_i = numerics::interval_point(std::isfinite(dk) ? dk : 0.0);
  const numerics::Interval f_i = numerics::interval_point(std::isfinite(f) ? f : 0.0);

  numerics::Interval numerator = numerics::interval_sub(d_i, b_i);
  if (parameters.has_dark) {
    const numerics::Interval dark_term = parameters.dark_contains_bias
                                             ? numerics::interval_sub(dk_i, b_i)
                                             : dk_i;
    numerator =
        numerics::interval_sub(numerator,
                               numerics::interval_scale(dark_term, parameters.exposure_ratio));
  }
  const numerics::Interval x_i = numerics::interval_div(numerator, f_i);

  std::uint16_t q = inputs.dq;
  double x = 0.0;
  if (!flat_usable) {
    x = std::numeric_limits<double>::quiet_NaN();
    if ((q & kDqDisqualifying) == 0u) q = static_cast<std::uint16_t>(q | kDqBad);
  } else {
    x = (d - b - (parameters.has_dark
                      ? parameters.exposure_ratio *
                            (parameters.dark_contains_bias ? (dk - b) : dk)
                      : 0.0)) /
        f;
    if (!std::isfinite(x)) {
      x = std::numeric_limits<double>::quiet_NaN();
      if ((q & kDqDisqualifying) == 0u) q = static_cast<std::uint16_t>(q | kDqBad);
      sample.value = x;
      sample.dq = q;
      sample.lower = std::numeric_limits<double>::quiet_NaN();
      sample.upper = std::numeric_limits<double>::quiet_NaN();
      sample.nonfinite_value = true;
      return model::ok_result(sample);
    }
  }
  sample.value = x;
  sample.dq = q;
  // The enclosure is always the one of the same expression.  The nominal value
  // is inside it; a non-finite raw sample keeps the enclosure of the finite
  // substitute (the interval never pretends to know an unavailable value).
  sample.lower = x_i.lower;
  sample.upper = x_i.upper;
  return model::ok_result(sample);
}

Result<Ref<NativeObservation>> assemble_calibrated_native(
    Context& context, Ref<RawObservation> raw_observation, const Calibration& request,
    const CalibrationAssemblyInputs& inputs, ArrayRef value_array, ArrayRef lower_array,
    ArrayRef upper_array, ArrayRef dq_array, const modules::CalibrationSampleStats& stats) {
  if (!raw_observation || !raw_observation->source_index) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the calibrated frame has no source index",
                                               kOrigin, "the observation cannot be calibrated");
  }
  const U64 count = static_cast<U64>(inputs.values.size());
  if (inputs.flags.size() != count ||
      count != raw_observation->width * raw_observation->height) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the calibrated payload does not match the frame", kOrigin,
        "the observation cannot be calibrated");
  }
  if (!value_array || !lower_array || !upper_array || !dq_array) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the calibrated arrays are incomplete", kOrigin,
                                               "the observation cannot be calibrated");
  }
  const bool has_bias = static_cast<bool>(request.bias);
  const bool has_dark = static_cast<bool>(request.dark);
  const bool has_flat = static_cast<bool>(request.flat);

  // -----------------------------------------------------------------------
  // The noise budget.  The Jacobian is the one of the actual formula:
  //   d x / d d = 1/flat
  //   d x / d flat = -x/flat
  // and a dark map that contains the bias contributes one root, not two.
  // -----------------------------------------------------------------------
  std::vector<double> inverse_flat;
  std::vector<double> flat_values;
  std::vector<double> raw_values;
  std::vector<double> bias_values;
  auto size_ok = [&](std::span<const double> values) { return values.size() == count; };
  if (has_flat) {
    if (!size_ok(inputs.flat)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the flat values do not cover the calibrated frame",
                                                 kOrigin,
                                                 "the observation cannot be calibrated");
    }
    flat_values.assign(inputs.flat.begin(), inputs.flat.end());
  }

  const Id index_id = raw_observation->source_index->identity.object_id;
  const std::string identity_text = detector_identity(*raw_observation->source_index);
  const Unit adu_unit = model::unit_adu(identity_text);
  const Unit signal_unit = model::unit_signal(identity_text);
  const Unit variance_unit = model::unit_variance(identity_text);

  const bool has_gain = request.gain_e_per_adu.has_value() &&
                        std::isfinite(*request.gain_e_per_adu) && *request.gain_e_per_adu > 0.0;
  const bool has_read = request.read_rms_e.has_value() &&
                        std::isfinite(*request.read_rms_e) && *request.read_rms_e >= 0.0;
  const bool poisson_plugin = !request.raw_noise && has_gain && has_read;

  if (!request.raw_noise) {
    if (poisson_plugin) {
      if (!size_ok(inputs.raw)) {
        return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                   "the raw values do not cover the calibrated frame",
                                                   kOrigin,
                                                   "the observation cannot be calibrated");
      }
      raw_values.assign(inputs.raw.begin(), inputs.raw.end());
      if (has_bias) {
        if (!size_ok(inputs.bias)) {
          return model::fail<Ref<NativeObservation>>(
              Code::invalid_model, "the bias values do not cover the calibrated frame", kOrigin,
              "the observation cannot be calibrated");
        }
        bias_values.assign(inputs.bias.begin(), inputs.bias.end());
      }
    }
  }

  inverse_flat.assign(count, 1.0);
  for (U64 i = 0; i < count; ++i) {
    const double f = has_flat ? flat_values[i] / request.flat_normalization : 1.0;
    inverse_flat[i] = (std::isfinite(f) && f > 0.0) ? 1.0 / f : 0.0;
  }

  auto noise_model = std::make_shared<NoiseModel>();
  noise_model->variance_unit = variance_unit;
  noise_model->condition = model::make_condition("native_calibrated_noise", "calibration_jacobian");
  noise_model->state = NoiseState::unknown;
  std::vector<std::string> noise_included;
  std::vector<std::string> noise_omitted;
  NoiseState noise_state = NoiseState::unknown;
  std::string noise_source =
      "the noise source is unknown: neither a supplied raw noise nor an explicit Poisson plugin "
      "was available";

  if (request.raw_noise) {
    // The user supplied the complete electron expectation and readout factor.
    noise_model->innovations = request.raw_noise->innovations;
    noise_model->shared_factors = request.raw_noise->shared_factors;
    noise_model->influence = request.raw_noise->influence;
    noise_model->unknown_terms = request.raw_noise->unknown_terms;
    noise_state = request.raw_noise->state;
    noise_included.push_back("raw_noise_user_supplied");
    noise_source =
        "the noise source is the supplied raw noise: the complete electron expectation and the "
        "readout factor are given, so the variance is the one the user declared";
    // The calibration Jacobian d x / d d = 1/flat maps the raw amplitudes onto
    // the calibrated samples.
    Result<Ref<LinearAction>> scale = make_calibration_diagonal(
        context, inverse_flat, raw_observation->source_index, adu_unit, signal_unit);
    if (!ok(scale)) {
      return model::fail<Ref<NativeObservation>>(scale.code,
                                                 "the calibration Jacobian cannot be built",
                                                 kOrigin, "the observation cannot be calibrated");
    }
    std::vector<Ref<LinearAction>> composed;
    composed.reserve(noise_model->influence.size());
    for (const Ref<LinearAction>& influence : noise_model->influence) {
      Result<Ref<LinearAction>> chained = acsd::compose(context, val(scale), influence);
      if (!ok(chained)) {
        return model::fail<Ref<NativeObservation>>(chained.code,
                                                   "the calibration Jacobian cannot be applied",
                                                   kOrigin, "the observation cannot be calibrated");
      }
      composed.push_back(val(chained));
    }
    noise_model->influence = composed;
  } else if (poisson_plugin) {
    // The explicit Poisson plug-in: the electron expectation is taken from
    // the data as max(d-b,0); the shot variance is therefore approximate.
    const double gain = *request.gain_e_per_adu;
    std::vector<double> shot(count, 0.0);
    std::vector<double> read(count, 0.0);
    for (U64 i = 0; i < count; ++i) {
      const double d = raw_values[i];
      const double b = has_bias ? bias_values[i] : 0.0;
      const double signal = std::max(d - b, 0.0);
      const double variance_adu = signal / gain;
      shot[i] = std::sqrt(variance_adu > 0.0 ? variance_adu : 0.0);
      read[i] = *request.read_rms_e / gain;
    }
    SharedId shot_id;
    shot_id.external_namespace = "acsd.native";
    shot_id.entity = "shot";
    Result<Ref<NoiseRoot>> shot_root =
        make_native_root(context, raw_observation->source_index, adu_unit, shot_id, shot,
                         "shot_poisson_plugin");
    if (!ok(shot_root)) {
      return model::fail<Ref<NativeObservation>>(shot_root.code, "the shot root cannot be built",
                                                 kOrigin, "the observation cannot be calibrated");
    }
    SharedId read_id;
    read_id.external_namespace = "acsd.native";
    read_id.entity = "read";
    Result<Ref<NoiseRoot>> read_root =
        make_native_root(context, raw_observation->source_index, adu_unit, read_id, read,
                         "readout");
    if (!ok(read_root)) {
      return model::fail<Ref<NativeObservation>>(read_root.code, "the readout root cannot be built",
                                                 kOrigin, "the observation cannot be calibrated");
    }
    Result<Ref<LinearAction>> scale = make_calibration_diagonal(
        context, inverse_flat, raw_observation->source_index, adu_unit, signal_unit);
    if (!ok(scale)) {
      return model::fail<Ref<NativeObservation>>(scale.code,
                                                 "the calibration Jacobian cannot be built",
                                                 kOrigin, "the observation cannot be calibrated");
    }
    Result<Ref<LinearAction>> shot_influence =
        acsd::compose(context, val(scale), val(shot_root)->factor);
    if (!ok(shot_influence)) {
      return model::fail<Ref<NativeObservation>>(shot_influence.code,
                                                 "the shot influence cannot be built", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    Result<Ref<LinearAction>> read_influence =
        acsd::compose(context, val(scale), val(read_root)->factor);
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
    noise_source =
        "the noise source is the explicit Poisson plugin: the electron expectation is "
        "max(d-b,0) and the readout factor is the declared read noise, so the shot variance "
        "is approximate and the exact photon statistics are not recovered";
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
    Result<Ref<LinearAction>> placeholder = make_calibration_diagonal(
        context, std::vector<double>(count, 1.0), raw_observation->source_index, signal_unit,
        signal_unit);
    if (!ok(placeholder)) {
      return model::fail<Ref<NativeObservation>>(placeholder.code,
                                                 "the unknown term cannot be built", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    term.influence = val(placeholder);
    noise_model->unknown_terms.push_back(term);
    noise_state = NoiseState::unknown;
    noise_source =
        "the noise source is unknown: the gain and readout metadata alone do not recover the "
        "shot variance, so the complete variance is unknown and is never filled with zero";
    noise_omitted.push_back("shot_variance_unknown");
    if (has_read && !has_gain) {
      noise_omitted.push_back("readout_noise_without_gain");
    }
  }

  if (request.master_noise) {
    // The master root is merged by its shared identity; it is not rebuilt for
    // every frame.  The flat Jacobian d x / d flat = -x/flat maps the master
    // amplitude onto the calibrated samples.
    std::vector<double> flat_jacobian(count, 0.0);
    for (U64 i = 0; i < count; ++i) {
      const double f = has_flat ? flat_values[i] / request.flat_normalization : 1.0;
      flat_jacobian[i] = (std::isfinite(f) && f > 0.0 && std::isfinite(inputs.values[i]))
                             ? -inputs.values[i] / f
                             : 0.0;
    }
    Result<Ref<LinearAction>> jacobian =
        make_calibration_diagonal(context, flat_jacobian, raw_observation->source_index, signal_unit,
                             signal_unit);
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
    const std::size_t offset =
        request.master_noise->influence.size() - shared_count;
    for (std::size_t k = 0; k < shared_count; ++k) {
      const Ref<LinearAction>& influence = request.master_noise->influence[offset + k];
      Result<Ref<LinearAction>> chained = acsd::compose(context, val(jacobian), influence);
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
      model::sha256_hex(raw_observation->frame + ":" + std::to_string(count)), "acsd");

  const std::vector<U64> shape{count};
  Result<Ref<LinearAction>> identity_action =
      make_identity_action(context, raw_observation->source_index, signal_unit);
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
                                         model::sha256_hex("calibrate:" + raw_observation->frame),
                                         "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto observation_model = std::make_shared<NativeObservation>();
  observation_model->integrated_values = value_array;
  observation_model->dq = dq_array;
  observation_model->native_index = raw_observation->source_index;
  observation_model->footprints = raw_observation->footprints;
  observation_model->regrouping = val(identity_action);
  observation_model->scale_applications.clear();
  observation_model->noise = std::shared_ptr<const NoiseModel>(noise_model);
  observation_model->unit = signal_unit;
  observation_model->response = nullptr;
  observation_model->kernel = nullptr;
  observation_model->native_parent = nullptr;
  observation_model->physical_collection = true;
  observation_model->context = raw_observation->context;
  observation_model->frame = raw_observation->frame;
  observation_model->numerical_lower = lower_array;
  observation_model->numerical_upper = upper_array;
  observation_model->error = std::shared_ptr<const ErrorRecord>(error);

  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  assessment.noise = noise_state;
  assessment.science = ScienceState::bounded;
  note_calibration_jacobian(assessment);
  for (const std::string& item : noise_included) model::set_included(assessment, item);
  for (const std::string& item : noise_omitted) model::set_omitted(assessment, item);
  // The branch of the noise rule that was taken, with its reason.
  model::set_included(assessment, "noise_source=" + noise_source);
  if (stats.repaired > 0) {
    model::add_issue(assessment, Code::unavailable,
                     "repaired samples need a G/influence before they may be used", kOrigin,
                     "the repaired samples stay flagged in the quality array");
    model::set_omitted(assessment, "repaired_samples_without_influence");
  }
  if (stats.bad_flat > 0) {
    model::add_issue(assessment, Code::invalid_model,
                     "a non-positive or non-finite flat was not repaired numerically", kOrigin,
                     "the affected samples keep their quality flag and carry no value");
  }
  if (!raw_observation->footprints) {
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
      payload_hash("acsd.NativeObservation", raw_observation->frame,
                   {model::sha256_hex(model::encode_f64(inputs.values)),
                    model::sha256_hex(model::encode_u16(inputs.flags))}),
      "acsd");

  return model::ok_result(std::shared_ptr<const NativeObservation>(observation_model));
}


// ---------------------------------------------------------------------------
// The single metadata validation of the process (P1-4).  It is called by the
// synchronous calibrate_observation and by the typed validate/bounds/plan
// hooks, so the same RawObservation/Calibration can never produce two
// different metadata verdicts.  It reads no payload: the NaN/DQ consistency
// and the per-value flat range stay in the CPU stage after the block hash.
// ---------------------------------------------------------------------------
namespace {

// The declared root identity of a noise model: every concatenated root must
// carry a complete SharedId and a factor, otherwise the shared/cross term
// cannot be merged later.
Result<Done> validate_noise_model_metadata(const Ref<NoiseModel>& model, const char* label) {
  if (!model) return model::ok_result(Done{});
  const std::size_t roots = model->innovations.size() + model->shared_factors.size();
  if (model->influence.size() < roots) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label +
                                 " noise model has fewer influences than roots",
                             kOrigin, "the calibration cannot be planned");
  }
  auto check_root = [&](const Ref<NoiseRoot>& root, const char* what) -> Result<Done> {
    if (!root || !root->factor) {
      return model::fail<Done>(Code::invalid_model,
                               std::string("the ") + label + " " + what +
                                   " root has no factor",
                               kOrigin, "the noise model cannot be merged");
    }
    const SharedId& id = root->shared_id;
    if (id.external_namespace.empty() || id.entity.empty() || id.model_revision.empty() ||
        id.component.empty()) {
      return model::fail<Done>(Code::invalid_model,
                               std::string("the ") + label + " " + what +
                                   " root has an incomplete shared identity",
                               kOrigin, "the noise root cannot be merged by identity");
    }
    return model::ok_result(Done{});
  };
  for (const Ref<NoiseRoot>& root : model->innovations) {
    Result<Done> check = check_root(root, "innovation");
    if (!ok(check)) return check;
  }
  for (const Ref<NoiseRoot>& root : model->shared_factors) {
    Result<Done> check = check_root(root, "shared");
    if (!ok(check)) return check;
  }
  for (std::size_t i = 0; i < model->shared_factors.size(); ++i) {
    for (std::size_t j = i + 1; j < model->shared_factors.size(); ++j) {
      const SharedId& a = model->shared_factors[i]->shared_id;
      const SharedId& b = model->shared_factors[j]->shared_id;
      if (a.external_namespace == b.external_namespace && a.entity == b.entity &&
          a.model_revision == b.model_revision && a.component == b.component) {
        return model::fail<Done>(Code::invalid_model,
                                 std::string("the ") + label +
                                     " noise model lists the same shared root twice",
                                 kOrigin, "the cross term would be counted twice");
      }
    }
  }
  return model::ok_result(Done{});
}

Result<Done> check_array_metadata(const ArrayRef& array, U64 count, DType dtype,
                                  const char* label, bool check_dimensionless) {
  if (!array) return model::ok_result(Done{});
  if (array->dtype != dtype) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has the wrong dtype",
                             kOrigin, "the calibration cannot be planned");
  }
  if (array->index_id.empty()) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has no index", kOrigin,
                             "the calibration cannot be planned");
  }
  if (!array_has_count(*array, count)) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array does not match the frame",
                             kOrigin, "the calibration cannot be planned");
  }
  if (array->unit.scale.empty()) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has no unit", kOrigin,
                             "the calibration cannot be planned");
  }
  if (check_dimensionless && (array->unit.scale != "1" || array->unit.linear_power != 0)) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array must be dimensionless", kOrigin,
                             "the data quality flags are not a physical quantity");
  }
  return model::ok_result(Done{});
}

}  // namespace

Result<rt::Done> validate_calibration_input(Ref<RawObservation> raw, const Calibration& request) {
  if (!raw) {
    return model::fail<rt::Done>(Code::invalid_request, "no raw observation supplied", kOrigin,
                                 "the observation cannot be calibrated");
  }
  if (!raw->adu || !raw->dq) {
    return model::fail<rt::Done>(Code::invalid_model,
                                 "the raw observation has no data or quality array", kOrigin,
                                 "the observation cannot be calibrated");
  }
  if (!raw->source_index) {
    return model::fail<rt::Done>(Code::invalid_model, "the raw observation has no source index",
                                 kOrigin, "the observation cannot be calibrated");
  }
  if (raw->width == 0 || raw->height == 0) {
    return model::fail<rt::Done>(Code::invalid_model, "the frame has no samples", kOrigin,
                                 "the observation cannot be calibrated");
  }
  if (raw->width > std::numeric_limits<U64>::max() / raw->height) {
    return model::fail<rt::Done>(Code::invalid_model, "the frame size overflows", kOrigin,
                                 "the observation cannot be calibrated");
  }
  const U64 count = raw->width * raw->height;
  if (raw->source_index->size != count) {
    return model::fail<rt::Done>(Code::invalid_model, "the source index does not cover the frame",
                                 kOrigin, "the observation cannot be calibrated");
  }
  Result<rt::Done> adu =
      check_array_metadata(raw->adu, count, DType::f64, "raw data", false);
  if (!ok(adu)) return adu;
  Result<rt::Done> dq = check_array_metadata(raw->dq, count, DType::u16, "raw quality", true);
  if (!ok(dq)) return dq;
  Result<rt::Done> bias = check_array_metadata(request.bias, count, DType::f64, "bias", false);
  if (!ok(bias)) return bias;
  Result<rt::Done> dark = check_array_metadata(request.dark, count, DType::f64, "dark", false);
  if (!ok(dark)) return dark;
  Result<rt::Done> flat = check_array_metadata(request.flat, count, DType::f64, "flat", false);
  if (!ok(flat)) return flat;
  if (raw->frame.empty()) {
    return model::fail<rt::Done>(Code::invalid_model, "the frame has no identity", kOrigin,
                                 "the calibrated observation cannot keep the frame");
  }
  if (!raw->footprints) {
    return model::fail<rt::Done>(Code::unavailable, "the frame has no detector footprint", kOrigin,
                                 "the footprint capability is missing");
  }
  if (!std::isfinite(request.exposure_seconds) || !(request.exposure_seconds > 0.0)) {
    return model::fail<rt::Done>(Code::invalid_model,
                                 "the exposure time must be finite and positive", kOrigin,
                                 "the observation cannot be calibrated");
  }
  if (!request.flat) {
    if (request.flat_normalization != 1.0) {
      return model::fail<rt::Done>(Code::invalid_model,
                                   "an absent flat requires the explicit normalisation 1", kOrigin,
                                   "the calibration cannot be planned");
    }
  } else {
    if (!std::isfinite(request.flat_normalization) || !(request.flat_normalization > 0.0)) {
      return model::fail<rt::Done>(Code::invalid_model,
                                   "the flat normalisation must be finite and positive", kOrigin,
                                   "the observation cannot be calibrated");
    }
  }
  if (request.dark) {
    if (!std::isfinite(request.dark_exposure_seconds) || !(request.dark_exposure_seconds > 0.0)) {
      return model::fail<rt::Done>(Code::invalid_model,
                                   "the dark exposure time must be positive when a dark is used",
                                   kOrigin, "the observation cannot be calibrated");
    }
    const double ratio = request.exposure_seconds / request.dark_exposure_seconds;
    if (!std::isfinite(ratio) || !(ratio > 0.0)) {
      return model::fail<rt::Done>(Code::invalid_model, "the exposure ratio must be positive",
                                   kOrigin, "the observation cannot be calibrated");
    }
  }
  Result<rt::Done> raw_noise = validate_noise_model_metadata(request.raw_noise, "raw");
  if (!ok(raw_noise)) return raw_noise;
  Result<rt::Done> master_noise = validate_noise_model_metadata(request.master_noise, "master");
  if (!ok(master_noise)) return master_noise;
  return model::ok_result(rt::Done{});
}

}  // namespace acsd::observation
