// ProcessHost: the single typed invocation boundary and lifecycle state machine.
//
// Implements FOUNDATION_API "计划、注册和Host", SYSTEM "统一生命周期" and the
// admission/replan order of SCHEDULER.md.  The public ProcessHost::invoke()
// template routes through detail::host_invoke_erased; every entry point uses the
// same typed binding, resource ledger and Task executor.
#include "internal.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace detail {

// ---------------------------------------------------------------------------
// Host core
// ---------------------------------------------------------------------------

struct HostCore {
  std::shared_ptr<Services> services;
  std::shared_ptr<const Registry> registry_handle;
  std::shared_ptr<const RegistryCore> registry;

  mutable std::mutex mutex;
  std::condition_variable drained;
  bool closed = false;
  U64 invocation_counter = 0;
  U32 active = 0;
  std::vector<std::weak_ptr<CancelState>> cancels;
};

namespace {

// Epoch lineage of one invocation.  Host records the initial plan epoch; a
// module's admit_epoch must then be exactly +1 (SCHEDULER: no re-running a
// terminal (region, epoch), shared work never resets).
std::mutex& epoch_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::map<Id, U64>& epoch_map() {
  static std::map<Id, U64> epochs;
  return epochs;
}

void record_epoch(const Id& invocation, U64 epoch) {
  if (invocation.empty()) return;
  std::lock_guard<std::mutex> lock(epoch_mutex());
  epoch_map()[invocation] = epoch;
}

void forget_epoch(const Id& invocation) {
  if (invocation.empty()) return;
  std::lock_guard<std::mutex> lock(epoch_mutex());
  epoch_map().erase(invocation);
}

std::optional<U64> last_epoch(const Id& invocation) {
  std::lock_guard<std::mutex> lock(epoch_mutex());
  auto found = epoch_map().find(invocation);
  if (found == epoch_map().end()) return std::nullopt;
  return found->second;
}

void set_epoch(const Id& invocation, U64 epoch) {
  std::lock_guard<std::mutex> lock(epoch_mutex());
  epoch_map()[invocation] = epoch;
}

template <class T>
Result<T> failure(Code code, std::string origin, std::string reason,
                  std::string impact = {}) {
  Result<T> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  Issue issue;
  issue.code = code;
  issue.origin = std::move(origin);
  issue.reason = std::move(reason);
  issue.impact = std::move(impact);
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

template <class T, class From>
Result<T> remap(const Result<From>& from) {
  Result<T> out;
  out.code = from.code;
  out.assessment = from.assessment;
  out.usage = from.usage;
  out.dependencies = from.dependencies;
  return out;
}

// Completes an output node with a payload even for an API-allowed non-ok
// partial.  rt::ready only boxes code==ok results; the partial value must stay
// observable for settle/finalize and for the wire encoder.
template <class T>
Task<T> complete_task(InvocationContext ctx, Result<T> result) {
  auto node = detail::make_node(ctx, TaskSpec{});
  std::shared_ptr<void> box;
  if (result.value) box = detail::box_value<T>(std::move(*result.value));
  detail::node_complete(node, result.code, std::move(result.assessment),
                        std::move(result.usage),
                        std::move(result.dependencies), std::move(box));
  return wrap_task<T>(std::move(node));
}

// Copies a completed node into another while keeping a value that belongs to an
// API-allowed non-ok partial.  node_complete_from() (used by then/settle) only
// moves the value for code==ok, which would drop the partial payload before the
// Host finalizer or the wire encoder can observe it.
void copy_node_preserving(const std::shared_ptr<detail::TaskNode>& destination,
                          const std::shared_ptr<detail::TaskNode>& source) {
  Code code = detail::node_code(source);
  Assessment assessment = detail::node_assessment(source);
  Usage usage = detail::node_usage(source);
  std::vector<Id> dependencies = detail::node_dependencies(source);
  std::shared_ptr<void> value = detail::node_take_value(source);
  if (code == Code::ok && !value) code = Code::internal;
  detail::node_complete(destination, code, std::move(assessment),
                        std::move(usage), std::move(dependencies),
                        std::move(value));
}

// settle() that preserves a value on a non-ok partial (API-allowed partials do
// carry a valid domain).  The executor's take_result() only exposes value for
// code==ok, which is fine for then() but not for the Host's finalize hook and
// the wire encoder.
template <class T, class F>
auto settle_preserving(InvocationContext ctx, Task<T>&& source, F f)
    -> std::invoke_result_t<F, Result<T>> {
  using Child = std::invoke_result_t<F, Result<T>>;
  using U = typename Child::value_type;
  static_assert(std::is_same_v<Child, Task<U>>,
                "settle_preserving: the continuation must return Task<U>");
  auto out = detail::make_node(ctx, TaskSpec{});
  auto src = detail::TaskAccess::release<T>(std::move(source));
  if (!src) {
    detail::node_complete(out, Code::invalid_request, Assessment{}, Usage{}, {},
                          nullptr);
    return wrap_task<U>(std::move(out));
  }
  detail::node_set_continuation(
      src, [ctx, src, out, f = std::move(f)]() mutable {
        try {
          Result<T> result;
          result.code = detail::node_code(src);
          result.assessment = detail::node_assessment(src);
          result.usage = detail::node_usage(src);
          result.dependencies = detail::node_dependencies(src);
          if (auto box = detail::node_take_value(src))
            result.value = std::move(*static_cast<T*>(box.get()));
          Child child = std::invoke(std::move(f), std::move(result));
          auto child_node = detail::TaskAccess::release<U>(std::move(child));
          if (!child_node) {
            detail::node_complete(out, Code::invalid_request, Assessment{},
                                  Usage{}, {}, nullptr);
            return;
          }
          detail::node_set_continuation(child_node, [child_node, out]() {
            copy_node_preserving(out, child_node);
          });
        } catch (...) {
          detail::node_complete_exception(out);
        }
      });
  return wrap_task<U>(std::move(out));
}

Usage usage_from_snapshot(const ScopeSnapshot& snapshot) {
  Usage usage{};
  usage.work = snapshot.work;
  usage.peak_payload_bytes = snapshot.peak_allocated.host_bytes;
  usage.read_bytes = snapshot.read_bytes;
  usage.written_bytes = snapshot.written_bytes;
  usage.spill_bytes = snapshot.spill_written_bytes;
  usage.elapsed_seconds =
      static_cast<double>(snapshot.elapsed_ns) / 1'000'000'000.0;
  return usage;
}

bool allows_finalize(Code code) {
  return code == Code::ok || code == Code::nonconverged ||
         code == Code::precision_exhausted || code == Code::resource_exhausted;
}

// Narrows the parent scope's absolute limits to what is still free.  A child
// scope never receives capacity it did not inherit; nothing is invented.
ScopeLimits remaining_limits(const ScopeRef& parent) {
  ScopeLimits limits{};
  if (!parent) return limits;
  const ScopeSnapshot snapshot = rt::snapshot(parent);
  const ScopeLimits& absolute = parent->limits;
  U64 metadata_used = 0;
  {
    std::lock_guard<std::mutex> lock(parent->mutex);
    metadata_used = parent->metadata_bytes;
  }
  auto subtract = [](U64 total, U64 used) { return total > used ? total - used : 0; };
  limits.ceiling.host_bytes =
      subtract(absolute.ceiling.host_bytes, snapshot.held.host_bytes);
  limits.ceiling.pinned_bytes =
      subtract(absolute.ceiling.pinned_bytes, snapshot.held.pinned_bytes);
  limits.ceiling.device_bytes =
      subtract(absolute.ceiling.device_bytes, snapshot.held.device_bytes);
  limits.ceiling.spill_bytes =
      subtract(absolute.ceiling.spill_bytes, snapshot.held.spill_bytes);
  limits.ceiling.cpu_tokens = absolute.ceiling.cpu_tokens;
  limits.ceiling.io_slots = absolute.ceiling.io_slots;
  limits.max_work = subtract(absolute.max_work, snapshot.work);
  limits.metadata_bytes = subtract(absolute.metadata_bytes, metadata_used);
  return limits;
}

struct InvocationState {
  std::shared_ptr<HostCore> host;
  InvocationContext ctx;
  Id invocation;
  const ErasedOperation* operation = nullptr;
  std::shared_ptr<const void> input;
  Ref<InvocationPlan> plan;
  ArenaRef persistent, progress;
  ScopeRef scope;
  std::function<void(Id, const ScopeSnapshot&, Id, Id)> observer;
  U64 root_span = 0;
};

void record_phase(const std::shared_ptr<InvocationState>& state,
                  std::string_view phase, U64 start_ns) {
  if (!state->ctx.services) return;
  PerfRecord record;
  record.probe_id = "framework.invocation.phase";
  record.tags.component = phase;
  record.kind = PerfKind::span;
  record.start_steady_ns = start_ns;
  const U64 now = steady_ns();
  record.wall_ns = now > start_ns ? now - start_ns : 0;
  record.span_id = allocate_span_id(state->ctx);
  record.parent_span_id = state->root_span;
  const ScopeSnapshot snapshot = rt::snapshot(state->scope);
  record.bytes = snapshot.peak_allocated.host_bytes;
  state->ctx.services->record_probe(state->ctx, record);
}

void record_usage(const std::shared_ptr<InvocationState>& state,
                  const ScopeSnapshot& snapshot) {
  if (!state->ctx.services) return;
  PerfRecord record;
  record.probe_id = "framework.invocation.usage";
  record.kind = PerfKind::counter;
  record.start_steady_ns = steady_ns();
  record.wall_ns = 0;
  record.count = snapshot.work;
  record.bytes = snapshot.peak_allocated.host_bytes;
  record.span_id = state->root_span;
  state->ctx.services->record_probe(state->ctx, record);
}

void release_invocation(const std::shared_ptr<InvocationState>& state) {
  // Host owns the invocation arenas and must close them (FOUNDATION_API:
  // "Host/区域所有者必须在结束时 close").  Live output Leases keep their
  // committed capacity until the last Lease dies.
  if (state->persistent) state->persistent->close();
  if (state->progress) state->progress->close();
  if (state->scope) detail::close_scope(state->scope);
}

Task<std::shared_ptr<const void>> finish_invocation(
    const std::shared_ptr<InvocationState>& state,
    Result<std::shared_ptr<const void>> result) {
  const ScopeSnapshot snapshot = rt::snapshot(state->scope);
  record_usage(state, snapshot);
  if (state->observer) {
    Id input_hash;
    Id request_hash;
    if (state->plan) {
      input_hash = state->plan->input_hash;
      request_hash = state->plan->request_hash;
    }
    state->observer(state->invocation, snapshot, std::move(input_hash),
                    std::move(request_hash));
  }
  result.usage = usage_from_snapshot(snapshot);
  release_invocation(state);
  forget_epoch(state->invocation);
  if (state->host) {
    {
      std::lock_guard<std::mutex> lock(state->host->mutex);
      if (state->host->active > 0) --state->host->active;
      state->host->cancels.erase(
          std::remove_if(state->host->cancels.begin(),
                         state->host->cancels.end(),
                         [](const std::weak_ptr<CancelState>& weak) {
                           return weak.expired();
                         }),
          state->host->cancels.end());
    }
    state->host->drained.notify_all();
  }
  return complete_task<std::shared_ptr<const void>>(state->ctx,
                                                    std::move(result));
}

using ErasedValue = std::shared_ptr<const void>;

Task<ErasedValue> fail_invocation(const std::shared_ptr<InvocationState>& state,
                                  Code code, std::string origin,
                                  std::string reason) {
  return finish_invocation(
      state, failure<ErasedValue>(code, std::move(origin), std::move(reason)));
}

template <class From>
Task<ErasedValue> finish_from(const std::shared_ptr<InvocationState>& state,
                              const Result<From>& from) {
  return finish_invocation(state, remap<ErasedValue>(from));
}

}  // namespace

// ---------------------------------------------------------------------------
// The erased invocation entry point
// ---------------------------------------------------------------------------

Task<std::shared_ptr<const void>> host_invoke_erased_observed(
    std::shared_ptr<HostCore> host, InvocationContext ctx, Id process,
    Id operation, std::shared_ptr<const void> input, const void* input_cookie,
    const void* output_cookie,
    std::function<void(Id, const ScopeSnapshot&, Id, Id)> observer) {
  constexpr const char* kOrigin = "host.invoke";
  if (!host)
    return complete_task<ErasedValue>(
        ctx, failure<ErasedValue>(Code::invalid_request, kOrigin,
                                  "ProcessHost has no host core"));
  auto registry = host->registry;
  if (!registry)
    return complete_task<ErasedValue>(
        ctx, failure<ErasedValue>(Code::unsupported, kOrigin,
                                  "no frozen registry is attached to this host"));

  auto found = registry->operations.find({process, operation});
  if (found == registry->operations.end())
    return complete_task<ErasedValue>(ctx,
                                      failure<ErasedValue>(
                                          Code::unsupported, kOrigin,
                                          "unknown process '" + process +
                                              "' operation '" + operation +
                                              "'; the frozen registry has no "
                                              "typed binding"));
  const ErasedOperation* binding = &found->second.erased;
  if (binding->input_cookie != input_cookie ||
      binding->output_cookie != output_cookie)
    return complete_task<ErasedValue>(
        ctx, failure<ErasedValue>(
                 Code::invalid_request, kOrigin,
                 "typed binding cookie mismatch for process '" + process +
                     "' operation '" + operation +
                     "': the in-process call does not match the registered "
                     "TypeId/C++ type pair"));

  U64 sequence = 0;
  {
    std::lock_guard<std::mutex> lock(host->mutex);
    if (host->closed)
      return complete_task<ErasedValue>(
          ctx, failure<ErasedValue>(Code::unsupported, kOrigin,
                                    "the ProcessHost is closed; new "
                                    "invocations are unsupported"));
    sequence = ++host->invocation_counter;
  }

  auto state = std::make_shared<InvocationState>();
  state->host = host;
  state->operation = binding;
  state->input = std::move(input);
  state->observer = std::move(observer);
  const Id command = ctx.ids.command.empty() ? "command" : ctx.ids.command;
  state->invocation = "invocation:" + command + ":" + std::to_string(sequence);

  {
    std::lock_guard<std::mutex> lock(host->mutex);
    if (host->closed) {
      return complete_task<ErasedValue>(
          ctx, failure<ErasedValue>(Code::unsupported, kOrigin,
                                    "the ProcessHost was closed while the "
                                    "invocation was being admitted"));
    }
    ++host->active;
    if (ctx.cancel) host->cancels.push_back(ctx.cancel);
  }

  try {
    if (!ctx.services || !ctx.scope) {
      return fail_invocation(state, Code::invalid_request, kOrigin,
                             "the InvocationContext was not opened by "
                             "make_services/open_command");
    }
    Result<ScopeRef> child =
        rt::child_scope(ctx, state->invocation, remaining_limits(ctx.scope));
    if (child.code != Code::ok || !child.value)
      return fail_invocation(state, child.code == Code::ok ? Code::internal
                                                           : child.code,
                             kOrigin,
                             "cannot create the invocation child scope within "
                             "the process scope limits");
    state->scope = *child.value;
    state->ctx = ctx;
    state->ctx.scope = state->scope;
    state->ctx.ids.run = ctx.ids.run;
    state->ctx.ids.command = ctx.ids.command;
    state->ctx.ids.process = process;
    state->ctx.ids.operation = operation;
    state->ctx.ids.invocation = state->invocation;
    state->ctx.ids.parent_invocation = ctx.ids.command;
    state->ctx.region.reset();
    state->ctx.persistent.reset();
    state->ctx.progress.reset();
    if (!state->ctx.cancel && state->scope->cancel)
      state->ctx.cancel = state->scope->cancel;
    state->root_span = allocate_span_id(state->ctx);
    record_epoch(state->invocation, 0);

    // -- resolve is complete; the binding is immutable --------------------
    // validate: read-only metadata and module science checks, no run.
    U64 phase_start = steady_ns();
    Result<Done> validated = state->operation->validate(state->ctx, state->input);
    record_phase(state, "validate", phase_start);
    if (validated.code != Code::ok) return finish_from(state, validated);

    // bounds: legal block/window/spill range, no reservation yet.
    phase_start = steady_ns();
    Result<PlanBounds> bounds =
        state->operation->bounds(state->ctx, state->input);
    record_phase(state, "bounds", phase_start);
    if (bounds.code != Code::ok || !bounds.value)
      return finish_from(state, bounds);
    const PlanBounds allowed = *bounds.value;
    if (allowed.min_block_elements == 0 ||
        allowed.max_block_elements < allowed.min_block_elements ||
        allowed.max_window == 0)
      return fail_invocation(state, Code::invalid_model, kOrigin,
                             "plan bounds are not a usable range: min=" +
                                 std::to_string(allowed.min_block_elements) +
                                 " max=" +
                                 std::to_string(allowed.max_block_elements) +
                                 " window=" +
                                 std::to_string(allowed.max_window));

    // plan/admit loop.  The order is fixed by SCHEDULER: initial
    // max_block_elements, window=1, memory; shrink block_elements by halving
    // down to the declared minimum, then switch placement to spill.  Resource
    // vectors are never scaled by a ratio.
    PlanOptions options;
    options.block_elements = allowed.max_block_elements;
    options.window = 1;
    options.placement = Placement::memory;
    bool admitted = false;
    for (int attempt = 0; attempt < 256 && !admitted; ++attempt) {
      phase_start = steady_ns();
      Result<InvocationPlan> planned =
          state->operation->plan(state->ctx, state->input, options);
      record_phase(state, "plan", phase_start);
      if (planned.code != Code::ok || !planned.value)
        return finish_from(state, planned);
      const InvocationPlan& plan = *planned.value;
      if (!is_capacity_only(plan.persistent) ||
          !is_capacity_only(plan.progress_floor))
        return fail_invocation(
            state, Code::invalid_model, kOrigin,
            "the plan puts CPU/I/O execution credits into the long-lived "
            "persistent/progress_floor vectors; those are per-execution-segment "
            "credits");

      ArenaRef persistent;
      ArenaRef progress;
      if (!is_zero(plan.persistent)) {
        Result<ArenaRef> reserved = rt::reserve(state->ctx, plan.persistent);
        if (reserved.code != Code::ok || !reserved.value) {
          // Replan below; the ledger has no side effect on failure.
        } else {
          persistent = *reserved.value;
        }
      }
      if (persistent || is_zero(plan.persistent)) {
        if (!is_zero(plan.progress_floor)) {
          Result<ArenaRef> reserved =
              rt::reserve(state->ctx, plan.progress_floor);
          if (reserved.code != Code::ok || !reserved.value) {
            if (persistent) persistent->close();
            persistent.reset();
          } else {
            progress = *reserved.value;
          }
        }
      }
      if (persistent || is_zero(plan.persistent)) {
        if (progress || is_zero(plan.progress_floor)) {
          admitted = true;
          state->persistent = persistent;
          state->progress = progress;
          state->plan =
              std::make_shared<InvocationPlan>(std::move(*planned.value));
          state->ctx.persistent = persistent;
          state->ctx.progress = progress;
          record_epoch(state->invocation, state->plan->epoch);
          break;
        }
      }

      // The admission attempt failed: the same plan function is called again
      // with explicit new PlanOptions, never with a scaled ResourceVector.
      if (options.block_elements > allowed.min_block_elements) {
        const U64 halved = options.block_elements / 2;
        options.block_elements =
            std::max(allowed.min_block_elements, halved);
      } else if (options.placement == Placement::memory &&
                 allowed.allow_spill) {
        options.placement = Placement::spill;
      } else {
        return fail_invocation(
            state, Code::resource_exhausted, kOrigin,
            "R+F cannot be admitted at the declared minimum block size and "
            "placement; the plan must reduce its working set or the request "
            "must fail before run");
      }
    }
    if (!admitted)
      return fail_invocation(state, Code::resource_exhausted, kOrigin,
                             "no admissible plan was found within the "
                             "declared bound range");
  } catch (...) {
    return fail_invocation(state, Code::internal, kOrigin,
                           "an exception escaped validate/bounds/plan/admit; "
                           "the invocation is failed and cleaned up");
  }

  // -- run ------------------------------------------------------------------
  U64 run_start = steady_ns();
  Task<ErasedValue> run_task;
  try {
    run_task = state->operation->run(state->ctx, state->input, state->plan);
  } catch (...) {
    return fail_invocation(state, Code::internal, kOrigin,
                           "an exception escaped run before a Task was "
                           "produced; the invocation is failed and cleaned up");
  }
  if (!run_task.valid())
    return fail_invocation(state, Code::internal, kOrigin,
                           "run returned an invalid Task handle");

  try {
    return settle_preserving(
        state->ctx, std::move(run_task),
        [state, run_start](Result<ErasedValue> run_result) -> Task<ErasedValue> {
          record_phase(state, "run", run_start);
          if (!allows_finalize(run_result.code)) {
            // Hard failure or cancellation: finalize is not called and no
            // publishable value is delivered.
            run_result.value.reset();
            return finish_invocation(state, std::move(run_result));
          }
          Result<ErasedValue> original = run_result;
          const U64 finalize_start = steady_ns();
          Task<ErasedValue> finalize_task;
          try {
            finalize_task = state->operation->finalize(
                state->ctx, state->input, state->plan, std::move(run_result));
          } catch (...) {
            return finish_invocation(
                state, failure<ErasedValue>(
                           Code::internal, "host.finalize",
                           "an exception escaped the finalize hook; the sealed "
                           "value is not published"));
          }
          if (!finalize_task.valid())
            return finish_invocation(
                state, failure<ErasedValue>(
                           Code::internal, "host.finalize",
                           "the finalize hook returned an invalid Task handle"));
          try {
            return settle_preserving(
                state->ctx, std::move(finalize_task),
                [state, original, finalize_start](
                    Result<ErasedValue> finalized) mutable -> Task<ErasedValue> {
                  record_phase(state, "finalize", finalize_start);
                  const bool allowed_partial =
                      finalized.code == Code::nonconverged ||
                      finalized.code == Code::precision_exhausted ||
                      finalized.code == Code::resource_exhausted;
                  if (finalized.code != Code::ok && !allowed_partial) {
                    // Sealing failed (io_error/internal/...): lower the outcome
                    // and never publish a value, but keep the original issues
                    // for diagnostics.  An API-allowed science partial is NOT a
                    // sealing failure; its value must survive finalize.
                    Result<ErasedValue> lowered = finalized;
                    lowered.value.reset();
                    lowered.assessment.issues.insert(
                        lowered.assessment.issues.end(),
                        original.assessment.issues.begin(),
                        original.assessment.issues.end());
                    lowered.dependencies = original.dependencies;
                    return finish_invocation(state, std::move(lowered));
                  }
                  Result<ErasedValue> merged = original;
                  // finalize may only keep or lower the usable domain, never
                  // upgrade a non-ok science code to ok.  If the run succeeded
                  // and finalize itself reports an allowed partial, the
                  // published code is lowered to that partial.
                  if (original.code == Code::ok && finalized.code != Code::ok)
                    merged.code = finalized.code;
                  merged.assessment.issues.insert(
                      merged.assessment.issues.end(),
                      finalized.assessment.issues.begin(),
                      finalized.assessment.issues.end());
                  if (!finalized.assessment.complete)
                    merged.assessment.complete = false;
                  if (finalized.value) {
                    merged.value = finalized.value;
                  } else if (original.value) {
                    // finalize sealed no new value but an API-allowed partial
                    // from run is still the only valid output.
                    merged.value = original.value;
                  } else if (merged.code == Code::ok) {
                    merged.code = Code::internal;
                    merged.value.reset();
                  }
                  return finish_invocation(state, std::move(merged));
                });
          } catch (...) {
            return finish_invocation(
                state, failure<ErasedValue>(
                           Code::internal, "host.finalize",
                           "an exception escaped the finalize continuation"));
          }
        });
  } catch (...) {
    return finish_invocation(
        state, failure<ErasedValue>(Code::internal, kOrigin,
                                    "an exception escaped the run "
                                    "continuation; the invocation is failed"));
  }
}

Task<std::shared_ptr<const void>> host_invoke_erased(
    std::shared_ptr<HostCore> host, InvocationContext ctx, Id process,
    Id operation, std::shared_ptr<const void> input, const void* input_cookie,
    const void* output_cookie) {
  return host_invoke_erased_observed(
      std::move(host), ctx, std::move(process), std::move(operation),
      std::move(input), input_cookie, output_cookie, {});
}

}  // namespace detail

