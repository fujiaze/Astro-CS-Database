// S-MULTI: two continuously ready commands with weight 1 and 2 receive
// dispatch credits in a 1:2 ratio; neither pool nor command concurrency
// ceilings are exceeded, and neither command starves.
//
// The runtime exposes PoolConfig.dispatch_observer: the pool calls it BEFORE a
// dispatch decision with its own snapshot {sequence, command, ready, running,
// ceiling}.  This case consumes that hook directly; it does not infer
// readiness from callable entry (P=2 wall-clock entry order is not credit
// order, and a callable-entry snapshot can see a deferred item as ready).
//
// Criterion (sliding windows, resource feasibility, 1:2 unchanged):
//   sort events by sequence; for every i where the three events i,i+1,i+2 are
//   all feasible for BOTH commands (weight1 ready>=1 and running<ceiling,
//   weight2 ready>=2 and running<ceiling, using the latest snapshot at/before
//   each event), require commands[i..i+2] multiset == {w1,w2,w2}.
//   feasible windows >= 100, violations = 0, 5/5 quiet runs.
// A command that is not ready or is at its ceiling is not "participating", so
// such windows are skipped and counted.
#include <acsd/contracts.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;
using acsd::U32;
using acsd::U64;

constexpr int kThreads = 2;
constexpr U32 kCommandCpuCeiling = 2;
constexpr int kWindow = 6;
constexpr int kReadyWindow = 4;
constexpr int kQuotaOne = 2;
constexpr int kQuotaTwo = 4;
constexpr int kDispatchesPerRun = 800;
constexpr int kMinWindows = 100;
constexpr int kRuns = 5;
constexpr int kUnitsPerCommand = 2000;

struct CommandState {
  std::atomic<int> submitted{0};
  std::atomic<int> started{0};
  std::atomic<int> completed{0};
  std::atomic<int> running{0};
  std::atomic<int> max_running{0};
};

struct PoolState {
  std::atomic<int> running{0};
  std::atomic<int> max_running{0};
  std::atomic<int> in_flight{0};
};

struct EventLog {
  std::mutex mutex;
  std::vector<rt::DispatchEvent> events;
  std::atomic<int> count{0};
  void clear() {
    std::lock_guard<std::mutex> lock(mutex);
    events.clear();
    count.store(0);
  }
  void add(const rt::DispatchEvent& event) {
    std::lock_guard<std::mutex> lock(mutex);
    events.push_back(event);
    count.fetch_add(1);
  }
  int size() const { return count.load(); }
  std::vector<rt::DispatchEvent> snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return events;
  }
};

struct RunResult {
  int dispatches = 0;
  int windows = 0;
  int violations = 0;
  int skipped = 0;
  int window_rejections = 0;
  int max_running_one = 0;
  int max_running_two = 0;
  bool all_units_ok = true;
  double ratio = 0.0;
  std::string violation_detail;
  std::string sample;
};

void note_concurrency(std::atomic<int>& counter, std::atomic<int>& peak) {
  const int now = counter.fetch_add(1) + 1;
  int previous = peak.load();
  while (now > previous && !peak.compare_exchange_weak(previous, now)) {
  }
}

rt::Task<rt::Done> submit_unit(const rt::InvocationContext& ctx, U64 ordinal,
                               CommandState& state, PoolState& pool) {
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, ordinal};
  spec.running.cpu_tokens = 1;
  spec.work_upper = 1;
  spec.semantic = "validation.multi.unit";
  state.submitted.fetch_add(1);
  pool.in_flight.fetch_add(1);
  return rt::submit_cpu<rt::Done>(
      ctx, spec,
      [&state, &pool](rt::InvocationContext task) -> Result<rt::Done> {
        state.started.fetch_add(1);
        (void)rt::consume_work(task, 1);
        note_concurrency(state.running, state.max_running);
        note_concurrency(pool.running, pool.max_running);
        const auto until =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
        while (std::chrono::steady_clock::now() < until) {
        }
        state.running.fetch_sub(1);
        pool.running.fetch_sub(1);
        pool.in_flight.fetch_sub(1);
        state.completed.fetch_add(1);
        Result<rt::Done> out;
        out.code = Code::ok;
        out.value = rt::Done{};
        return out;
      });
}

