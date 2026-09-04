#!/usr/bin/env python3
"""RT-007 验收测试: 统一取消 token / 错误传播 / 节点失败 / 临时产物清理 / 恢复边界。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-007; commit:
  feat(runtime): RT-007 固化取消失败与恢复语义):
  - 统一取消 token: Scheduler::cancel() 同时置位调度器 cancel_ 与当前运行
    RunContext 的 CancellationToken; run 中取消 → run 返回 CANCELLED; 取消
    latch 后运行中/未运行节点不得标 COMPLETED (无 false-success);
    run() 起始清除陈旧取消位 → 历史取消不跨 run 泄漏; run 外 cancel() 安全
    (active_token_ 已清空, 无悬垂);
  - 错误传播: 节点 fn 抛 C++ 异常 → 节点 FAILED(INTERNAL), 依赖 SKIPPED,
    独立节点 COMPLETED, run 失败不杀死 worker; 显式 Result 失败保留原始错误域;
  - 节点失败无半成品: FAILED 根 + 传递依赖 SKIPPED + 独立节点完成;
  - 临时产物清理: 同 Scheduler 取消 run 后可再次 run() 全新成功 (队列不残留、
    lease RAII、worker 可复用);
  - 恢复边界: CheckpointStore phase scope 隔离 (不同 scope 互不可见; 同 store
    中途切换 run_id/scope → 拒绝; 无产物 commit = 半成品拒绝);
    resume 只使用 hash/schema 匹配的已发布 artifact (validate_resume_inputs:
    id 未发布 / schema 不匹配 / hash 不匹配 → DATA 拒绝)。

方法 (照 tests/runtime/test_rt006_trace.py 先例):
  Python unittest 内嵌 C++ driver, g++ 真实编译链接 lib/core/src
  (context/scheduler/checkpoint/artifact/artifact_store) + include 头, 运行断言。
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
INC = REPO / "include"
CORE = REPO / "lib" / "core" / "src"
TP = REPO / "third_party"

_DRIVER = r'''
// RT-007 harness: 取消/错误传播/checkpoint scope/resume 门真实编译链接验收
#include "astrocs/core/artifact.h"
#include "astrocs/core/artifact_store.h"
#include "astrocs/core/checkpoint.h"
#include "astrocs/core/context.h"
#include "astrocs/core/scheduler.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace astrocs::core;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, msg)                                              \
  do {                                                                \
    ++g_checks;                                                       \
    if (!(cond)) {                                                    \
      std::printf("FAIL[%s:%d] %s\n", __func__, __LINE__, msg);       \
      ++g_failures;                                                   \
    }                                                                 \
  } while (0)

static NodeStatus status_of(
    const std::vector<std::pair<std::string, NodeStatus>>& st,
    const std::string& id) {
  for (const auto& [n, s] : st) {
    if (n == id) return s;
  }
  return NodeStatus::PLANNED;  // 未列出视为 PLANNED（防御: 不应出现）
}

// ── 场景 1: cancel → 无 false-success; 同 Scheduler 复用不残留 ──
static void scenario_cancel() {
  Scheduler sch(2u, 2u);
  RunContext ctx;

  std::mutex mu;
  std::condition_variable cv;
  bool a_entered = false;
  bool release_a = false;

  sch.add_node(Scheduler::NodeSpec{
      "A", {}, [&](const std::string&, RunContext&) -> Result<void> {
        {
          std::lock_guard<std::mutex> lk(mu);
          a_entered = true;
        }
        cv.notify_all();
        std::unique_lock<std::mutex> lk(mu);
        cv.wait_for(lk, std::chrono::seconds(15),
                    [&] { return release_a; });
        // fn 自身不检查取消: 由 scheduler 的取消观察 latch 处理
        return Result<void>::success();
      },
      "cpu_heavy", 0, 1, 1});
  sch.add_node(Scheduler::NodeSpec{
      "B", {}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});

  CHECK(sch.build().ok(), "build cancel graph");

  std::vector<std::pair<std::string, NodeStatus>> st;
  Result<void> rr;
  std::thread runner([&] { rr = sch.run(ctx, &st); });

  // 等 A 真正进入 fn 后再取消（确定性: cancel 必落在 run 内 A 在途时）
  {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait_for(lk, std::chrono::seconds(15), [&] { return a_entered; });
  }
  CHECK(a_entered, "A entered before cancel");
  sch.cancel();  // 外部线程取消: 桥到 ctx token + 置位调度器取消位
  {
    std::lock_guard<std::mutex> lk(mu);
    release_a = true;
  }
  cv.notify_all();
  runner.join();

  CHECK(rr.failed(), "cancel run must fail");
  CHECK(rr.error().domain() == ErrorDomain::CANCELLED,
        "cancel run error domain CANCELLED");
  CHECK(ctx.cancelled(), "ctx token cancelled by Scheduler::cancel bridge");
  // 运行中节点(A)在取消后返回 → 不得 COMPLETED (无 false-success)
  CHECK(status_of(st, "A") != NodeStatus::COMPLETED,
        "in-flight node A not COMPLETED after cancel");
  CHECK(status_of(st, "A") == NodeStatus::CANCELLED,
        "in-flight node A status CANCELLED");
  CHECK(status_of(st, "B") != NodeStatus::RUNNING, "no node left RUNNING");

  // 同 Scheduler 复用: 陈旧取消已被 run() 起始清除 → 再次 run 全新成功
  RunContext ctx2;
  std::vector<std::pair<std::string, NodeStatus>> st2;
  Result<void> rr2 = sch.run(ctx2, &st2);
  CHECK(rr2.ok(), "rerun same scheduler succeeds (no stale cancel leak)");
  CHECK(!ctx2.cancelled(), "fresh ctx2 token not cancelled");
  CHECK(status_of(st2, "A") == NodeStatus::COMPLETED, "A COMPLETED on rerun");
  CHECK(status_of(st2, "B") == NodeStatus::COMPLETED, "B COMPLETED on rerun");

  // run 外 cancel() 安全: 无活动 run → 只置位 cancel_, 不触碰悬垂 token
  Scheduler idle(2u, 2u);
  idle.add_node(Scheduler::NodeSpec{
      "X", {}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});
  idle.build();
  idle.cancel();  // run 外取消（active_token_ == nullptr）: 不得崩溃
  RunContext ctx3;
  Result<void> rr3 = idle.run(ctx3, nullptr);
  CHECK(rr3.ok(), "cancel before run is cleared at run() start");
}

// ── 场景 2: 节点 fn 抛 C++ 异常 → FAILED(INTERNAL) + dep SKIPPED + 独立 COMPLETED ──
static void scenario_exception_propagation() {
  Scheduler sch(2u, 2u);
  sch.add_node(Scheduler::NodeSpec{
      "A", {}, [&](const std::string&, RunContext&) -> Result<void> {
        throw std::runtime_error("rt007 boom");
      },
      "cpu_heavy", 0, 1, 1});
  sch.add_node(Scheduler::NodeSpec{
      "B", {"A"}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});
  sch.add_node(Scheduler::NodeSpec{
      "C", {}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});
  CHECK(sch.build().ok(), "build exception graph");

  RunContext ctx;
  std::vector<std::pair<std::string, NodeStatus>> st;
  Result<void> rr = sch.run(ctx, &st);
  CHECK(rr.failed(), "exception node run fails");
  CHECK(rr.error().domain() == ErrorDomain::INTERNAL,
        "C++ exception maps to INTERNAL domain");
  CHECK(rr.error().message().find("rt007 boom") != std::string::npos,
        "exception message preserved");
  CHECK(status_of(st, "A") == NodeStatus::FAILED, "throwing node A FAILED");
  CHECK(status_of(st, "B") == NodeStatus::SKIPPED, "dep B SKIPPED");
  CHECK(status_of(st, "C") == NodeStatus::COMPLETED,
        "independent node C COMPLETED");
  // worker 未死/未泄漏: scheduler 状态机正常收尾即证明
}

// ── 场景 3: 显式 Result 失败保留原始错误域 (IO) + dep SKIPPED + 独立 COMPLETED ──
static void scenario_result_domain_preserved() {
  Scheduler sch(2u, 2u);
  sch.add_node(Scheduler::NodeSpec{
      "P", {}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::fail(
            Error(ErrorDomain::IO, "disk write failed"));
      },
      "io", 0, 1, 1});
  sch.add_node(Scheduler::NodeSpec{
      "Q", {"P"}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});
  sch.add_node(Scheduler::NodeSpec{
      "R", {}, [&](const std::string&, RunContext&) -> Result<void> {
        return Result<void>::success();
      },
      "cpu_light", 0, 1, 1});
  CHECK(sch.build().ok(), "build result-domain graph");

  RunContext ctx;
  std::vector<std::pair<std::string, NodeStatus>> st;
  Result<void> rr = sch.run(ctx, &st);
  CHECK(rr.failed(), "result-failure run fails");
  CHECK(rr.error().domain() == ErrorDomain::IO,
        "explicit Result failure keeps original IO domain");
  CHECK(rr.error().message().find("disk write failed") != std::string::npos,
        "IO failure message preserved");
  CHECK(status_of(st, "P") == NodeStatus::FAILED, "P FAILED");
  CHECK(status_of(st, "Q") == NodeStatus::SKIPPED, "dep Q SKIPPED");
  CHECK(status_of(st, "R") == NodeStatus::COMPLETED, "independent R COMPLETED");
}

// ── 场景 4: CheckpointStore phase scope 隔离 / 中途切换拒绝 / 半成品拒绝 ──
static void scenario_checkpoint_scope() {
  CheckpointStore cp;
  CHECK(cp.begin("run1", "phase2").ok(), "begin run1 phase2");
  // 空产物 commit = 半成品 → 拒绝
  CHECK(cp.commit_node("n1", {}).failed(), "half-done commit rejected");
  CHECK(cp.commit_node("n1", {"a1"}).ok(), "commit n1 a1 in phase2");
  CHECK(cp.commit_node("n2", {"a2"}).ok(), "commit n2 a2 in phase2");

  CHECK(cp.is_committed_in_scope("n1", "phase2"), "n1 visible in phase2");
  CHECK(!cp.is_committed_in_scope("n1", "phase3"),
        "n1 invisible in other scope (phase3 isolation)");
  CHECK(!cp.is_committed_in_scope("n2", "phase3"),
        "n2 invisible in other scope (phase3 isolation)");
  CHECK(cp.is_committed("n1"), "is_committed uses current scope (phase2)");

  const auto nodes = cp.committed_nodes();
  CHECK(nodes.size() == 2, "committed_nodes only current scope entries");
  CHECK(nodes[0] == "n1" && nodes[1] == "n2", "committed_nodes order n1,n2");

  // 同 store 中途切换 run_id → 拒绝
  CHECK(cp.begin("run2", "phase2").failed(), "run_id switch mid-run rejected");
  // 同 store 中途切换 scope → 拒绝 (阶段 run 不能误读/串写另一阶段)
  CHECK(cp.begin("run1", "phase3").failed(), "scope switch mid-run rejected");

  // 幂等 replay: 同 run/scope 重复提交 → 覆盖, 不报错
  CHECK(cp.replay_same_run("n1", {"a1"}).ok(), "replay same run idempotent");

  // 跨 store 天然隔离: 新 store 的 phase3 看不到 phase2 提交
  CheckpointStore cp2;
  CHECK(cp2.begin("run9", "phase3").ok(), "begin run9 phase3");
  CHECK(!cp2.is_committed_in_scope("n1", "phase3"),
        "other store cannot see phase2 checkpoint");
  CHECK(cp2.commit_node("m1", {"b1"}).ok(), "commit m1 in phase3");
  CHECK(cp2.is_committed_in_scope("m1", "phase3"), "m1 visible in phase3");
  CHECK(!cp.is_committed_in_scope("m1", "phase2"),
        "phase2 store cannot see phase3 checkpoint m1");
}

// ── 场景 5: resume 门 —— 只使用 hash/schema 匹配的已发布 artifact ──
static ArtifactDescriptor make_hips(const std::string& id,
                                    const std::string& schema_id,
                                    uint64_t version,
                                    const std::string& sha) {
  ArtifactDescriptor d;
  d.id.id = id;
  d.role = ArtifactRole::P2_HIPS;
  d.data_schema_id = schema_id;
  d.schema_version = version;
  d.path_or_uri = "/run/rt007/fake.hips";
  d.size_bytes = 42;
  d.content_sha256 = sha;
  d.producer_node = "hips_gen";
  d.producer_module = "astrocs.phase2";
  d.producer_version = "0.1.0";
  d.source_commit = "b2e3b0af";
  d.created_utc = "2026-09-03T00:00:00.000Z";
  return d;
}

static void scenario_resume_gate() {
  const std::string sha_ok(64, 'a');
  const std::string sha_bad(64, 'b');

  ArtifactStore store;
  CHECK(store.store(make_hips("hips:cal:v1", "DATA-CAT", 1, sha_ok)).ok(),
        "store published P2 hips");

  // 全部匹配 → 放行
  CHECK(validate_resume_inputs(
            store,
            {{"hips:cal:v1", "DATA-CAT", 1, sha_ok}}, "consumer_x")
            .ok(),
        "matching schema+version+hash passes resume gate");

  // id 未发布 → 拒绝
  CHECK(validate_resume_inputs(
            store,
            {{"hips:missing", "DATA-CAT", 1, sha_ok}}, "consumer_x")
            .failed(),
        "unpublished artifact id rejected");
  // schema id 不匹配 → 拒绝
  CHECK(validate_resume_inputs(
            store,
            {{"hips:cal:v1", "DATA-OTHER", 1, sha_ok}}, "consumer_x")
            .failed(),
        "schema id mismatch rejected");
  // schema version 不匹配 → 拒绝
  CHECK(validate_resume_inputs(
            store,
            {{"hips:cal:v1", "DATA-CAT", 2, sha_ok}}, "consumer_x")
            .failed(),
        "schema version mismatch rejected");
  // content hash 不匹配 → 拒绝 (禁止凭 id/文件名猜内容后恢复)
  CHECK(validate_resume_inputs(
            store,
            {{"hips:cal:v1", "DATA-CAT", 1, sha_bad}}, "consumer_x")
            .failed(),
        "content hash mismatch rejected");

  // 错误域为 DATA
  auto r = validate_resume_inputs(
      store, {{"hips:cal:v1", "DATA-CAT", 9, sha_ok}}, "consumer_x");
  CHECK(r.failed() && r.error().domain() == ErrorDomain::DATA,
        "resume rejection is DATA domain");
}

int main(int argc, char** argv) {
  const std::string which = argc > 1 ? argv[1] : "all";
  if (which == "all" || which == "cancel") scenario_cancel();
  if (which == "all" || which == "throw") scenario_exception_propagation();
  if (which == "all" || which == "domain") scenario_result_domain_preserved();
  if (which == "all" || which == "checkpoint") scenario_checkpoint_scope();
  if (which == "all" || which == "resume") scenario_resume_gate();

  std::printf("RT-007 checks=%d failures=%d\n", g_checks, g_failures);
  if (g_failures == 0) {
    std::printf("RT-007_ALL_PASS\n");
    return 0;
  }
  std::printf("RT-007_FAIL\n");
  return 1;
}
'''


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestRt007CancelResumeCpp(unittest.TestCase):
    """C++ harness：真实编译链接 lib/core 源码运行 RT-007 全部验收断言。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt007_"))
        cls.exe = cls.build_driver(cls.tmp)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @staticmethod
    def build_driver(tmp: pathlib.Path) -> pathlib.Path:
        drv = tmp / "rt007_driver.cpp"
        drv.write_text(_DRIVER, encoding="utf-8")
        exe = tmp / "rt007_cancel_resume"
        # RT-007 最小链接集: scheduler/context(checkpoint 记录+取消/预算)/checkpoint/
        # artifact/artifact_store (纯 C++ harness; 不需要 runtime/executor/module)
        srcs = ("context.cpp", "scheduler.cpp", "checkpoint.cpp",
                "artifact.cpp", "artifact_store.cpp")
        objs = []
        for src in srcs:
            o = tmp / (src + ".o")
            r = subprocess.run(
                ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-c",
                 str(CORE / src), f"-I{INC}", f"-I{TP}", "-o", str(o)],
                capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                raise RuntimeError(f"compile {src} failed:\n{r.stderr[-2000:]}")
            objs.append(str(o))
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", str(drv), f"-I{INC}", f"-I{TP}",
             *objs, "-pthread", "-o", str(exe)],
            capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            raise RuntimeError(f"link driver failed:\n{r.stderr[-2000:]}")
        return exe

    def run_scenario(self, scenario: str) -> str:
        r = subprocess.run([str(self.exe), scenario], capture_output=True,
                           text=True, timeout=180)
        self.assertEqual(r.returncode, 0,
                         f"scenario {scenario} rc={r.returncode}\n"
                         f"stdout:\n{r.stdout[-3000:]}\nstderr:\n{r.stderr[-3000:]}")
        return r.stdout

    def test_rt007_01_cancel_no_false_success(self):
        """cancel → CANCELLED run, 在途节点不 COMPLETED; 同 Scheduler 复用不残留。"""
        out = self.run_scenario("cancel")
        self.assertIn("RT-007_ALL_PASS", out)

    def test_rt007_02_exception_internal_propagation(self):
        """节点 fn 抛 C++ 异常 → FAILED(INTERNAL) + dep SKIPPED + 独立 COMPLETED。"""
        out = self.run_scenario("throw")
        self.assertIn("RT-007_ALL_PASS", out)

    def test_rt007_03_result_domain_preserved(self):
        """显式 Result 失败保留原始错误域 (IO), 依赖 SKIPPED, 独立节点完成。"""
        out = self.run_scenario("domain")
        self.assertIn("RT-007_ALL_PASS", out)

    def test_rt007_04_checkpoint_scope_isolation(self):
        """Checkpoint phase scope 隔离; 中途切换/半成品拒绝; 跨 store 互不可见。"""
        out = self.run_scenario("checkpoint")
        self.assertIn("RT-007_ALL_PASS", out)

    def test_rt007_05_resume_gate_hash_schema(self):
        """resume 只使用 hash/schema 匹配的已发布 artifact; 任何不一致 → DATA 拒绝。"""
        out = self.run_scenario("resume")
        self.assertIn("RT-007_ALL_PASS", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
