// S-MULTI: two continuously ready commands with weight 1 and 2 receive
// dispatch credits in a 1:2 ratio; neither pool nor command concurrency
// ceilings are exceeded, and neither command starves.
//
// Normative reading (SCHEDULER.md "多command分配与公平性"):
//   * "只有ready且资源可行的command参与" -> the executor is work-conserving;
//     it must NOT idle-wait for a weighted command that currently has no ready
//     item.  A command that cannot supply its full weight loses the unused
//     credit ("每轮未用credit不跨轮累积").
//   * "每完整可行轮dispatch比例1:2" -> the 1:2 ratio is owed only for rounds
//     where BOTH commands are ready and resource-feasible with their full
//     weight (1 ready item for weight 1, 2 ready items for weight 2).
//   * A P=1 pool has a 2P=2 ready window: it cannot hold 1+2 ready items at
//     once, so a P=1 fixture cannot create a complete feasible round; it
//     measures "who happens to be ready", not the weights.  This case therefore
//     uses P=2 (ready window 2P=4) and maintains per-command ready quotas
//     (weight1 >= 1, weight2 >= 2) so every counted round is a complete round.
//   * The runtime bounds in-flight work (ready 2P + deferred 8P = 20 for P=2);
//     submissions stay inside a rolling window of 12 and are never rejected.
//
// The measured ratio is over 300 completed unit segments; the tolerance is
// declared here because the standard promises bounded execution-segment counts,
// not exact CPU seconds.  The ready-occupancy fraction is recorded and must be
// high, so a ratio is only judged inside a complete-round environment.
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
using acsd::U64;

constexpr int kThreads = 2;         // P=2 -> ready window 2P=4
constexpr int kWindow = 12;         // <= ready 2P + deferred 8P = 20
constexpr int kSample = 300;        // completed unit segments
constexpr int kUnitsPerCommand = 400;
constexpr int kQuotaOne = 2;        // weight-1 command keeps >=2 outstanding
constexpr int kQuotaTwo = 4;        // weight-2 command keeps >=4 outstanding

