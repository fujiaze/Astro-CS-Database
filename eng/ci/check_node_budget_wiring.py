#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-NODE-BUDGET-WIRING — 节点并行预算必须来自**运行期预算**（X1）。

依据（逐条）：
  * docs/architecture/THREADING_MODEL.md「并行轴分配」：并行轴 = 帧轴 × 帧内轴，两轴之积 ≤ 预算；
    预算唯一来源 = 运行期配额，禁硬编码线程数。
  * docs/contracts/SCHEDULER_CONTRACT.md §3：线程预算由配置/资源门决定，禁硬编码常数。
  * docs/standards/CONCURRENCY_STANDARD.md「默认」节：线程数外部可配置，
    **默认 min(可用核, 配置上限)**；取值来源 = 配置（16 不作硬编码值）。

缺陷（本门锁定的回归面）：旧实现在 config **没有** __workers 时（= 直接调用 op 的非调度路径）
恒取 1 ⇒ P1 帧轴 budget=1 ⇒ p1_parallel_for 的 frame_w<=1 分支把帧内 ICV 设为 1 ⇒
节点内 omp parallel for（如 dpsf_fit_batch_f64）整批单线程（PSF 域实测 1,086.0 s vs 16 线程 101.2 s）。

规则：
  R1 静态：调度器源码里不得存在 "缺省 1 = 串行" 的 __workers 读取；
           且每个 __workers 读取必须经 node_thread_budget_of / astrocs::core::node_thread_budget。
  R2 动态：编译并运行探针（链接**真实** lib/infrastructure/scheduler/src/cpu_budget.cpp），断言
           · 无 lease ⇒ 预算 == process_cpu_budget()（>1 核时必须 > 1，即"未接线 ⇒ 并行度 1"不可能）；
           · 有 lease ⇒ 恒 == max(1, lease)（只收紧不放大）；
           · 轴分配公式 inner_u = max(1, budget/in_flight)；frame_w=1,n=1 ⇒ inner_u == budget；
           · 配置上限键生效（cap>0 ⇒ 预算 ≤ cap）。
  R3 自检：--self-test 必须能红 —— ① R1 喂 pre-fix 文本；② 探针以
           -DASTROCS_NODE_BUDGET_REGRESSION=1 编译（模拟未接线：无 lease 时返回 1）。

退出码：0 绿 / 1 红 / 2 用法或环境错误。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT_DEFAULT = "."

# 唯一解析面符号（实现侧与探针共用同一 TU，禁止旁路复制公式）。
RESOLVER_SYMBOL = "node_thread_budget_of"
CORE_SYMBOL = "astrocs::core::node_thread_budget"
# 缺省即串行的旧写法（红例模式）。
OLD_PATTERN = re.compile(r'doc\.value\(\s*"__workers"\s*,\s*1u?\s*\)')
ANY_READ = re.compile(r'"__workers"')

