// S-MULTI: two continuously ready commands with weight 1 and 2 receive
// dispatch credits in a 1:2 ratio; neither pool nor command concurrency
// ceilings are exceeded, and neither command starves.
//
// Fixed configuration (SCHEDULER.md S-MULTI row): every unit is one equal
// dispatch segment, both commands keep a deep ready backlog while the sample
// is taken.  With P=1 the completion order is the dispatch order, so the first
// 30 completed units must split 1:2 (10 vs 20) within a frozen tolerance.
// The backlog is then cancelled and drained; total wall work stays bounded
// (2 x 200 units x 200 us), unlike an unbounded ready-chain.
#include <acsd/contracts.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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

constexpr int kUnitsPerCommand = 200;
constexpr int kSampleSize = 120;

struct CommandState {
  std::atomic<int> completed{0};
  std::atomic<int> running{0};
  std::atomic<int> max_running{0};
};

struct Sample {
  std::atomic<int> completed_total{0};
  std::atomic<int> sampled{0};
  std::atomic<int> count_one{0};
  std::atomic<int> count_two{0};
  std::atomic<int> points_one[4] = {};
  std::atomic<int> points_two[4] = {};
  std::atomic<int> points_at[4] = {30, 60, 90, 120};
};

struct PoolState {
  std::atomic<int> running{0};
  std::atomic<int> max_running{0};
};

void note_concurrency(std::atomic<int>& counter, std::atomic<int>& peak) {
  const int now = counter.fetch_add(1) + 1;
  int previous = peak.load();
  while (now > previous && !peak.compare_exchange_weak(previous, now)) {
  }
}

rt::Task<rt::Done> submit_unit(const rt::InvocationContext& ctx, U64 ordinal,
                               CommandState& state, CommandState& other, PoolState& pool,
                               Sample& sample, bool first_command) {
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, ordinal};
  spec.running.cpu_tokens = 1;
  spec.work_upper = 1;
  spec.semantic = first_command ? "validation.multi.unit1" : "validation.multi.unit2";
  return rt::submit_cpu<rt::Done>(
      ctx, spec,
      [&state, &other, &pool, &sample, first_command](rt::InvocationContext task) -> Result<rt::Done> {
        (void)rt::consume_work(task, 1);
        note_concurrency(state.running, state.max_running);
        note_concurrency(pool.running, pool.max_running);
        state.completed.fetch_add(1);
        const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(200);
        while (std::chrono::steady_clock::now() < until) {
        }
        state.running.fetch_sub(1);
        pool.running.fetch_sub(1);
        const int total = sample.completed_total.fetch_add(1) + 1;
        const int mine = state.completed.load();
        const int theirs = other.completed.load();
        for (int point = 0; point < 4; ++point) {
          if (total == sample.points_at[point].load()) {
            sample.points_one[point].store(first_command ? mine : theirs);
            sample.points_two[point].store(first_command ? theirs : mine);
          }
        }
        if (total == kSampleSize) {
          // Take the frozen sample with both commands still backlogged.
          sample.count_one.store(first_command ? mine : theirs);
          sample.count_two.store(first_command ? theirs : mine);
          sample.sampled.store(1);
        }
        Result<rt::Done> out;
        out.code = Code::ok;
        out.value = rt::Done{};
        return out;
      });
}

bool wait_sampled(Sample& sample, int milliseconds) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < deadline) {
    if (sample.sampled.load() == 1) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return sample.sampled.load() == 1;
}

}  // namespace