struct CommandState {
  std::atomic<int> submitted{0};
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

struct OrderLog {
  std::vector<int> commands;
  std::atomic<int> next{0};
  explicit OrderLog(int size) : commands(static_cast<std::size_t>(size), 0) {}
};

void note_concurrency(std::atomic<int>& counter, std::atomic<int>& peak) {
  const int now = counter.fetch_add(1) + 1;
  int previous = peak.load();
  while (now > previous && !peak.compare_exchange_weak(previous, now)) {
  }
}

rt::Task<rt::Done> submit_unit(const rt::InvocationContext& ctx, U64 ordinal,
                               int command_id, CommandState& state, PoolState& pool,
                               OrderLog& dispatch_order) {
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, ordinal};
  spec.running.cpu_tokens = 1;
  spec.work_upper = 1;
  spec.semantic = "validation.multi.unit";
  state.submitted.fetch_add(1);
  pool.in_flight.fetch_add(1);
  int previous = pool.max_in_flight.load();
  int now = pool.in_flight.load();
  while (now > previous && !pool.max_in_flight.compare_exchange_weak(previous, now)) {
  }
  return rt::submit_cpu<rt::Done>(
      ctx, spec,
      [&state, &pool, &dispatch_order, command_id](rt::InvocationContext task) -> Result<rt::Done> {
        // Dispatch event: the unit starts on a CPU token.  Counting starts (not
        // completions) measures the dispatch credits themselves.
        {
          const int slot = dispatch_order.next.fetch_add(1);
          if (slot < static_cast<int>(dispatch_order.commands.size()))
            dispatch_order.commands[static_cast<std::size_t>(slot)] = command_id;
        }
        (void)rt::consume_work(task, 1);
        note_concurrency(state.running, state.max_running);
        note_concurrency(pool.running, pool.max_running);
        state.completed.fetch_add(1);
        const auto until =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
        while (std::chrono::steady_clock::now() < until) {
        }
        state.running.fetch_sub(1);
        pool.running.fetch_sub(1);
        pool.in_flight.fetch_sub(1);
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

}  // namespace

int main() {
  vrt::Recorder recorder("multi01", "validation/runtime/multi01/multi01.cpp");

  // P=2: ready window 2P=4 can hold 1 weight-1 + 2 weight-2 ready items.
  auto services = vrt::start_services(256ull << 20, 1000000, kThreads, 2, 2,
                                      vrt::scratch_root("multi01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("multi.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }

  auto command_one = rt::open_command(*services.value, "validation.multi.weight1",
                                      vrt::limits(64ull << 20, 1000000, 4096, kThreads, 2), 1);
  auto command_two = rt::open_command(*services.value, "validation.multi.weight2",
                                      vrt::limits(64ull << 20, 1000000, 4096, kThreads, 2), 2);
  recorder.expect_code("multi.open_weight1_ok", "ok", command_one);
  recorder.expect_code("multi.open_weight2_ok", "ok", command_two);
  if (command_one.code != Code::ok || command_two.code != Code::ok)
    return recorder.finish();

  CommandState state_one;
  CommandState state_two;
  PoolState pool;
  OrderLog dispatch_order(kSample + 64);
  std::vector<rt::Task<rt::Done>> tasks_one;
  std::vector<rt::Task<rt::Done>> tasks_two;
  tasks_one.reserve(kUnitsPerCommand);
  tasks_two.reserve(kUnitsPerCommand);
  int window_rejections = 0;
  U64 ordinal = 0;
  long long environment_samples = 0;
  long long complete_round_samples = 0;

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (dispatch_order.next.load() < kSample &&
         std::chrono::steady_clock::now() < deadline) {
    // Ready-occupancy evidence: the ratio is only meaningful when both commands
    // are simultaneously ready with their full weight (1 + 2 items).
    const int ready_one = count_state(tasks_one, rt::TaskState::ready);
    const int ready_two = count_state(tasks_two, rt::TaskState::ready);
    ++environment_samples;
    if (ready_one >= 1 && ready_two >= 2) ++complete_round_samples;

    // Refill each command to its quota, staying inside the rolling window.
    const bool can_submit = pool.in_flight.load() < kWindow;
    if (can_submit && state_one.submitted.load() - state_one.completed.load() < kQuotaOne &&
        state_one.submitted.load() < kUnitsPerCommand) {
      auto task = submit_unit(*command_one.value, ordinal++, 1, state_one, pool, dispatch_order);
      if (!task.valid()) ++window_rejections;
      tasks_one.push_back(std::move(task));
    }
    if (pool.in_flight.load() < kWindow &&
        state_two.submitted.load() - state_two.completed.load() < kQuotaTwo &&
        state_two.submitted.load() < kUnitsPerCommand) {
      auto task = submit_unit(*command_two.value, ordinal++, 2, state_two, pool, dispatch_order);
      if (!task.valid()) ++window_rejections;
      tasks_two.push_back(std::move(task));
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  const bool sampled = dispatch_order.next.load() >= kSample;
  recorder.check("multi.sample_reached", sampled, "300 dispatch events within 60 s",
                 std::to_string(dispatch_order.next.load()) + " dispatches");
  if (!sampled) return recorder.finish();

  bool all_units_ok = true;
  for (auto& task : tasks_one) {
    auto result = rt::wait_external(std::move(task));
    if (result.code == Code::resource_exhausted) ++window_rejections;
    all_units_ok = all_units_ok && (result.code == Code::ok || result.code == Code::cancelled);
  }
  for (auto& task : tasks_two) {
    auto result = rt::wait_external(std::move(task));
    if (result.code == Code::resource_exhausted) ++window_rejections;
    all_units_ok = all_units_ok && (result.code == Code::ok || result.code == Code::cancelled);
  }

  const double environment = environment_samples > 0
                                ? static_cast<double>(complete_round_samples) / environment_samples
                                : 0.0;
  recorder.value("multi.environment_samples", environment_samples);
  recorder.value("multi.complete_round_samples", complete_round_samples);
  recorder.value("multi.complete_round_fraction", environment);
  recorder.check("multi.complete_round_environment", environment >= 0.90,
                 ">=90% of samples have weight1 ready>=1 and weight2 ready>=2",
                 vrt::number(environment, 4),
                 "a P=1 / 2P=2 ready window cannot hold 1+2 ready items; this fixture "
                 "uses P=2 and keeps both commands at their full weight");

  int count_one = 0;
  int count_two = 0;
  for (int i = 0; i < kSample; ++i) {
    if (dispatch_order.commands[static_cast<std::size_t>(i)] == 1) ++count_one;
    if (dispatch_order.commands[static_cast<std::size_t>(i)] == 2) ++count_two;
  }
  std::string prefix;
  for (int i = 0; i < kSample && i < 36; ++i) {
    if (i) prefix += ",";
    prefix += std::to_string(dispatch_order.commands[static_cast<std::size_t>(i)]);
  }
  recorder.value("multi.order_prefix", prefix);
  recorder.value("multi.sample_count_weight1", count_one);
  recorder.value("multi.sample_count_weight2", count_two);
  const double ratio = count_one > 0 ? static_cast<double>(count_two) / count_one : 0.0;
  recorder.value("multi.sample_ratio", ratio);
  recorder.value("multi.window", kWindow);
  recorder.value("multi.window_rejections", window_rejections);
  recorder.value("multi.max_in_flight", pool.max_in_flight.load());
  recorder.check("multi.window_contract_respected",
                 window_rejections == 0 && pool.max_in_flight.load() <= kWindow,
                 "no resource_exhausted and in-flight <= 12 (ready+deferred=20)",
                 "max_in_flight=" + std::to_string(pool.max_in_flight.load()) +
                     " rejections=" + std::to_string(window_rejections));
  recorder.check("multi.all_units_ok", all_units_ok, "all submitted units complete",
                 all_units_ok ? "ok" : "failed unit");
  recorder.check("multi.no_starvation", count_one > 0 && count_two > 0,
                 "both commands dispatched in the sample",
                 std::to_string(count_one) + "/" + std::to_string(count_two));
  recorder.check("multi.ratio_1_to_2_complete_rounds",
                 count_two >= 2 * count_one - 5 && count_two <= 2 * count_one + 5,
                 "2*count1-5 <= count2 <= 2*count1+5 over 300 dispatch events",
                 std::to_string(2 * count_one - 5) + " <= " + std::to_string(count_two) +
                     " <= " + std::to_string(2 * count_one + 5) + " (ratio=" +
                     vrt::number(ratio, 4) + ")",
                 "tolerance is +/-5 credits on 300 bounded execution segments; the "
                 "complete-round environment is checked above");
  recorder.resource("multi.max_running_pool", pool.max_running.load());
  recorder.resource("multi.max_running_weight1", state_one.max_running.load());
  recorder.resource("multi.max_running_weight2", state_two.max_running.load());
  recorder.check("multi.pool_concurrency_within_threads", pool.max_running.load() <= kThreads,
                 "<= 2", std::to_string(pool.max_running.load()));
  recorder.check("multi.command_concurrency_within_ceiling",
                 state_one.max_running.load() <= kThreads &&
                     state_two.max_running.load() <= kThreads,
                 "each <= 2", std::to_string(state_one.max_running.load()) + "/" +
                                   std::to_string(state_two.max_running.load()));

  auto closed_one = rt::wait_external(rt::close_command(*command_one.value));
  auto closed_two = rt::wait_external(rt::close_command(*command_two.value));
  recorder.expect_code("multi.close_weight1_ok", "ok", closed_one);
  recorder.expect_code("multi.close_weight2_ok", "ok", closed_two);
  return recorder.finish();
}
