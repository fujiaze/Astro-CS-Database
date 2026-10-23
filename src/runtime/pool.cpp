// The single CPU pool, command queues, weighted round-robin and I/O lanes.
//
// The executor owns exactly one Services instance per process.  Scientific
// work runs only on the CPU workers; the fixed I/O workers execute submitted
// syscall bodies and never run science.  Every lock here is a scheduler lock:
// no user function, allocator, hash table lookup or I/O runs while one is held
// (the control-plane command index is the only exception and is never taken on
// the dispatch hot path).
#include "internal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace detail {
// Defined in task.cpp; marks the CPU/I/O workers so wait_external and the
// heartbeat paths never block the pool they are running on.
void mark_executor_thread(bool on) noexcept;
}  // namespace detail

namespace {

using namespace std::chrono_literals;

// How often parked waiters are retried even without an explicit capacity
// notification (defensive bound; the normal path is event driven).
constexpr U64 kRetryIntervalNs = 1'000'000;  // 1 ms
// A control-lane item runs at least after this many scientific dispatches.
constexpr U32 kControlStarvationThreshold = 7;  // => 1 control per <=8 science
constexpr auto kIdleWait = 5ms;
constexpr auto kShutdownPoll = 2ms;

Id control_semantic() { return Id("service.control"); }

// Runtime service/cleanup nodes are the only execution segments allowed to run
// after their command was cancelled: FOUNDATION_API requires abort/unlink/
// close cleanup to drain even though the scientific work stops.  They never
// start new scientific work.
bool is_service_spec(const TaskSpec& spec) {
  return spec.semantic.rfind("service.", 0) == 0;
}

Assessment cancelled_assessment(const char* reason) {
  Assessment assessment;
  assessment.complete = false;
  assessment.validity = Validity::invalid;
  assessment.execution = ExecutionState::cancelled;
  assessment.capability = Capability::unavailable;
  assessment.issues.push_back(Issue{Code::cancelled, Scope{}, "runtime.pool",
                                    reason, "the task never produced a value",
                                    ""});
  return assessment;
}

Assessment failure_assessment(Code code, const char* reason) {
  Assessment assessment;
  assessment.complete = false;
  assessment.validity = Validity::invalid;
  assessment.execution = ExecutionState::failed;
  assessment.capability = Capability::unsupported;
  assessment.issues.push_back(Issue{code, Scope{}, "runtime.pool", reason,
                                    "the execution segment was rejected", ""});
  return assessment;
}

// Shared per-Services wake state.  The public Services class has no room for
// extra executor state (internal.hpp is the fixed contract), so the executor
// keeps one small hub keyed by the Services address.  All entries are removed
// by the destructor.
struct PoolHub {
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<U64> generation{0};
  std::atomic<bool> capacity_event{false};
  std::atomic<bool> cpu_done{false};
  std::atomic<U32> io_pending{0};
  std::atomic<U32> science_since_control{0};
  std::atomic<U64> last_retry_ns{0};