U32 ceiling_of(const rt::DispatchEvent& event) {
  return event.ceiling == 0 ? static_cast<U32>(kThreads) : event.ceiling;
}

int command_number(const rt::DispatchEvent& event, const std::string& one,
                   const std::string& two) {
  if (event.command == one) return 1;
  if (event.command == two) return 2;
  return 0;
}

RunResult analyse(std::vector<rt::DispatchEvent> events, const std::string& one,
                  const std::string& two) {
  RunResult result;
  std::sort(events.begin(), events.end(),
            [](const rt::DispatchEvent& a, const rt::DispatchEvent& b) {
              return a.sequence < b.sequence;
            });
  std::vector<rt::DispatchEvent> selected;
  selected.reserve(static_cast<std::size_t>(kDispatchesPerRun));
  for (const auto& event : events) {
    if (event.control) continue;
    if (command_number(event, one, two) == 0) continue;
    selected.push_back(event);
    if (static_cast<int>(selected.size()) >= kDispatchesPerRun) break;
  }
  const int count = static_cast<int>(selected.size());
  result.dispatches = count;
  // Last snapshot of each command at or before each event index.
  std::vector<int> last_one(static_cast<std::size_t>(count), -1);
  std::vector<int> last_two(static_cast<std::size_t>(count), -1);
  int one_index = -1;
  int two_index = -1;
  for (int i = 0; i < count; ++i) {
    const int number = command_number(selected[static_cast<std::size_t>(i)], one, two);
    if (number == 1) one_index = i;
    if (number == 2) two_index = i;
    last_one[static_cast<std::size_t>(i)] = one_index;
    last_two[static_cast<std::size_t>(i)] = two_index;
  }
  auto feasible = [&](int command, int at) {
    const int index = command == 1 ? last_one[static_cast<std::size_t>(at)]
                                   : last_two[static_cast<std::size_t>(at)];
    if (index < 0) return false;
    const auto& event = selected[static_cast<std::size_t>(index)];
    const U32 need = command == 1 ? 1u : 2u;
    return event.ready >= need && event.running < ceiling_of(event);
  };
  int count_one = 0;
  int count_two = 0;
  for (int i = 0; i + 2 < count; ++i) {
    bool ok = true;
    for (int j = i; j <= i + 2 && ok; ++j)
      ok = feasible(1, j) && feasible(2, j);
    if (!ok) {
      ++result.skipped;
      continue;
    }
    ++result.windows;
    const int a = command_number(selected[static_cast<std::size_t>(i)], one, two);
    const int b = command_number(selected[static_cast<std::size_t>(i + 1)], one, two);
    const int c = command_number(selected[static_cast<std::size_t>(i + 2)], one, two);
    const int ones = (a == 1) + (b == 1) + (c == 1);
    const int twos = (a == 2) + (b == 2) + (c == 2);
    if (ones != 1 || twos != 2) {
      ++result.violations;
      if (result.violation_detail.size() < 300) {
        const auto& e = selected[static_cast<std::size_t>(i)];
        const auto& f = selected[static_cast<std::size_t>(i + 2)];
        result.violation_detail += "seq " + std::to_string(e.sequence) + "-" +
                                   std::to_string(f.sequence) + ": " +
                                   std::to_string(a) + std::to_string(b) +
                                   std::to_string(c) + "(ready " +
                                   std::to_string(e.ready) + ", run " +
                                   std::to_string(e.running) + ", ceil " +
                                   std::to_string(ceiling_of(e)) + ");";
      }
    }
    count_one += ones;
    count_two += twos;
  }
  result.ratio = count_one > 0 ? static_cast<double>(count_two) / count_one : 0.0;
  for (int i = 0; i < count && i < 36; ++i) {
    if (i) result.sample += ",";
    result.sample += std::to_string(command_number(selected[static_cast<std::size_t>(i)],
                                                   one, two));
  }
  for (const auto& event : selected) {
    const int number = command_number(event, one, two);
    const int running = static_cast<int>(event.running) + 1;
    if (number == 1) result.max_running_one = std::max(result.max_running_one, running);
    if (number == 2) result.max_running_two = std::max(result.max_running_two, running);
  }
  return result;
}

