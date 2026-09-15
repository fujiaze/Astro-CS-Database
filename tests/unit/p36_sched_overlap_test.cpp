// tests/unit/p36_sched_overlap_test.cpp — P36 D1 调度重叠准入契约单元测试
// 覆盖: (A) 需求准入: 需求之和 <= 预算的节点真正重叠 (含满额节点的对照);
//       (B) 需求上限即租约上限; (C) 不超卖 + 无空租约; (D) 阴性对照: legacy 全局
//       互斥 ⇒ 重叠消失, fair 模式 ⇒ 满额节点被降级 (机制非恒真);
//       (E) 依赖顺序不被重叠破坏; (F) 失败传播在重写后仍成立。
#include "astrocs/core/scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

namespace {

struct Rec {
  std::atomic<int> cur{0};
  std::atomic<int> peak{0};
  std::atomic<uint64_t> sum_granted{0};
  std::atomic<int> zero_lease{0};
  std::atomic<int> oversell{0};   // Σ 在途租约 > budget 的次数
  std::atomic<int> held{0};
};

NodeFn node_fn(Rec* r, uint32_t budget, int ms) {
  return [r, budget, ms](const std::string&, RunContext& ctx) -> Result<void> {
    ThreadLease lease = ctx.acquire_lease(budget);
    const uint32_t got = lease.acquired() ? lease.size() : 0u;
    if (got == 0) r->zero_lease.fetch_add(1);
    r->sum_granted.fetch_add(got);
    const int h = r->held.fetch_add(static_cast<int>(got)) + static_cast<int>(got);
    if (h > static_cast<int>(budget)) r->oversell.fetch_add(1);
    const int c = r->cur.fetch_add(1) + 1;
    int p = r->peak.load();
    while (c > p && !r->peak.compare_exchange_weak(p, c)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    r->cur.fetch_sub(1);
    r->held.fetch_sub(static_cast<int>(got));
    return Result<void>::success();
  };
}

// (A1) 需求准入: 4 个各需 1 槽的节点在 budget=4 下应完全重叠 (旧全局互斥下为 1)。
void test_demand_overlap_small() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  Rec r;
  for (int i = 0; i < 4; ++i) {
    sched.add_node({"s" + std::to_string(i), {}, node_fn(&r, 4, 120), "cpu_light", 0, 1, 1});
  }
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(r.peak.load() == 4);          // 互不冲突的节点真正重叠
  CHECK(r.zero_lease.load() == 0);
  CHECK(r.oversell.load() == 0);
  CHECK(r.sum_granted.load() == 4);
}

// (A2) 需求准入: 两个各需 2 槽的节点在 budget=4 下重叠。
void test_demand_overlap_half() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  Rec r;
  sched.add_node({"a", {}, node_fn(&r, 4, 120), "cpu_heavy", 0, 1, 2});
  sched.add_node({"b", {}, node_fn(&r, 4, 120), "cpu_heavy", 0, 1, 2});
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(r.peak.load() == 2);
  CHECK(r.sum_granted.load() == 4);
  CHECK(r.zero_lease.load() == 0);
}

// (A3) 需求准入的边界: 两个声明满额 (max_workers=0 -> budget) 的节点互冲突 ⇒ 串行
// (OMP team 起始固定, 不给满额就不该启动 —— 记录为契约的已知边界, 非缺陷)。
void test_full_demand_conflicts() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  Rec r;
  sched.add_node({"a", {}, node_fn(&r, 4, 80), "cpu_heavy", 0, 1, 0});
  sched.add_node({"b", {}, node_fn(&r, 4, 80), "cpu_heavy", 0, 1, 0});
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(r.peak.load() == 1);          // 满额-满额冲突 -> 串行 (零退化)
  CHECK(r.sum_granted.load() == 8);   // 各得整份预算 (4+4)
}

// (B) 声明 max_workers=1 的节点即便独占也只得 1 槽 (需求上限即租约上限)。
void test_demand_cap() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  Rec r;
  sched.add_node({"w", {}, node_fn(&r, 4, 20), "io", 0, 1, 1});
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(r.sum_granted.load() == 1);
  CHECK(r.zero_lease.load() == 0);
}

