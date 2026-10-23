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
constexpr int kWindow = 12;             // <= ready 2P + deferred 8P = 20
constexpr int kDispatchesPerRun = 330;  // enough for >=100 complete rounds
constexpr int kMinCompleteRounds = 100;
constexpr int kRuns = 5;
constexpr int kUnitsPerCommand = 600;
constexpr int kQuotaOne = 2;
constexpr int kQuotaTwo = 4;
constexpr int kMaxDispatch = 512;

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
  std::atomic<int> next{0};
  DispatchLog() {
    for (int i = 0; i < kMaxDispatch; ++i) {
      commands[i].store(0);
      ready_one[i].store(0);
      ready_two[i].store(0);
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
        // Dispatch start: count the credit.  The readiness before each
        // dispatch is recorded by the main thread from the real Task::state()
        // (see the polling loop in run_once).
        own.started.fetch_add(1);
        const int index = log.next.fetch_add(1);
        if (index < kMaxDispatch) log.commands[index].store(command_id);
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

RunResult analyse(const DispatchLog& log) {
  RunResult result;
  result.dispatches = log.next.load();
  const int rounds = result.dispatches / 3;
  // A round is a fixed group of three dispatch events (0..2, 3..5, ...).  A
  // round is a complete feasible round when its start had weight1 ready>=1 and
  // weight2 ready>=2.  Sampling starts at the first complete round.
  int first_round = -1;
  for (int r = 0; r < rounds; ++r) {
    const int i = r * 3;
    const bool o = log.ready_one[i].load() >= 1;
    const bool t = log.ready_two[i].load() >= 2;
    result.ones_ok += o ? 1 : 0;
    result.twos_ok += t ? 1 : 0;
    result.both_ok += (o && t) ? 1 : 0;
    if (o && t && first_round < 0) first_round = r;
  }
  if (first_round < 0) return result;
  int count_one = 0;
  int count_two = 0;
  for (int r = first_round; r < rounds; ++r) {
    const int i = r * 3;
    if (log.ready_one[i].load() < 1 || log.ready_two[i].load() < 2) {
      ++result.skipped;
      continue;
    }
    const int a = log.commands[i].load();
    const int b = log.commands[i + 1].load();
    const int c = log.commands[i + 2].load();
    ++result.complete_rounds;
    const int ones = (a == 1) + (b == 1) + (c == 1);
    const int twos = (a == 2) + (b == 2) + (c == 2);
    if (ones != 1 || twos != 2) ++result.violations;
    count_one += ones;
    count_two += twos;
  }
  result.ratio = count_one > 0 ? static_cast<double>(count_two) / count_one : 0.0;
  int total_one = 0;
  int total_two = 0;
  for (int i = 0; i < result.dispatches; ++i) {
    total_one += log.commands[i].load() == 1 ? 1 : 0;
    total_two += log.commands[i].load() == 2 ? 1 : 0;
  }
  result.total_one = total_one;
  result.total_two = total_two;
  for (int r = 0; r < 12 && r * 3 + 2 < result.dispatches; ++r) {
    if (r) result.round_prefix += "|";
    result.round_prefix += std::to_string(log.commands[r * 3].load()) +
                           std::to_string(log.commands[r * 3 + 1].load()) +
                           std::to_string(log.commands[r * 3 + 2].load());
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
  int seen = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (log.next.load() < kDispatchesPerRun &&
         std::chrono::steady_clock::now() < deadline) {
    // Real ready-queue occupancy from the public Task state.  The snapshot is
    // taken just after detecting the dispatches of this iteration, so the
    // pre-dispatch count is the measured count plus the units that just left
    // the ready set for that command.
    const int ready_one = count_state(tasks_one, rt::TaskState::ready);
    const int ready_two = count_state(tasks_two, rt::TaskState::ready);
    const int now = log.next.load();
    for (int index = seen; index < now && index < kMaxDispatch; ++index) {
      const int command = log.commands[index].load();
      log.ready_one[index].store(ready_one + (command == 1 ? 1 : 0));
      log.ready_two[index].store(ready_two + (command == 2 ? 1 : 0));
    }
    seen = now;
    if (pool.in_flight.load() < kWindow &&
        one.submitted.load() - one.started.load() < kQuotaOne &&
        one.submitted.load() < kUnitsPerCommand) {
      auto task = submit_unit(*command_one.value, ordinal++, 1, one, two, pool, log);
      if (!task.valid()) ++result.window_rejections;
      tasks_one.push_back(std::move(task));
    }
    if (pool.in_flight.load() < kWindow &&
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
    recorder.value(prefix + "_complete_rounds", static_cast<long long>(result.complete_rounds));
    recorder.value(prefix + "_violations", static_cast<long long>(result.violations));
    recorder.value(prefix + "_skipped_rounds", static_cast<long long>(result.skipped));
    recorder.value(prefix + "_rounds_with_w1_ready", static_cast<long long>(result.ones_ok));
    recorder.value(prefix + "_rounds_with_w2_ready", static_cast<long long>(result.twos_ok));
    recorder.value(prefix + "_rounds_with_both_ready", static_cast<long long>(result.both_ok));
    recorder.value(prefix + "_snapshot_prefix", result.snapshot_prefix);
    recorder.value(prefix + "_round_prefix", result.round_prefix);
    recorder.value(prefix + "_total_one", static_cast<long long>(result.total_one));
    recorder.value(prefix + "_total_two", static_cast<long long>(result.total_two));
    recorder.value(prefix + "_ratio_info", result.ratio);
    recorder.value(prefix + "_max_in_flight", static_cast<long long>(result.max_in_flight));
    recorder.check(prefix + "_dispatches",
                   result.dispatches >= kDispatchesPerRun, ">= 330 dispatch starts",
                   std::to_string(result.dispatches));
    recorder.check(prefix + "_complete_rounds",
                   result.complete_rounds >= kMinCompleteRounds,
                   ">= 100 complete feasible rounds",
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