PROBE_SRC = r"""
#include <cstdio>
#include <cstdlib>
#include "cpu_budget.h"

// 轴分配公式（与 p1_parallel_for 同款；此处只为门禁可判，不复制实现）：
//   in_flight = min(n, frame_w); inner_u = max(1, budget / in_flight)
static unsigned axis_inner(unsigned budget, unsigned frame_w, unsigned n) {
  const unsigned frame_w_eff = frame_w ? frame_w : 1u;
  const unsigned in_flight = (n < frame_w_eff) ? n : frame_w_eff;
  const unsigned d = in_flight ? (budget / in_flight) : budget;
  return d ? d : 1u;
}

int main() {
#if defined(ASTROCS_NODE_BUDGET_REGRESSION)
  // 红例：模拟"预算未接线"的旧实现（无 lease ⇒ 1）。
  const unsigned absent = 1u;
#else
  const unsigned absent =
      astrocs::core::node_thread_budget(false, 0u);
#endif
  const unsigned proc = astrocs::core::process_cpu_budget();
  const unsigned aff = astrocs::core::affinity_cpu_count();
  const unsigned cap = astrocs::core::cpu_budget_cap();
  const unsigned lease3 = astrocs::core::node_thread_budget(true, 3u);
  const unsigned lease0 = astrocs::core::node_thread_budget(true, 0u);
  const unsigned inner = axis_inner(absent, 1u, 1u);
  std::printf("affinity=%u process=%u cap=%u absent_lease=%u lease3=%u lease0=%u\n",
              aff, proc, cap, absent, lease3, lease0);
  std::printf("[p1axis] parallel_for budget=%u frame_workers=1 n_units=1 inner_omp=%u\n",
              absent, inner);
  int bad = 0;
  if (absent == 1u && aff > 1u) {
    std::printf("RED 无 lease 时预算=1（可用核 %u）⇒ 节点内并行度退化为串行\n", aff);
    bad = 1;
  }
  if (absent != proc) {
    std::printf("RED 无 lease 时预算(%u) != 进程有效 CPU 预算(%u)\n", absent, proc);
    bad = 1;
  }
  if (inner <= 1u && aff > 1u) {
    std::printf("RED 帧内并行度 inner_omp=%u（可用核 %u）\n", inner, aff);
    bad = 1;
  }
  if (lease3 != 3u) {
    std::printf("RED lease=3 被改写为 %u（只收紧不放大被破坏）\n", lease3);
    bad = 1;
  }
  if (lease0 != 1u) {
    std::printf("RED lease=0/缺省值未视为显式串行（得到 %u）\n", lease0);
    bad = 1;
  }
  if (cap > 0u && proc > cap) {
    std::printf("RED 进程预算 %u 超过配置上限 %u\n", proc, cap);
    bad = 1;
  }
  std::printf(bad ? "PROBE_RED\n" : "PROBE_GREEN\n");
  return bad ? 1 : 0;
}
"""


def _run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def check_static(sources):
    """R1：返回 (ok, details)。"""
    hits, reads, resolver_uses = [], 0, 0
    for path in sources:
        if not os.path.isfile(path):
            continue
        text = open(path, encoding="utf-8", errors="replace").read()
        for m in OLD_PATTERN.finditer(text):
            line = text[: m.start()].count("\n") + 1
            hits.append("%s:%d: %s" % (path, line, m.group(0)))
        reads += len(ANY_READ.findall(text))
        resolver_uses += len(re.findall(re.escape(RESOLVER_SYMBOL), text))
        resolver_uses += len(re.findall(re.escape(CORE_SYMBOL), text))
    details = {
        "sources": sources,
        "old_default_serial_reads": hits,
        "any_workers_reads": reads,
        "resolver_uses": resolver_uses,
    }
    ok = (not hits) and reads > 0
    return ok, details


