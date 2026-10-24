// S-MULTI: two continuously ready commands with weight 1 and 2 receive
// dispatch credits in a 1:2 ratio; neither pool nor command concurrency
// ceilings are exceeded, and neither command starves.
//
// Normative reading (SCHEDULER.md "多command分配与公平性"):
//   * "只有ready且资源可行的command参与" -> the executor is work-conserving and
//     must not idle-wait for a weighted command without a ready item.
//   * "每轮未用credit不跨轮累积" -> a command that cannot supply its full weight
//     loses the unused credit.
//   * "每完整可行轮dispatch比例1:2" -> the ratio is owed per complete feasible
//     round: a round of three dispatches whose start has weight1 ready>=1 and
//     weight2 ready>=2.  Every such round must dispatch exactly {w1,w2,w2}.
//   * P=1 has a 2P=2 ready window, too small to hold 1+2 ready items, so a P=1
//     fixture cannot form a complete feasible round and only measures "who is
//     ready".  This case uses P=2 (ready window 2P=4), keeps per-command ready
//     quotas (weight1>=1, weight2>=2) and a rolling in-flight window of 12
//     (contract: ready 2P + deferred 8P = 20).
//
// Dispatch credits are measured at the unit callable entry (dispatch start),
// not at completion: with P>=2 completions lag dispatches and a completion
// count is not a credit count.  Readiness before each dispatch is derived from
// the per-command submitted/started/completed counters.  Five consecutive quiet
// runs are required (5/5); every run contributes its own attempts, and a run
// with one violating round stays a failed attempt.
#include <acsd/contracts.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
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

constexpr int kThreads = 2;             // P=2 -> ready window 2P=4
constexpr U32 kCommandCpuCeiling = 2;   // per-command ceiling.cpu_tokens
constexpr int kWindow = 6;              // 2 running + up to 4 queued (ready window 2P=4)
constexpr int kReadyWindow = 4;         // never exceed the admitted ready window
constexpr int kDispatchesPerRun = 12000; // >=100 complete rounds at the small in-flight window
constexpr int kMinCompleteRounds = 100;
constexpr int kRuns = 5;
constexpr int kUnitsPerCommand = 20000;
constexpr int kQuotaOne = 2;
constexpr int kQuotaTwo = 4;
constexpr int kMaxDispatch = 16384;

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
  std::atomic<int> max_in_flight{0};
};

struct DispatchLog {
  std::atomic<int> commands[kMaxDispatch];
  std::atomic<int> ready_one[kMaxDispatch];
  std::atomic<int> ready_two[kMaxDispatch];
  std::atomic<int> running_one[kMaxDispatch];
  std::atomic<int> running_two[kMaxDispatch];
  std::atomic<U64> sequence[kMaxDispatch];
  std::atomic<int> next{0};
  DispatchLog() {
    for (int i = 0; i < kMaxDispatch; ++i) {
      commands[i].store(0);
      ready_one[i].store(0);
      ready_two[i].store(0);
      running_one[i].store(0);
      running_two[i].store(0);
      sequence[i].store(0);
    }
  }
};

struct RunResult {
  int dispatches = 0;
  int complete_rounds = 0;
  int violations = 0;
  int skipped = 0;
  int ones_ok = 0;
  int twos_ok = 0;
  int both_ok = 0;
  std::string snapshot_prefix;
  std::string round_prefix;
  int total_one = 0;
  int total_two = 0;
  std::string violation_detail;
  int window_rejections = 0;
  int max_in_flight = 0;
  int max_running_one = 0;
  int max_running_two = 0;
  bool all_units_ok = true;
  double ratio = 0.0;
};

void note_concurrency(std::atomic<int>& counter, std::atomic<int>& peak) {
  const int now = counter.fetch_add(1) + 1;
  int previous = peak.load();
  while (now > previous && !peak.compare_exchange_weak(previous, now)) {
  }
}