RunResult run_once(std::shared_ptr<rt::Services> services, EventLog& log, int run,
                   vrt::Recorder& recorder) {
  RunResult result;
  log.clear();
  const std::string one = "validation.multi.run" + std::to_string(run) + ".w1";
  const std::string two = "validation.multi.run" + std::to_string(run) + ".w2";
  auto command_one = rt::open_command(services, one, vrt::limits(64ull << 20, 1000000,
                                                                4096, kCommandCpuCeiling, 2), 1);
  auto command_two = rt::open_command(services, two, vrt::limits(64ull << 20, 1000000,
                                                                4096, kCommandCpuCeiling, 2), 2);
  recorder.check("multi.run" + std::to_string(run) + "_open_ok",
                 command_one.code == Code::ok && command_two.code == Code::ok, "ok",
                 vrt::code_of(command_one) + "/" + vrt::code_of(command_two));
  if (command_one.code != Code::ok || command_two.code != Code::ok) return result;

  CommandState state_one;
  CommandState state_two;
  PoolState pool;
  std::vector<rt::Task<rt::Done>> tasks_one;
  std::vector<rt::Task<rt::Done>> tasks_two;
  U64 ordinal = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (static_cast<int>(log.snapshot().size()) < kDispatchesPerRun + 64 &&
         std::chrono::steady_clock::now() < deadline) {
    const int queued = pool.in_flight.load() - pool.running.load();
    if (queued < kReadyWindow && pool.in_flight.load() < kWindow &&
        state_one.submitted.load() - state_one.started.load() < kQuotaOne &&
        state_one.submitted.load() < kUnitsPerCommand) {
      tasks_one.push_back(submit_unit(*command_one.value, ordinal++, state_one, pool));
    }
    if (pool.in_flight.load() - pool.running.load() < kReadyWindow &&
        pool.in_flight.load() < kWindow &&
        state_two.submitted.load() - state_two.started.load() < kQuotaTwo &&
        state_two.submitted.load() < kUnitsPerCommand) {
      tasks_two.push_back(submit_unit(*command_two.value, ordinal++, state_two, pool));
    }
    std::this_thread::sleep_for(std::chrono::microseconds(20));
  }
  for (auto& task : tasks_one) {
    auto r = rt::wait_external(std::move(task));
    if (r.code == Code::resource_exhausted) ++result.window_rejections;
    result.all_units_ok = result.all_units_ok && (r.code == Code::ok || r.code == Code::cancelled);
  }
  for (auto& task : tasks_two) {
    auto r = rt::wait_external(std::move(task));
    if (r.code == Code::resource_exhausted) ++result.window_rejections;
    result.all_units_ok = result.all_units_ok && (r.code == Code::ok || r.code == Code::cancelled);
  }
  if (log.size() == 0) {
    recorder.check("multi.run" + std::to_string(run) + "_observer_delivered", false,
                   "PoolConfig.dispatch_observer delivered events", "0 events",
                   "the hook is declared in the header but never invoked by the pool");
    return result;
  }
  const RunResult analysed = analyse(log.snapshot(), one, two);
  result.dispatches = analysed.dispatches;
  result.windows = analysed.windows;
  result.violations = analysed.violations;
  result.skipped = analysed.skipped;
  result.ratio = analysed.ratio;
  result.violation_detail = analysed.violation_detail;
  result.sample = analysed.sample;
  result.max_running_one = analysed.max_running_one;
  result.max_running_two = analysed.max_running_two;
  auto closed_one = rt::wait_external(rt::close_command(*command_one.value));
  auto closed_two = rt::wait_external(rt::close_command(*command_two.value));
  recorder.check("multi.run" + std::to_string(run) + "_close_ok",
                 closed_one.code == Code::ok && closed_two.code == Code::ok, "ok",
                 vrt::code_of(closed_one) + "/" + vrt::code_of(closed_two));
  return result;
}

}  // namespace

