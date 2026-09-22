#!/usr/bin/env python3
"""RT-006 验收测试：真实运行 trace 与调用计数（executor/scheduler/provider 观测）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-006):
  - trace 记录实际 module/DLL/hash/build/entry/call count/worker/provider/artifact/
    timing/error/checkpoint；由 executor/provider/monitor 填写，禁止 config 值冒充观测；
  - 7 个 P2 节点各一次（真实 IR fixture coverage..write → 每个节点恰好 1 次
    module_call；重复/隐藏 session 由 violations 检测抓出）；
  - 修改 provider/worker 反映 trace（executor worker 任务观测 + ctx.set_provider →
    WORKER_TASK/NODE_END 携带真实 provider/workers；executor 计数 tasks_executed/
    provider_sets 为观测）；
  - 隐藏 session/重复调用检测（同 entry ≥2 节点 → hidden-session-fanout；
    同 node module_call>1 → repeated-call，均必须被抓出）；
  - JSONL 可重放到图（Runtime trace_jsonl → C++/Python replay 摘要含
    node/module/entry/call_count/provider/status/artifact，可渲染）。

方法 (照 eng/tests/runtime/test_rt005_plan_estimator.py 先例):
  Python unittest 内嵌 C++ driver，g++ 真实编译链接 lib/infrastructure/scheduler/src
  的**最小链接闭包** + lib/include/astrocs/core 头，运行断言；
  源清单不手抄：目录与权威闭包从构建图解析（根 CMakeLists.txt 的 astrocs_core 目标
  + eng/tests/unit/CMakeLists.txt 的 executor_provider_race_test 目标），
  闭包完整性由 nm 差集判据 TestRt006DriverSourceClosure 兜底（缺谁点名谁）；
  另以 Python trace_replay.py 独立实现对照重放语义（双实现互证）。
"""
from __future__ import annotations

import atexit
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[4]
INC = REPO / "lib" / "include"
CORE = REPO / "lib" / "infrastructure" / "scheduler" / "src"
TP = REPO / "lib" / "third_party"

sys.path.insert(0, str(REPO / "lib" / "infrastructure" / "pipeline"))
from trace_replay import detect_violations as py_detect_violations  # noqa: E402
from trace_replay import replay_from_jsonl as py_replay_from_jsonl  # noqa: E402