  std::mutex close_mutex;
  std::condition_variable close_cv;
  std::map<Id, U32> control_pending;
};

std::mutex g_registry_mutex;
std::unordered_map<const Services*, std::shared_ptr<PoolHub>> g_hubs;
std::unordered_map<const Services*,
                   std::unordered_map<Id, std::weak_ptr<detail::CommandState>>>
    g_command_index;
std::weak_ptr<Services> g_active_services;

std::shared_ptr<PoolHub> hub_of(const Services* services) {
  if (!services) return nullptr;
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  auto it = g_hubs.find(services);
  return it == g_hubs.end() ? nullptr : it->second;
}

void index_command(const Services* services,
                   const std::shared_ptr<detail::CommandState>& command) {
  if (!services || !command) return;
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  g_command_index[services][command->id] = command;
}

void unindex_command(const Services* services, const Id& id) {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  auto it = g_command_index.find(services);
  if (it == g_command_index.end()) return;
  it->second.erase(id);
  if (it->second.empty()) g_command_index.erase(it);
}

std::shared_ptr<detail::CommandState> find_command(const Services* services,
                                                   const Id& id) {
  if (!services || id.empty()) return nullptr;
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  auto it = g_command_index.find(services);
  if (it == g_command_index.end()) return nullptr;
  auto found = it->second.find(id);
  if (found == it->second.end()) return nullptr;
  return found->second.lock();
}

void wake_hub(const Services* services) {
  auto hub = hub_of(services);
  if (!hub) return;
  {
    std::lock_guard<std::mutex> lock(hub->mutex);
    hub->generation.fetch_add(1, std::memory_order_relaxed);
  }
  hub->cv.notify_all();
}

Code code_from_error(const std::string& error) {
  if (error.rfind("identity_conflict", 0) == 0) return Code::identity_conflict;
  if (error.rfind("resource_exhausted", 0) == 0)
    return Code::resource_exhausted;
  if (error.rfind("unsupported", 0) == 0) return Code::unsupported;
  return Code::invalid_request;
}

// Every node that already holds an admitted execution segment releases its
// unclaimed capacity ticket and its unconsumed work reservation exactly once.
void release_execution(const std::shared_ptr<detail::TaskNode>& node) {
  if (!node) return;
  auto scope = std::move(node->task_scope);
  node->task_scope.reset();
  node->running_claim.reset();
  const U64 upper = node->work_upper;
  node->work_upper = 0;
  node->work_consumed = 0;
  if (!scope) return;
  U64 used = 0;
  {
    std::lock_guard<std::mutex> lock(scope->mutex);
    used = scope->own_work;
  }
  if (upper > used) detail::ledger_release_reserved_work(scope, upper - used);
  detail::close_scope(scope);
}

// Completes a node as cancelled.  `dispatched` means take_runnable already
// popped it and charged one CPU token to its command; that token is returned
// here, otherwise the command would stop dispatching forever after the first
// cancellation (verify-rt cancel repro).
void cancel_pending_node(const Services* services,
                         const std::shared_ptr<detail::TaskNode>& node,
                         bool dispatched = false) {
  if (!node || node->terminal.load(std::memory_order_acquire)) return;
  const TaskState state = node->state.load(std::memory_order_acquire);
  const bool counted = dispatched || state == TaskState::ready ||
                       state == TaskState::running ||
                       state == TaskState::suspended;
  auto command = node->command;
  release_execution(node);
  detail::node_complete(
      node, Code::cancelled,
      cancelled_assessment("the command was cancelled or drained"),
      Usage{}, {}, nullptr);
  if (!command) return;
  {
    std::lock_guard<std::mutex> lock(command->mutex);
    if (counted && command->in_flight > 0) --command->in_flight;
    if ((dispatched || state == TaskState::running) &&
        command->cpu_running > 0)
      --command->cpu_running;
  }
  command->drained_cv.notify_all();
  if (auto hub = hub_of(services)) hub->close_cv.notify_all();
}

bool units_can_ever_fit(const ScopeRef& scope, U64 units) {
  for (auto& item : detail::scope_chain(scope)) {
    std::lock_guard<std::mutex> lock(item->mutex);
    if (item->limits.max_work == 0) return false;  // zero budget is a real zero
    if (units > item->limits.max_work) return false;
    // Consumed work is never returned, so an already-consumed budget cannot be
    // regained by waiting for reserved work to be released.
    if (item->work + units > item->limits.max_work) return false;
  }
  return true;
}

enum class AdmitResult { ok, retry, invalid };

struct AdmitOutcome {
  AdmitResult result = AdmitResult::retry;
  Code code = Code::resource_exhausted;
  const char* reason = "insufficient capacity";
};

// Capacity admission of one execution segment.  cpu_tokens/io_slots are
// execution credits owned by the pool, so only the capacity components are
// claimed from the progress (or persistent) arena.
AdmitOutcome admit_task(Services& services,
                        const std::shared_ptr<detail::TaskNode>& node) {
  AdmitOutcome outcome;
  const TaskSpec& spec = node->spec;
  if (!node->context.scope) {
    outcome.result = AdmitResult::invalid;
    outcome.code = Code::invalid_request;
    outcome.reason = "InvocationContext has no scope";
    return outcome;
  }

  ResourceVector capacity = spec.running;
  capacity.cpu_tokens = 0;
  capacity.io_slots = 0;
  if (!is_zero(capacity)) {
    ArenaRef arena = node->context.progress ? node->context.progress
                                            : node->context.persistent;
    if (!arena) {
      outcome.result = AdmitResult::invalid;
      outcome.code = Code::invalid_request;
      outcome.reason =
          "TaskSpec.running needs capacity but the context has no progress or "
          "persistent arena";
      return outcome;
    }
    auto claimed = detail::claim_capacity(arena, capacity);
    if (claimed.code != Code::ok || !claimed.value) {
      if (claimed.code == Code::invalid_request) {
        outcome.result = AdmitResult::invalid;
        outcome.code = Code::invalid_request;
        outcome.reason = "the execution arena rejected the capacity claim";
        return outcome;
      }
      outcome.result = AdmitResult::retry;
      outcome.code = claimed.code;
      outcome.reason = "waiting for execution capacity";
      return outcome;
    }
    node->running_claim = *claimed.value;
  }

  if (!node->task_scope) {
    ScopeLimits limits = node->context.scope->limits;
    limits.max_work = spec.work_upper;
    limits.metadata_bytes = 0;
    auto child = detail::make_child_scope(
        node->context.scope,
        node->context.ids.invocation + "/task/" + std::to_string(node->id),
        limits);
    if (child.code != Code::ok || !child.value) {
      node->running_claim.reset();
      outcome.result = AdmitResult::invalid;
      outcome.code = Code::invalid_request;
      outcome.reason =
          "cannot create the task sub-scope under the invocation scope";
      return outcome;
    }
    node->task_scope = *child.value;
  }

  if (spec.work_upper != 0) {
    auto admitted = services.admit_work(node->task_scope, spec.work_upper);
    if (admitted.code != Code::ok) {
      node->running_claim.reset();
      if (admitted.code == Code::resource_exhausted &&
          !units_can_ever_fit(node->task_scope, spec.work_upper)) {
        outcome.result = AdmitResult::invalid;
        outcome.code = Code::resource_exhausted;
        outcome.reason =
            "TaskSpec.work_upper exceeds the declared max_work budget";
        return outcome;
      }
      outcome.result = AdmitResult::retry;
      outcome.code = admitted.code;
      outcome.reason = "waiting for work budget";
      return outcome;
    }
  }

  node->work_upper = spec.work_upper;
  node->task_scope->work_upper = spec.work_upper;
  outcome.result = AdmitResult::ok;
  outcome.code = Code::ok;
  outcome.reason = "";
  return outcome;
}

}  // namespace

