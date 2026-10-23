#pragma once
// Typed, reusable calibration module functions (observation.calibrate).
//
// CalibrateInput is the only typed request of the process.  The block
// functions compose the same per-sample kernel as the synchronous public entry
// point: read_calibration_block -> calibrate_block -> append_signal, sealed by
// seal_signal and finished by finish_native.  The process adapter in
// calibration_process.cpp only sequences these functions; it repeats no pixel
// formula.
//
// The declarations follow docs/engineering/rebuild/standards/
// CALIBRATION_WALKTHROUGH.md and COMPUTE_MODULES.md.  `stream_map_ordered` is
// declared here (rather than in a separate modules/stream.hpp) because the
// first user is this process; new processes may include this header and add an
// explicit instantiation in their own translation unit (the generic definition
// lives in src/observation/stream_map.cpp).
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/storage.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace acsd::modules {

// Counters of one signal block / one full frame that the finish assembly needs
// for the assessment issues.  They are module-side bookkeeping, never a wire
// field of the scientific result.
struct CalibrationSampleStats {
  U64 bad_flat = 0;
  U64 repaired = 0;
};

struct CalibrateInput {
  Ref<RawObservation> raw;
  Calibration request;
};

// One bounded block of the flattened native index.
struct BlockContext {
  rt::InvocationContext invocation;
  rt::ArenaRef arena;
  rt::Key key;
  BlockRequest range;
};

// The four calibrated output arrays of one block.  The block owns the Lease of
// every array until the writer accepted and actually wrote it.
struct SignalBlock {
  rt::Block<double> value, lower, upper;
  rt::Block<std::uint16_t> dq;
  CalibrationSampleStats stats;
};

// One block of the read input: the raw plane, its quality flags and the master
// maps that actually exist.  An absent master is an empty optional; it is
// never materialised as a constant block.
struct CalibrationBlock {
  rt::Block<double> raw;
  rt::Block<std::uint16_t> dq;
  std::optional<rt::Block<double>> bias, dark, flat;
  // The native index of the frame.  rt::read_async carries the range/unit/
  // model hash of the block but not the index object, so the block coordinator
  // passes the source index explicitly; the writers must use the same index as
  // the input arrays.
  Ref<Index> index;
  Calibration policy;
};

// The four private writers of one calibrated frame.  `stats` accumulates the
// per-block counters of the blocks that were actually appended.
struct SignalWriter {
  rt::WriterRef value, lower, upper, dq;
  std::shared_ptr<CalibrationSampleStats> stats;
};

// The four sealed arrays of one calibrated frame, before the NativeObservation
// is bound.  The arrays remain private staging until finish_native delivers
// them.
struct SignalArrays {
  ArrayRef value, lower, upper, dq;
  CalibrationSampleStats stats;
};

// Ordered block map over one stream region: at most `window` cohorts are
// expanded; the sink consumes the lowest required key in order.  The definition
// and the calibration instantiation live in src/observation/stream_map.cpp.
template <class In, class Out>
rt::Task<rt::Done> stream_map_ordered(
    rt::InvocationContext, Ref<rt::InvocationPlan>, U32 region_id,
    std::function<rt::Task<In>(BlockContext)> load,
    std::function<rt::Task<Out>(BlockContext, In)> transform,
    std::function<rt::Task<rt::Done>(BlockContext, Out)> sink);

rt::Task<CalibrationBlock> read_calibration_block(BlockContext,
                                                  Ref<CalibrateInput>);
rt::Task<SignalBlock> calibrate_block(BlockContext, CalibrationBlock);
rt::Task<SignalWriter> begin_signal(rt::InvocationContext,
                                    Ref<rt::InvocationPlan>,
                                    Ref<CalibrateInput>);
rt::Task<rt::Done> append_signal(BlockContext, SignalWriter, SignalBlock);
rt::Task<SignalArrays> seal_signal(rt::InvocationContext, SignalWriter);
rt::Task<rt::Done> abort_signal(rt::InvocationContext, SignalWriter);
rt::Task<Ref<NativeObservation>> finish_native(rt::InvocationContext,
                                               Ref<CalibrateInput>,
                                               SignalArrays);

Result<rt::InvocationPlan> plan_calibration(rt::InvocationContext,
                                            Ref<CalibrateInput>,
                                            rt::PlanOptions);
Result<rt::PlanBounds> bounds_calibration(rt::InvocationContext,
                                          Ref<CalibrateInput>);
Result<rt::Done> validate_calibration(rt::InvocationContext,
                                      Ref<CalibrateInput>);
rt::Task<Ref<NativeObservation>> validate_native(rt::InvocationContext,
                                                 Ref<CalibrateInput>,
                                                 Ref<NativeObservation>);

// The unique typed registration of observation.calibrate.  The generated
// assembly (acsd::generated::register_processes) calls it once; the type codecs
// of CalibrateInput and of the shared NativeObservation are registered by
// register_builtin_types, so this function never registers a TypeId itself.
Result<rt::Done> register_calibration(rt::RegistryBuilder& builder);

// Copies the original code/assessment/usage/dependencies of a failed task and
// clears the value: a cleanup failure never rewrites the scientific failure.
template <class T>
Result<T> failure_like(const Result<rt::Done>& from) {
  Result<T> out;
  out.code = from.code;
  out.assessment = from.assessment;
  out.usage = from.usage;
  out.dependencies = from.dependencies;
  return out;
}

}  // namespace acsd::modules