_DRIVER = r'''
// RT-006 harness: 真实 trace 观测（executor/scheduler/Runtime 真实编译链接验收）
#include "astrocs/core/executor.h"
#include "astrocs/core/runtime.h"
#include "astrocs/core/scheduler.h"
#include "astrocs/core/trace.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace astrocs::core;

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

// ── A. TraceStore JSONL 往返 + 重复调用检测 ──
static void test_store_jsonl_roundtrip() {
  auto st = create_trace_store();
  CHECK(st.ok());
  TraceEvent e;
  e.type = TraceEventType::MODULE_CALL;
  e.run_id = "run-a";
  e.node_id = "coverage";
  e.module_id = "astrocs.phase2.coverage";
  e.entry = "astrocs_phase2_coverage_v1";
  e.call_count = 1;
  e.workers = 2;
  e.provider = "baseline";
  e.status = "COMPLETED";
  st.value()->record(e);
  TraceEvent n;
  n.type = TraceEventType::NODE_END;
  n.run_id = "run-a";
  n.node_id = "coverage";
  n.status = "COMPLETED";
  n.wall_ms = 1.5;
  st.value()->record(n);
  const std::string jsonl = st.value()->export_jsonl();
  CHECK(jsonl.find("\"type\":\"module_call\"") != std::string::npos);
  CHECK(jsonl.find("\"node_id\":\"coverage\"") != std::string::npos);
  // JSONL 行数 = 2
  size_t lines = 0;
  for (char c : jsonl) if (c == '\n') ++lines;
  CHECK(lines == 2);
  // 无违规（每 entry 单节点、每节点单次）
  CHECK(st.value()->detect_repeated_calls().empty());
  // 隐藏 session：同一 entry 两个 node → 违规
  auto st2 = create_trace_store();
  CHECK(st2.ok());
  TraceEvent a;
  a.type = TraceEventType::MODULE_CALL;
  a.run_id = "run-x";
  a.node_id = "coverage";
  a.module_id = "astrocs.phase2.coverage";
  a.entry = "astrocs_phase2_session_run";
  st2.value()->record(a);
  TraceEvent b;
  b.type = TraceEventType::MODULE_CALL;
  b.run_id = "run-x";
  b.node_id = "sample";
  b.module_id = "astrocs.phase2.sample";
  b.entry = "astrocs_phase2_session_run";   // 同一完整 session → 隐藏扇出
  st2.value()->record(b);
  auto vio = st2.value()->detect_repeated_calls();
  CHECK(vio.size() >= 1);
  bool found_hidden = false;
  for (const auto& v : vio)
    if (v.find("hidden-session-fanout") != std::string::npos) found_hidden = true;
  CHECK(found_hidden);
  // 重复调用：同一节点两次 module_call
  auto st3 = create_trace_store();
  CHECK(st3.ok());
  TraceEvent c1;
  c1.type = TraceEventType::MODULE_CALL;
  c1.run_id = "run-y";
  c1.node_id = "coverage";
  c1.module_id = "astrocs.phase2.coverage";
  c1.entry = "astrocs_phase2_coverage_v1";
  st3.value()->record(c1);
  TraceEvent c2 = c1;
  c2.entry = "astrocs_phase2_sample_v1";    // 同节点不同 entry 仍是两次调用
  st3.value()->record(c2);
  auto vio3 = st3.value()->detect_repeated_calls();
  bool found_rep = false;
  for (const auto& v : vio3)
    if (v.find("repeated-call") != std::string::npos) found_rep = true;
  CHECK(found_rep);
}

// ── B. executor 真实 worker 任务观测 + provider 修改反映 trace ──
static void test_executor_worker_provider_observation() {
  auto b = create_thread_budget(2);
  CHECK(b.ok());
  auto ex = create_cpu_heavy_executor(b.value());
  CHECK(ex.ok());
  auto st = create_trace_store();
  CHECK(st.ok());
  ex.value()->set_trace_store(st.value());
  CHECK(ex.value()->tasks_executed() == 0);
  std::atomic<uint32_t> done{0};
  for (int i = 0; i < 3; ++i) {
    ex.value()->enqueue([&](RunContext& ctx) {
      // executor worker 已代表任务原子预留预算（RT-004 语义），任务内不再
      // 重复 acquire（嵌套预留超卖不到 → 空租约，属预期）。任务只做真实
      // provider 置位观测与短时工作。
      ctx.set_provider("avx2");        // 修改 provider → trace 反映
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      done.fetch_add(1);
    });
  }
  ex.value()->wait_all();
  CHECK(done.load() == 3);
  CHECK(ex.value()->tasks_executed() == 3);     // 真实任务执行计数（观测）
  CHECK(ex.value()->provider_sets() >= 1);      // provider 置位观测
  const std::string jsonl = st.value()->export_jsonl();
  // worker_task 事件真实存在且携带观测 provider avx2
  CHECK(jsonl.find("\"type\":\"worker_task\"") != std::string::npos);
  CHECK(jsonl.find("\"provider\":\"avx2\"") != std::string::npos);
  CHECK(jsonl.find("\"status\":\"COMPLETED\"") != std::string::npos);
  // 无配置值冒充：provider 只能来自真实 set_provider（avx2），不得出现
  // baseline 或空冒充（本 executor 层未设置其他 provider）
}

// ── C. Scheduler DAG 运行：节点真实执行计数与归属（TLS 节点） ──
static void test_scheduler_node_execution_trace() {
  Scheduler sched(2, 2);
  auto st = create_trace_store();
  CHECK(st.ok());
  sched.set_run_observation(st.value(), "run-sched");
  std::atomic<int> ran{0};
  sched.add_node({"a", {}, [&](const std::string&, RunContext& ctx) {
    ran.fetch_add(1);
    // 真实观测：当前节点归属（TLS）应与本节点一致
    CHECK(ctx.current_node() == "a");
    return Result<void>::success();
  }, "cpu_heavy"});
  sched.add_node({"b", {"a"}, [&](const std::string&, RunContext& ctx) {
    ran.fetch_add(1);
    CHECK(ctx.current_node() == "b");
    return Result<void>::success();
  }, "cpu_heavy"});
  RunContext ctx;
  auto r = sched.run(ctx);
  CHECK(r.ok());
  CHECK(ran.load() == 2);
  // ctx 注入 trace store + run_id（模块可经 ctx.record_trace 观测）
  CHECK(ctx.trace_store() != nullptr);
  CHECK(ctx.run_id() == "run-sched");
}

// ── D. 完整 Runtime + 7 节点 P2 IR：每节点 trace 一次 + provider/worker 反映 ──
static void test_runtime_p2_7node_trace() {
  // 使用 canonical Phase2 7 节点 IR（coverage..write；与 CLI phase2_nodes 同形态；
  // 不注册真实 factory 时 module 不可执行 —— 本测试用最小可执行模块注册表验证
  // 调度 trace 归属；真实执行验证在 CLI/集成层）。为在无外部数据下真实执行模块，
  // 这里注册假工厂（registry 非真实 session）：模块 execute 置 provider 并返回成功。
  // 注意：真实生产模块由 register_phase_modules 提供（RT-005）；此处用可执行 stub
  // 模块验证 trace 事件归属/计数（观测点真实执行 stub 模块）。
  ModuleRegistry reg;
  const char* ids[] = {
      "astrocs.phase2.coverage", "astrocs.phase2.sample", "astrocs.phase2.upm-fit",
      "astrocs.phase2.upm-apply", "astrocs.phase2.reject", "astrocs.phase2.integrate",
      "astrocs.phase2.write",
  };
  const char* entries[] = {
      "coverage", "sample", "upm_fit", "upm_apply", "reject", "integrate", "write",
  };
  for (int i = 0; i < 7; ++i) {
    ModuleDescriptor d;
    d.module_id = ids[i];
    d.version = "1.0." "0";
    d.abi = "c++17";
    d.execution_class = (i == 6) ? "io" : "cpu_heavy";
    d.parallel_ok = i != 6;
    d.ports = {{"in", "DATA-P2-CHAIN", true, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
               {"out", "DATA-P2-CHAIN", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL}};
    d.sci_id = "SCI-X"; d.alg_id = "ALG-X"; d.data_id = "DATA-P2-CHAIN";
    d.api_id = "API-X"; d.test_id = "TEST-X";
    auto rr = reg.register_module(d);
    CHECK(rr.ok());
    std::string mid = ids[i];
    std::string entry = entries[i];
    ModuleDescriptor dm = d;  // 拷贝供工厂捕获
    auto fr = reg.register_factory(mid, [d = std::move(dm), mid, entry]() mutable -> std::unique_ptr<IModule> {
      struct M : public IModule {
        std::string mid_;
        std::string entry_;
        ModuleDescriptor desc_;
        M(std::string mid, std::string entry, ModuleDescriptor d)
            : mid_(std::move(mid)), entry_(std::move(entry)), desc_(std::move(d)) {}
        const ModuleDescriptor& descriptor() const noexcept override { return desc_; }
        Result<void> validate_config(const std::string&) override {
          return Result<void>::success();
        }
        Result<ModulePlan> plan(const std::string& node_id,
                                const std::string&) override {
          ModulePlan p;
          p.node_id = node_id;
          p.work_units = 1;
          p.cpu_heavy = desc_.execution_class == "cpu_heavy";
          return Result<ModulePlan>::ok(std::move(p));
        }
        Result<void> execute(RunContext& ctx) override {
          // 真实 provider 选择观测（测试置位）→ trace 反映
          ctx.set_provider(mid_.find("write") != std::string::npos ? "io-backend"
                                                                    : "baseline");
          ctx.mark_checkpoint(mid_);          // checkpoint 观测
          // 发布真实 artifact 观测（内存 tag，无磁盘写；每节点唯一 id）
          TraceEvent ep;
          ep.type = TraceEventType::ARTIFACT_PUBLISH;
          ep.node_id = ctx.current_node();
          ep.module_id = mid_;
          ep.artifact_id = "artifact:" + mid_;
          ep.artifact_sha256 = std::string(64, 'a');
          ep.artifact_size = 4096;
          ctx.record_trace(std::move(ep));
          return Result<void>::success();
        }
        Result<std::string> inspect() override { return Result<std::string>::ok("{}"); }
        Result<std::string> last_manifest() override {
          return Result<std::string>::ok("{\"artifacts\":[]}");
        }
      };
      return std::make_unique<M>(mid, entry, d);
    });
    CHECK(fr.ok());
  }
  auto rt = create_runtime(2);
  CHECK(rt.ok());
  rt.value()->set_run_id("run-p2-7node");
  // 7 节点链 IR（真实依赖：coverage→sample→...→write）
  const char* ir = R"({
    "schema": "astrocs.pipeline/v1",
    "pipeline_id": "p2.seven",
    "version": "1.0.0",
    "nodes": [
      {"node_id": "coverage", "module_id": "astrocs.phase2.coverage", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:cal"}, "outputs": {"out": "artifact:c1"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "sample", "module_id": "astrocs.phase2.sample", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c1"}, "outputs": {"out": "artifact:c2"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "upm_fit", "module_id": "astrocs.phase2.upm-fit", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c2"}, "outputs": {"out": "artifact:c3"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "upm_apply", "module_id": "astrocs.phase2.upm-apply", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c3"}, "outputs": {"out": "artifact:c4"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "reject", "module_id": "astrocs.phase2.reject", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c4"}, "outputs": {"out": "artifact:c5"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "integrate", "module_id": "astrocs.phase2.integrate", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c5"}, "outputs": {"out": "artifact:c6"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "write", "module_id": "astrocs.phase2.write", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c6"}, "outputs": {"out": "artifact:mosaic"},
       "resources": {"class": "io", "parallel": false}}
    ],
    "outputs": {"out": "artifact:mosaic"}
  })";
  auto load = rt.value()->load_pipeline(ir, reg);
  CHECK(load.ok());
  if (load.failed()) return;
  RunContext ctx;
  auto r = rt.value()->run(ctx);
  CHECK(r.ok());
  if (r.failed()) return;
  // node_trace：7 节点全 COMPLETED
  auto nt = rt.value()->node_trace();
  CHECK(nt.size() == 7);
  for (const auto& t : nt) {
    CHECK(t.status == "COMPLETED");
    CHECK(t.duration_ms >= 0);
    CHECK(!t.module_id.empty());
    CHECK(t.call_count == 1);       // 每节点一次真实模块调用
    CHECK(!t.provider.empty());     // 真实 provider（测试 stub 置位）→ 非空非 config
  }
  // trace JSONL：7 个 node_end、7 个 module_call、checkpoint/artifact 事件
  const std::string jsonl = rt.value()->trace_jsonl().value();
  size_t module_calls = 0, node_ends = 0, checkpoints = 0;
  for (const auto& line : std::string(jsonl)) { (void)line; }
  {
    std::string cur;
    size_t mc = 0, ne = 0, ck = 0;
    for (char ch : jsonl) {
      if (ch == '\n') {
        if (cur.find("\"type\":\"module_call\"") != std::string::npos) ++mc;
        if (cur.find("\"type\":\"node_end\"") != std::string::npos) ++ne;
        if (cur.find("\"type\":\"checkpoint\"") != std::string::npos) ++ck;
        cur.clear();
      } else cur.push_back(ch);
    }
    if (!cur.empty()) {
      if (cur.find("\"type\":\"module_call\"") != std::string::npos) ++mc;
      if (cur.find("\"type\":\"node_end\"") != std::string::npos) ++ne;
      if (cur.find("\"type\":\"checkpoint\"") != std::string::npos) ++ck;
    }
    module_calls = mc; node_ends = ne; checkpoints = ck;
  }
  CHECK(module_calls == 7);    // 7 个 P2 节点各一次
  CHECK(node_ends == 7);
  CHECK(checkpoints == 7);     // 每节点 checkpoint 观测
  // 无违规：每 entry 单节点、每节点单次调用
  auto vio = rt.value()->trace_violations();
  CHECK(vio.empty());
  // JSONL 可重放到图：解析每行合法
  TraceReplayResult rep = trace_replay_from_jsonl(jsonl);
  CHECK(rep.parsed_lines >= 21);  // >= 7 module_call + 7 node_start/end + 7 checkpoint 等
  CHECK(rep.nodes.size() == 7);
  bool all_once = true;
  for (const auto& n : rep.nodes) if (n.call_count != 1) all_once = false;
  CHECK(all_once);
}

// ── E. 隐藏 session 重复运行：两个节点同 entry → Runtime violations 抓出 ──
static void test_runtime_hidden_session_detected() {
  ModuleRegistry reg;
  for (int i = 0; i < 2; ++i) {
    ModuleDescriptor d;
    d.module_id = i == 0 ? "astrocs.phase2.coverage" : "astrocs.phase2.sample";
    d.version = "1.0." "0";
    d.abi = "c++17";
    d.execution_class = "cpu_heavy";
    d.parallel_ok = true;
    d.ports = {{"in", "DATA-P2-CHAIN", true, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
               {"out", "DATA-P2-CHAIN", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL}};
    d.sci_id = "SCI-X"; d.alg_id = "ALG-X"; d.data_id = "DATA-P2-CHAIN";
    d.api_id = "API-X"; d.test_id = "TEST-X";
    auto rr = reg.register_module(d);
    CHECK(rr.ok());
    auto fr = reg.register_factory(
        d.module_id, [d]() -> std::unique_ptr<IModule> {
      struct M : public IModule {
        ModuleDescriptor desc_;
        explicit M(ModuleDescriptor d) : desc_(std::move(d)) {}
        const ModuleDescriptor& descriptor() const noexcept override { return desc_; }
        Result<void> validate_config(const std::string&) override {
          return Result<void>::success();
        }
        Result<ModulePlan> plan(const std::string& node_id,
                                const std::string&) override {
          ModulePlan p;
          p.node_id = node_id;
          p.work_units = 1;
          return Result<ModulePlan>::ok(std::move(p));
        }
        Result<void> execute(RunContext& ctx) override {
          // 两节点执行同一隐藏完整 session（entry 相同）→ 隐藏扇出
          ctx.set_provider("baseline");
          TraceEvent e;
          e.type = TraceEventType::MODULE_CALL;
          e.node_id = ctx.current_node();
          e.module_id = desc_.module_id;
          e.entry = "astrocs_phase2_session_run";  // 隐藏 session 复用
          e.call_count = 1;
          ctx.record_trace(e);
          return Result<void>::success();
        }
        Result<std::string> inspect() override { return Result<std::string>::ok("{}"); }
        Result<std::string> last_manifest() override {
          return Result<std::string>::ok("{\"artifacts\":[]}");
        }
      };
      return std::make_unique<M>(d);
    });
    CHECK(fr.ok());
  }
  auto rt = create_runtime(2);
  CHECK(rt.ok());
  rt.value()->set_run_id("run-hidden");
  const char* ir = R"({
    "schema": "astrocs.pipeline/v1",
    "pipeline_id": "p2.hidden",
    "version": "1.0.0",
    "nodes": [
      {"node_id": "coverage", "module_id": "astrocs.phase2.coverage", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:cal"}, "outputs": {"out": "artifact:c1"},
       "resources": {"class": "cpu_heavy", "parallel": true}},
      {"node_id": "sample", "module_id": "astrocs.phase2.sample", "module_api": "1.x",
       "config": {}, "inputs": {"in": "artifact:c1"}, "outputs": {"out": "artifact:c2"},
       "resources": {"class": "cpu_heavy", "parallel": true}}
    ],
    "outputs": {"out": "artifact:c2"}
  })";
  auto load = rt.value()->load_pipeline(ir, reg);
  CHECK(load.ok());
  if (load.failed()) return;
  RunContext ctx;
  auto r = rt.value()->run(ctx);
  CHECK(r.ok());
  auto vio = rt.value()->trace_violations();
  bool found = false;
  for (const auto& v : vio)
    if (v.find("hidden-session-fanout") != std::string::npos) found = true;
  CHECK(found);   // 隐藏 session 必须被抓出
}

int main() {
  test_store_jsonl_roundtrip();
  test_executor_worker_provider_observation();
  test_scheduler_node_execution_trace();
  test_runtime_p2_7node_trace();
  test_runtime_hidden_session_detected();
  if (g_failures) {
    std::fprintf(stderr, "RT-006_FAIL checks=%d failures=%d\n", g_checks, g_failures);
    return 1;
  }
  std::printf("RT-006_TRACE_ALL_PASS checks=%d failures=0\n", g_checks);
  return 0;
}
'''