// ---------------------------------------------------------------------------
// Services lifecycle
// ---------------------------------------------------------------------------

std::shared_ptr<Services> Services::create(const PoolConfig& config,
                                           std::string& error) {
  error.clear();
  if (config.cpu_threads == 0) {
    error = "invalid_request: PoolConfig.cpu_threads must be at least 1";
    return nullptr;
  }
  if (config.read_slots == 0 || config.write_slots == 0) {
    error =
        "invalid_request: PoolConfig.read_slots and write_slots must be at "
        "least 1";
    return nullptr;
  }
  if (config.process.ceiling.pinned_bytes > config.process.ceiling.host_bytes) {
    error = "invalid_request: process pinned_bytes must not exceed host_bytes";
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    if (auto active = g_active_services.lock()) {
      (void)active;
      error =
          "identity_conflict: an active Services pool already exists in this "
          "process; share it instead of creating a second ledger";
      return nullptr;
    }
  }

  auto services = std::shared_ptr<Services>(new Services());
  services->config_ = config;
  unsigned hardware = std::thread::hardware_concurrency();
  if (hardware == 0) hardware = 1;
  services->cpu_threads_ =
      std::max<U32>(1, std::min<U32>(config.cpu_threads, hardware));
  services->read_slots_ = config.read_slots;
  services->write_slots_ = config.write_slots;

  auto root = std::make_shared<ScopeState>();
  root->local_id = Id("service.root");
  root->limits = config.process;
  // CPU/IO tokens are execution credits, never long-lived F capacity: the root
  // must be able to lend the whole pool to its commands.
  if (root->limits.ceiling.cpu_tokens < services->cpu_threads_)
    root->limits.ceiling.cpu_tokens = services->cpu_threads_;
  const U32 io_total = services->read_slots_ + services->write_slots_;
  if (root->limits.ceiling.io_slots < io_total)
    root->limits.ceiling.io_slots = io_total;
  root->cancel = std::make_shared<CancelState>();
  root->start = std::chrono::steady_clock::now();
  root->owner = services;
  services->root = root;
  services->blocks = std::make_shared<detail::BlockStore>();

  auto hub = std::make_shared<PoolHub>();
  {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_hubs[services.get()] = hub;
    g_active_services = services;
  }

#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
  {
    std::string probe_error;
    std::weak_ptr<Services> weak = services;
    services->probes = detail::make_probe_sink(
        config,
        [weak](detail::IoRequest request) {
          if (auto owner = weak.lock()) owner->submit_io(std::move(request));
        },
        probe_error);
    // A probe sink failure never changes a scientific code and never blocks
    // the pool; diagnostics stay absent and are reported as dropped.
  }
#else
  services->probes = nullptr;
#endif

  try {
    services->workers_.reserve(services->cpu_threads_ + io_total);
    for (U32 i = 0; i < services->cpu_threads_; ++i)
      services->workers_.emplace_back([services] { services->worker_loop(); });
    for (U32 i = 0; i < services->read_slots_; ++i)
      services->workers_.emplace_back(
          [services] { services->io_loop(false); });
    for (U32 i = 0; i < services->write_slots_; ++i)
      services->workers_.emplace_back([services] { services->io_loop(true); });
  } catch (...) {
    error = "internal: failed to start the executor threads";
    services->shutdown();
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_hubs.erase(services.get());
    g_active_services.reset();
    return nullptr;
  }
  return services;
}

Services::~Services() {
  shutdown();
  {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_hubs.erase(this);
    g_command_index.erase(this);
    if (g_active_services.lock().get() == this) g_active_services.reset();
  }
}

void Services::shutdown() {
  static std::mutex shutdown_mutex;
  std::lock_guard<std::mutex> serialise(shutdown_mutex);
  const bool first = !shutdown_.exchange(true);
  if (first) {
    std::vector<std::shared_ptr<detail::CommandState>> commands;
    {
      std::lock_guard<std::mutex> lock(commands_mutex_);
      commands = commands_;
    }
    for (auto& command : commands) {
      {
        std::lock_guard<std::mutex> lock(command->mutex);
        command->accept_new = false;
        command->draining = true;
      }
      if (command->cancel) command->cancel->request();
    }
    if (auto hub = hub_of(this)) {
      hub->capacity_event.store(true);
      {
        std::lock_guard<std::mutex> lock(hub->mutex);
        hub->generation.fetch_add(1, std::memory_order_relaxed);
      }
      hub->cv.notify_all();
    }
    ready_cv_.notify_all();
    control_cv_.notify_all();
    read_cv_.notify_all();
    write_cv_.notify_all();
  }
  if (workers_.empty()) return;

  const std::thread::id self = std::this_thread::get_id();
  const std::size_t cpu_count =
      std::min<std::size_t>(cpu_threads_, workers_.size());
  auto join_range = [&](std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
      auto& worker = workers_[i];
      if (!worker.joinable()) continue;
      if (worker.get_id() == self) {
        // Shutdown from inside a pooled body cannot join itself.  The lifetime
        // contract requires an external owner; detaching keeps the program from
        // deadlocking on a call that is already a contract violation.
        worker.detach();
        continue;
      }
      worker.join();
    }
  };

  // CPU workers drain cancelled science and the control lane, then stop.
  join_range(0, cpu_count);

  // After the CPU workers stop, no other thread can execute the completion
  // callbacks that are still parked in the control lane.  A Task produced
  // after shutdown (for example an invoke rejected as unsupported) must still
  // become terminal, so drain the lane here; anything left is completed as
  // cancelled instead of being silently dropped.
  detail::mark_executor_thread(true);
  for (;;) {
    std::shared_ptr<detail::TaskNode> node;
    {
      std::lock_guard<std::mutex> lock(control_mutex_);
      if (control_.empty()) break;
      node = control_.front();
      control_.pop_front();
    }
    auto body = std::move(node->body);
    node->body = nullptr;
    if (body) {
      try {
        body(node->context);
      } catch (...) {
      }
    }
    if (!node->terminal.load(std::memory_order_acquire)) {
      detail::node_complete(
          node, Code::cancelled,
          cancelled_assessment("the service pool shut down before the "
                               "completion callback ran"),
          Usage{}, {}, nullptr);
    }
  }
  detail::mark_executor_thread(false);

  if (auto hub = hub_of(this)) {
    hub->cpu_done.store(true);
    {
      std::lock_guard<std::mutex> lock(hub->mutex);
      hub->generation.fetch_add(1, std::memory_order_relaxed);
    }
    hub->cv.notify_all();
  }
  read_cv_.notify_all();
  write_cv_.notify_all();
  join_range(cpu_count, workers_.size());
  workers_.clear();
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

