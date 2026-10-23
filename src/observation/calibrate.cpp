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
  // The metadata verdict is not duplicated here: the synchronous entry point
  // and the typed validate/bounds/plan hooks all call the single shared
  // validation of the module, so the same RawObservation/Calibration cannot
  // produce two different conclusions (P1-4).
  const Result<rt::Done> metadata = observation::validate_calibration_input(raw, request);
  if (!ok(metadata)) {
    const std::string reason =
        metadata.assessment.issues.empty()
            ? std::string("the calibration input is invalid")
            : metadata.assessment.issues.front().reason;
    return model::fail<Ref<NativeObservation>>(metadata.code, reason, kOrigin,
                                               "the observation cannot be calibrated");
  }
  const U64 count = raw->width * raw->height;

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
    Result<std::vector<double>> values = observation::read_f64_values(context, request.bias);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the bias map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    bias = val(values);
  }
  if (has_dark) {
    Result<std::vector<double>> values = observation::read_f64_values(context, request.dark);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the dark map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    dark = val(values);
  }
  if (has_flat) {
    Result<std::vector<double>> values = observation::read_f64_values(context, request.flat);
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the flat map cannot be read", kOrigin,
                                                 "the observation cannot be calibrated");
    }
    flat = val(values);
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