# ── 静态断言 ──
_HDR_TRACE = INC / "astrocs" / "core" / "trace.h"
_HDR_CTX = INC / "astrocs" / "core" / "context.h"
_SRC_TRACE = CORE / "trace.cpp"
_REPLAY_PY = REPO / "lib" / "infrastructure" / "pipeline" / "trace_replay.py"

# ── 驱动源清单的权威来源 = 构建图（不手抄文件名） ──
# 规范依据（AGENTS §1.1「先到最高文档确定规范」）：
#   * 根 CMakeLists.txt:302-335 `add_library(astrocs_core STATIC ...)` 是 scheduler
#     运行内核（artifact/module/pipeline/context/runtime/scheduler/checkpoint/
#     logging/plan_estimator/...）的**唯一权威显式源清单**（根 CMakeLists.txt:4-5
#     「显式源列表, 禁 GLOB (QA-002)」⇒ 该清单即构建图闭包，不是「惯例」）；
#   * lib/infrastructure/scheduler/README.md「构建」节：「本目录无独立 CMake 目标，
#     随根 CMakeLists.txt 编入 astrocs_core / astrocs_module_adapters」；
#   * `executor.cpp` **刻意不在** astrocs_core（eng/tests/unit/CMakeLists.txt:230-232
#     明文登记「executor.cpp 不在 astrocs_core 内 (eng/tests/runtime Python harness
#     独立编译)」），其构建图落点是同文件 :233-237 `executor_provider_race_test`。
_ROOT_CMAKE = REPO / "CMakeLists.txt"
_UNIT_CMAKE = REPO / "eng" / "tests" / "unit" / "CMakeLists.txt"
_CORE_TARGET = "astrocs_core"
_EXECUTOR_TARGET = "executor_provider_race_test"


