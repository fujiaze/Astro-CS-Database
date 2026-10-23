#pragma once
// Bounded typed tasks, completion dependency and external wait.
//
// A Task<T> is a non-blocking handle to one node of the executor graph.  The
// final payload of a task is Result<T>; there is no Task<Result<T>> nesting.
// Success with no value uses Done instead of Result<void>.  The declarations
// are fixed by docs/engineering/rebuild/standards/FOUNDATION_API.md.
#include <acsd/runtime/types.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace detail {
class TaskNode;

// Non-template node operations.  They are implementation details, not a
// second public task API: modules only use Task<T> and the combinators below.
std::shared_ptr<TaskNode> make_node(InvocationContext, TaskSpec);
TaskId node_id(const std::shared_ptr<TaskNode>&) noexcept;
TaskState node_state(const std::shared_ptr<TaskNode>&) noexcept;
bool node_finished(const std::shared_ptr<TaskNode>&) noexcept;
Code node_code(const std::shared_ptr<TaskNode>&) noexcept;
const Assessment& node_assessment(const std::shared_ptr<TaskNode>&);
const Usage& node_usage(const std::shared_ptr<TaskNode>&);
const std::vector<Id>& node_dependencies(const std::shared_ptr<TaskNode>&);
std::shared_ptr<void> node_take_value(const std::shared_ptr<TaskNode>&);
std::shared_ptr<const void> node_peek_value(const std::shared_ptr<TaskNode>&);
void node_set_continuation(const std::shared_ptr<TaskNode>&, std::function<void()>);
void node_complete(const std::shared_ptr<TaskNode>&, Code, Assessment, Usage,
                   std::vector<Id>, std::shared_ptr<void> value);
void node_complete_from(const std::shared_ptr<TaskNode>& dst,
                        const std::shared_ptr<TaskNode>& src);
void node_complete_exception(const std::shared_ptr<TaskNode>&);
void node_submit(InvocationContext, const std::shared_ptr<TaskNode>&,
                 TaskSpec, std::function<void(InvocationContext)>);
void node_wait(const std::shared_ptr<TaskNode>&);
void node_schedule_control(InvocationContext, std::function<void()>);
bool on_executor_thread() noexcept;

template <class T>
std::shared_ptr<void> box_value(T value) {
  return std::static_pointer_cast<void>(std::make_shared<T>(std::move(value)));
}
template <class T>
T take_value(const std::shared_ptr<TaskNode>& node) {
  auto box = node_take_value(node);
  if (!box) return T{};
  return std::move(*static_cast<T*>(box.get()));
}
template <class T>
const T& peek_value(const std::shared_ptr<TaskNode>& node) {
  auto box = node_peek_value(node);
  return *static_cast<const T*>(box.get());
}
template <class T>
Result<T> take_result(const std::shared_ptr<TaskNode>& node) {
  Result<T> r;
  r.code = node_code(node);
  r.assessment = node_assessment(node);
  r.usage = node_usage(node);
  r.dependencies = node_dependencies(node);
  // A non-ok scientific stop may still carry the API-allowed partial value;
  // only nodes without a payload stay valueless.
  if (node_peek_value(node)) r.value = take_value<T>(node);
  return r;
}
struct TaskAccess;
}  // namespace detail

template <class T>
class Task {
 public:
  using value_type = T;

  Task() noexcept = default;
  Task(Task&& other) noexcept = default;
  Task& operator=(Task&& other) noexcept = default;
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  ~Task() = default;

  bool valid() const noexcept { return node_ != nullptr; }
  TaskId id() const noexcept {
    return node_ ? detail::node_id(node_) : TaskId{0};
  }
  TaskState state() const noexcept {
    return node_ ? detail::node_state(node_) : TaskState::success;
  }

 private:
  explicit Task(std::shared_ptr<detail::TaskNode> node) noexcept
      : node_(std::move(node)) {}
  std::shared_ptr<detail::TaskNode> release_node() noexcept {
    return std::move(node_);
  }
  friend struct detail::TaskAccess;
  std::shared_ptr<detail::TaskNode> node_;
};

namespace detail {
struct TaskAccess {
  template <class T>
  static Task<T> wrap(std::shared_ptr<TaskNode> node) noexcept {
    return Task<T>{std::move(node)};
  }
  template <class T>
  static std::shared_ptr<TaskNode> release(Task<T>&& task) noexcept {
    return task.release_node();
  }
};
}  // namespace detail

template <class T>
Task<T> wrap_task(std::shared_ptr<detail::TaskNode> node) noexcept {
  return detail::TaskAccess::wrap<T>(std::move(node));
}

