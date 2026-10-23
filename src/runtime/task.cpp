// Task node state machine, completion/continuation dispatch and cancellation.
//
// This translation unit owns the node side of FOUNDATION_API's "Task、错误与
// 取消的精确规则": exactly one terminal transition per node, success always
// carrying a value, continuations scheduled on the unique CPU pool (never run
// inline on an I/O completion thread) and the executor-thread marker that makes
// wait_external reject a blocking call from inside the pool.
#include "internal.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace detail {

namespace {

thread_local bool t_executor_thread = false;
std::atomic<TaskId> g_next_task_id{1};

bool terminal_state(TaskState state) noexcept {
  return state == TaskState::success || state == TaskState::failed ||
         state == TaskState::cancelled;
}


Assessment cancelled_assessment(const char* reason) {
  Assessment assessment;
  assessment.complete = false;
  assessment.validity = Validity::invalid;
  assessment.execution = ExecutionState::cancelled;
  assessment.capability = Capability::unavailable;
  assessment.issues.push_back(Issue{Code::cancelled, Scope{}, "runtime.task",
                                    reason, "task was cancelled before use",
                                    "re-submit after the cancel is cleared"});
  return assessment;
}

Assessment failure_assessment(Code code, const char* reason) {
  Assessment assessment;
  assessment.complete = false;
  assessment.validity = Validity::invalid;
  assessment.execution = ExecutionState::failed;
  assessment.capability = Capability::unsupported;
  assessment.issues.push_back(Issue{code, Scope{}, "runtime.task", reason,
                                    "the execution segment did not run", ""});
  return assessment;
}

// Schedules one completion callback.  A runtime context always goes through
// the CPU control lane; a context without Services (used by local helpers and
// tests that never opened a pool) falls back to an inline call because there is
// no pool that could run it.
void schedule_continuation(const std::shared_ptr<TaskNode>& node,
                           std::function<void()> continuation) {
  if (!continuation) return;
  auto services = node ? node->context.services : nullptr;
  if (!services || services->shutting_down()) {
    // No pool is left to run this completion callback (either the context was
    // never opened by make_services, or shutdown already retired the workers).
    // Running it inline keeps the dependent node observable as unsupported or
    // cancelled instead of hanging wait_external forever.
    try {
      continuation();
    } catch (...) {
    }
    return;
  }
  auto body = [fn = std::move(continuation)](InvocationContext) mutable {
    try {
      fn();
    } catch (...) {
      // The dependent node is completed by the continuation itself; an
      // escaping exception must not tear down the worker.
    }
  };
  try {
    services->schedule_control(node, std::move(body));
  } catch (...) {
    // Allocation failure while scheduling a completion callback: the node is
    // already terminal, so the observable failure is limited to the dependent
    // node never being completed.  Do not unwind through the completing caller.
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Executor-thread marker
// ---------------------------------------------------------------------------

void mark_executor_thread(bool on) noexcept { t_executor_thread = on; }

bool on_executor_thread() noexcept { return t_executor_thread; }

// ---------------------------------------------------------------------------
// TaskNode lifecycle
// ---------------------------------------------------------------------------

TaskNode::TaskNode(InvocationContext context, TaskSpec specification)
    : id(g_next_task_id.fetch_add(1, std::memory_order_relaxed)),
      context(std::move(context)),
      spec(std::move(specification)) {}

TaskNode::~TaskNode() = default;

std::shared_ptr<TaskNode> make_node(InvocationContext ctx, TaskSpec spec) {
  return std::make_shared<TaskNode>(std::move(ctx), std::move(spec));
}

TaskId node_id(const std::shared_ptr<TaskNode>& node) noexcept {
  return node ? node->id : TaskId{0};
}

TaskState node_state(const std::shared_ptr<TaskNode>& node) noexcept {
  return node ? node->state.load(std::memory_order_acquire)
              : TaskState::success;
}

bool node_finished(const std::shared_ptr<TaskNode>& node) noexcept {
  if (!node) return true;
  if (node->terminal.load(std::memory_order_acquire)) return true;
  return terminal_state(node->state.load(std::memory_order_acquire));
}

Code node_code(const std::shared_ptr<TaskNode>& node) noexcept {
  if (!node) return Code::internal;
  // Only meaningful once the node is terminal; the terminal flag is published
  // before the payload fields are read by any observer (release/acquire via
  // node_finished and the condition variable).
  return node->code;
}

namespace {
const Assessment& empty_assessment() noexcept {
  static const Assessment value{};
  return value;
}
const Usage& empty_usage() noexcept {
  static const Usage value{};
  return value;
}
const std::vector<Id>& empty_dependencies() noexcept {
  static const std::vector<Id> value{};
  return value;
}
}  // namespace

const Assessment& node_assessment(const std::shared_ptr<TaskNode>& node) {
  return node ? node->assessment : empty_assessment();
}

const Usage& node_usage(const std::shared_ptr<TaskNode>& node) {
  return node ? node->usage : empty_usage();
}

const std::vector<Id>& node_dependencies(const std::shared_ptr<TaskNode>& node) {
  return node ? node->dependencies : empty_dependencies();
}

std::shared_ptr<void> node_take_value(const std::shared_ptr<TaskNode>& node) {
  if (!node) return nullptr;
  std::lock_guard<std::mutex> lock(node->mutex);
  return std::move(node->value);
}

std::shared_ptr<const void> node_peek_value(
    const std::shared_ptr<TaskNode>& node) {
  if (!node) return nullptr;
  std::lock_guard<std::mutex> lock(node->mutex);
  return node->value;
}

void node_set_continuation(const std::shared_ptr<TaskNode>& node,
                           std::function<void()> continuation) {
  if (!node || !continuation) return;
  bool run_now = false;
  {
    std::lock_guard<std::mutex> lock(node->mutex);
    if (node->terminal.load(std::memory_order_acquire)) {
      run_now = true;
    } else if (!node->continuation_attached) {
      node->continuation = std::move(continuation);
      node->continuation_attached = true;
      return;
    } else {
      // At most one continuation is expected, but never silently drop one:
      // run the previous callback first, then the new one.
      auto previous = std::move(node->continuation);
      node->continuation = [previous = std::move(previous),
                            next = std::move(continuation)]() mutable {
        try {
          if (previous) previous();
        } catch (...) {
        }
        try {
          if (next) next();
        } catch (...) {
        }
      };
      return;
    }
  }
  if (run_now) schedule_continuation(node, std::move(continuation));
}

void node_complete(const std::shared_ptr<TaskNode>& node, Code code,
                   Assessment assessment, Usage usage,
                   std::vector<Id> dependencies, std::shared_ptr<void> value) {
  if (!node) return;
  std::function<void()> continuation;
  std::shared_ptr<ClaimImpl> claim;
  {
    std::lock_guard<std::mutex> lock(node->mutex);
    if (node->terminal.load(std::memory_order_acquire)) return;
    node->terminal.store(true, std::memory_order_release);

    // A successful Result must always carry a value (Done for Task<Done>).
    if (code == Code::ok && !value) code = Code::internal;

    if (code == Code::ok) {
      assessment.execution = ExecutionState::complete;
    } else if (code == Code::cancelled) {
      assessment.complete = false;
      assessment.execution = ExecutionState::cancelled;
    } else {
      assessment.complete = false;
      assessment.execution = ExecutionState::failed;
    }

    node->code = code;
    node->assessment = std::move(assessment);
    node->usage = std::move(usage);
    node->dependencies = std::move(dependencies);
    node->value = std::move(value);
    node->state.store(code == Code::ok
                          ? TaskState::success
                          : (code == Code::cancelled ? TaskState::cancelled
                                                     : TaskState::failed),
                      std::memory_order_release);
    continuation = std::move(node->continuation);
    node->continuation = nullptr;
    node->continuation_attached = false;
    claim = std::move(node->running_claim);
  }
  // Releasing the execution ticket must not happen under the node mutex: the
  // ledger takes its own locks and may notify parked tasks.
  claim.reset();
  node->cv.notify_all();
  if (continuation) schedule_continuation(node, std::move(continuation));
}

void node_complete_from(const std::shared_ptr<TaskNode>& dst,
                        const std::shared_ptr<TaskNode>& src) {
  if (!dst || !src) return;
  Code code;
  Assessment assessment;
  Usage usage;
  std::vector<Id> dependencies;
  std::shared_ptr<void> value;
  {
    std::lock_guard<std::mutex> lock(src->mutex);
    code = src->code;
    assessment = src->assessment;
    usage = src->usage;
    dependencies = src->dependencies;
    // A non-ok Result may still carry an API-allowed partial value (for
    // example nonconverged); settle/finalize must see it.  Only an ok Result
    // without any value is downgraded below.
    value = std::move(src->value);
  }
  if (code == Code::ok && !value) code = Code::internal;
  node_complete(dst, code, std::move(assessment), std::move(usage),
                std::move(dependencies), std::move(value));
}

void node_complete_exception(const std::shared_ptr<TaskNode>& node) {
  if (!node) return;
  node_complete(node, Code::internal,
                failure_assessment(Code::internal,
                                   "task body or continuation threw an exception"),
                Usage{}, {}, nullptr);
}

// ---------------------------------------------------------------------------
// Submission, waiting and control scheduling
// ---------------------------------------------------------------------------

void node_submit(InvocationContext ctx, const std::shared_ptr<TaskNode>& node,
                 TaskSpec spec, std::function<void(InvocationContext)> body) {
  if (!node) return;
  node->context = ctx;
  node->spec = spec;

  if (!ctx.services) {
    node_complete(node, Code::invalid_request,
                  failure_assessment(Code::invalid_request,
                                     "InvocationContext has no Services; the "
                                     "task was not opened by make_services"),
                  Usage{}, {}, nullptr);
    return;
  }
  if (spec.running.cpu_tokens == 0) {
    node_complete(node, Code::invalid_request,
                  failure_assessment(Code::invalid_request,
                                     "submit_cpu requires TaskSpec.running."
                                     "cpu_tokens >= 1"),
                  Usage{}, {}, nullptr);
    return;
  }
  // Runtime service/cleanup nodes ("service.*") must still run after a cancel:
  // FOUNDATION_API requires abort/unlink/close cleanup to drain even when the
  // scientific work has been cancelled.  They still go through resource
  // admission; they simply do not start new scientific work.
  const bool service_node = spec.semantic.rfind("service.", 0) == 0;
  if (!service_node && cancellation_requested(ctx)) {
    node_complete(node, Code::cancelled,
                  cancelled_assessment("cancel requested before submission"),
                  Usage{}, {}, nullptr);
    return;
  }
  if (!service_node && ctx.services->shutting_down()) {
    node_complete(node, Code::cancelled,
                  cancelled_assessment("the service pool is shutting down"),
                  Usage{}, {}, nullptr);
    return;
  }
  node->state.store(TaskState::waiting_resources, std::memory_order_release);
  ctx.services->submit(node, std::move(body));
}

void node_wait(const std::shared_ptr<TaskNode>& node) {
  if (!node) return;
  std::unique_lock<std::mutex> lock(node->mutex);
  node->cv.wait(lock,
                [&] { return node->terminal.load(std::memory_order_acquire); });
}

void node_schedule_control(InvocationContext ctx, std::function<void()> fn) {
  auto services = ctx.services;
  if (!services) {
    try {
      fn();
    } catch (...) {
    }
    return;
  }
  auto node = make_node(ctx, TaskSpec{});
  node->spec.semantic = Id("service.control");
  auto body = [node, fn = std::move(fn)](InvocationContext) mutable {
    try {
      fn();
    } catch (...) {
    }
    if (!node->terminal.load(std::memory_order_acquire)) {
      node_complete(node, Code::ok, Assessment{}, Usage{}, {},
                    box_value<Done>(Done{}));
    }
  };
  services->schedule_control(node, std::move(body));
}

}  // namespace detail
}  // namespace acsd::rt