std::shared_ptr<detail::CommandState> Services::add_command(Id id,
                                                            ScopeLimits limits,
                                                            U32 weight,
                                                            std::string& error) {
  error.clear();
  if (id.empty()) {
    error = "invalid_request: command id must not be empty";
    return nullptr;
  }
  if (weight < 1 || weight > 16) {
    error = "invalid_request: command weight must be in 1..16";
    return nullptr;
  }
  if (limits.ceiling.pinned_bytes > limits.ceiling.host_bytes) {
    error = "invalid_request: command pinned_bytes must not exceed host_bytes";
    return nullptr;
  }
  const U32 command_tokens = limits.ceiling.cpu_tokens == 0
                                 ? cpu_threads_
                                 : limits.ceiling.cpu_tokens;
  if (command_tokens > cpu_threads_) {
    error =
        "resource_exhausted: the command CPU ceiling exceeds the unique pool";
    return nullptr;
  }
 {
    std::lock_guard<std::mutex> lock(commands_mutex_);
    for (auto& command : commands_)
      if (command->id == id) {
        error = "identity_conflict: an active command with this id exists";
        return nullptr;
      }
  }

  auto scope = detail::make_child_scope(root, id, limits);
  if (scope.code != Code::ok || !scope.value) {
    error =
        "resource_exhausted: cannot create the command scope under the process "
        "scope";
    return nullptr;
  }

  auto command = std::make_shared<detail::CommandState>();
  command->id = std::move(id);
  command->weight = weight;
  command->scope = *scope.value;
  command->cancel = std::make_shared<CancelState>();
  {
    std::lock_guard<std::mutex> lock(command->scope->mutex);
    command->scope->cancel = command->cancel;
  }
  command->credit = weight;
  command->accept_new = true;
  {
    std::lock_guard<std::mutex> lock(commands_mutex_);
    commands_.push_back(command);
  }
  index_command(this, command);
  ready_cv_.notify_all();
  wake_hub(this);
  return command;
}

void Services::remove_command(
    const std::shared_ptr<detail::CommandState>& command) {
  if (!command) return;
  bool removed = false;
  {
    std::lock_guard<std::mutex> lock(commands_mutex_);
    auto it = std::find(commands_.begin(), commands_.end(), command);
    if (it != commands_.end()) {
      const std::size_t index =
          static_cast<std::size_t>(std::distance(commands_.begin(), it));
      commands_.erase(it);
      if (commands_.empty())
        rr_index_ = 0;
      else if (index < rr_index_)
        --rr_index_;
      rr_index_ %= std::max<std::size_t>(1, commands_.size());
      removed = true;
    }
  }
  unindex_command(this, command->id);
  {
    std::lock_guard<std::mutex> lock(command->mutex);
    command->accept_new = false;
    command->draining = true;
  }
  (void)removed;
  ready_cv_.notify_all();
  wake_hub(this);
}

// ---------------------------------------------------------------------------
// Submission
// ---------------------------------------------------------------------------

