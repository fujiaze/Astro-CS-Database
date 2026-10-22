#pragma once
// Stage accounting of the geometry path.
//
// The overlap kernel and the containment fallback report where their time
// goes: the chart enclosure and its derivatives, the candidate cell
// enumeration, the exact rational cut cell clipping, the boundary
// segmentation and the separation test of the fallback, the work metering and
// the dyadic quantisation of the ledger.
//
// The counters are always on: they are deterministic and they cost nothing.
// The wall clock is optional: the caller switches it on when it measures, so
// the science path pays only a branch per stage.
#include <chrono>
#include <cstdint>

#include "acsd/contracts.hpp"

namespace acsd::geometry {

// The stage counters of one geometry path.
struct StageCounts {
  // The chart enclosure of a detector rectangle and the derivatives of the
  // affine chart model over it.
  U64 chart_models = 0;
  // The candidate cells enumerated over a chart enclosure.
  U64 candidate_cells = 0;
  // The exact rational half space clips of the cut cell kernel.
  U64 rational_clips = 0;
  // The blocks the kernel processed.
  U64 blocks = 0;
  // The boundary segments of the containment fallback.
  U64 boundary_segments = 0;
  // The cells the fallback decided (inside or outside).
  U64 cells_decided = 0;
  // The cells the fallback could not decide (their mass is kept).
  U64 cells_undecided = 0;
  // The inverse solutions of the membership test of the fallback.
  U64 inverse_calls = 0;
  // The charges to the shared work ledger.
  U64 work_charges = 0;
  // The work units charged to the shared work ledger.
  U64 work_units = 0;
  // The leaves published by the path.
  U64 leaves = 0;

  // The sum of the two counters.
  U64 total() const {
    return chart_models + candidate_cells + rational_clips + boundary_segments + inverse_calls;
  }
};

// The stage counters of the two geometry algorithms.
struct GeometryCounts {
  StageCounts kernel;
  StageCounts containment;
};

// The stage timers of one geometry path (nanoseconds).
struct StageTimings {
  // The chart enclosure and the derivatives of the affine chart model.
  U64 chart_model_ns = 0;
  // The candidate cell enumeration.
  U64 candidate_enumeration_ns = 0;
  // The exact rational cut cell clipping and the integration.
  U64 rational_clip_ns = 0;
  // The boundary segmentation of the containment fallback.
  U64 boundary_segmentation_ns = 0;
  // The separation test of the containment fallback.
  U64 separation_test_ns = 0;
  // The membership test of the containment fallback (the inverse solutions).
  U64 membership_test_ns = 0;
  // The work metering.
  U64 work_metering_ns = 0;
  // The dyadic quantisation and the ledger merge.
  U64 dyadic_merge_ns = 0;
};

// The counters of the current thread.  The geometry module writes them, the
// caller reads them after the run.
StageCounts& kernel_counts();
StageCounts& containment_counts();
StageTimings& kernel_timings();
StageTimings& containment_timings();

// The counters of the current thread, reset.
void reset_stage_counts();

// The wall clock of the stages is measured only when it is switched on.
bool stage_timing_enabled();
void set_stage_timing(bool enabled);

// The accumulator of one stage.
class StageTimer {
 public:
  StageTimer(U64& slot) : slot_(slot), start_(std::chrono::steady_clock::now()) {}
  ~StageTimer() {
    if (stage_timing_enabled()) {
      const auto now = std::chrono::steady_clock::now();
      slot_ += static_cast<U64>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count());
    }
  }
  StageTimer(const StageTimer&) = delete;
  StageTimer& operator=(const StageTimer&) = delete;

 private:
  U64& slot_;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace acsd::geometry
