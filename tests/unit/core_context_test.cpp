// CORE-005 单元测试: RunContext 服务接口
#include "astrocs/core/context.h"

#include <cstdio>
#include <string>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static DataArtifactDescriptor make_artifact() {
  DataArtifactDescriptor d;
  d.id.id = "sha256:xyz";
  d.data_schema_id = "DATA-IMG-CAL-001";
  d.scalar = ScalarType::F32;
  d.unit = UnitId::ADU;
  d.shape.dims = {16, 16};
  d.storage = "run/x.fits";
  return d;
}

static void test_log() {
  RunContext ctx;
  ctx.log(LogLevel::INFO, "phase1", "hello");
  ctx.log(LogLevel::ERROR, "phase1", "boom");
  CHECK(ctx.log_entries().size() == 2);
  CHECK(ctx.log_entries()[0].find("[INFO]") != std::string::npos);
  CHECK(ctx.log_entries()[1].find("ERROR") != std::string::npos);
}

static void test_budget_lease() {
  // RT-003: 真实 ThreadBudget 经 set_budget 注入 (scheduler 语义); 旧
  // set_thread_budget 仅记录计数, 不再作为伪授权源
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  RunContext ctx;
  ctx.set_budget(b.value());
  CHECK(ctx.thread_budget() == 4);
  auto lease = ctx.acquire_lease(8);
  CHECK(lease.size() == 4);  // 超预算请求 cap 到 budget (非伪授权)
  auto lease2 = ctx.acquire_lease(2);
  // 真实原子预算已用 4/4 → 空租约 (非伪授权 cap)
  CHECK(!lease2.acquired());
  { auto _ = std::move(lease); }  // RAII 归还
  auto lease3 = ctx.acquire_lease(2);
  CHECK(lease3.size() == 2);
}

static void test_artifact_store() {
  RunContext ctx;
  CHECK(ctx.store_artifact(make_artifact()).ok());
  CHECK(ctx.artifact_ids().size() == 1);
  DataArtifactDescriptor a;
  CHECK(ctx.get_artifact("sha256:xyz", &a));
  CHECK(a.data_schema_id == "DATA-IMG-CAL-001");
  // 不存在的 id → false（不返回内部指针）
  CHECK(!ctx.get_artifact("sha256:nope", nullptr));
  // duplicate rejected
  CHECK(ctx.store_artifact(make_artifact()).failed());
  // invalid rejected
  DataArtifactDescriptor bad = make_artifact();
  bad.shape.dims.clear();
  CHECK(ctx.store_artifact(bad).failed());
}

static void test_cancel() {
  RunContext ctx;
  CHECK(!ctx.cancelled());
  ctx.cancel_token().cancel();
  CHECK(ctx.cancelled());
}

static void test_checkpoint() {
  RunContext ctx;
  ctx.mark_checkpoint("node1");
  ctx.mark_checkpoint("node2");
  CHECK(ctx.checkpoints().size() == 2);
}

static void test_metrics() {
  RunContext ctx;
  ctx.add_metric("wall_us", 1000);
  Metrics m;
  m.wall_us = 42;
  ctx.record_tick(m);
  CHECK(ctx.metrics().at("wall_us") == 1000);
  CHECK(ctx.ticks().size() == 1);
}

int main() {
  test_log();
  test_budget_lease();
  test_artifact_store();
  test_cancel();
  test_checkpoint();
  test_metrics();
  if (failures == 0) {
    std::printf("CORE-005 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-005 TESTS FAIL (%d)\n", failures);
  return 1;
}
