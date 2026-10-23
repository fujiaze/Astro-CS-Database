// Minimal executor stubs for the standalone registry test.
//
// RegistryBuilder::add<In,Out> instantiates the transport thunk, which mentions
// rt::then/rt::ready even when a negative registry case never runs it.  The
// production ctest links the real runtime library(s) instead of this file; it
// exists so the registry contract can be compiled and exercised independently
// of the executor during parallel M0/M2 development.
//
// If this file is linked together with a real task.cpp, drop it from the link
// line: the stubs must never satisfy a real execution.
#include <acsd/runtime/task.hpp>

#include <memory>
#include <string>
#include <vector>

namespace acsd::rt::detail {

std::shared_ptr<TaskNode> make_node(InvocationContext, TaskSpec) {
  return nullptr;
}
TaskId node_id(const std::shared_ptr<TaskNode>&) noexcept { return 0; }
TaskState node_state(const std::shared_ptr<TaskNode>&) noexcept {
  return TaskState::failed;
}
bool node_finished(const std::shared_ptr<TaskNode>&) noexcept { return true; }
Code node_code(const std::shared_ptr<TaskNode>&) noexcept {
  return Code::internal;
}
const Assessment& node_assessment(const std::shared_ptr<TaskNode>&) {
  static const Assessment assessment{};
  return assessment;
}
const Usage& node_usage(const std::shared_ptr<TaskNode>&) {
  static const Usage usage{};
  return usage;
}
const std::vector<Id>& node_dependencies(const std::shared_ptr<TaskNode>&) {
  static const std::vector<Id> dependencies;
  return dependencies;
}
std::shared_ptr<void> node_take_value(const std::shared_ptr<TaskNode>&) {
  return nullptr;
}
std::shared_ptr<const void> node_peek_value(
    const std::shared_ptr<TaskNode>&) {
  return nullptr;
}
void node_set_continuation(const std::shared_ptr<TaskNode>&,
                           std::function<void()>) {}
void node_complete(const std::shared_ptr<TaskNode>&, Code, Assessment, Usage,
                   std::vector<Id>, std::shared_ptr<void>) {}
void node_complete_from(const std::shared_ptr<TaskNode>&,
                        const std::shared_ptr<TaskNode>&) {}
void node_complete_exception(const std::shared_ptr<TaskNode>&) {}
void node_submit(InvocationContext, const std::shared_ptr<TaskNode>&, TaskSpec,
                 std::function<void(InvocationContext)>) {}
void node_wait(const std::shared_ptr<TaskNode>&) {}
void node_schedule_control(InvocationContext, std::function<void()>) {}
bool on_executor_thread() noexcept { return false; }

}  // namespace acsd::rt::detail

namespace acsd::rt {

Result<ScopeRef> child_scope(InvocationContext, Id, ScopeLimits) {
  Result<ScopeRef> out;
  out.code = Code::unsupported;
  return out;
}
Result<ArenaRef> reserve(InvocationContext, ResourceVector) {
  Result<ArenaRef> out;
  out.code = Code::unsupported;
  return out;
}
Result<Done> consume_work(InvocationContext, U64) {
  Result<Done> out;
  out.code = Code::unsupported;
  return out;
}
ScopeSnapshot snapshot(const ScopeRef&) noexcept { return ScopeSnapshot{}; }
bool cancellation_requested(const InvocationContext&) noexcept { return false; }
void request_cancel(const InvocationContext&) noexcept {}

}  // namespace acsd::rt