// ---------------------------------------------------------------------------
// ProcessHost public entry points
// ---------------------------------------------------------------------------

ProcessHost::~ProcessHost() = default;

Task<Done> ProcessHost::close_async() {
  InvocationContext ctx;
  Result<Done> result;
  result.code = Code::ok;
  result.value = Done{};
  if (!core_)
    return ready<Done>(ctx, std::move(result));
  if (core_->services) {
    ctx.services = core_->services;
    ctx.scope = core_->services->root;
    if (ctx.scope) ctx.cancel = ctx.scope->cancel;
  }

  {
    std::lock_guard<std::mutex> lock(core_->mutex);
    core_->closed = true;
    for (auto& weak : core_->cancels)
      if (auto cancel = weak.lock()) cancel->request();
  }
  {
    std::unique_lock<std::mutex> lock(core_->mutex);
    core_->drained.wait(lock, [this] { return core_->active == 0; });
  }
  // The pool is owned by Services; the host closes its writers/arenas above and
  // then stops the unique pool once no invocation remains.  Immutable outputs
  // keep their allocator/ledger/Storage dependencies alive through their Lease.
  if (core_->services) core_->services->shutdown();
  return ready<Done>(ctx, std::move(result));
}

namespace {

WireResponse wire_response_for(const Ref<WireRequest>& request) {
  WireResponse response;
  response.assessment = Assessment{};
  if (request) {
    response.request_id = request->request_id;
    response.process = request->process;
    response.operation = request->operation;
    response.output_type = request->input_type;
  }
  return response;
}

Task<WireResponse> wire_failure(InvocationContext ctx, WireResponse response,
                                Code code, std::string origin,
                                std::string reason) {
  response.result_code = code;
  response.assessment.complete = false;
  response.assessment.validity = Validity::invalid;
  Issue issue;
  issue.code = code;
  issue.origin = std::move(origin);
  issue.reason = std::move(reason);
  response.assessment.issues.push_back(std::move(issue));
  Result<WireResponse> out;
  out.code = Code::ok;  // protocol handling completed; the science code is above
  out.value = std::move(response);
  return ready<WireResponse>(ctx, std::move(out));
}

}  // namespace