// (C) 多节点同批: 不超卖、无空租约、峰值并发 <= 预算/需求。
void test_no_oversell_many() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  Rec r;
  for (int i = 0; i < 8; ++i) {
    sched.add_node({"n" + std::to_string(i), {}, node_fn(&r, 4, 20), "cpu_heavy", 0, 1, 2});
  }
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(r.zero_lease.load() == 0);
  CHECK(r.oversell.load() == 0);
  CHECK(r.peak.load() <= 2);  // 每节点 2 槽, budget 4
  CHECK(r.peak.load() >= 2);
}

// (D) 阴性对照/非恒真: legacy 全局互斥 ⇒ 峰值 1; fair ⇒ 满额节点被降级后重叠。
void test_negative_control_modes() {
  {
    Scheduler serial(4, 4);
    Rec r1;
    serial.add_node({"a", {}, node_fn(&r1, 4, 80), "cpu_heavy", 0, 1, 0});
    serial.add_node({"b", {}, node_fn(&r1, 4, 80), "cpu_heavy", 0, 1, 0});
    RunContext c1;
    setenv("ASTROCS_SCHED_OVERLAP", "0", 1);
    CHECK(serial.run(c1).ok());
    CHECK(r1.peak.load() == 1);   // 人为串行化 ⇒ 重叠消失
  }
  {
    Scheduler ov(4, 4);
    Rec r2;
    ov.add_node({"a", {}, node_fn(&r2, 4, 80), "cpu_heavy", 0, 1, 0});
    ov.add_node({"b", {}, node_fn(&r2, 4, 80), "cpu_heavy", 0, 1, 0});
    RunContext c2;
    setenv("ASTROCS_SCHED_OVERLAP", "fair", 1);
    CHECK(ov.run(c2).ok());
    CHECK(r2.peak.load() == 2);            // fair: 满额节点被降级为 free/2 后重叠
    CHECK(r2.sum_granted.load() <= 4);     // 仍不超卖
    CHECK(r2.zero_lease.load() == 0);
  }
  unsetenv("ASTROCS_SCHED_OVERLAP");
}

// (E) 依赖顺序不被重叠破坏 (b 必须在 a 完成后才执行)。
void test_dependency_order() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  std::atomic<bool> a_done{false};
  std::atomic<bool> b_before_a{false};
  sched.add_node({"a", {}, [&](const std::string&, RunContext&) -> Result<void> {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    a_done.store(true);
    return Result<void>::success();
  }, "cpu_heavy", 0, 1, 2});
  sched.add_node({"b", {"a"}, [&](const std::string&, RunContext&) -> Result<void> {
    if (!a_done.load()) b_before_a.store(true);
    return Result<void>::success();
  }, "cpu_heavy", 0, 1, 2});
  RunContext ctx;
  CHECK(sched.run(ctx).ok());
  CHECK(!b_before_a.load());
}

// (F) 失败传播: FAILED 根 + 依赖 SKIPPED + 独立节点 COMPLETED。
void test_failure_propagation() {
  unsetenv("ASTROCS_SCHED_OVERLAP");
  Scheduler sched(4, 4);
  sched.add_node({"bad", {}, [](const std::string&, RunContext&) -> Result<void> {
    return Result<void>::fail(Error(ErrorDomain::DATA, "boom"));
  }, "cpu_heavy", 0, 1, 2});
  sched.add_node({"dep", {"bad"}, [](const std::string&, RunContext&) -> Result<void> {
    return Result<void>::success();
  }, "cpu_heavy", 0, 1, 2});
  sched.add_node({"indep", {}, [](const std::string&, RunContext&) -> Result<void> {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return Result<void>::success();
  }, "cpu_heavy", 0, 1, 2});
  RunContext ctx;
  std::vector<std::pair<std::string, NodeStatus>> st;
  auto r = sched.run(ctx, &st);
  CHECK(r.failed());
  bool bad = false, dep = false, indep = false;
  for (const auto& [id, s] : st) {
    if (id == "bad") bad = (s == NodeStatus::FAILED);
    if (id == "dep") dep = (s == NodeStatus::SKIPPED);
    if (id == "indep") indep = (s == NodeStatus::COMPLETED);
  }
  CHECK(bad && dep && indep);
}

}  // namespace

int main() {
  test_demand_overlap_small();
  test_demand_overlap_half();
  test_full_demand_conflicts();
  test_demand_cap();
  test_no_oversell_many();
  test_negative_control_modes();
  test_dependency_order();
  test_failure_propagation();
  if (failures == 0) {
    std::printf("P36_SCHED_OVERLAP_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "P36_SCHED_OVERLAP_FAIL failures=%d\n", failures);
  return 1;
}
