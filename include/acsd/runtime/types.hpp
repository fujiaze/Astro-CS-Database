#pragma once
// Runtime identity, resource, plan and configuration DTOs.
//
// This header is the single owner of these declarations; the field names and
// semantics are fixed by docs/engineering/rebuild/standards/FOUNDATION_API.md
// and the scheduling rules in SCHEDULER.md.  No science algorithm lives here.
#include <acsd/model.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace acsd::rt {
using Done = std::monostate;
using TaskId = U64;
using ScopeId = U64;

// A fixed work-unit key.  Ordering is lexicographic on the tuple: the
// scheduler and every ordered pipe use exactly this order.
struct Key {
  U64 epoch = 0, cohort = 0, stage = 0, item = 0;
};
inline bool operator<(const Key& a, const Key& b) noexcept {
  return std::tie(a.epoch, a.cohort, a.stage, a.item) <
         std::tie(b.epoch, b.cohort, b.stage, b.item);
}
inline bool operator==(const Key& a, const Key& b) noexcept {
  return std::tie(a.epoch, a.cohort, a.stage, a.item) ==
         std::tie(b.epoch, b.cohort, b.stage, b.item);
}
inline bool operator!=(const Key& a, const Key& b) noexcept { return !(a == b); }

struct InvocationIds {
  Id run, command, process, operation, invocation;
  std::optional<Id> parent_invocation;
};

enum class MemoryKind { host, pinned_host, device };

// host_bytes includes pinned_bytes; the two are never added together.
// cpu_tokens and io_slots are execution credits, not capacity.
struct ResourceVector {
  U64 host_bytes = 0, pinned_bytes = 0, device_bytes = 0, spill_bytes = 0;
  U32 cpu_tokens = 0, io_slots = 0;
};
inline ResourceVector operator+(const ResourceVector& a, const ResourceVector& b) {
  return {a.host_bytes + b.host_bytes, a.pinned_bytes + b.pinned_bytes,
          a.device_bytes + b.device_bytes, a.spill_bytes + b.spill_bytes,
          a.cpu_tokens + b.cpu_tokens, a.io_slots + b.io_slots};
}
inline ResourceVector& operator+=(ResourceVector& a, const ResourceVector& b) {
  a = a + b;
  return a;
}
inline ResourceVector operator-(const ResourceVector& a, const ResourceVector& b) {
  return {a.host_bytes - b.host_bytes, a.pinned_bytes - b.pinned_bytes,
          a.device_bytes - b.device_bytes, a.spill_bytes - b.spill_bytes,
          a.cpu_tokens - b.cpu_tokens, a.io_slots - b.io_slots};
}
inline ResourceVector& operator-=(ResourceVector& a, const ResourceVector& b) {
  a = a - b;
  return a;
}
// True when every component of a is at most the matching component of b.
inline bool fits_within(const ResourceVector& a, const ResourceVector& b) {
  return a.host_bytes <= b.host_bytes && a.pinned_bytes <= b.pinned_bytes &&
         a.device_bytes <= b.device_bytes && a.spill_bytes <= b.spill_bytes &&
         a.cpu_tokens <= b.cpu_tokens && a.io_slots <= b.io_slots;
}
inline bool is_zero(const ResourceVector& a) {
  return a.host_bytes == 0 && a.pinned_bytes == 0 && a.device_bytes == 0 &&
         a.spill_bytes == 0 && a.cpu_tokens == 0 && a.io_slots == 0;
}
inline bool is_capacity_only(const ResourceVector& a) {
  return a.cpu_tokens == 0 && a.io_slots == 0;
}

struct ScopeLimits {
  ResourceVector ceiling;
  U64 max_work = 0, metadata_bytes = 0;
};

struct ScopeSnapshot {
  ResourceVector held, claimed, allocated, peak_allocated;
  U64 work = 0, read_bytes = 0, written_bytes = 0, spill_written_bytes = 0;
  U64 cleanup_work = 0;
  U64 elapsed_ns = 0;
  std::optional<U64> cpu_ns;
};

