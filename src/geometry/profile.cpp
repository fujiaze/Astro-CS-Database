#include "geometry/profile.hpp"

namespace acsd::geometry {

namespace {

// The counters are per thread: the geometry of two frames can run in
// parallel and the accounting must not interleave.
thread_local StageCounts g_kernel;
thread_local StageCounts g_containment;
thread_local StageTimings g_kernel_timings;
thread_local StageTimings g_containment_timings;
thread_local bool g_timing_enabled = false;

}  // namespace

StageCounts& kernel_counts() { return g_kernel; }
StageCounts& containment_counts() { return g_containment; }

StageTimings& kernel_timings() { return g_kernel_timings; }
StageTimings& containment_timings() { return g_containment_timings; }

void reset_stage_counts() {
  g_kernel = StageCounts{};
  g_containment = StageCounts{};
  g_kernel_timings = StageTimings{};
  g_containment_timings = StageTimings{};
}

bool stage_timing_enabled() { return g_timing_enabled; }
void set_stage_timing(bool enabled) { g_timing_enabled = enabled; }

}  // namespace acsd::geometry
