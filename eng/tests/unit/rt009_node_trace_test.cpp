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
  // P1-001 后: 节点 FAILED 触发面=io 阶段不可读 light（error_kind=input）。
  // 退出码映射: INPUT→3（与 CLI 04 合同一致; 旧空-lights DATA→2 面随
  // P1NodeModule "空=0 帧作业"语义变更移至 CLI validate_config_full 面）。
  std::string cfg = R"({"input_lights":["/nonexistent/nope.fits"],"output_dir":"/tmp/rt9_unit"})";
  std::string fr;
  // cal 节点 execute 失败（INPUT→3）；trace 仍应捕获节点 FAILED 记录
  int rc = astrocs::cli::run_pipeline({1}, cfg, 2, &fr);
  CHECK(rc == 3);
  std::vector<astrocs::core::Runtime::NodeTrace> tr;
  astrocs::cli::collect_node_trace(&tr);
  CHECK(!tr.empty());  // 失败也捕获 trace
  bool saw_cal = false;
  for (const auto& t : tr) {
    if (t.node_id != "cal") continue;
    saw_cal = true;
    CHECK(t.status == "FAILED");                       // io 读取失败 → execute 失败
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
  // P1-001 后: execute 失败（io error_kind=input）→ 节点 manifest 仍被
  // 捕获（fail-closed manifest 供 observed graph 消费）且 trace 记录
  // 节点 FAILED。
  std::string cfg = R"({"input_lights":["/nonexistent/nope.fits"],"output_dir":"/tmp/rt9_unit"})";
  std::string fr;
  int rc = astrocs::cli::run_pipeline({1}, cfg, 2, &fr);
  CHECK(rc == 3);
  std::vector<std::pair<std::string, std::string>> mans;
  astrocs::cli::collect_node_manifests(&mans);
  bool saw_input_kind = false;
  for (const auto& [nid, mtext] : mans) {
    if (nid != "cal") continue;
    if (mtext.find("input") != std::string::npos) saw_input_kind = true;
  }
  CHECK(!mans.empty());
  CHECK(saw_input_kind);  // 失败 manifest 带 error_kind=input（fail-closed）
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