Task<WireResponse> ProcessHost::invoke_wire(InvocationContext ctx,
                                            Ref<WireRequest> request) {
  WireResponse response = wire_response_for(request);
  constexpr const char* kOrigin = "host.invoke_wire";
  if (!core_)
    return wire_failure(ctx, std::move(response), Code::invalid_request, kOrigin,
                        "ProcessHost has no host core");
  if (!request)
    return wire_failure(ctx, std::move(response), Code::invalid_request, kOrigin,
                        "invoke_wire received a null WireRequest");
  {
    std::lock_guard<std::mutex> lock(core_->mutex);
    if (core_->closed)
      return wire_failure(ctx, std::move(response), Code::unsupported, kOrigin,
                          "the ProcessHost is closed; new wire requests are "
                          "unsupported");
  }
  if (!core_->registry || !core_->registry_handle)
    return wire_failure(ctx, std::move(response), Code::unsupported, kOrigin,
                        "no frozen registry is attached to this host");
  auto found = core_->registry->operations.find(
      {request->process, request->operation});
  if (found == core_->registry->operations.end())
    return wire_failure(ctx, std::move(response), Code::unsupported, kOrigin,
                        "unknown process '" + request->process +
                            "' operation '" + request->operation + "'");
  const detail::ErasedOperation& binding = found->second.erased;
  if (request->input_type.schema != binding.input_type.schema ||
      request->input_type.revision != binding.input_type.revision)
    return wire_failure(
        ctx, std::move(response), Code::invalid_request, kOrigin,
        "request input TypeId '" + request->input_type.schema + "'/revision " +
            std::to_string(request->input_type.revision) +
            " does not match the registered typed binding '" +
            binding.input_type.schema + "'/revision " +
            std::to_string(binding.input_type.revision));
  response.output_type = binding.output_type;
  auto codec = core_->registry->codecs.find(
      {request->input_type.schema, request->input_type.revision});
  if (codec == core_->registry->codecs.end())
    return wire_failure(ctx, std::move(response), Code::unsupported, kOrigin,
                        "no codec is registered for the request input TypeId");

  Result<std::shared_ptr<WireObjects>> objects =
      make_wire_objects(ctx, core_->registry_handle, request);
  if (objects.code != Code::ok || !objects.value) {
    const Code code =
        objects.code == Code::ok ? Code::internal : objects.code;
    std::string reason = "make_wire_objects rejected the request object table";
    if (!objects.assessment.issues.empty())
      reason = objects.assessment.issues.front().reason;
    return wire_failure(ctx, std::move(response), code, kOrigin,
                        std::move(reason));
  }
  auto table = *objects.value;

  // The erased codec receives exactly one JSON value: input_json.  Object
  // references are resolved through the request-local WireObjects table
  // (resolve/bind); input_objects_json is never concatenated into the value
  // text and never becomes a second encoded document.
  Result<detail::ErasedValue> decoded;
  try {
    decoded = codec->second.decode(ctx, request->input_json, *table);
  } catch (...) {
    return wire_failure(ctx, std::move(response), Code::internal, kOrigin,
                        "an exception escaped the input codec decode");
  }
  if (decoded.code != Code::ok || !decoded.value) {
    const Code code = decoded.code == Code::ok ? Code::invalid_model
                                                : decoded.code;
    return wire_failure(ctx, std::move(response), code, kOrigin,
                        "the input codec did not produce a typed input");
  }

  struct WireMeta {
    Id invocation;
    Id input_hash;
    Id request_hash;
    ScopeSnapshot resources;
  };
  auto meta = std::make_shared<WireMeta>();
  Task<detail::ErasedValue> invoked;
  try {
    invoked = detail::host_invoke_erased_observed(
        core_, ctx, request->process, request->operation, *decoded.value,
        binding.input_cookie, binding.output_cookie,
        [meta](Id invocation, const ScopeSnapshot& snapshot, Id input_hash,
               Id request_hash) {
          meta->invocation = std::move(invocation);
          meta->resources = snapshot;
          meta->input_hash = std::move(input_hash);
          meta->request_hash = std::move(request_hash);
        });
  } catch (...) {
    return wire_failure(ctx, std::move(response), Code::internal, kOrigin,
                        "an exception escaped the Host invocation boundary");
  }

  return detail::settle_preserving(
      ctx, std::move(invoked),
      [ctx, table, codec, meta, response, request](
          Result<detail::ErasedValue> science) mutable -> Task<WireResponse> {
        WireResponse out = response;
        out.invocation = meta->invocation;
        out.resources = meta->resources;
        out.backend = "cpu";
        out.input_hash = meta->input_hash;
        if (out.input_hash.empty()) {
          // The plan did not declare a resolved input identity: derive a stable
          // transport identity from the decoded request value.
          U64 hash = 1469598103934665603ull;
          for (unsigned char byte : request->input_json) {
            hash ^= byte;
            hash *= 1099511628211ull;
          }
          static const char hex[] = "0123456789abcdef";
          std::string text = "wire.input.fnv1a64:";
          for (int shift = 60; shift >= 0; shift -= 4)
            text.push_back(hex[(hash >> shift) & 0x0f]);
          out.input_hash = std::move(text);
        }
        out.request_hash = meta->request_hash;
        out.result_code = science.code;
        out.assessment = science.assessment;
        out.dependencies = science.dependencies;
        if (science.value) {
          Result<std::string> encoded;
          try {
            encoded = codec->second.encode(ctx, *science.value, *table);
          } catch (...) {
            encoded = detail::failure<std::string>(
                Code::internal, kOrigin,
                "an exception escaped the output codec encode");
          }
          if (encoded.code == Code::ok && encoded.value) {
            out.value_json = std::move(*encoded.value);
          } else {
            out.result_code = encoded.code == Code::ok ? Code::internal
                                                        : encoded.code;
            out.value_json.reset();
            if (!encoded.assessment.issues.empty())
              out.assessment.issues.push_back(
                  encoded.assessment.issues.front());
          }
        } else if (science.code == Code::ok) {
          out.result_code = Code::internal;
          Issue issue;
          issue.code = Code::internal;
          issue.origin = kOrigin;
          issue.reason =
              "the invocation reported ok without a typed output value";
          out.assessment.issues.push_back(std::move(issue));
          out.assessment.complete = false;
        }
        // Object export is owned by the wire envelope; the local object table
        // has no public serializer yet, so the response carries an empty table
        // instead of inventing aliases.  artifact_lease is process-lifetime only
        // and is never serialized.
        out.objects_json = "{}";
        Result<WireResponse> done;
        done.code = Code::ok;  // protocol handling completed
        done.value = std::move(out);
        return ready<WireResponse>(ctx, std::move(done));
      });
}