void Services::submit(const std::shared_ptr<detail::TaskNode>& node,
                      std::function<void(InvocationContext)> body) {
  if (!node) return;
  node->body = std::move(body);

  const bool service_node = is_service_spec(node->spec);
  if (shutdown_.load() && !service_node) {
    cancel_pending_node(this, node);
    return;
  }
  if (node->spec.running.cpu_tokens == 0) {
    detail::node_complete(
        node, Code::invalid_request,
        failure_assessment(Code::invalid_request,
                           "a CPU execution segment requires cpu_tokens >= 1"),
        Usage{}, {}, nullptr);
    return;
  }

  std::shared_ptr<detail::CommandState> command;
  {
    std::lock_guard<std::mutex> lock(commands_mutex_);
    for (auto& candidate : commands_)
      if (candidate->id == node->context.ids.command) {
        command = candidate;
        break;
      }
  }
  node->command = command;

  if (shutdown_.load() && service_node) {
    // No worker is left to pick cleanup work up; run it synchronously on the
    // calling thread, with normal resource admission.
    detail::mark_executor_thread(true);
    const AdmitOutcome outcome = admit_task(*this, node);
    if (outcome.result != AdmitResult::ok) {
      release_execution(node);
      detail::node_complete(node, outcome.code,
                            failure_assessment(outcome.code, outcome.reason),
                            Usage{}, {}, nullptr);
    } else {
      run_node(node);
    }
    detail::mark_executor_thread(false);
    return;
  }

  if (!command) {
    // A context that was not opened by open_command cannot participate in the
    // weighted round-robin; it runs on the same CPU pool through the control
    // lane so the scientific work is still never executed on an I/O thread.
    if (!service_node && cancellation_requested(node->context)) {
      cancel_pending_node(this, node);
      return;
    }
    node->state.store(TaskState::ready, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(control_mutex_);
      control_.push_back(node);
    }
    control_cv_.notify_all();
    wake_hub(this);
    return;
  }

  bool rejected = false;
  {
    std::lock_guard<std::mutex> lock(command->mutex);
    rejected = !command->accept_new || command->draining;
  }
  if (rejected || (!service_node && cancellation_requested(node->context))) {
    cancel_pending_node(this, node);
    return;
  }

  const AdmitOutcome outcome = admit_task(*this, node);
  if (outcome.result == AdmitResult::invalid) {
    release_execution(node);
    detail::node_complete(node, outcome.code,
                          failure_assessment(outcome.code, outcome.reason),
                          Usage{}, {}, nullptr);
    return;
  }
  if (outcome.result == AdmitResult::retry) {
    node->state.store(TaskState::waiting_resources, std::memory_order_release);
    bool accepted = false;
    {
      std::lock_guard<std::mutex> lock(command->mutex);
      if (command->accept_new && !command->draining) {
        command->waiting.push_back(node);
        accepted = true;
      }
    }
    if (!accepted) {
      cancel_pending_node(this, node);
      return;
    }
    notify_capacity();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(command->mutex);
    if (!command->accept_new || command->draining) {
      rejected = true;
    } else {
      node->state.store(TaskState::ready, std::memory_order_release);
      command->ready.push_back(node);
      ++command->in_flight;
    }
  }
  if (rejected) {
    cancel_pending_node(this, node);
    return;
  }
  ready_cv_.notify_all();
  wake_hub(this);
}

void Services::schedule_control(const std::shared_ptr<detail::TaskNode>& node,
                                std::function<void(InvocationContext)> body) {
  if (!node) return;
  if (shutdown_.load()) {
    // The workers are gone or draining: run the completion callback on the
    // completing thread so a Task produced after shutdown still reaches its
    // terminal state instead of leaving wait_external blocked forever.
    try {
      if (body) body(node->context);
    } catch (...) {
    }
    return;
  }
  if (node->spec.semantic.empty())
    node->spec.semantic = control_semantic();
  auto hub = hub_of(this);
  const Id command_id = node->context.ids.command;
  if (hub && !command_id.empty()) {
    std::lock_guard<std::mutex> lock(hub->close_mutex);
    ++hub->control_pending[command_id];
  }
  auto inner = std::move(body);
  node->body = [hub, command_id, inner = std::move(inner)](
                   InvocationContext context) mutable {
    try {
      if (inner) inner(context);
    } catch (...) {
    }
    if (hub && !command_id.empty()) {
      {
        std::lock_guard<std::mutex> lock(hub->close_mutex);
        auto it = hub->control_pending.find(command_id);
        if (it != hub->control_pending.end() && it->second > 0) {
          if (--it->second == 0) hub->control_pending.erase(it);
        }
      }
      hub->close_cv.notify_all();
    }
  };
  {
    std::lock_guard<std::mutex> lock(control_mutex_);
    control_.push_back(node);
  }
  control_cv_.notify_all();
  wake_hub(this);
}

void Services::submit_io(detail::IoRequest request) {
  if (auto hub = hub_of(this))
    hub->io_pending.fetch_add(1, std::memory_order_relaxed);
  const bool write = request.write;
  {
    std::lock_guard<std::mutex> lock(io_mutex_);
    (write ? writes_ : reads_).push_back(std::move(request));
  }
  (write ? write_cv_ : read_cv_).notify_all();
  wake_hub(this);
}

void Services::notify_capacity() {
  if (auto hub = hub_of(this)) {
    hub->capacity_event.store(true, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(hub->mutex);
      hub->generation.fetch_add(1, std::memory_order_relaxed);
    }
    hub->cv.notify_all();
  }
  ready_cv_.notify_all();
  control_cv_.notify_all();
}

Result<Done> Services::admit_work(const ScopeRef& scope, U64 units) {
  return detail::ledger_admit_work(scope, units);
}

void Services::release_work(const ScopeRef& scope, U64 units) {
  detail::ledger_release_reserved_work(scope, units);
}

// ---------------------------------------------------------------------------
// Executor loops
// ---------------------------------------------------------------------------