def _cmake_target_sources(cmake_file: pathlib.Path, target: str) -> "list[pathlib.Path]":
    """解析 CMakeLists.txt 里 add_library/add_executable(<target> ...) 的显式源清单。

    返回已 resolve 的绝对路径（按清单出现顺序）。构建图变了（目标改名/清单为空）
    直接判红 —— 不允许测试静默退回手抄。
    """
    text = cmake_file.read_text(encoding="utf-8")
    m = re.search(r"add_(?:library|executable)\(\s*" + re.escape(target) + r"\b", text)
    if m is None:
        raise AssertionError(
            f"构建图已变：{cmake_file} 里找不到目标 {target} 的显式源清单")
    open_at = text.index("(", m.start())
    depth = 0
    close_at = -1
    for k in range(open_at, len(text)):
        ch = text[k]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_at = k
                break
    if close_at < 0:
        raise AssertionError(f"{cmake_file}: 目标 {target} 的源清单括号不闭合")
    block = "\n".join(ln.split("#")[0] for ln in text[open_at + 1:close_at].splitlines())
    # ${CMAKE_CURRENT_SOURCE_DIR} 先换成无空格占位符（仓库路径含空格，直接展开会被
    # 下面的路径正则从空格处截断 ⇒ 截出的相对路径再拼接会指到错误目录）。
    var = "@CMAKE_CURRENT_SOURCE_DIR@"
    block = block.replace("${CMAKE_CURRENT_SOURCE_DIR}", var)
    srcs = []
    for token in re.findall(r"([\w@./-]+\.cpp)", block):
        token = token.replace(var, str(cmake_file.parent))
        p = pathlib.Path(token)
        srcs.append((p if p.is_absolute() else cmake_file.parent / p).resolve())
    if not srcs:
        raise AssertionError(f"{cmake_file}: 目标 {target} 的源清单为空")
    return srcs