class Services;
class ScopeState;
class CancelState;
class Arena;
class Claim;
using ScopeRef = std::shared_ptr<ScopeState>;
using ArenaRef = std::shared_ptr<Arena>;

struct InvocationContext {
  std::shared_ptr<Services> services;
  ScopeRef scope;
  std::shared_ptr<CancelState> cancel;
  InvocationIds ids;
  std::optional<U32> region;
  ArenaRef persistent, progress;
  // Monotonic dispatch ordinal assigned when the pool picks this execution
  // segment (0 for a context that has not been dispatched).  With several
  // workers the wall-clock entry order is not the scheduling order; this field
  // makes the credit round observable without exposing engine internals.
  U64 dispatch_sequence = 0;
};

enum class TaskState {
  waiting_dependencies, waiting_resources, ready, running, suspended,
  success, failed, cancelled
};

struct TaskSpec {
  Key key;
  ResourceVector running;
  U64 work_upper = 0;
  Id semantic;
};

// ---------------------------------------------------------------------------
// Invocation plan DTOs
// ---------------------------------------------------------------------------

struct StageSpec {
  U32 id = 0;
  std::vector<U32> predecessors;
  ResourceVector scratch, output_upper;
  U64 control_bytes = 0, work_upper = 0;
};

enum class PipeOrder { ordered, unordered };

// The capacity credit of one pipe edge.  max_items/max_queued_bytes of zero do
// not mean "unbounded": make_pipe rejects them as invalid_request.
struct PipeSpec {
  U32 max_items = 0;
  U64 max_queued_bytes = 0;
  PipeOrder order = PipeOrder::ordered;
};

struct EdgeSpec {
  U32 id = 0, from = 0, to = 0;
  U64 max_block_bytes = 0;
  PipeSpec window;
  std::optional<U32> shared_allocation_group;
};

enum class RegionKind { stream, barrier };
enum class RegionTransfer { materialized, ordered_accumulator };

struct RegionSpec {
  U32 id = 0;
  RegionKind kind = RegionKind::stream;
  U64 cohorts = 0;
  ResourceVector persistent, progress_floor;
  std::vector<StageSpec> stages;
  std::vector<EdgeSpec> edges;
};

struct RegionEdge {
  U32 from = 0, to = 0;
  RegionTransfer transfer = RegionTransfer::materialized;
  U64 accumulator_bytes = 0, expected_contributions = 0;
};

struct PlanBounds {
  U64 min_block_elements = 1, max_block_elements = 1;
  U32 max_window = 1;
  bool allow_spill = false;
};

enum class Placement { memory, spill };

struct PlanOptions {
  U64 block_elements = 0;
  U32 window = 1;
  Placement placement = Placement::memory;
};

struct InvocationPlan {
  Id input_hash, request_hash, algorithm_hash;
  U64 epoch = 0;
  PlanOptions options;
  U64 elements = 0, block_elements = 0, cohorts = 0;
  ResourceVector persistent, progress_floor, elastic;
  std::vector<RegionSpec> regions;
  std::vector<RegionEdge> region_edges;
  U64 control_bytes = 0, cleanup_bytes = 0, cleanup_work_upper = 0;
  bool adaptive = false;
};

// ---------------------------------------------------------------------------
// Pool configuration
// ---------------------------------------------------------------------------

// One dispatch decision, recorded before the picked execution segment runs.
// The observer is called with the pool's own pre-dispatch snapshot, so a
// weighted-share test does not have to infer readiness from callable entry.
struct DispatchEvent {
  U64 sequence = 0;
  Id command;
  U32 ready = 0;    // ready items of this command before the pop
  U32 running = 0;  // execution segments of this command still running
  U32 ceiling = 0;  // this command's CPU ceiling (0 means pool width)
  bool control = false;
};
using DispatchObserver = std::function<void(const DispatchEvent&)>;

struct PoolConfig {
  ScopeLimits process;
  U32 cpu_threads = 0, read_slots = 0, write_slots = 0;
  Id scratch_root;
  // Optional diagnostic hook; it must not call Services (it runs inside the
  // dispatcher).  nullptr keeps the production path unchanged.
  DispatchObserver dispatch_observer;
};

}  // namespace acsd::rt
