// CORE-007 单元测试: checkpoint 幂等恢复 / 半成品拒绝
#include "astrocs/core/checkpoint.h"

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

static void test_begin_commit() {
  CheckpointStore cs;
  CHECK(cs.begin("run-1").ok());
  CHECK(cs.commit_node("node-a", {"sha256:aaa"}).ok());
  CHECK(cs.commit_node("node-b", {"sha256:bbb", "sha256:ccc"}).ok());
  CHECK(cs.size() == 2);
  CHECK(cs.is_committed("node-a"));
  CHECK(!cs.is_committed("node-x"));
  CHECK(cs.committed_nodes().size() == 2);
}

static void test_half_done_rejected() {
  CheckpointStore cs;
  CHECK(cs.begin("run-2").ok());
  auto r = cs.commit_node("node-c", {});
  CHECK(r.failed());  // 无 artifact = 半成品, 拒绝
  CHECK(r.error().message().find("half-done") != std::string::npos);
  CHECK(cs.size() == 0);
}

static void test_begin_required() {
  CheckpointStore cs;
  auto r = cs.commit_node("node-d", {"sha256:ddd"});
  CHECK(r.failed());  // begin 未调用
  CHECK(r.error().message().find("begin") != std::string::npos);
}

static void test_run_id_change_rejected() {
  CheckpointStore cs;
  CHECK(cs.begin("run-3").ok());
  CHECK(cs.commit_node("node-e", {"sha256:eee"}).ok());
  auto r = cs.begin("run-other");
  CHECK(r.failed());  // run_id 中途变更拒绝
}

static void test_replay_idempotent() {
  CheckpointStore cs;
  CHECK(cs.begin("run-4").ok());
  CHECK(cs.commit_node("node-f", {"sha256:f1"}).ok());
  // 故障后重放: 同 run_id 重复提交同节点 -> 幂等覆盖
  CHECK(cs.replay_same_run("node-f", {"sha256:f1"}).ok());
  CHECK(cs.size() == 1);  // 不新增条目
  CHECK(cs.committed_nodes()[0] == "node-f");
}

static void test_replay_new_node_commits() {
  CheckpointStore cs;
  CHECK(cs.begin("run-5").ok());
  CHECK(cs.replay_same_run("node-new", {"sha256:n1"}).ok());
  CHECK(cs.size() == 1);
  CHECK(cs.is_committed("node-new"));
}

int main() {
  test_begin_commit();
  test_half_done_rejected();
  test_begin_required();
  test_run_id_change_rejected();
  test_replay_idempotent();
  test_replay_new_node_commits();
  if (failures == 0) {
    std::printf("CORE-007 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-007 TESTS FAIL (%d)\n", failures);
  return 1;
}