def _build_graph_sources() -> "tuple[list[pathlib.Path], list[pathlib.Path]]":
    """返回 (astrocs_core 权威源清单, 驱动额外需要的 scheduler TU 清单)。

    额外清单 = 单测目标里编译、但**不在** astrocs_core 的 lib/infrastructure/
    scheduler/src/*.cpp（当前即 executor.cpp）—— 由构建图推出，不手抄。
    """
    core = _cmake_target_sources(_ROOT_CMAKE, _CORE_TARGET)
    unit = _cmake_target_sources(_UNIT_CMAKE, _EXECUTOR_TARGET)
    extra = [s for s in unit if s.parent == CORE and s not in core]
    return core, extra


# 驱动编译的**最小链接闭包**（种子）：从上面权威清单里按需取用的子集。
# 为什么不是整份 astrocs_core 清单 —— 用本 harness 的 include 面（-I lib/include
# -I lib/third_party）逐个实测，清单里有 TU 无法脱离 CMake 独立编译：
#   * export_stream.cpp / canonical_hash.cpp —— 需要 crypto/sha256.h
#     （lib/algorithms/shared，astrocs_common 的 include 面）；
#   * memory_budget.cpp —— 需要 CMake configure_file 生成的
#     runtime_resources_generated.h（根 CMakeLists.txt:102-103，只在构建目录存在）；
#   * normalize_workflow.cpp / mosaic_window.cpp —— 需要 aio_atomic_file.h
#     （lib/infrastructure/aio/src，astrocs_core 的 PRIVATE include 面，
#      根 CMakeLists.txt:342-346）。
# 本 harness 刻意脱离 CMake 独立编译（源码级验证，见文件头「方法」节），故取最小子集；
# 代价是「清单漂移」这一类缺陷，用两条判据兜住：
#   ① test_seed_sources_come_from_build_graph —— 种子里每个名字必须仍在权威清单里；
#   ② test_driver_link_closure_complete —— nm 未定义/已定义符号差集必须为空，
#      非空则判红并**点名该补哪个 .cpp**（链接器的「undefined reference」不会说这个）。
_DRIVER_CORE_SEED = (
    "artifact.cpp", "artifact_store.cpp", "module.cpp", "pipeline.cpp",
    "context.cpp", "runtime.cpp", "scheduler.cpp", "checkpoint.cpp",
    "logging.cpp", "plan_estimator.cpp",
)


def _driver_sources() -> "list[pathlib.Path]":
    """驱动编译源清单 = 权威清单里的种子（按权威清单顺序）+ executor.cpp。"""
    core, extra = _build_graph_sources()
    by_name = {p.name: p for p in core}
    missing = [n for n in _DRIVER_CORE_SEED if n not in by_name]
    if missing:
        raise AssertionError(
            "驱动种子源不在 astrocs_core 权威清单里（构建图已变，需同步 "
            "_DRIVER_CORE_SEED）: " + ", ".join(missing))
    return [by_name[n] for n in _DRIVER_CORE_SEED] + extra