bool Services::take_runnable(std::shared_ptr<detail::TaskNode>& node) {
  auto hub = hub_of(this);

  // 0) Retry tasks parked in waiting_resources.  This is throttled so a dispatch
  //    loop cannot become a busy retry loop, and forced after any capacity
  //    release or during shutdown.
  bool retry = shutdown_.load();
  if (hub) {
    if (hub->capacity_event.exchange(false, std::memory_order_acq_rel))
      retry = true;
    if (!retry) {
      const U64 now = static_cast<U64>(
          std::chrono::steady_clock::now().time_since_epoch().count());
      U64 last = hub->last_retry_ns.load(std::memory_order_relaxed);
      if (now - last >= kRetryIntervalNs &&
          hub->last_retry_ns.compare_exchange_strong(last, now))
        retry = true;
    }
  }
  if (retry) {
    std::vector<std::shared_ptr<detail::CommandState>> commands;
    {
      std::lock_guard<std::mutex> lock(commands_mutex_);
      commands = commands_;
    }
    for (auto& command : commands) {
      std::deque<std::shared_ptr<detail::TaskNode>> waiting;
      {
        std::lock_guard<std::mutex> lock(command->mutex);
        waiting.swap(command->waiting);
      }
      for (auto& parked : waiting) {
        bool accepted = false;
        {
          std::lock_guard<std::mutex> lock(command->mutex);
          accepted = command->accept_new && !command->draining;
        }
        if (!accepted || shutdown_.load() ||
            (!is_service_spec(parked->spec) &&
             cancellation_requested(parked->context))) {
          cancel_pending_node(this, parked);
          continue;
        }
        const AdmitOutcome outcome = admit_task(*this, parked);
        if (outcome.result == AdmitResult::invalid) {
          release_execution(parked);
          detail::node_complete(
              parked, outcome.code,
              failure_assessment(outcome.code, outcome.reason), Usage{}, {},
              nullptr);
          continue;
        }
        if (outcome.result == AdmitResult::retry) {
          std::lock_guard<std::mutex> lock(command->mutex);
          command->waiting.push_back(parked);
          continue;
        }
        bool queued = false;
        {
          std::lock_guard<std::mutex> lock(command->mutex);
          if (command->accept_new && !command->draining) {
            parked->state.store(TaskState::ready, std::memory_order_release);
            command->ready.push_back(parked);
            ++command->in_flight;
            queued = true;
          }
        }
        if (!queued) cancel_pending_node(this, parked);
      }
    }
  }

  // 1) Control lane must not starve: at most eight scientific dispatches
  //    between two control items.
  bool control_ready = false;
  {
    std::lock_guard<std::mutex> lock(control_mutex_);
    control_ready = !control_.empty();
  }
  if (control_ready) {
    const U32 since =
        hub ? hub->science_since_control.load(std::memory_order_relaxed) : 0;
    if (since >= kControlStarvationThreshold) {
      std::lock_guard<std::mutex> lock(control_mutex_);
      if (!control_.empty()) {
        node = control_.front();
        control_.pop_front();
        if (hub) hub->science_since_control.store(0);
        return true;
      }
    }
  }

  // 2) Weighted round-robin over ready scientific work.  Only commands with
  //    ready work and a free per-command CPU token participate; unused credit
  //    never accumulates into the next round.
  if (!shutdown_.load()) {
    std::lock_guard<std::mutex> lock(commands_mutex_);
    if (!commands_.empty()) {
      const std::size_t count = commands_.size();
      for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t step = 0; step < count; ++step) {
          const std::size_t index = (rr_index_ + step) % count;
          auto command = commands_[index];
          if (detail::scope_status(command->scope) != ScopeStatus::accepting)
            continue;
          U32 ceiling = command->scope->limits.ceiling.cpu_tokens;
          if (ceiling == 0) ceiling = cpu_threads_;
          std::lock_guard<std::mutex> command_lock(command->mutex);
          if (!command->accept_new || command->draining) continue;
          if (command->ready.empty() || command->credit == 0) continue;
          if (command->cpu_running >= ceiling) continue;
          node = command->ready.front();
          command->ready.pop_front();
          --command->credit;
          ++command->cpu_running;
          if (command->credit == 0)
            rr_index_ = (index + 1) % count;
          else
            rr_index_ = index;
          if (hub) hub->science_since_control.fetch_add(1);
          return true;
        }
        // No command could dispatch: start a new round for every command that
        // actually has ready work, then scan once more.  Commands without
        // ready work are not granted credit.
        bool any_ready = false;
        for (auto& command : commands_) {
          std::lock_guard<std::mutex> lock(command->mutex);
          if (!command->ready.empty()) {
            command->credit = command->weight;
            any_ready = true;
          }
        }
        if (!any_ready) break;
      }
    }
  }

  // 3) Control work when no scientific work can run.
  {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (!control_.empty()) {
      node = control_.front();
      control_.pop_front();
      if (hub) hub->science_since_control.store(0);
      return true;
    }
  }
  return false;
}

void Services::run_node(std::shared_ptr<detail::TaskNode> node) {
  if (!node) return;
  const bool control = node->spec.semantic == control_semantic();

  // A terminal node is used as the carrier of one completion callback; it is
  // not re-completed.
  if (node->terminal.load(std::memory_order_acquire)) {
    auto body = std::move(node->body);
    node->body = nullptr;
    if (body) {
      try {
        body(node->context);
      } catch (...) {
      }
    }
    return;
  }

  const bool service = is_service_spec(node->spec);
  if (!service && !control &&
      (shutdown_.load() || cancellation_requested(node->context))) {
    cancel_pending_node(this, node, /*dispatched=*/true);
    return;
  }

  auto command = node->command;
  node->state.store(TaskState::running, std::memory_order_release);
  cpu_in_use_.fetch_add(1, std::memory_order_relaxed);
  auto body = std::move(node->body);
  node->body = nullptr;
  InvocationContext task_context = node->context;
  if (node->task_scope) task_context.scope = node->task_scope;
  try {
    if (body) body(std::move(task_context));
  } catch (...) {
    detail::node_complete_exception(node);
  }
  if (!node->terminal.load(std::memory_order_acquire)) {
    detail::node_complete(
        node, Code::internal,
        failure_assessment(Code::internal,
                           "the task body returned without completing the node"),
        Usage{}, {}, nullptr);
  }
  cpu_in_use_.fetch_sub(1, std::memory_order_relaxed);
  release_execution(node);
  if (command && !control) {
    {
      std::lock_guard<std::mutex> lock(command->mutex);
      if (command->cpu_running > 0) --command->cpu_running;
      if (command->in_flight > 0) --command->in_flight;
    }
    command->drained_cv.notify_all();
  }
  if (auto hub = hub_of(this)) hub->close_cv.notify_all();
  notify_capacity();
}