template <class T>
Task<T> ready(InvocationContext ctx, Result<T> result);

template <class T>
Task<T> submit_cpu(InvocationContext, TaskSpec,
                   std::function<Result<T>(InvocationContext)>);

template <class T>
Task<T> ready(InvocationContext ctx, Result<T> result) {
  auto node = detail::make_node(ctx, TaskSpec{});
  std::shared_ptr<void> box;
  if (result.value) box = detail::box_value<T>(std::move(*result.value));
  auto code = result.code;
  if (code == Code::ok && !box) code = Code::internal;
  detail::node_complete(node, code, std::move(result.assessment),
                        std::move(result.usage),
                        std::move(result.dependencies), std::move(box));
  return wrap_task<T>(std::move(node));
}

template <class T>
Task<T> submit_cpu(InvocationContext ctx, TaskSpec spec,
                   std::function<Result<T>(InvocationContext)> work) {
  auto node = detail::make_node(ctx, spec);
  detail::node_submit(ctx, node, spec,
                      [node, work = std::move(work)](InvocationContext task) {
                        Result<T> r = work(task);
                        std::shared_ptr<void> box;
                        if (r.value) box = detail::box_value<T>(std::move(*r.value));
                        auto code = r.code;
                        if (code == Code::ok && !box) code = Code::internal;
                        detail::node_complete(node, code,
                                              std::move(r.assessment),
                                              std::move(r.usage),
                                              std::move(r.dependencies),
                                              std::move(box));
                      });
  return wrap_task<T>(std::move(node));
}

template <class T, class F>
auto then(InvocationContext ctx, Task<T>&& source, F f)
    -> std::invoke_result_t<F, T> {
  using Child = std::invoke_result_t<F, T>;
  using U = typename Child::value_type;
  static_assert(std::is_same_v<Child, Task<U>>,
                "then: the continuation must return a Task<U>");
  auto out = detail::make_node(ctx, TaskSpec{});
  auto src = detail::TaskAccess::release<T>(std::move(source));
  if (!src) {
    detail::node_complete(out, Code::invalid_request, Assessment{},
                          Usage{}, {}, nullptr);
    return wrap_task<U>(std::move(out));
  }
  detail::node_set_continuation(
      src, [ctx, src, out, f = std::move(f)]() mutable {
        if (detail::node_code(src) != Code::ok) {
          // The source payload has type T and the continuation result has type
          // U; only the Result metadata crosses this boundary.  A partial
          // value is preserved by settle(), not by then().
          detail::node_complete(out, detail::node_code(src),
                                detail::node_assessment(src),
                                detail::node_usage(src),
                                detail::node_dependencies(src), nullptr);
          return;
        }
        try {
          Child child = std::invoke(std::move(f), detail::take_value<T>(src));
          auto child_node = detail::TaskAccess::release<U>(std::move(child));
          if (!child_node) {
            detail::node_complete(out, Code::invalid_request, Assessment{},
                                  Usage{}, {}, nullptr);
            return;
          }
          detail::node_set_continuation(child_node, [child_node, out]() {
            detail::node_complete_from(out, child_node);
          });
        } catch (...) {
          detail::node_complete_exception(out);
        }
      });
  return wrap_task<U>(std::move(out));
}

template <class T, class F>
auto settle(InvocationContext ctx, Task<T>&& source, F f)
    -> std::invoke_result_t<F, Result<T>> {
  using Child = std::invoke_result_t<F, Result<T>>;
  using U = typename Child::value_type;
  static_assert(std::is_same_v<Child, Task<U>>,
                "settle: the continuation must return a Task<U>");
  auto out = detail::make_node(ctx, TaskSpec{});
  auto src = detail::TaskAccess::release<T>(std::move(source));
  if (!src) {
    detail::node_complete(out, Code::invalid_request, Assessment{},
                          Usage{}, {}, nullptr);
    return wrap_task<U>(std::move(out));
  }
  detail::node_set_continuation(
      src, [ctx, src, out, f = std::move(f)]() mutable {
        try {
          Child child =
              std::invoke(std::move(f), detail::take_result<T>(src));
          auto child_node = detail::TaskAccess::release<U>(std::move(child));
          if (!child_node) {
            detail::node_complete(out, Code::invalid_request, Assessment{},
                                  Usage{}, {}, nullptr);
            return;
          }
          detail::node_set_continuation(
              child_node, [child_node, out]() {
                detail::node_complete_from(out, child_node);
              });
        } catch (...) {
          detail::node_complete_exception(out);
        }
      });
  return wrap_task<U>(std::move(out));
}