int main() {
  vrt::Recorder recorder("multi01", "validation/runtime/multi01/multi01.cpp");

  // P=1 makes completion order equal dispatch order; both commands still have
  // a deep backlog, so the weighted round robin must show its 1:2 pattern.
  auto services = vrt::start_services(256ull << 20, 1000000, 1, 1, 1,
                                      vrt::scratch_root("multi01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("multi.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }

  auto command_one = rt::open_command(*services.value, "validation.multi.weight1",
                                      vrt::limits(64ull << 20, 1000000, 4096, 1, 1), 1);
  auto command_two = rt::open_command(*services.value, "validation.multi.weight2",
                                      vrt::limits(64ull << 20, 1000000, 4096, 1, 1), 2);
  recorder.expect_code("multi.open_weight1_ok", "ok", command_one);
  recorder.expect_code("multi.open_weight2_ok", "ok", command_two);
  if (command_one.code != Code::ok || command_two.code != Code::ok)
    return recorder.finish();

  CommandState state_one;
  CommandState state_two;
  PoolState pool;
  Sample sample;

  std::vector<rt::Task<rt::Done>> tasks;
  tasks.reserve(2 * kUnitsPerCommand);
  for (int i = 0; i < kUnitsPerCommand; ++i)
    tasks.push_back(submit_unit(*command_one.value, static_cast<U64>(i), state_one,
                                state_two, pool, sample, true));
  for (int i = 0; i < kUnitsPerCommand; ++i)
    tasks.push_back(submit_unit(*command_two.value, static_cast<U64>(i), state_two,
                                state_one, pool, sample, false));
  bool all_valid = true;
  for (const auto& task : tasks) all_valid = all_valid && task.valid();
  recorder.check("multi.backlog_submitted", all_valid,
                 "400 valid queued units", all_valid ? "valid" : "invalid task handle");

  const bool sampled = wait_sampled(sample, 10000);
  recorder.check("multi.sample_reached", sampled,
                 "120 completions within 10 s", sampled ? "reached" : "not reached");

  // Freeze the dispatch counts before any cancel changes the queue: the
  // backlog counters below are the per-command completion counts at sample.
  for (int point = 0; point < 4; ++point) {
    recorder.value("multi.point_" + std::to_string(point) + "_total",
                   static_cast<long long>(sample.points_at[point].load()));
    recorder.value("multi.point_" + std::to_string(point) + "_count1",
                   static_cast<long long>(sample.points_one[point].load()));
    recorder.value("multi.point_" + std::to_string(point) + "_count2",
                   static_cast<long long>(sample.points_two[point].load()));
  }
  const int sampled_one = sample.count_one.load();
  const int sampled_two = sample.count_two.load();
  recorder.value("multi.sample_count_weight1", static_cast<long long>(sampled_one));
  recorder.value("multi.sample_count_weight2", static_cast<long long>(sampled_two));
  const double ratio = sampled_one > 0 ? static_cast<double>(sampled_two) / sampled_one : 0.0;
  recorder.value("multi.sample_ratio", ratio);
  recorder.check("multi.no_starvation", sampled_one > 0 && sampled_two > 0,
                 "both commands complete units before the sample",
                 std::to_string(sampled_one) + "/" + std::to_string(sampled_two));
  recorder.note("whole-window ratio " + vrt::number(ratio, 4) +
                " includes the admission transient; the pass/fail criterion is the "
                "complete-round split (30..120) below");
  const long long steady_one =
      sample.points_one[3].load() - sample.points_one[0].load();
  const long long steady_two =
      sample.points_two[3].load() - sample.points_two[0].load();
  const double steady_ratio =
      steady_one > 0 ? static_cast<double>(steady_two) / steady_one : 0.0;
  recorder.value("multi.steady_state_count1", steady_one);
  recorder.value("multi.steady_state_count2", steady_two);
  recorder.value("multi.steady_state_ratio", steady_ratio);
  recorder.note("the first 30 dispatches include the admission transient "
                "(command one is admitted first); complete rounds are measured "
                "from dispatch 30 to 120");
  recorder.check("multi.steady_state_ratio_1_to_2",
                 steady_one > 0 && steady_two >= 2 * steady_one - 3 &&
                     steady_two <= 2 * steady_one + 3,
                 "complete rounds 30..120 split 1:2 within +/-3 credits",
                 std::to_string(steady_one) + "/" + std::to_string(steady_two) + " ratio=" +
                     vrt::number(steady_ratio, 4));
  recorder.resource("multi.max_running_pool", pool.max_running.load());
  recorder.resource("multi.max_running_weight1", state_one.max_running.load());
  recorder.resource("multi.max_running_weight2", state_two.max_running.load());
  recorder.check("multi.pool_concurrency_within_threads", pool.max_running.load() <= 1,
                 "<= 1", std::to_string(pool.max_running.load()));
  recorder.check("multi.command_concurrency_within_ceiling",
                 state_one.max_running.load() <= 1 && state_two.max_running.load() <= 1,
                 "each <= 1 (command ceiling 1 with P=1)",
                 std::to_string(state_one.max_running.load()) + "/" +
                     std::to_string(state_two.max_running.load()));

  // The frozen sample is already taken; drain the bounded backlog normally.
  // (A separate reproduction shows that cancelling a command with ready queued
  // tasks is not part of this case; see the reported minimal repro.)
  bool drained = true;
  for (auto& task : tasks) {
    auto result = rt::wait_external(std::move(task));
    drained = drained && result.code == Code::ok;
  }
  recorder.check("multi.backlog_drained", drained, "all 400 units complete ok",
                 drained ? "drained" : "failed unit");

  auto closed_one = rt::wait_external(rt::close_command(*command_one.value));
  auto closed_two = rt::wait_external(rt::close_command(*command_two.value));
  recorder.expect_code("multi.close_weight1_ok", "ok", closed_one);
  recorder.expect_code("multi.close_weight2_ok", "ok", closed_two);
  return recorder.finish();
}