void Services::worker_loop() {
  detail::mark_executor_thread(true);
  auto hub = hub_of(this);
  while (true) {
    std::shared_ptr<detail::TaskNode> node;
    if (take_runnable(node)) {
      run_node(std::move(node));
      continue;
    }
    if (shutdown_.load()) {
      // Drain every queued scientific node as cancelled; cleanup/control items
      // (already submitted) still run.
      std::vector<std::shared_ptr<detail::CommandState>> commands;
      {
        std::lock_guard<std::mutex> lock(commands_mutex_);
        commands = commands_;
      }
      std::vector<std::shared_ptr<detail::TaskNode>> pending;
      for (auto& command : commands) {
        std::lock_guard<std::mutex> lock(command->mutex);
        while (!command->ready.empty()) {
          pending.push_back(std::move(command->ready.front()));
          command->ready.pop_front();
        }
        while (!command->waiting.empty()) {
          pending.push_back(std::move(command->waiting.front()));
          command->waiting.pop_front();
        }
      }
      if (!pending.empty()) {
        for (auto& queued : pending) cancel_pending_node(this, queued);
        continue;
      }
      bool control_empty = false;
      {
        std::lock_guard<std::mutex> lock(control_mutex_);
        control_empty = control_.empty();
      }
      if (!control_empty) continue;
      if (hub && hub->io_pending.load(std::memory_order_relaxed) != 0) {
        std::unique_lock<std::mutex> lock(hub->mutex);
        hub->cv.wait_for(lock, kShutdownPoll);
        continue;
      }
      if (cpu_in_use_.load(std::memory_order_relaxed) != 0) {
        std::unique_lock<std::mutex> lock(hub ? hub->mutex : control_mutex_);
        if (hub)
          hub->cv.wait_for(lock, kShutdownPoll);
        else
          std::this_thread::sleep_for(kShutdownPoll);
        continue;
      }
      break;
    }
    if (hub) {
      std::unique_lock<std::mutex> lock(hub->mutex);
      const U64 generation = hub->generation.load(std::memory_order_relaxed);
      hub->cv.wait_for(lock, kIdleWait, [&] {
        return hub->generation.load(std::memory_order_relaxed) != generation ||
               shutdown_.load();
      });
    } else {
      std::this_thread::sleep_for(kIdleWait);
    }
  }
  detail::mark_executor_thread(false);
}

void Services::io_loop(bool write) {
  detail::mark_executor_thread(true);
  auto hub = hub_of(this);
  while (true) {
    detail::IoRequest request;
    {
      std::unique_lock<std::mutex> lock(io_mutex_);
      auto& queue = write ? writes_ : reads_;
      auto& wake = write ? write_cv_ : read_cv_;
      wake.wait(lock, [&] {
        if (!queue.empty()) return true;
        if (!shutdown_.load()) return false;
        return hub && hub->cpu_done.load(std::memory_order_acquire);
      });
      if (queue.empty()) break;
      request = std::move(queue.front());
      queue.pop_front();
    }
    try {
      if (request.body) request.body();
    } catch (...) {
      // The submission owns its completion node; an escaping exception must
      // not kill the fixed I/O worker before shutdown drains it.
    }
    if (hub) hub->io_pending.fetch_sub(1, std::memory_order_relaxed);
    notify_capacity();
  }
  detail::mark_executor_thread(false);
}

// ---------------------------------------------------------------------------
// Probes and small helpers
// ---------------------------------------------------------------------------

U64 Services::next_span_id() noexcept {
  U64 id = span_counter_.fetch_add(1, std::memory_order_relaxed);
  if (id == 0) id = span_counter_.fetch_add(1, std::memory_order_relaxed);
  return id;
}

// ---------------------------------------------------------------------------
// Public cancellation entry points
// ---------------------------------------------------------------------------

bool cancellation_requested(const InvocationContext& context) noexcept {
  try {
    auto state = context.cancel ? context.cancel
                                : (context.scope ? context.scope->cancel
                                                 : nullptr);
    return state && state->is_requested();
  } catch (...) {
    return true;
  }
}

void request_cancel(const InvocationContext& context) noexcept {
  try {
    auto state = context.cancel ? context.cancel
                                : (context.scope ? context.scope->cancel
                                                 : nullptr);
    if (state) state->request();
    // Wake tasks parked in waiting_resources and the workers scanning them.
    if (context.scope) detail::services_notify_capacity(context.scope);
  } catch (...) {
  }
}

