// RT-009 运行图数据源测试: run_pipeline 后 collect_node_trace 提供
// 节点级 status/started_utc/ended_utc/duration_ms/workers/provider；
// last_pipeline_ir_json 提供静态图 IR。走真实 CLI Runtime 路径（非假模块）。
#include "runtime_client.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void test_trace_after_run() {
  // phase1 真实配置（cal 节点; 输入缺失时 exit 3; 用最小合法配置走成功路径）
  std::string cfg = R"({"output_dir":"/tmp/rt9_unit","inputs":{"lights":[]},"darks":[],"flats":[],"bias":[]})";
  std::string fr;
  // inputs.lights 为空 → session validate 拒绝（DATA→2）；trace 仍应捕获节点 FAILED 记录
  int rc = astrocs::cli::run_pipeline({1}, cfg, 2, &fr);
  CHECK(rc == 2);
  std::vector<astrocs::core::Runtime::NodeTrace> tr;
  astrocs::cli::collect_node_trace(&tr);
  CHECK(!tr.empty());  // 失败也捕获 trace
  bool saw_cal = false;
  for (const auto& t : tr) {
    if (t.node_id != "cal") continue;
    saw_cal = true;
    CHECK(t.status == "FAILED");                       // 空输入 → validate 失败
    CHECK(!t.started_utc.empty());
    CHECK(!t.ended_utc.empty());
    CHECK(t.duration_ms >= 0);
    CHECK(t.workers == 2);
    CHECK(!t.provider.empty());
  }
  CHECK(saw_cal);
  // IR 已保留（静态图来源）
  CHECK(!astrocs::cli::last_pipeline_ir_json().empty());
  CHECK(astrocs::cli::last_pipeline_ir_json().find("\"cal\"") != std::string::npos);
}

static void test_validate_failure_no_manifest() {
  // validate 失败（空输入）→ session 未 run → 不捕获 manifest（execute 只在
  // validate 通过后才 inspect 捕获）；trace 仍记录节点 FAILED。
  std::string cfg = R"({"output_dir":"/tmp/rt9_unit2","inputs":{"lights":[]},"darks":[],"flats":[],"bias":[]})";
  std::string fr;
  int rc = astrocs::cli::run_pipeline({1}, cfg, 2, &fr);
  CHECK(rc == 2);
  std::vector<std::pair<std::string, std::string>> mans;
  astrocs::cli::collect_node_manifests(&mans);
  CHECK(mans.empty());  // validate 失败 → 无 manifest（文档化行为）
  std::vector<astrocs::core::Runtime::NodeTrace> tr;
  astrocs::cli::collect_node_trace(&tr);
  bool saw_cal = false;
  for (const auto& t : tr) {
    if (t.node_id == "cal") { saw_cal = true; CHECK(t.status == "FAILED"); }
  }
  CHECK(saw_cal);
}

int main() {
  test_trace_after_run();
  test_validate_failure_no_manifest();
  if (failures == 0) {
    std::printf("RT-009_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RT-009_FAIL failures=%d\n", failures);
  return 1;
}