rt::Task<rt::Done> submit_unit(const rt::InvocationContext& ctx, U64 ordinal,
                               int command_id, CommandState& own, CommandState& other,
                               PoolState& pool, DispatchLog& log) {
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, ordinal};
  spec.running.cpu_tokens = 1;
  spec.work_upper = 1;
  spec.semantic = "validation.multi.unit";
  own.submitted.fetch_add(1);
  pool.in_flight.fetch_add(1);
  int previous = pool.max_in_flight.load();
  int now = pool.in_flight.load();
  while (now > previous && !pool.max_in_flight.compare_exchange_weak(previous, now)) {
  }
  return rt::submit_cpu<rt::Done>(
      ctx, spec,
      [&own, &other, &pool, &log, command_id](rt::InvocationContext task) -> Result<rt::Done> {
        // Dispatch start: count the credit and snapshot the queued readiness.
        // The rolling window is small enough (<=5 in flight, ready window 4)
        // that submitted-but-not-started units are exactly the ready queue;
        // deferred admission does not arise in steady state.
        own.started.fetch_add(1);
        const int index = log.next.fetch_add(1);
        const int own_queued = own.submitted.load() - own.started.load();
        const int other_queued = other.submitted.load() - other.started.load();
        const int own_running = own.running.load();
        const int other_running = other.running.load();
        if (index < kMaxDispatch) {
          log.sequence[index].store(task.dispatch_sequence);
          log.commands[index].store(command_id);
          log.ready_one[index].store(command_id == 1 ? own_queued + 1 : other_queued);
          log.ready_two[index].store(command_id == 2 ? own_queued + 1 : other_queued);
          log.running_one[index].store(command_id == 1 ? own_running : other_running);
          log.running_two[index].store(command_id == 2 ? own_running : other_running);
        }
        (void)rt::consume_work(task, 1);
        note_concurrency(own.running, own.max_running);
        note_concurrency(pool.running, pool.max_running);
        const auto until =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
        while (std::chrono::steady_clock::now() < until) {
        }
        own.running.fetch_sub(1);
        pool.running.fetch_sub(1);
        pool.in_flight.fetch_sub(1);
        own.completed.fetch_add(1);
        Result<rt::Done> out;
        out.code = Code::ok;
        out.value = rt::Done{};
        return out;
      });
}

int count_state(const std::vector<rt::Task<rt::Done>>& tasks, rt::TaskState wanted) {
  int count = 0;
  for (const auto& task : tasks)
    if (task.valid() && task.state() == wanted) ++count;
  return count;
}

bool feasible_at(const std::vector<int>& ready_one, const std::vector<int>& ready_two,
                  const std::vector<int>& running_one, const std::vector<int>& running_two,
                  int i) {
  // SCHEDULER: "只有 ready 且资源可行的 command 参与".
  return ready_one[static_cast<std::size_t>(i)] >= 1 &&
         running_one[static_cast<std::size_t>(i)] < static_cast<int>(kCommandCpuCeiling) &&
         ready_two[static_cast<std::size_t>(i)] >= 2 &&
         running_two[static_cast<std::size_t>(i)] < static_cast<int>(kCommandCpuCeiling);
}

RunResult analyse(const DispatchLog& log) {
  RunResult result;
  // P>1: order by the pool-assigned dispatch_sequence, not callable-entry time.
  std::vector<int> order;
  order.reserve(kMaxDispatch);
  for (int i = 0; i < kMaxDispatch; ++i) {
    if (log.sequence[i].load() != 0 && log.commands[i].load() != 0) order.push_back(i);
  }
  std::sort(order.begin(), order.end(), [&log](int a, int b) {
    return log.sequence[a].load() < log.sequence[b].load();
  });
  const int count = std::min(static_cast<int>(order.size()), kDispatchesPerRun);
  result.dispatches = count;
  std::vector<int> command;
  std::vector<int> ready_one;
  std::vector<int> ready_two;
  std::vector<int> running_one;
  std::vector<int> running_two;
  std::vector<U64> sequence;
  for (int i = 0; i < count; ++i) {
    const int index = order[static_cast<std::size_t>(i)];
    command.push_back(log.commands[index].load());
    ready_one.push_back(log.ready_one[index].load());
    ready_two.push_back(log.ready_two[index].load());
    running_one.push_back(log.running_one[index].load());
    running_two.push_back(log.running_two[index].load());
    sequence.push_back(log.sequence[index].load());
  }
  // Sliding windows, not fixed three-element groups: a skipped group must not
  // shift the phase of later groups.  A window is checked only when both
  // commands are ready and below their own ceiling at all three dispatch
  // starts, so no legitimate sequence can be cut into a false 222.
  int count_one = 0;
  int count_two = 0;
  for (int i = 0; i + 2 < count; ++i) {
    if (!feasible_at(ready_one, ready_two, running_one, running_two, i) ||
        !feasible_at(ready_one, ready_two, running_one, running_two, i + 1) ||
        !feasible_at(ready_one, ready_two, running_one, running_two, i + 2)) {
      ++result.skipped;
      continue;
    }
    ++result.complete_rounds;  // complete feasible windows
    const int a = command[static_cast<std::size_t>(i)];
    const int b = command[static_cast<std::size_t>(i + 1)];
    const int c = command[static_cast<std::size_t>(i + 2)];
    const int ones = (a == 1) + (b == 1) + (c == 1);
    const int twos = (a == 2) + (b == 2) + (c == 2);
    if (ones != 1 || twos != 2) {
      ++result.violations;
      if (result.violation_detail.size() < 300)
        result.violation_detail +=
            "seq " + std::to_string(sequence[static_cast<std::size_t>(i)]) + "-" +
            std::to_string(sequence[static_cast<std::size_t>(i + 2)]) + ": " +
            std::to_string(a) + std::to_string(b) + std::to_string(c) +
            "(ready " + std::to_string(ready_one[static_cast<std::size_t>(i)]) + "/" +
            std::to_string(ready_two[static_cast<std::size_t>(i)]) + ", run " +
            std::to_string(running_one[static_cast<std::size_t>(i)]) + "/" +
            std::to_string(running_two[static_cast<std::size_t>(i)]) + " @" +
            std::to_string(running_one[static_cast<std::size_t>(i + 1)]) + "/" +
            std::to_string(running_two[static_cast<std::size_t>(i + 1)]) + "," +
            std::to_string(running_one[static_cast<std::size_t>(i + 2)]) + "/" +
            std::to_string(running_two[static_cast<std::size_t>(i + 2)]) + ", ceil " +
            std::to_string(kCommandCpuCeiling) + "/" +
            std::to_string(kCommandCpuCeiling) + ");";
    }
    count_one += ones;
    count_two += twos;
  }
  result.ratio = count_one > 0 ? static_cast<double>(count_two) / count_one : 0.0;
  for (int i = 0; i < count; ++i) {
    result.total_one += command[static_cast<std::size_t>(i)] == 1 ? 1 : 0;
    result.total_two += command[static_cast<std::size_t>(i)] == 2 ? 1 : 0;
  }
  for (int i = 0; i < 36 && i < count; ++i) {
    if (i) result.round_prefix += ",";
    result.round_prefix += std::to_string(command[static_cast<std::size_t>(i)]);
  }
  return result;
}