int main() {
  vrt::Recorder recorder("multi01", "validation/runtime/multi01/multi01.cpp");
  EventLog log;

  auto services = vrt::start_services(
      512ull << 20, 2000000, kThreads, 2, 2, vrt::scratch_root("multi01"), 0,
      [&log](const rt::DispatchEvent& event) { log.add(event); });
  recorder.expect_code("multi.services_ok", "ok", services);
  if (services.code != Code::ok || !services.value) return recorder.finish();

  bool all_runs_pass = true;
  for (int run = 0; run < kRuns; ++run) {
    const RunResult result = run_once(*services.value, log, run, recorder);
    const std::string prefix = "multi.run" + std::to_string(run);
    recorder.value(prefix + "_dispatches", static_cast<long long>(result.dispatches));
    recorder.value(prefix + "_complete_windows", static_cast<long long>(result.windows));
    recorder.value(prefix + "_violations", static_cast<long long>(result.violations));
    recorder.value(prefix + "_skipped_windows", static_cast<long long>(result.skipped));
    recorder.value(prefix + "_ratio_info", result.ratio);
    recorder.value(prefix + "_sample", result.sample);
    recorder.value(prefix + "_violation_detail", result.violation_detail);
    recorder.check(prefix + "_dispatches", result.dispatches >= kDispatchesPerRun,
                   ">= 12000 dispatch events", std::to_string(result.dispatches));
    recorder.check(prefix + "_complete_windows", result.windows >= kMinWindows,
                   ">= 100 feasible sliding windows", std::to_string(result.windows));
    recorder.check(prefix + "_violations_zero", result.violations == 0,
                   "0 windows whose multiset != {w1,w2,w2}",
                   std::to_string(result.violations) + " violations");
    recorder.check(prefix + "_window_ok", result.window_rejections == 0,
                   "no resource_exhausted from the bounded window",
                   std::to_string(result.window_rejections) + " rejections");
    recorder.check(prefix + "_command_concurrency",
                   result.max_running_one <= static_cast<int>(kCommandCpuCeiling) &&
                       result.max_running_two <= static_cast<int>(kCommandCpuCeiling),
                   "each command <= ceiling.cpu_tokens=2",
                   std::to_string(result.max_running_one) + "/" +
                       std::to_string(result.max_running_two));
    recorder.check(prefix + "_units_ok", result.all_units_ok,
                   "all submitted units complete", result.all_units_ok ? "ok" : "failed");
    all_runs_pass = all_runs_pass && result.violations == 0 &&
                    result.windows >= kMinWindows && result.window_rejections == 0 &&
                    result.all_units_ok &&
                    result.max_running_one <= static_cast<int>(kCommandCpuCeiling) &&
                    result.max_running_two <= static_cast<int>(kCommandCpuCeiling);
  }
  recorder.check("multi.stability_5_of_5", all_runs_pass,
                 "5/5 quiet runs: feasible windows>=100, violations=0", 
                 all_runs_pass ? "5/5" : "at least one run failed");
  recorder.note("dispatch events come from PoolConfig.dispatch_observer (pool's own "
                "pre-dispatch snapshot), sorted by sequence; sliding windows are checked "
                "only when both commands are ready and below ceiling for all three events.");
  return recorder.finish();
}