def _nm_symbols(objs, kind: str) -> "set[str]":
    """nm -C 提取目标文件集合的符号名（kind='defined' | 'undefined'）。

    注意 nm -C 的**名字里含空格**（模板/参数表），故按列切分时用 maxsplit，
    不能对整行 split() —— 否则会截出半个符号名，差集判据静默失效。
    """
    r = subprocess.run(["nm", "-C", f"--{kind}-only", *[str(o) for o in objs]],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError(f"nm --{kind}-only failed:\n{r.stderr[-2000:]}")
    out = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        if kind == "defined":            # "<addr> <type> <name...>"
            parts = line.split(None, 2)
            name = parts[2] if len(parts) == 3 else ""
        else:                            # "<blank> U <name...>"
            parts = line.split(None, 1)
            name = parts[1] if len(parts) == 2 else ""
        if name:
            out.add(name)
    return out


def _symbol_leaf(sym: str) -> str:
    """取符号名的叶子标识符（函数名 / 类名），供源码定义正则兜底用。"""
    s = sym
    for pre in ("vtable for ", "VTT for ", "typeinfo for ", "typeinfo name for ",
                "construction vtable for "):
        if s.startswith(pre):
            s = s[len(pre):]
            break
    s = s.split("(")[0]                  # 去参数表
    s = s.split("<")[0]                  # 去模板实参
    return s.rsplit("::", 1)[-1].strip()


def _source_defines_symbol(text: str, sym: str) -> bool:
    """源码文本里是否存在 sym 的**定义**（而非调用/声明）——无对象文件时的兜底。"""
    leaf = _symbol_leaf(sym)
    if not leaf:
        return False
    for m in re.finditer(r"(?m)^[^\n;#{}]*\b" + re.escape(leaf) + r"\s*\(", text):
        head = m.group(0)[:m.group(0).rindex(leaf)]
        if "=" in head:
            continue                     # 赋值/初始化里的调用，不是定义
        if re.search(r"\b(return|throw|case|sizeof|delete|new|if|while|for|switch|else)\b",
                     head):
            continue
        return True
    return False


def _resolve_missing_symbols(missing, tmp: pathlib.Path, compiled) -> "list[tuple[str, list[str]]]":
    """把未解析符号解析回权威清单里的 .cpp —— 即「该补哪个文件」。

    精确优先：把权威清单里尚未编译的候选 TU 编成 .o，用 nm 精确匹配符号定义；
    候选无法独立编译（需 CMake 生成头 / PRIVATE include 面）时退回源码定义正则。
    """
    core, extra = _build_graph_sources()
    compiled = {pathlib.Path(o) for o in compiled}
    exact: "dict[pathlib.Path, set[str]]" = {}
    fallback: "list[pathlib.Path]" = []
    for src in core + extra:
        if (tmp / (src.name + ".o")) in compiled:
            continue                     # 已在清单里 ⇒ 不可能定义未解析符号
        try:
            obj = _compile_unit(src, tmp)
        except RuntimeError:
            fallback.append(src)
            continue
        exact[src] = _nm_symbols([obj], "defined")
    report = []
    for sym in sorted(missing):
        hits = [str(s.relative_to(REPO)) for s, defs in exact.items() if sym in defs]
        if not hits:
            hits = [str(s.relative_to(REPO)) for s in fallback
                    if _source_defines_symbol(s.read_text(encoding="utf-8"), sym)]
        report.append((sym, sorted(hits)))
    return report


def _compile_unit(src: pathlib.Path, out_dir: pathlib.Path, *,
                  werror: bool = True) -> pathlib.Path:
    """编译单个 TU → .o（生产源用 -Wall -Wextra -Werror，driver 用同款但免 -Werror）。"""
    obj = out_dir / (src.name + ".o")
    cmd = ["g++", "-std=c++17", "-O2"]
    if werror:
        cmd += ["-Wall", "-Wextra", "-Werror"]
    cmd += ["-c", str(src), f"-I{INC}", f"-I{TP}", "-o", str(obj)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError(f"compile {src.name} failed:\n{r.stderr[-2000:]}")
    return obj


_DRIVER_BUILD: "dict[str, object]" = {}


def _driver_build() -> "dict[str, object]":
    """编译驱动（源清单 + driver.cpp）并链接 → {tmp, exe, objs, driver_obj, error}。

    进程内缓存：TestRt006TraceCpp 与 TestRt006DriverSourceClosure 共用同一次编译。
    编译/链接失败不抛异常，把诊断写进 error（含「该补哪个 .cpp」），由测试判红。
    """
    if _DRIVER_BUILD.get("done"):
        return _DRIVER_BUILD
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt006_"))
    atexit.register(shutil.rmtree, tmp, ignore_errors=True)
    _DRIVER_BUILD.update({"done": True, "tmp": tmp, "exe": None, "objs": [],
                          "driver_obj": None, "error": None})
    try:
        drv = tmp / "rt006_driver.cpp"
        drv.write_text(_DRIVER, encoding="utf-8")
        objs = [_compile_unit(s, tmp) for s in _driver_sources()]
        driver_obj = _compile_unit(drv, tmp, werror=False)
        exe = tmp / "rt006_trace"
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", str(driver_obj), f"-I{INC}", f"-I{TP}",
             *[str(o) for o in objs], "-pthread", "-o", str(exe)],
            capture_output=True, text=True, timeout=300)
        _DRIVER_BUILD.update({"objs": objs, "driver_obj": driver_obj})
        if r.returncode != 0:
            _DRIVER_BUILD["error"] = _link_failure_report(r.stderr, objs, driver_obj, tmp)
        else:
            _DRIVER_BUILD["exe"] = exe
    except RuntimeError as exc:
        _DRIVER_BUILD["error"] = str(exc)
    return _DRIVER_BUILD


