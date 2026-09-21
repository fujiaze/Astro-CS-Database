// RT-002 单元测试: 全局原子 ThreadBudget 租约
// 覆盖:
// 1. 两个并发 heavy node 各请求全部核 → 全程 sum(active)<=budget（不超卖）
// 2. 嵌套租约共享同一预算（不因嵌套而超卖）
// 3. 异常注入 → available 恢复（RAII）
// 4. 取消路径 → 租约归还
// 5. NONBLOCK/BEST_EFFORT/BLOCK 策略行为
// 6. 1 worker 仅允许 available=1（本测试 reference 明确标注）
#include "astrocs/core/context.h"

#include <atomic>
#include <chrono>
#include <cstdio>
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

// ── 1) 两个 heavy node 并发各请求全部核 ──
static void test_two_heavy_nodes_no_oversubscribe() {
  const uint32_t budget_n = 4;
  auto b = create_thread_budget(budget_n);
  CHECK(b.ok());
  std::atomic<uint32_t> peak_active{0};
  std::atomic<uint32_t> running{0};
  std::atomic<bool> violated{false};

  auto node = [&](uint32_t req) {
    ThreadLease lease = b.value()->acquire(1, req, AcquirePolicy::NONBLOCK);
    if (!lease.acquired()) return;  // 未拿到就退出（不满足测试前提的路径不判定）
    uint32_t cur = running.fetch_add(lease.size()) + lease.size();
    uint32_t peak = peak_active.load();
    while (cur > peak && !peak_active.compare_exchange_weak(peak, cur)) {}
    if (cur > budget_n) violated.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    running.fetch_sub(lease.size());
    // RAII 析构自动归还
  };

  std::vector<std::thread> ts;
  for (int i = 0; i < 2; ++i) {
    ts.emplace_back([&, i] { node(static_cast<uint32_t>(i == 0 ? 4 : 4)); });
  }
  for (auto& t : ts) t.join();
  CHECK(!violated.load());
  CHECK(peak_active.load() <= budget_n);
  CHECK(b.value()->available() == budget_n);  // 全部归还
}

// ── 2) 嵌套租约共享同一预算 ──
static void test_nested_lease_no_double_count() {
  const uint32_t budget_n = 2;
  auto b = create_thread_budget(budget_n);
  CHECK(b.ok());
  ThreadLease outer = b.value()->acquire(2, 2, AcquirePolicy::NONBLOCK);
  CHECK(outer.acquired());
  CHECK(outer.size() == 2);
  CHECK(b.value()->available() == 0);
  // 嵌套请求无法再超卖（available=0 → 空租约）
  ThreadLease inner = b.value()->acquire(1, 2, AcquirePolicy::NONBLOCK);
  CHECK(!inner.acquired());
  outer.release();
  CHECK(b.value()->available() == budget_n);
}

// ── 3) 异常注入 → available 恢复 ──
static void test_exception_restores_available() {
  const uint32_t budget_n = 3;
  auto b = create_thread_budget(budget_n);
  CHECK(b.ok());
  try {
    ThreadLease lease = b.value()->acquire(3, 3, AcquirePolicy::NONBLOCK);
    CHECK(lease.size() == 3);
    CHECK(b.value()->available() == 0);
    throw std::runtime_error("boom");  // 异常 → lease 析构 → 归还
  } catch (const std::runtime_error&) {
    // expected
  }
  CHECK(b.value()->available() == budget_n);
}

// ── 4) 取消路径 → 租约归还 ──
static void test_cancel_restores_available() {
  const uint32_t budget_n = 2;
  auto b = create_thread_budget(budget_n);
  CHECK(b.ok());
  auto run = [&]() {
    ThreadLease lease = b.value()->acquire(2, 2, AcquirePolicy::NONBLOCK);
    if (!lease.acquired()) return;
    // 模拟取消：提前 return，析构归还
    CHECK(b.value()->available() == 0);
    return;
  };
  run();
  CHECK(b.value()->available() == budget_n);
}

// ── 5) 策略行为 ──
static void test_policies() {
  // BEST_EFFORT 降级: budget=1, min=2 → 取 available=1
  auto b = create_thread_budget(1);
  CHECK(b.ok());
  ThreadLease l1 = b.value()->acquire(2, 2, AcquirePolicy::BEST_EFFORT);
  CHECK(l1.size() == 1);
  l1.release();
  CHECK(b.value()->available() == 1);

  // NONBLOCK: budget=1 已占用, min=1 → 空（不等待）
  auto b2 = create_thread_budget(1);
  CHECK(b2.ok());
  ThreadLease held = b2.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);
  CHECK(held.size() == 1);
  ThreadLease l2 = b2.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);
  CHECK(!l2.acquired());
  held.release();

  // BLOCK: 等待直至满足 min=2（确定性：T 先持 1 并延时归还，主线程等 available>=2）
  auto b3 = create_thread_budget(2);
  CHECK(b3.ok());
  ThreadLease l_t = b3.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);  // 取 1（max=1）
  CHECK(l_t.size() == 1);
  std::thread releaser([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    l_t.release();  // available 1→2, 唤醒 BLOCK 等待者
  });
  ThreadLease l3 = b3.value()->acquire(2, 2, AcquirePolicy::BLOCK);  // 等待到 2
  CHECK(l3.size() == 2);
  CHECK(b3.value()->available() == 0);
  l3.release();
  releaser.join();
  CHECK(b3.value()->available() == 2);
}

// ── 6) 1 worker 仅 allowed when available=1（reference 明确） ──
static void test_single_worker_reference() {
  auto b = create_thread_budget(1);
  CHECK(b.ok());
  ThreadLease l = b.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);
  CHECK(l.size() == 1);  // 仅当 available=1 时 1 worker 合法（reference 测试路径）
  l.release();
  CHECK(b.value()->available() == 1);
}

int main() {
  test_two_heavy_nodes_no_oversubscribe();
  test_nested_lease_no_double_count();
  test_exception_restores_available();
  test_cancel_restores_available();
  test_policies();
  test_single_worker_reference();
  if (failures) {
    std::fprintf(stderr, "RT-002_FAIL failures=%d\n", failures);
    return 1;
  }
  std::printf("RT-002_PASS\n");
  return 0;
}
