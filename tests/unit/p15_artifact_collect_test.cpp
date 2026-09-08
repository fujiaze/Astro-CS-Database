// P1-5 单元测试: phase2 run manifest artifacts 收集（collect_node_artifact_paths）
//
// 缺陷回归: cmd_phase2_run 旧实现按 node_id=="res" 过滤节点 manifest, 而 P2-006
// Canonical Phase2 IR 7 节点链的 node id 是 coverage/sample/upm_fit/upm_apply/
// reject/integrate/write（runtime_client.cpp build_pipeline_ir）, "res" 已随 CLI-002
// 拆分移除 → 过滤恒真 → run manifest artifacts 恒空而 status 仍标 complete。
// 修复后按 manifest 内容收集（与 phase3 收集同型）: 任何节点 manifest 带
// artifacts 数组即收集, 链式节点共享同一 session 产物 → 按路径去重。
//
// 取舍说明: 本测试在收集函数层级注入 IR 结果（node manifest 文本 + 真实临时
// 文件）, 不做端到端 phase2 run（资源门禁 MON-002 对共享 CI 机的负载敏感, 端到端
// 断言 rc=0 会抖动; sha256/size_bytes 的真实证据由 tests/cli/test_phase2_inprocess.py
// 的 run manifest 证据断言覆盖, collect_node_artifact_paths 只产路径）。
#include "runtime_client.h"

#include <cstdio>
#include <fstream>
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

using astrocs::cli::collect_node_artifact_paths;
using Manifests = std::vector<std::pair<std::string, std::string>>;

// 真实 P2 session manifest 形态（p2_session inspect 摘要）: 链式节点共享同一
// session 产物, 本例以 persist_upm 落盘的 UPM 模型为工件。
static std::string p2_manifest(const std::string& artifact_path) {
  return R"({"kind":"astrocs_phase2_session",)"
         R"("stages":[{"name":"coverage","status":"ok"},{"name":"sample","status":"ok"},)"
         R"({"name":"upm_build","status":"ok"},{"name":"persist","status":"ok"}],)"
         R"("n_inputs":2,"n_obs":1536,"status":"complete",)" +
         std::string(R"("artifacts":[")") + artifact_path + R"("]})";
}

static void test_collects_from_real_ir_node_ids() {
  // P2-006 IR 链真实 node id（runtime_client.cpp build_pipeline_ir phase2_nodes）
  const Manifests mans = {
      {"coverage", p2_manifest("/tmp/a.bin")},
      {"sample", p2_manifest("/tmp/a.bin")},
      {"upm_fit", p2_manifest("/tmp/a.bin")},
      {"upm_apply", p2_manifest("/tmp/a.bin")},
      {"reject", p2_manifest("/tmp/a.bin")},
      {"integrate", p2_manifest("/tmp/a.bin")},
      {"write", p2_manifest("/tmp/a.bin")},
  };
  const auto paths = collect_node_artifact_paths(mans);
  CHECK(paths.size() == 1);  // 7 链式节点共享同一产物 → 按 path 去重
  CHECK(!paths.empty() && paths[0] == "/tmp/a.bin");
}

static void test_no_node_id_filtering() {
  // 回归核心: 旧实现只认 node_id=="res"（已不存在的 id）→ 恒空。
  // 修复后任意 node id（含历史遗留 "res"）的 manifest 都按内容收集。
  const Manifests mans = {{"res", p2_manifest("/tmp/x.bin")},
                          {"write", p2_manifest("/tmp/y.bin")}};
  const auto paths = collect_node_artifact_paths(mans);
  CHECK(paths.size() == 2);
  CHECK(paths[0] == "/tmp/x.bin");
  CHECK(paths[1] == "/tmp/y.bin");
}

static void test_real_file_artifact_evidence() {
  // 注入真实文件路径（收集函数产路径; 文件级 sha/size 证据由调用方收集, 见
  // cmd_phase2_run 对每条路径做 file_sha256+file_size 后入 run manifest）。
  const std::string dir = "/tmp/astrocs_p15_test";
  std::system("mkdir -p /tmp/astrocs_p15_test");
  const std::string file = dir + "/upm_model.bin";
  {
    std::ofstream f(file, std::ios::binary);
    f << "P1-5 artifact evidence payload";
  }
  const Manifests mans = {{"write", p2_manifest(file)}};
  const auto paths = collect_node_artifact_paths(mans);
  CHECK(paths.size() == 1);
  CHECK(!paths.empty() && paths[0] == file);
  std::system("rm -rf /tmp/astrocs_p15_test");
}

static void test_malformed_and_non_string_entries_skipped() {
  const Manifests mans = {
      {"write", R"({"artifacts":[1,2],"status":"complete"})"},  // 非字符串条目跳过
      {"write", "{oops"},                                        // 解析失败跳过
      {"write", R"("not an object")"},                           // 非对象跳过
      {"write", R"({"status":"complete"})"},                     // 无 artifacts 键
      {"write", p2_manifest("/tmp/only_real.bin")},              // 真实条目收集
  };
  const auto paths = collect_node_artifact_paths(mans);
  CHECK(paths.size() == 1);
  CHECK(!paths.empty() && paths[0] == "/tmp/only_real.bin");
}

int main() {
  test_collects_from_real_ir_node_ids();
  test_no_node_id_filtering();
  test_real_file_artifact_evidence();
  test_malformed_and_non_string_entries_skipped();
  if (failures == 0) {
    std::printf("P15_ARTIFACT_COLLECT_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "P15_ARTIFACT_COLLECT_FAIL failures=%d\n", failures);
  return 1;
}
