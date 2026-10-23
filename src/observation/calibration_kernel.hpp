#pragma once
// The unique per-sample calibration kernel of observation.calibrate.
//
//   x = (raw - b - beta * (dark_contains_bias ? D - b : D)) / f
//   beta = exposure_seconds / dark_exposure_seconds,   f = flat / flat_normalization
//
// Every entry point of the process (the synchronous public function
// `calibrate_observation`, the block adapter `modules::calibrate_block`, the
// runtime `finish_native` assembly and the validation fixtures) composes these
// functions; the arithmetic expression, the outward interval propagation and
// the data-quality rules exist exactly once, in calibration_kernel.cpp.
//
// The kernel has no I/O, thread pool, arena or Task dependency.  The numeric
// constants are the fixed FP64 outward primitives from src/numerics/interval;
// fast-math and implicit FMA are disabled by the project compile flags.
#include <cstdint>
#include <span>
#include <vector>

#include "acsd/contracts.hpp"
#include "acsd/modules/observation.hpp"

namespace acsd::observation {

// The policy of one calibration request, resolved once per invocation.
struct CalibrationSampleParameters {
  bool has_bias = false;
  bool has_dark = false;
  bool has_flat = false;
  bool dark_contains_bias = false;
  // beta = exposure_seconds / dark_exposure_seconds; exactly 0.0 when there is
  // no dark map, so that the absent map contributes nothing (and never a
  // second random root).
  double exposure_ratio = 0.0;
  double flat_normalization = 1.0;
};

// One raw sample and the matching master samples.  Every value is already
// resolved by the caller; `flat` is the raw flat value before normalisation.
struct CalibrationSampleInputs {
  double raw = 0.0;
  double bias = 0.0;
  double dark = 0.0;
  double flat = 0.0;
  std::uint16_t dq = 0;
  CalibrationSampleParameters parameters;
};

// The calibrated sample.  `value` is the nominal FP64 value, `lower`/`upper`
// are the outward enclosure of the same expression (never a confidence
// interval and never a random-error propagation).  `dq` keeps the input flags
// except for the documented additions.
struct CalibratedSample {
  double value = 0.0;
  double lower = 0.0;
  double upper = 0.0;
  std::uint16_t dq = 0;
  // The flat was non-positive or non-finite and the sample was not repaired.
  bool flat_usable = true;
  // The input carried the repaired bit; a repaired sample still needs a legal
  // G/influence before a consumer may use it.
  bool repaired = false;
  // The nominal value was not finite, so the sample carries NaN and the bad
  // flag (unless a disqualifying flag was already set).
  bool nonfinite_value = false;
};

// The frame-level counters are declared once in the public module header
// (modules::CalibrationSampleStats) and reused by the synchronous entry point,
// the block adapter and the finish assembly.

// Resolves the policy from the public request; the caller performs the
// metadata validation (finite positive exposure, positive dark exposure when a
// dark is used, positive flat normalisation when a flat is used).
CalibrationSampleParameters calibration_parameters(const Calibration& request);

// The single metadata validation of the process.  It reads no payload and is
// called by BOTH the synchronous public entry point (calibrate_observation)
// and the typed hooks (validate_calibration / bounds_calibration /
// plan_calibration), so the same RawObservation/Calibration can never produce
// two different verdicts.  The payload-level NaN/DQ conflict and the flat range
// stay in the CPU stage after the block hash, because validate must not do
// payload I/O.
Result<rt::Done> validate_calibration_input(Ref<RawObservation> raw,
                                            const Calibration& request);

// The unique per-sample calibration and its outward numerical enclosure.
//
// A non-finite raw value whose data-quality flags claim the sample is valid is
// an invalid model and fails the whole operation, exactly like the
// synchronous entry point always did.  A non-finite raw value with a
// disqualifying flag is accepted and carries no usable signal.
Result<CalibratedSample> calibrate_sample(const CalibrationSampleInputs& inputs);

// The shared finish assembly of the calibrated native observation.
//
// It binds already created value/lower/upper/dq arrays, the raw
// context/frame/index/footprints, the identity regrouping and the actual
// NoiseModel/ErrorRecord.  It never touches the pixels again and never writes
// the four signal arrays: callers own the storage of those arrays (the
// synchronous entry point writes them through the science Storage, the
// runtime writes them through the Storage writers).  `values` and `flags` are
// the canonical payloads of the value and dq arrays and are only used for the
// NativeObservation content identity (and, for a master noise model, for the
// flat Jacobian -x/f); both must have the frame element count.
// The caller resolves the per-sample planes once (the synchronous entry point
// from its read vectors, the runtime from the same block storage).  The kernel
// itself never touches storage: `raw`/`bias`/`flat` are only consumed by the
// noise branches that need them and may be empty when those branches are not
// taken (absent map, no explicit Poisson plug-in).
struct CalibrationAssemblyInputs {
  std::span<const double> raw;
  std::span<const double> bias;
  std::span<const double> flat;
  std::span<const double> values;
  std::span<const std::uint16_t> flags;
};

Result<Ref<NativeObservation>> assemble_calibrated_native(
    Context& context, Ref<RawObservation> raw_observation, const Calibration& request,
    const CalibrationAssemblyInputs& inputs, ArrayRef value_array, ArrayRef lower_array,
    ArrayRef upper_array, ArrayRef dq_array, const modules::CalibrationSampleStats& stats);

}  // namespace acsd::observation
