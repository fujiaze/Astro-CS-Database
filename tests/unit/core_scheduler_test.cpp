// CORE-006 单元测试: DAG 调度 + 线程租约 + 取消/失败传播
#include "astrocs/core/scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void test_linear_dag() {
  Scheduler sched(2, 2);
  std::vector<std::string> order;
  std::mutex m;
  sched.add_node({"a", {}, [&](const std::string& id, RunContext&) {
    std::lock_guard<std::mutex> l(m); order.push_back(id); return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"b", {"a"}, [&](const std::string& id, RunContext&) {
    std::lock_guard<std::mutex> l(m); order.push_back(id); return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"c", {"b"}, [&](const std::string& id, RunContext&) {
    std::lock_guard<std::mutex> l(m); order.push_back(id); return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  ctx.set_thread_budget(2);
  auto r = sched.run(ctx);
  CHECK(r.ok());
  CHECK(order.size() == 3);
  CHECK(order[0] == "a" && order[1] == "b" && order[2] == "c");
}

static void test_parallel_independent() {
  // 2 独立 heavy 节点在 2 核 budget 下应并发 (总 wall < 2×单节点)
  Scheduler sched(2, 2);
  std::atomic<int> concurrent{0};
  std::atomic<int> max_concurrent{0};
  sched.add_node({"x", {}, [&](const std::string&, RunContext&) {
    int c = ++concurrent;
    int cur = max_concurrent.load();
    while (cur < c && !max_concurrent.compare_exchange_weak(cur, c)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    --concurrent;
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"y", {}, [&](const std::string&, RunContext&) {
    int c = ++concurrent;
    int cur = max_concurrent.load();
    while (cur < c && !max_concurrent.compare_exchange_weak(cur, c)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    --concurrent;
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  ctx.set_thread_budget(2);
  auto t0 = std::chrono::steady_clock::now();
  auto r = sched.run(ctx);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
  CHECK(r.ok());
  CHECK(max_concurrent.load() >= 2);  // 2 核并发验证
  CHECK(ms < 350);  // 并发后总时长 < 串行 400ms
}

static void test_failure_propagation() {
  Scheduler sched(2, 2);
  sched.add_node({"ok1", {}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"bad", {}, [](const std::string&, RunContext&) {
    return Result<void>::fail(Error(ErrorDomain::SCIENCE_PRECONDITION, "bad node"));
  }, "cpu_heavy"});
  sched.add_node({"after", {"bad"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.failed());
  CHECK(r.error().message().find("bad") != std::string::npos);
}

static void test_cancel_propagation() {
  Scheduler sched(2, 2);
  std::atomic<bool> started{false};
  sched.add_node({"slow", {}, [&](const std::string&, RunContext&) {
    started.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"blocked", {"slow"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  std::thread canceller([&] {
    while (!started.load()) std::this_thread::yield();
    sched.cancel();
  });
  auto r = sched.run(ctx);
  canceller.join();
  CHECK(r.failed());
}

static void test_backpressure_bounded_concurrency() {
  // budget=1: 即使 3 个独立节点, 并发上限必须 =1 (backpressure)
  Scheduler sched(4, 1);
  std::atomic<int> concurrent{0};
  std::atomic<int> max_concurrent{0};
  for (int i = 0; i < 3; ++i) {
    sched.add_node({"n" + std::to_string(i), {}, [&](const std::string&, RunContext&) {
      int c = ++concurrent;
      int cur = max_concurrent.load();
      while (cur < c && !max_concurrent.compare_exchange_weak(cur, c)) {}
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      --concurrent;
      return Result<void>::success();
    }, "cpu_heavy"});
  }
  RunContext ctx;
  ctx.set_thread_budget(1);
  auto r = sched.run(ctx);
  CHECK(r.ok());
  CHECK(max_concurrent.load() <= 1);  // backpressure: 不超过 budget
  CHECK(max_concurrent.load() >= 1);
}

static void test_recovery_skip_after_failure() {
  // 失败后: 依赖失败的节点被 SKIPPED; 独立节点仍完成
  Scheduler sched(2, 2);
  sched.add_node({"bad", {}, [](const std::string&, RunContext&) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "boom"));
  }, "cpu_heavy"});
  sched.add_node({"dep", {"bad"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"indep", {}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  std::vector<std::pair<std::string, NodeStatus>> statuses;
  auto r = sched.run(ctx, &statuses);
  CHECK(r.failed());
  CHECK(statuses.size() == 3);
  // 带 node ID 断言（RT-006）
  bool saw_bad = false, saw_dep_skipped = false, saw_indep_ok = false;
  for (const auto& [id, st] : statuses) {
    if (id == "bad") saw_bad = st == NodeStatus::FAILED;
    if (id == "dep") saw_dep_skipped = st == NodeStatus::SKIPPED;
    if (id == "indep") saw_indep_ok = st == NodeStatus::COMPLETED;
  }
  CHECK(saw_bad && saw_dep_skipped && saw_indep_ok);
}

static void test_cycle_rejected() {
  Scheduler sched(2, 2);
  sched.add_node({"a", {"b"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"b", {"a"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.failed());
  CHECK(r.error().message().find("cycle") != std::string::npos);
}

static void test_unknown_dep_rejected() {
  Scheduler sched(2, 2);
  sched.add_node({"a", {"ghost"}, [](const std::string&, RunContext&) {
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.failed());
  CHECK(r.error().message().find("unknown dep") != std::string::npos);
}

int main() {
  test_linear_dag();
  test_parallel_independent();
  test_failure_propagation();
  test_cancel_propagation();
  test_backpressure_bounded_concurrency();
  test_recovery_skip_after_failure();
  test_cycle_rejected();
  test_unknown_dep_rejected();
  if (failures == 0) {
    std::printf("CORE-006 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-006 TESTS FAIL (%d)\n", failures);
  return 1;
}
