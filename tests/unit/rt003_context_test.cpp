// RT-003 单元测试: RunContext 并发安全
// 覆盖: 多 node 并发 log/metrics/store/checkpoint/cancel；
// 唯一 producer 写 + duplicate 冲突确定；
// 不暴露容器裸引用（快照语义，编译期即验证）。
// 本测试在 TSan 下运行（-fsanitize=thread）验证无数据竞争。
#include "astrocs/core/context.h"

#include <atomic>
#include <chrono>
#include <cstdio>
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

static DataArtifactDescriptor make_artifact(const std::string& id) {
  DataArtifactDescriptor d;
  d.id.id = id;
  d.data_schema_id = "DATA-IMG-CAL-001";
  d.scalar = ScalarType::F32;
  d.unit = UnitId::ADU;
  d.shape.dims = {8, 8};
  d.storage = "run/x.fits";
  return d;
}

// ── 多 node 并发 log/metrics/store/checkpoint ──
static void test_concurrent_nodes() {
  const int kN = 8;  // 8 个并发 node
  RunContext ctx;
  std::vector<std::thread> ts;
  std::atomic<int> store_ok{0};

  for (int i = 0; i < kN; ++i) {
    ts.emplace_back([&, i] {
      for (int j = 0; j < 200; ++j) {
        ctx.log(LogLevel::INFO, "node" + std::to_string(i), "msg" + std::to_string(j));
        ctx.add_metric("m" + std::to_string(i), static_cast<uint64_t>(j));
      }
      Metrics m;
      m.wall_us = static_cast<uint64_t>(i);
      ctx.record_tick(m);
      ctx.mark_checkpoint("cp" + std::to_string(i));
      // 唯一 producer 写：每 node 一个不同 id → 全部成功
      if (ctx.store_artifact(make_artifact("sha256:n" + std::to_string(i))).ok()) {
        store_ok.fetch_add(1);
      }
    });
  }
  for (auto& t : ts) t.join();

  CHECK(store_ok.load() == kN);
  CHECK(ctx.artifact_ids().size() == static_cast<size_t>(kN));
  CHECK(ctx.log_entries().size() == static_cast<size_t>(kN) * 200);
  CHECK(ctx.checkpoints().size() == static_cast<size_t>(kN));
  CHECK(ctx.ticks().size() == static_cast<size_t>(kN));
  CHECK(ctx.metrics().size() == static_cast<size_t>(kN));
  // 快照读取不持有内部引用（析构安全）
  auto logs = ctx.log_entries();
  CHECK(logs.size() == static_cast<size_t>(kN) * 200);
}

// ── duplicate 冲突确定性：同一 id 双写只有一次成功 ──
static void test_duplicate_conflict() {
  RunContext ctx;
  std::atomic<int> ok1{0}, ok2{0};
  std::thread a([&] {
    if (ctx.store_artifact(make_artifact("sha256:dup")).ok()) ok1.fetch_add(1);
  });
  std::thread b([&] {
    if (ctx.store_artifact(make_artifact("sha256:dup")).ok()) ok2.fetch_add(1);
  });
  a.join();
  b.join();
  CHECK(ok1.load() + ok2.load() == 1);  // 恰好一次成功（唯一 producer）
  CHECK(ctx.artifact_ids().size() == 1);
}

// ── 取消原子：并发 cancel 与 cancelled 检查无竞争 ──
static void test_concurrent_cancel() {
  RunContext ctx;
  std::vector<std::thread> ts;
  std::atomic<int> cancels{0};
  for (int i = 0; i < 4; ++i) {
    ts.emplace_back([&, i] {
      if (i % 2 == 0) {
        ctx.cancel_token().cancel();
        cancels.fetch_add(1);
      } else {
        (void)ctx.cancelled();
      }
    });
  }
  for (auto& t : ts) t.join();
  CHECK(ctx.cancelled());
  CHECK(cancels.load() == 2);
}

int main() {
  test_concurrent_nodes();
  test_duplicate_conflict();
  test_concurrent_cancel();
  if (failures) {
    std::fprintf(stderr, "RT-003_FAIL failures=%d\n", failures);
    return 1;
  }
  std::printf("RT-003_PASS\n");
  return 0;
}
