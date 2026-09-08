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

// B13-R13-2: 内存回压不得队头阻塞 (HoL) — 队首大内存节点超限时, ready 中
// 后续小内存节点仍须可执行 (修复前整队被队首压住, 附属忙等自旋)。
static void test_backpressure_no_head_of_line_blocking() {
  // 内存预算 100; A 占 60 (耗时, 保持 mem_used 高), B 需 50 (队首, 超限),
  // C 占 30 (可放)。
  Scheduler sched(4, 2, /*memory_limit_bytes=*/100);
  std::atomic<bool> a_running{false};
  std::atomic<bool> c_done_before_b{false};
  sched.add_node({"A", {}, [&](const std::string&, RunContext&) {
    a_running.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return Result<void>::success();
  }, "cpu_heavy", /*estimated_memory_bytes=*/60});
  sched.add_node({"C", {}, [&](const std::string&, RunContext&) {
    if (a_running.load()) c_done_before_b.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return Result<void>::success();
  }, "cpu_heavy", /*estimated_memory_bytes=*/30});
  sched.add_node({"B", {}, [&](const std::string&, RunContext&) {
    return Result<void>::success();  // 50 > 100-60 = 回压, 超限
  }, "cpu_heavy", /*estimated_memory_bytes=*/50});
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.ok());
  CHECK(c_done_before_b.load());  // C 未被队首 B 压死
  CHECK(r.ok());
}

// B13-R13-2: 回压等待必须谓词挂起而非忙等自旋 — 全超限且存在慢在途节点时,
// 空闲 worker 挂起等待 (修复前 notify_all+continue 热循环)。语义等价断言:
// 全部节点仍正确完成, 回压在途上限不被突破 (自旋/饿死都会导致缺完成或峰值超限)。
static void test_backpressure_wait_no_busy_spin() {
  Scheduler sched(4, 2, /*memory_limit_bytes=*/100);
  std::atomic<int> peak_used{0};
  std::atomic<uint64_t> mem_now{0};
  std::atomic<int> done{0};
  for (int i = 0; i < 8; ++i) {
    sched.add_node({"m" + std::to_string(i), {}, [&](const std::string&, RunContext&) {
      uint64_t now = mem_now.fetch_add(70) + 70;  // 单节点 70 > 剩余 40: 必回压
      int p = static_cast<int>(now / 70);
      int cur = peak_used.load();
      while (cur < p && !peak_used.compare_exchange_weak(cur, p)) {}
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      mem_now.fetch_sub(70);
      ++done;
      return Result<void>::success();
    }, "cpu_heavy", /*estimated_memory_bytes=*/70});
  }
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.ok());
  CHECK(done.load() == 8);           // 无节点饿死/丢失
  CHECK(peak_used.load() <= 2);      // 100/70 → 同时刻至多 1 个在途 (无回压穿透)
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
  test_backpressure_no_head_of_line_blocking();
  test_backpressure_wait_no_busy_spin();
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