namespace detail {

std::shared_ptr<Services> services_of(const InvocationContext& context) {
  return context.services;
}

bool cancellation_requested(
    const std::shared_ptr<CancelState>& state) noexcept {
  return state && state->is_requested();
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Bootstrap and close entry points
// ---------------------------------------------------------------------------

Result<std::shared_ptr<Services>> make_services(PoolConfig config) {
  Result<std::shared_ptr<Services>> result;
  std::string error;
  auto services = Services::create(config, error);
  if (!services) {
    result.code = code_from_error(error);
    result.assessment = failure_assessment(result.code, error.c_str());
    return result;
  }
  result.code = Code::ok;
  result.value = std::move(services);
  return result;
}

Result<InvocationContext> open_command(std::shared_ptr<Services> services,
                                       Id command_id, ScopeLimits limits,
                                       U32 weight) {
  static std::atomic<U64> invocation_counter{1};
  Result<InvocationContext> result;
  if (!services || command_id.empty()) {
    result.code = Code::invalid_request;
    result.assessment = failure_assessment(
        Code::invalid_request, "open_command needs a pool and a command id");
    return result;
  }
  std::string error;
  auto command = services->add_command(command_id, limits, weight, error);
  if (!command) {
    result.code = code_from_error(error);
    result.assessment = failure_assessment(result.code, error.c_str());
    return result;
  }
  InvocationContext context;
  context.services = std::move(services);
  context.scope = command->scope;
  context.cancel = command->cancel;
  context.ids.command = command->id;
  context.ids.process = Id("service.command");
  context.ids.operation = Id("open");
  context.ids.invocation =
      Id("service.command/open/") +
      std::to_string(invocation_counter.fetch_add(1, std::memory_order_relaxed));
  result.code = Code::ok;
  result.value = std::move(context);
  return result;
}

Task<Done> close_command(InvocationContext context) {
  auto services = detail::services_of(context);
  if (!services || !context.scope) {
    Result<Done> failure;
    failure.code = Code::invalid_request;
    failure.assessment = failure_assessment(
        Code::invalid_request, "close_command needs an open command context");
    return ready<Done>(context, std::move(failure));
  }
  auto command = find_command(services.get(), context.ids.command);
  if (!command) {
    // Already closed: idempotent success, not a new failure.
    Result<Done> already;
    already.code = Code::ok;
    already.value = Done{};
    return ready<Done>(context, std::move(already));
  }

  {
    std::lock_guard<std::mutex> lock(command->mutex);
    command->accept_new = false;
    command->draining = true;
  }
  if (command->cancel) command->cancel->request();
  detail::set_scope_status(command->scope, ScopeStatus::stopping);
  services->remove_command(command);

  std::vector<std::shared_ptr<detail::TaskNode>> pending;
  {
    std::lock_guard<std::mutex> lock(command->mutex);
    while (!command->ready.empty()) {
      pending.push_back(std::move(command->ready.front()));
      command->ready.pop_front();
    }
    while (!command->waiting.empty()) {
      pending.push_back(std::move(command->waiting.front()));
      command->waiting.pop_front();
    }
  }
  for (auto& queued : pending) cancel_pending_node(services.get(), queued);
  services->notify_capacity();

  auto out = detail::make_node(context, TaskSpec{});
  auto hub = hub_of(services.get());
  const Id command_id = command->id;
  auto drained = [services, command, command_id, hub, out]() -> bool {
    bool idle = false;
    {
      std::lock_guard<std::mutex> lock(command->mutex);
      idle = command->in_flight == 0;
    }
    U32 pending_control = 0;
    if (hub) {
      std::lock_guard<std::mutex> lock(hub->close_mutex);
      auto it = hub->control_pending.find(command_id);
      if (it != hub->control_pending.end()) pending_control = it->second;
    }
    if (!idle || pending_control != 0) return false;
    detail::set_scope_status(command->scope, ScopeStatus::draining);
    detail::close_scope(command->scope);
    if (!out->terminal.load(std::memory_order_acquire)) {
      detail::node_complete(out, Code::ok, Assessment{}, Usage{}, {},
                            detail::box_value<Done>(Done{}));
    }
    return true;
  };

  if (!detail::on_executor_thread()) {
    // External owner: block this thread only, never a pooled worker.
    while (!drained()) {
      if (hub) {
        std::unique_lock<std::mutex> lock(hub->close_mutex);
        hub->close_cv.wait_for(lock, kShutdownPoll);
      } else {
        std::this_thread::sleep_for(kShutdownPoll);
      }
    }
    return wrap_task<Done>(std::move(out));
  }

  // Executor thread: repost a poll through the control lane instead of
  // blocking the pool that has to drain the command.
  InvocationContext poll_context = context;
  poll_context.ids.command.clear();
  poll_context.ids.invocation =
      Id("service.command/close/") + command_id + "/" +
      std::to_string(detail::node_id(out));
  auto poll = std::make_shared<std::function<void()>>();
  *poll = [poll, drained, poll_context, out]() mutable {
    if (drained()) return;
    if (poll_context.services && poll_context.services->shutting_down()) {
      // The pool is stopping and cannot run another poll: close the handle as
      // cancelled instead of leaving the caller blocked forever.
      if (!out->terminal.load(std::memory_order_acquire)) {
        detail::node_complete(
            out, Code::cancelled,
            cancelled_assessment(
                "the service pool shut down while closing the command"),
            Usage{}, {}, nullptr);
      }
      return;
    }
    detail::node_schedule_control(poll_context, [poll]() { (*poll)(); });
  };
  (*poll)();
  return wrap_task<Done>(std::move(out));
}

}  // namespace acsd::rt