template <class T>
Task<std::vector<T>> when_all(InvocationContext ctx,
                              std::vector<Task<T>> tasks) {
  struct State {
    std::atomic<std::size_t> remaining{0};
    std::vector<std::optional<T>> values;
    std::vector<std::optional<std::pair<Code, Assessment>>> failures;
    std::shared_ptr<detail::TaskNode> out;
    InvocationContext ctx;
  };
  auto state = std::make_shared<State>();
  state->out = detail::make_node(ctx, TaskSpec{});
  state->ctx = ctx;
  state->remaining.store(tasks.size());
  state->values.resize(tasks.size());
  state->failures.resize(tasks.size());
  if (tasks.empty()) {
    detail::node_complete(state->out, Code::ok, Assessment{}, Usage{}, {},
                          detail::box_value(std::vector<T>{}));
    auto out = state->out;
    return wrap_task<std::vector<T>>(std::move(out));
  }
  for (std::size_t i = 0; i < tasks.size(); ++i) {
    auto node = detail::TaskAccess::release<T>(std::move(tasks[i]));
    if (!node) {
      if (state->remaining.fetch_sub(1) == 1) {
        detail::node_complete(state->out, Code::invalid_request, Assessment{},
                              Usage{}, {}, nullptr);
      }
      continue;
    }
    detail::node_set_continuation(node, [state, node, i]() {
      if (detail::node_code(node) == Code::ok &&
          detail::node_peek_value(node)) {
        state->values[i] = detail::take_value<T>(node);
      } else {
        state->failures[i] = std::make_pair(
            detail::node_code(node), detail::node_assessment(node));
      }
      if (state->remaining.fetch_sub(1) != 1) return;
      for (auto& failure : state->failures) {
        if (!failure) continue;
        auto [code, assessment] = *failure;
        detail::node_complete(state->out, code, std::move(assessment),
                              Usage{}, {}, nullptr);
        return;
      }
      std::vector<T> values;
      values.reserve(state->values.size());
      for (auto& v : state->values) values.push_back(std::move(*v));
      detail::node_complete(state->out, Code::ok, Assessment{}, Usage{}, {},
                            detail::box_value(std::move(values)));
    });
  }
  auto out = state->out;
  return wrap_task<std::vector<T>>(std::move(out));
}

template <class T>
Task<std::vector<Result<T>>> settle_all(InvocationContext ctx,
                                        std::vector<Task<T>> tasks) {
  struct State {
    std::atomic<std::size_t> remaining{0};
    std::vector<Result<T>> results;
    std::shared_ptr<detail::TaskNode> out;
  };
  auto state = std::make_shared<State>();
  state->out = detail::make_node(ctx, TaskSpec{});
  state->remaining.store(tasks.size());
  state->results.resize(tasks.size());
  if (tasks.empty()) {
    detail::node_complete(
        state->out, Code::ok, Assessment{}, Usage{}, {},
        detail::box_value(std::vector<Result<T>>{}));
    auto out = state->out;
    return wrap_task<std::vector<Result<T>>>(std::move(out));
  }
  for (std::size_t i = 0; i < tasks.size(); ++i) {
    auto node = detail::TaskAccess::release<T>(std::move(tasks[i]));
    if (!node) {
      state->results[i].code = Code::invalid_request;
      if (state->remaining.fetch_sub(1) == 1) {
        detail::node_complete(
            state->out, Code::ok, Assessment{}, Usage{}, {},
            detail::box_value(std::move(state->results)));
      }
      continue;
    }
    detail::node_set_continuation(node, [state, node, i]() {
      state->results[i] = detail::take_result<T>(node);
      if (state->remaining.fetch_sub(1) != 1) return;
      detail::node_complete(state->out, Code::ok, Assessment{}, Usage{}, {},
                            detail::box_value(std::move(state->results)));
    });
  }
  auto out = state->out;
  return wrap_task<std::vector<Result<T>>>(std::move(out));
}

template <class T>
Result<T> wait_external(Task<T>&& task) {
  Result<T> r;
  if (detail::on_executor_thread()) {
    // wait_external is an external entry point only; a CPU/I/O worker must not
    // block the pool it is running on.
    r.code = Code::invalid_request;
    return r;
  }
  if (!task.valid()) {
    r.code = Code::invalid_request;
    return r;
  }
  auto node = detail::TaskAccess::release<T>(std::move(task));
  detail::node_wait(node);
  return detail::take_result<T>(node);
}

bool cancellation_requested(const InvocationContext&) noexcept;
void request_cancel(const InvocationContext&) noexcept;

}  // namespace acsd::rt
