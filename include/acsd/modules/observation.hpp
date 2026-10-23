#pragma once
// Typed, reusable calibration module functions (observation.calibrate).
//
// CalibrateInput is the only typed request of the process; the block
// functions compose the same kernel as the synchronous public entry point.
#include <acsd/runtime/process.hpp>

#include <optional>

namespace acsd::modules {

struct CalibrateInput {
  Ref<RawObservation> raw;
  Calibration request;
};

}  // namespace acsd::modules