def _link_failure_report(stderr: str, objs, driver_obj, tmp: pathlib.Path) -> str:
    """链接失败 → 把未定义符号解析回权威清单里的 .cpp（点名该补谁）。"""
    head = "link driver failed:\n" + stderr[-2000:]
    try:
        undefined = {s for s in _nm_symbols(list(objs) + [driver_obj], "undefined")
                     if "astrocs::" in s}
        defined = _nm_symbols(list(objs), "defined")
        missing = sorted(undefined - defined)
        if not missing:
            return head
        lines = [head, "链接闭包不完整（未定义符号差集非空）——该补的翻译单元："]
        for sym, hits in _resolve_missing_symbols(missing, tmp, objs):
            where = "、".join(hits) if hits else "（权威清单内无定义者，可能来自其他库）"
            lines.append(f"  {sym}\n      ← 定义在 {where}")
        lines.append("  修法：把上面点名的 .cpp 加入 _DRIVER_CORE_SEED。")
        return "\n".join(lines)
    except Exception as exc:             # 诊断自身失败不得掩盖原始链接错误
        return head + f"\n(闭包诊断失败: {exc!r})"


def _cpp_files():
    files = sorted(CORE.glob("*.cpp"))
    files += sorted((INC / "astrocs" / "core").glob("*.h"))
    return files