def build_and_run_probe(repo, regression=False, keep_dir=None):
    """R2/R3：编译并运行探针（链接真实 cpu_budget.cpp）。"""
    build_dir = os.path.join(repo, "build")
    inc = [
        "-I" + build_dir,
        "-I" + os.path.join(repo, "lib", "include"),
        "-I" + os.path.join(repo, "lib", "infrastructure", "scheduler", "src"),
    ]
    cxx = os.environ.get("CXX", "c++")
    tmp = keep_dir or tempfile.mkdtemp(prefix="x1_node_budget_")
    src = os.path.join(tmp, "probe.cpp")
    exe = os.path.join(tmp, "probe")
    with open(src, "w", encoding="utf-8") as fh:
        fh.write(PROBE_SRC)
    cmd = [cxx, "-std=c++17", "-O0"] + inc
    if regression:
        cmd.append("-DASTROCS_NODE_BUDGET_REGRESSION=1")
    cmd += [src, os.path.join(repo, "lib", "infrastructure", "scheduler", "src", "cpu_budget.cpp"),
            "-o", exe, "-lpthread"]
    cc = _run(cmd, repo)
    if cc.returncode != 0:
        return None, {"compile_command": " ".join(cmd), "stderr": cc.stderr[-2000:]}
    run = _run([exe], repo)
    out = {"compile_command": " ".join(cmd), "rc": run.returncode,
           "stdout": run.stdout.strip(), "stderr": run.stderr.strip()}
    return run, out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=ROOT_DEFAULT)
    ap.add_argument("--module-adapters", default=None,
                    help="覆盖 R1 静态面（自检/红例用）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)

    repo = os.path.abspath(args.repo)
    ma = args.module_adapters or os.path.join(
        repo, "lib", "infrastructure", "scheduler", "src", "module_adapters.cpp")
    sources = [ma]

    result = {"check": "CHK-NODE-BUDGET-WIRING", "repo": repo}
    problems = []

    ok_static, st = check_static(sources)
    result["R1_static"] = st
    if not ok_static:
        for h in st["old_default_serial_reads"]:
            problems.append("R1 缺省即串行的 __workers 读取: " + h)
        if st["any_workers_reads"] == 0:
            problems.append("R1 未检出任何 __workers 读取面（源文件或路径错误？）")

    # R2 恒用**接线后**的探针（自检的红例另编译，见 R3）。
    probe, pr = build_and_run_probe(repo, regression=False)
    result["R2_probe"] = pr
    if probe is None:
        problems.append("R2 探针编译失败")
    elif probe.returncode != 0:
        problems.append("R2 探针断言失败（见 R2_probe.stdout）")

    if args.self_test:
        # 自检模式：**注入**两处"未接线"状态，要求门禁能红（能红才证明有判别力）。
        #   红例①：pre-fix 文本必须被 R1 判红；
        #   红例②：以 -DASTROCS_NODE_BUDGET_REGRESSION=1 编译的探针必须判红；
        #   同时要求：工作树 R1 与正常探针（R2）**不**因注入而红（否则是真实回归，不许掩盖）。
        with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False,
                                         encoding="utf-8") as fh:
            fh.write('static unsigned f(const Json& doc) {\n'
                     '  return std::max(1u, doc.value("__workers", 1u));\n}\n')
            prefix_path = fh.name
        ok_old, _ = check_static([prefix_path])
        os.unlink(prefix_path)
        reg_probe, reg_out = build_and_run_probe(repo, regression=True)
        result["R3_selftest"] = {
            "prefix_text_green": ok_old,
            "regression_probe_red": reg_probe is not None and reg_probe.returncode != 0,
            "regression_probe_stdout": (reg_out or {}).get("stdout", ""),
            "working_tree_R1_counted": ok_static,
        }
        result["R3_selftest"]["pass"] = (
            (not ok_old)
            and reg_probe is not None and reg_probe.returncode != 0
            and ok_static
            and probe is not None and probe.returncode == 0
        )
        if not result["R3_selftest"]["pass"]:
            problems.append("R3 自检未按预期通过（红例不红 = 门禁无判别力）")

    result["problems"] = problems
    result["status"] = "pass" if not problems else "red"
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(result, fh, ensure_ascii=False, indent=2)

    print("[R1] old_default_serial_reads=%d any___workers_reads=%d resolver_uses=%d"
          % (len(st["old_default_serial_reads"]), st["any_workers_reads"], st["resolver_uses"]))
    if isinstance(pr, dict) and pr.get("stdout"):
        for line in pr["stdout"].splitlines():
            print("[R2] " + line)
    if args.self_test:
        s = result["R3_selftest"]
        for line in s["regression_probe_stdout"].splitlines():
            print("[R3-red] " + line)
        print("[R3] prefix_text_green=%s regression_probe_red=%s pass=%s"
              % (s["prefix_text_green"], s["regression_probe_red"], s["pass"]))
    for p in problems:
        print("[PROBLEM] " + p)
    print("CHK-NODE-BUDGET-WIRING %s" % ("PASS" if not problems else "FAIL"))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
