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
// The per-sample formula, the outward interval and the data quality rules are
// owned by src/observation/calibration_kernel.cpp; the block adapter and the
// runtime finish assembly call the same functions.  This entry point only
// reads the input arrays, drives the kernel once per native sample and writes
// the four signal arrays before the shared finish assembly binds the model.
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "observation/calibration_kernel.hpp"
#include "observation/observation.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "calibrate_observation";

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
  if (has_dark) {
    if (!std::isfinite(request.dark_exposure_seconds) || !(request.dark_exposure_seconds > 0.0)) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model, "the dark exposure time must be positive", kOrigin,
          "the observation cannot be calibrated");
    }
    const double exposure_ratio = request.exposure_seconds / request.dark_exposure_seconds;
    if (!std::isfinite(exposure_ratio) || !(exposure_ratio > 0.0)) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the exposure ratio must be positive", kOrigin,
                                                 "the observation cannot be calibrated");
    }
  }

  // ---------------------------------------------------------------------
  // The calibrated mean and its numerical enclosure: one call per sample.
  // ---------------------------------------------------------------------
  const observation::CalibrationSampleParameters parameters =
      observation::calibration_parameters(request);
  std::vector<double> values(count, 0.0);
  std::vector<double> lower(count, 0.0);
  std::vector<double> upper(count, 0.0);
  std::vector<std::uint16_t> flags(count, 0);
  modules::CalibrationSampleStats stats;
  for (U64 i = 0; i < count; ++i) {
    observation::CalibrationSampleInputs inputs;
    inputs.raw = val(adu)[i];
    inputs.dq = val(dq)[i];
    inputs.bias = has_bias ? bias[i] : 0.0;
    inputs.dark = has_dark ? dark[i] : 0.0;
    inputs.flat = has_flat ? flat[i] : 0.0;
    inputs.parameters = parameters;
    Result<observation::CalibratedSample> sample = observation::calibrate_sample(inputs);
    if (!ok(sample)) {
      const std::string reason =
          sample.assessment.issues.empty() ? std::string("the sample cannot be calibrated")
                                           : sample.assessment.issues.front().reason;
      return model::fail<Ref<NativeObservation>>(sample.code, reason, kOrigin,
                                                 "the observation cannot be calibrated");
    }
    if (val(sample).repaired) ++stats.repaired;
    if (!val(sample).flat_usable) ++stats.bad_flat;
    values[i] = val(sample).value;
    lower[i] = val(sample).lower;
    upper[i] = val(sample).upper;
    flags[i] = val(sample).dq;
  }

  // ---------------------------------------------------------------------
  // The signal arrays and the shared finish assembly.
  // ---------------------------------------------------------------------
  const std::string identity_text = observation::detector_identity(*raw->source_index);
  const Unit signal_unit = model::unit_signal(identity_text);
  const Id index_id = raw->source_index->identity.object_id;
  const std::vector<U64> shape{count};
  Result<ArrayRef> value_array =
      observation::write_f64_values(context, values, shape, signal_unit, index_id);
  if (!ok(value_array)) {
    return model::fail<Ref<NativeObservation>>(value_array.code,
                                               "the calibrated values cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<ArrayRef> lower_array =
      observation::write_f64_values(context, lower, shape, signal_unit, index_id);
  if (!ok(lower_array)) {
    return model::fail<Ref<NativeObservation>>(lower_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<ArrayRef> upper_array =
      observation::write_f64_values(context, upper, shape, signal_unit, index_id);
  if (!ok(upper_array)) {
    return model::fail<Ref<NativeObservation>>(upper_array.code,
                                               "the numerical bounds cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }
  Result<ArrayRef> flag_array = observation::write_u16_values(
      context, flags, shape, model::unit_dimensionless(), index_id);
  if (!ok(flag_array)) {
    return model::fail<Ref<NativeObservation>>(flag_array.code,
                                               "the quality flags cannot be stored", kOrigin,
                                               "the observation cannot be calibrated");
  }

  observation::CalibrationAssemblyInputs assembly;
  assembly.raw = adu.value.has_value() ? std::span<const double>(*adu.value)
                                       : std::span<const double>{};
  assembly.bias = bias.empty() ? std::span<const double>{}
                               : std::span<const double>(bias);
  assembly.flat = flat.empty() ? std::span<const double>{}
                               : std::span<const double>(flat);
  assembly.values = values;
  assembly.flags = flags;
  return observation::assemble_calibrated_native(
      context, raw, request, assembly, val(value_array), val(lower_array), val(upper_array),
      val(flag_array), stats);
}

}  // namespace acsd