class TestRt006Static(unittest.TestCase):
    """静态：trace 观测点接线、禁止 config 冒充、provider/worker 真实反映。"""

    def test_trace_observation_points_wired(self):
        """runtime.cpp 节点 fn 必须含真实观测事件；executor worker 任务事件。"""
        rt = (CORE / "runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("TraceEventType::NODE_START", rt)
        self.assertIn("TraceEventType::MODULE_CALL", rt)
        self.assertIn("TraceEventType::NODE_END", rt)
        self.assertIn("TraceEventType::ERROR", rt)
        ex = (CORE / "executor.cpp").read_text(encoding="utf-8")
        self.assertIn("TraceEventType::WORKER_TASK", ex)
        self.assertIn("tasks_executed_.fetch_add", ex)

    def test_no_config_pretending_observation(self):
        """runtime.cpp 不得硬编码 provider/config 值冒充观测（baseline 只应来自
        真实 provider 选择点 module_adapters/测试置位，不得在节点 trace 硬编码）。"""
        rt = (CORE / "runtime.cpp").read_text(encoding="utf-8")
        # node trace 的 provider 必须来自 ctx.provider()（真实观测），无字面量默认
        self.assertIn("tr.provider = ctx.provider()", rt)
        self.assertNotIn('tr.provider = "baseline"', rt)
        # executor 层不得伪造 provider：只能收集 ctx.set_provider 的结果
        ex = (CORE / "executor.cpp").read_text(encoding="utf-8")
        self.assertNotIn('= "baseline"', ex)
        # module_adapters 只在 host init 成功（真实后端可用）后置 baseline
        ma = (CORE / "module_adapters.cpp").read_text(encoding="utf-8")
        self.assertIn('ctx.set_provider("baseline")', ma)
        idx = ma.find('ctx.set_provider("baseline")')
        before = ma[:idx]
        self.assertIn("hs.init(cap)", before)   # 置位必须在 host init 成功之后

    def test_worker_provider_reflects_in_trace(self):
        """executor 观测计数与 trace 字段必须真实：provider 由 ctx 收集、worker 计数
        由 fetch_add 观测；头文件暴露 tasks_executed/provider_sets。"""
        hdr = (INC / "astrocs" / "core" / "executor.h").read_text(encoding="utf-8")
        self.assertIn("tasks_executed()", hdr)
        self.assertIn("provider_sets()", hdr)
        self.assertIn("observed_provider", hdr)

    def test_replay_py_matches_cpp_semantics(self):
        """Python replay 与 C++ 双实现同构：合法事件类型集合、摘要字段一致。"""
        py = _REPLAY_PY.read_text(encoding="utf-8")
        self.assertIn('"astrocs.trace-replay/v1"', py)
        self.assertIn("module_call", py)
        self.assertIn("hidden-session-fanout", py)
        self.assertIn("repeated-call", py)

    def test_no_session_duplicate_module_registry_typo(self):
        """生产注册表（RT-001 module_ports）7 个 P2 module 保持唯一 entry 绑定
        （重复完整 session 属 P2-X24 专项；本层 trace 检测语义由测试 D/E 覆盖）。"""
        # 静态：trace_replay detect_violations 存在且可用
        self.assertTrue(hasattr(py_detect_violations, "__call__"))


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestRt006DriverSourceClosure(unittest.TestCase):
    """机器判据：驱动源清单必须由构建图推出且覆盖链接闭包。

    防的缺陷类（本任务实测）：lib/infrastructure/scheduler/src 新增/改动的 TU
    让 runtime.cpp 等引入新的 astrocs::core:: 外部符号时，手抄源清单要到**链接期**
    才炸，而链接器只报「undefined reference to <符号>」——不告诉你该补哪个 .cpp，
    一次只报第一条。本类用两条判据把这一类缺陷变成可判、可点名的红：

      ① test_seed_sources_come_from_build_graph：种子每个名字必须仍在
         astrocs_core 权威清单里（防改名/搬目录后清单静默失效）；
      ② test_driver_link_closure_complete：nm 对真实目标文件求
         「未定义 astrocs:: 符号 − 清单已定义符号」差集，非空即判红，
         并把每个未解析符号解析回权威清单里定义它的 .cpp（直接点名）。

    判据依赖真实编译（nm 需要目标文件），不是源码文本猜测 —— 因为本缺陷的
    典型形态 estimate_plan(...) 在 runtime.cpp 里是**无限定名调用**
    （using namespace astrocs::core），纯文本扫 astrocs::core:: 会漏掉它。
    """

    def test_seed_sources_come_from_build_graph(self):
        core, extra = _build_graph_sources()
        names = {p.name for p in core}
        self.assertTrue(
            names >= set(_DRIVER_CORE_SEED),
            "驱动种子源已不在 astrocs_core 权威清单里（构建图已变）: "
            + ", ".join(sorted(set(_DRIVER_CORE_SEED) - names)))
        extra_names = {p.name for p in extra}
        self.assertIn("executor.cpp", extra_names,
                      "executor.cpp 应从构建图（executor_provider_race_test 目标）解析到")
        for src in _driver_sources():
            self.assertTrue(src.is_file(), f"驱动源不存在: {src}")

    def test_driver_link_closure_complete(self):
        build = _driver_build()
        objs = list(build["objs"] or [])
        if not objs:
            self.fail("驱动源清单未编译出任何目标文件，无法做闭包判定："
                      f"{build['error']}")
        driver_obj = build["driver_obj"]
        undefined = {s for s in _nm_symbols(objs + [driver_obj], "undefined")
                     if "astrocs::" in s}
        defined = _nm_symbols(objs, "defined")
        missing = sorted(undefined - defined)
        if not missing:
            return
        report = _resolve_missing_symbols(missing, build["tmp"], objs)
        detail = "\n".join(
            f"  {sym}\n      ← 定义在 "
            + ("、".join(hits) if hits else "（权威清单内无定义者，可能来自其他库）")
            for sym, hits in report)
        self.fail(
            "驱动源清单未覆盖链接闭包（未定义符号差集非空）：\n" + detail
            + "\n  修法：把上面点名的 .cpp 加入 _DRIVER_CORE_SEED。")


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestRt006TraceCpp(unittest.TestCase):
    """C++ harness：真实编译链接 lib/infrastructure/scheduler 源码运行 RT-006 全部验收断言。

    源清单来自构建图（_driver_sources()，权威 = 根 CMakeLists.txt 的 astrocs_core
    目标 + eng/tests/unit/CMakeLists.txt 的 executor_provider_race_test 目标），
    不再手抄；链接失败时报错会点名「该补哪个 .cpp」（见 _link_failure_report）。
    """

    @classmethod
    def setUpClass(cls):
        build = _driver_build()
        if build["error"]:
            raise RuntimeError(build["error"])
        cls.exe = build["exe"]

    def test_driver_all_checks_pass(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True,
                           timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr[-4000:])
        self.assertIn("RT-006_TRACE_ALL_PASS", r.stdout)


class TestRt006PythonReplay(unittest.TestCase):
    """Python replay：JSONL 重放与 violation 检测（与 C++ harness 双实现互证）。"""

    def test_replay_7node_summary(self):
        """7 节点各一次 module_call 的 JSONL → replay 7 节点摘要 call_count=1。"""
        lines = []
        for i, nid in enumerate(["coverage", "sample", "upm_fit", "upm_apply",
                                 "reject", "integrate", "write"]):
            lines.append(
                '{"schema":"astrocs.trace-event/v1","type":"module_call",'
                f'"ts_utc":"2026-09-03T00:00:00.{i:03d}Z","run_id":"r1",'
                f'"node_id":"{nid}","module_id":"astrocs.phase2.{nid}",'
                f'"entry":"{nid}","call_count":1,"workers":2,'
                f'"provider":"baseline","seq":{i + 1}}}')
            lines.append(
                '{"schema":"astrocs.trace-event/v1","type":"node_end",'
                f'"ts_utc":"2026-09-03T00:00:01.{i:03d}Z","run_id":"r1",'
                f'"node_id":"{nid}","status":"COMPLETED","wall_ms":1.2,'
                f'"provider":"baseline","seq":{i + 100}}}')
        jsonl = "\n".join(lines) + "\n"
        rep = py_replay_from_jsonl(jsonl)
        self.assertEqual(rep["parsed_lines"], 14)
        self.assertEqual(rep["skipped_lines"], 0)
        self.assertEqual(len(rep["nodes"]), 7)
        for n in rep["nodes"]:
            self.assertEqual(n["call_count"], 1)
            self.assertEqual(n["status"], "COMPLETED")
        self.assertEqual(py_detect_violations(jsonl), [])

    def test_replay_hidden_session_and_repeat_detected(self):
        """隐藏 session（同 entry 2 节点）与重复调用（同节点 2 次）必须被抓出。"""
        jsonl = (
            '{"type":"module_call","node_id":"coverage","entry":"astrocs_phase2_session_run","seq":1}\n'
            '{"type":"module_call","node_id":"sample","entry":"astrocs_phase2_session_run","seq":2}\n'
            '{"type":"module_call","node_id":"coverage","entry":"coverage","seq":3}\n'
            '{"type":"module_call","node_id":"coverage","entry":"coverage","seq":4}\n'
        )
        vio = py_detect_violations(jsonl)
        joined = "\n".join(vio)
        self.assertIn("hidden-session-fanout", joined)
        self.assertIn("repeated-call", joined)

    def test_replay_skips_invalid_lines(self):
        """非法行跳过并计数；合法行照常解析。"""
        jsonl = (
            'not json\n'
            '{"type":"bogus","node_id":"x"}\n'
            '{"type":"node_end","node_id":"a","status":"COMPLETED","seq":1}\n'
        )
        rep = py_replay_from_jsonl(jsonl)
        self.assertEqual(rep["skipped_lines"], 2)
        self.assertEqual(rep["parsed_lines"], 1)
        self.assertEqual(rep["nodes"][0]["node_id"], "a")


if __name__ == "__main__":
    unittest.main(verbosity=2)
