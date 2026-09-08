// B13-R13-3 单元测试: CpuHeavyExecutor observed_provider_ 数据竞争回归
// (nice -n 19 timeout 300, TSAN 构建下运行; 修复前 worker 观测段锁外读
// observed_provider_ 与完成段锁内写构成跨线程 UB, TSAN 必报 data race)
#include "astrocs/core/executor.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
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

// 并发压力: 多 worker 任务内 ctx.set_provider(TLS) 写 + worker 观测段读
// observed_provider_ (obs_mu_) + 主线程 observed_provider() 读 — 全部真实
// 并发路径, 观察者与写者在线程级交错。
static void test_provider_observation_race() {
  auto budget = create_thread_budget(4);
  CHECK(budget.ok());
  CpuHeavyExecutor exec(budget.value());
  auto store = std::make_shared<TraceStore>();
  exec.set_trace_store(store);

  std::atomic<bool> stop{false};
  // 观察者线程: 持续读 observed_provider() (executor.h 锁内读)
  std::thread observer([&stop, &exec] {
    while (!stop.load()) {
      const std::string p = exec.observed_provider();
      (void)p;
    }
  });

  constexpr int kTasks = 512;
  for (int i = 0; i < kTasks; ++i) {
    exec.enqueue([i](RunContext& ctx) {
      // 抖动: 拉开 STARTED(观测读) 与 COMPLETED(收集写) 的交错相位,
      // 制造"worker A 锁外读 vs worker B 锁内写"重叠窗口
      std::this_thread::sleep_for(std::chrono::microseconds(300 * (i % 11)));
      ctx.set_provider("p" + std::to_string(i));  // 每任务唯一值 → 完成段必写
    });
  }
  exec.wait_all();
  stop.store(true);
  observer.join();

  CHECK(exec.tasks_executed() >= kTasks);
  CHECK(exec.provider_sets() >= 1);
  const auto snap = store->snapshot();
  CHECK(snap.size() >= 2 * static_cast<size_t>(kTasks));  // STARTED+COMPLETED
  for (const auto& e : snap) {
    // STARTED 快照允许为空 (尚无任何任务置位 provider); 每任务唯一 provider,
    // 非空值必须以 "p" 开头 (观测读的真实值域)
    CHECK(e.provider.empty() || e.provider.rfind("p", 0) == 0);
  }
}

int main() {
  test_provider_observation_race();
  if (failures == 0) {
    std::printf("B13-R13-3 EXECUTOR PROVIDER TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "B13-R13-3 EXECUTOR PROVIDER TESTS FAIL (%d)\n", failures);
  return 1;
}