Result<std::shared_ptr<ProcessHost>> make_host(
    std::shared_ptr<Services> services, std::shared_ptr<const Registry> registry) {
  Result<std::shared_ptr<ProcessHost>> result;
  if (!services || !registry || !registry->core_) {
    result.code = Code::invalid_request;
    result.assessment.complete = false;
    result.assessment.validity = Validity::invalid;
    Issue issue;
    issue.code = Code::invalid_request;
    issue.origin = "make_host";
    issue.reason =
        "make_host needs a live Services and a frozen Registry; modules must "
        "freeze the registry before creating the host";
    result.assessment.issues.push_back(std::move(issue));
    return result;
  }
  auto core = std::make_shared<detail::HostCore>();
  core->services = std::move(services);
  core->registry_handle = registry;
  core->registry = registry->core_;
  std::shared_ptr<ProcessHost> host(new ProcessHost());
  host->core_ = std::move(core);
  result.code = Code::ok;
  result.value = std::move(host);
  return result;
}

// ---------------------------------------------------------------------------
// Epoch admission (scientific iteration)
// ---------------------------------------------------------------------------

Task<Done> admit_epoch(InvocationContext ctx, Ref<InvocationPlan> plan) {
  Result<Done> result;
  if (!plan) {
    result = detail::failure<Done>(Code::invalid_request, "admit_epoch",
                                   "a null InvocationPlan cannot be admitted");
    return ready<Done>(ctx, std::move(result));
  }
  if (!ctx.scope) {
    result = detail::failure<Done>(
        Code::invalid_request, "admit_epoch",
        "the InvocationContext has no scope; admit_epoch needs the Host "
        "invocation scope");
    return ready<Done>(ctx, std::move(result));
  }
  const auto previous = detail::last_epoch(ctx.ids.invocation);
  if (!previous) {
    result = detail::failure<Done>(
        Code::invalid_request, "admit_epoch",
        "the invocation has no admitted plan lineage; the first plan is run by "
        "the Host, admit_epoch only admits a strictly increasing next epoch");
    return ready<Done>(ctx, std::move(result));
  }
  if (plan->epoch != *previous + 1) {
    result = detail::failure<Done>(
        Code::identity_conflict, "admit_epoch",
        "epoch must be strictly previous+1: previous=" +
            std::to_string(*previous) + " proposed=" +
            std::to_string(plan->epoch));
    return ready<Done>(ctx, std::move(result));
  }
  if (!is_capacity_only(plan->persistent) ||
      !is_capacity_only(plan->progress_floor)) {
    result = detail::failure<Done>(
        Code::invalid_model, "admit_epoch",
        "the next epoch puts CPU/I/O execution credits into the persistent/"
        "progress_floor vectors");
    return ready<Done>(ctx, std::move(result));
  }
  const ScopeSnapshot current = snapshot(ctx.scope);
  const ResourceVector required = plan->persistent + plan->progress_floor;
  const ScopeLimits& limits = ctx.scope->limits;
  const ResourceVector headroom{
      limits.ceiling.host_bytes > current.held.host_bytes
          ? limits.ceiling.host_bytes - current.held.host_bytes
          : 0,
      limits.ceiling.pinned_bytes > current.held.pinned_bytes
          ? limits.ceiling.pinned_bytes - current.held.pinned_bytes
          : 0,
      limits.ceiling.device_bytes > current.held.device_bytes
          ? limits.ceiling.device_bytes - current.held.device_bytes
          : 0,
      limits.ceiling.spill_bytes > current.held.spill_bytes
          ? limits.ceiling.spill_bytes - current.held.spill_bytes
          : 0,
      limits.ceiling.cpu_tokens,
      limits.ceiling.io_slots};
  if (!fits_within(required, headroom)) {
    result = detail::failure<Done>(
        Code::resource_exhausted, "admit_epoch",
        "the next epoch R+F does not fit in the remaining invocation scope; "
        "the Host must not scale a ResourceVector by a ratio");
    return ready<Done>(ctx, std::move(result));
  }
  detail::set_epoch(ctx.ids.invocation, plan->epoch);
  result.code = Code::ok;
  result.value = Done{};
  return ready<Done>(ctx, std::move(result));
}

}  // namespace acsd::rt