RunResult run_once(std::shared_ptr<rt::Services> services, int run, vrt::Recorder& recorder) {
  RunResult result;
  auto command_one = rt::open_command(services, "validation.multi.run" + std::to_string(run) + ".w1",
                                      vrt::limits(64ull << 20, 1000000, 4096,
                                                  kCommandCpuCeiling, 2), 1);
  auto command_two = rt::open_command(services, "validation.multi.run" + std::to_string(run) + ".w2",
                                      vrt::limits(64ull << 20, 1000000, 4096,
                                                  kCommandCpuCeiling, 2), 2);
  recorder.check("multi.run" + std::to_string(run) + "_open_ok",
                 command_one.code == Code::ok && command_two.code == Code::ok, "ok",
                 vrt::code_of(command_one) + "/" + vrt::code_of(command_two));
  if (command_one.code != Code::ok || command_two.code != Code::ok) return result;

  CommandState one;
  CommandState two;
  PoolState pool;
  DispatchLog log;
  std::vector<rt::Task<rt::Done>> tasks_one;
  std::vector<rt::Task<rt::Done>> tasks_two;
  U64 ordinal = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (log.next.load() < kDispatchesPerRun &&
         std::chrono::steady_clock::now() < deadline) {
    const int queued = pool.in_flight.load() - pool.running.load();
    if (queued < kReadyWindow && pool.in_flight.load() < kWindow &&
        one.submitted.load() - one.started.load() < kQuotaOne &&
        one.submitted.load() < kUnitsPerCommand) {
      auto task = submit_unit(*command_one.value, ordinal++, 1, one, two, pool, log);
      if (!task.valid()) ++result.window_rejections;
      tasks_one.push_back(std::move(task));
    }
    if (pool.in_flight.load() - pool.running.load() < kReadyWindow &&
        pool.in_flight.load() < kWindow &&
        two.submitted.load() - two.started.load() < kQuotaTwo &&
        two.submitted.load() < kUnitsPerCommand) {
      auto task = submit_unit(*command_two.value, ordinal++, 2, two, one, pool, log);
      if (!task.valid()) ++result.window_rejections;
      tasks_two.push_back(std::move(task));
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50));
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
  result.max_in_flight = pool.max_in_flight.load();
  result.max_running_one = one.max_running.load();
  result.max_running_two = two.max_running.load();
  const RunResult analysed = analyse(log);
  result.dispatches = analysed.dispatches;
  result.complete_rounds = analysed.complete_rounds;
  result.violations = analysed.violations;
  result.skipped = analysed.skipped;
  result.ones_ok = analysed.ones_ok;
  result.twos_ok = analysed.twos_ok;
  result.both_ok = analysed.both_ok;
  result.snapshot_prefix = analysed.snapshot_prefix;
  result.round_prefix = analysed.round_prefix;
  result.total_one = analysed.total_one;
  result.total_two = analysed.total_two;
  result.violation_detail = analysed.violation_detail;
  result.ratio = analysed.ratio;

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

  auto services = vrt::start_services(512ull << 20, 2000000, kThreads, 2, 2,
                                      vrt::scratch_root("multi01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("multi.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }

  bool all_runs_pass = true;
  for (int run = 0; run < kRuns; ++run) {
    const RunResult result = run_once(*services.value, run, recorder);
    const std::string prefix = "multi.run" + std::to_string(run);
    recorder.value(prefix + "_dispatches", static_cast<long long>(result.dispatches));
    recorder.value(prefix + "_complete_windows", static_cast<long long>(result.complete_rounds));
    recorder.value(prefix + "_violations", static_cast<long long>(result.violations));
    recorder.value(prefix + "_skipped_rounds", static_cast<long long>(result.skipped));
    recorder.value(prefix + "_rounds_with_w1_ready", static_cast<long long>(result.ones_ok));
    recorder.value(prefix + "_rounds_with_w2_ready", static_cast<long long>(result.twos_ok));
    recorder.value(prefix + "_rounds_with_both_ready", static_cast<long long>(result.both_ok));
    recorder.value(prefix + "_snapshot_prefix", result.snapshot_prefix);
    recorder.value(prefix + "_round_prefix", result.round_prefix);
    recorder.value(prefix + "_violation_detail", result.violation_detail);
    recorder.value(prefix + "_total_one", static_cast<long long>(result.total_one));
    recorder.value(prefix + "_total_two", static_cast<long long>(result.total_two));
    recorder.value(prefix + "_ratio_info", result.ratio);
    recorder.value(prefix + "_max_in_flight", static_cast<long long>(result.max_in_flight));
    recorder.check(prefix + "_dispatches",
                   result.dispatches >= kDispatchesPerRun, ">= 330 dispatch starts",
                   std::to_string(result.dispatches));
    recorder.check(prefix + "_complete_windows",
                   result.complete_rounds >= kMinCompleteRounds,
                   ">= 100 complete feasible windows",
                   std::to_string(result.complete_rounds));
    recorder.check(prefix + "_violations_zero",
                   result.violations == 0,
                   "0 rounds whose multiset != {w1,w2,w2}",
                   std::to_string(result.violations) + " violations");
    recorder.check(prefix + "_window_ok",
                   result.window_rejections == 0 && result.max_in_flight <= kWindow,
                   "no resource_exhausted; in-flight <= 12 (contract 20)",
                   "max_in_flight=" + std::to_string(result.max_in_flight) +
                       " rejections=" + std::to_string(result.window_rejections));
    recorder.check(prefix + "_command_concurrency",
                   result.max_running_one <= static_cast<int>(kCommandCpuCeiling) &&
                       result.max_running_two <= static_cast<int>(kCommandCpuCeiling),
                   "each command <= ceiling.cpu_tokens=2",
                   std::to_string(result.max_running_one) + "/" +
                       std::to_string(result.max_running_two));
    recorder.check(prefix + "_units_ok", result.all_units_ok,
                   "all submitted units complete", result.all_units_ok ? "ok" : "failed");
    all_runs_pass = all_runs_pass && result.violations == 0 &&
                    result.complete_rounds >= kMinCompleteRounds &&
                    result.window_rejections == 0 && result.all_units_ok &&
                    result.max_running_one <= static_cast<int>(kCommandCpuCeiling) &&
                    result.max_running_two <= static_cast<int>(kCommandCpuCeiling);
  }
  recorder.check("multi.stability_5_of_5", all_runs_pass,
                 "5/5 quiet runs: complete rounds>=100, violations=0, window ok",
                 all_runs_pass ? "5/5" : "at least one run failed",
                 "each run records its own attempts; an occasional violating round "
                 "keeps the case failed");
  recorder.note("per complete feasible round criterion: at the start of a round, "
                "weight1 must have >=1 ready item and weight2 >=2 ready items; the "
                "three dispatches of that round must be exactly {w1,w2,w2}.  P=2 is "
                "required because the P=1 ready window 2P=2 cannot hold 1+2 items; "
                "dispatch starts (not completions) are counted.");
  return recorder.finish();
}
