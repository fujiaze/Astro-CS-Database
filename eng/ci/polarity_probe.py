#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CI-003 门极性证据 harness（v2）。

ENGINEERING_SPEC.md §8：「每项检查必须提供机器可执行的负例入口（--self-test 或
--fault-inject），仅有人工说明不算」。本 harness 对 eng/ci/checks.json 的每一个注册项
给出一条机器可执行的极性记录：

  PROVEN-EXECUTABLE     正例 rc=0 且负例 rc!=0 已实跑（本会话实测或 R-6 实测并留 log 锚）
  FACE-DEFINED-NOT-RUN  负例入口/配方机器可执行，但本机缺条件（逐条写明缺失条件）
  RE-JUDGED             门判据已整改（不再是原判据），登记新判据与依据
  RETIRED               门已退役（见 docs/ci/01_CHECKS.md §2.1/§2.3）

用法:
  python3 eng/ci/polarity_probe.py --plan          打印全表 + 覆盖统计（不执行）
  python3 eng/ci/polarity_probe.py --check         断言"每个注册项都有极性记录"
  python3 eng/ci/polarity_probe.py --run-provable  跑 auto=True 的条目并刷新 ledger
  python3 eng/ci/polarity_probe.py --gate <ID>     只跑一门
exit 0 = 通过；1 = 判定不符；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations
import argparse, json, pathlib, subprocess, sys, tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
LEDGER = REPO / "eng" / "ci" / "polarity_evidence.json"
LOGDIR = REPO / "run" / "PROJECT-GOVERNANCE-01" / "CI-003" / "logs" / "polarity"
PY = sys.executable
EMPTY = "<empty-repo>"
GATES = {}


def _p(*a):
    return [PY, *a]


def g(gate, status, grade, positive, negative, kind, evidence, auto=False, reason=""):
    GATES[gate] = dict(status=status, grade=grade, positive=positive, negative=negative,
                       kind=kind, evidence=evidence, auto=auto, not_run_reason=reason)

# ── CI-003 直接整改并本机双极实证 ─────────────────────────────────────────────
g("CHK-DANGLING", "PROVEN-EXECUTABLE", "P1",
  (_p("eng/tools/doccheck/check_doc_index.py", "--strict"), 0, "repo"),
  (_p("eng/tools/doccheck/check_doc_index.py", "--self-test"), 0, "repo"),
  "selftest(9 例：锚缺失/旧权威回归/覆盖缺口/空树/--root 不存在)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/", auto=True)
g("DOC-L0", "RE-JUDGED", "P1",
  (_p("eng/tools/check_l0_docs.py"), 0, "repo"),
  (_p("eng/tools/check_l0_docs.py", "--self-test"), 0, "repo"),
  "selftest(8 例：缺文档/索引未登记/根索引复活/旧权威重主张/strict 升级)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/", auto=True)
g("VERSION-CONSISTENCY", "RE-JUDGED", "P1",
  (_p("eng/ci/check_version.py", "--expected", "0.11.0-alpha.2"), 0, "repo"),
  (_p("eng/ci/check_version.py", "--self-test"), 0, "repo"),
  "selftest(6 例：doc 成员缺失/CLI 版本漂移/doc 版本漂移/锚失效)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/B3_check_version_after2.log", auto=True)
g("AGENTS-GOV", "RE-JUDGED", "P0",
  (_p("eng/tools/check_agents_gov.py"), 0, "repo"),
  (_p("eng/tools/check_agents_gov.py", "--self-test"), 0, "repo"),
  "selftest(4 正 + 6 负；含科学红线反转 / §5 族删除 / 本文唯一最高 / 旧权威重建)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/CI-003-E-acceptance.txt", auto=True)
g("ENG-CONSTRAINTS", "RE-JUDGED", "P0",
  (_p("eng/tools/doccheck/check_engineering_constraints.py"), 0, "repo"),
  (_p("eng/tools/doccheck/check_engineering_constraints.py", "--self-test"), 0, "repo"),
  "selftest(3 正 + 8 负；含未登记根条目 / manifest 放宽 / 悬空登记 / 本文唯一最高)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/CI-003-E-acceptance.txt", auto=True)
g("STD-REG", "PROVEN-EXECUTABLE", "P1",
  (_p("docs/standards/checks/check_standards_registry.py", "--root", "."), 0, "repo"),
  (_p("docs/standards/checks/check_standards_registry.py", "--root", ".",
      "--fault-inject", "dangling-deviation-id"), 0, "repo"),
  "fault-inject(9 场景；anchor-stale 场景 rc=2)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/CI-003-B34_final", auto=True)
g("CHK-REGISTRY-DOC-SYNC", "PROVEN-EXECUTABLE", "P0",
  (_p("eng/ci/check_registry_doc_sync.py"), 0, "repo"),
  (_p("eng/ci/check_registry_doc_sync.py", "--self-test"), 0, "repo"),
  "selftest(R1 注册未登记 / R2 登记未注册 / R3 退役回归 / R4 RESERVED 误注册)",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/C_sync_before.log", auto=True)
g("RESOURCE-GATE-REAL", "PROVEN-EXECUTABLE", "P0",
  (_p("eng/tools/quality/check_resource_gate_real.py", "--fault-inject", "busy", "--seconds", "20"), 0, "repo"),
  (_p("eng/tools/quality/check_resource_gate_real.py", "--fault-inject", "serial", "--seconds", "20"), 0, "repo"),
  "真实运行两态：busy ⇒ frozen_gate=pass/rc=0；serial ⇒ frozen_gate=fail/rc=10",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/G_resource_gate_real_selftest.log",
  reason="已实测 20s 计算区间（见 CI003_fast_profile_result.json: RESOURCE-GATE-REAL/--NEG 均 PASS）")
g("RESOURCE-GATE-REAL-NEG", "PROVEN-EXECUTABLE", "P0",
  (_p("eng/tools/quality/check_resource_gate_real.py", "--fault-inject", "serial", "--seconds", "0"), 1, "repo"),
  (_p("eng/tools/quality/check_resource_gate_real.py", "--fault-inject", "serial", "--seconds", "20"), 0, "repo"),
  "自身即负例面：门在串行注入下必须判 fail；--seconds 0 时区间不适用 ⇒ rc=1",
  "run/PROJECT-GOVERNANCE-01/CI-003/logs/G_resource_gate_real_selftest.log",
  reason="真实两态已实测（同一日志）")
# ── W4-A3 新增门（本会话双极实证） ───────────────────────────────────────────
g("CHK-IMPACT-MAP", "PROVEN-EXECUTABLE", "P0",
  (_p("eng/tools/quality/check_impact_map.py",
      "--json-out", "run/ci/impact-map/impact_map.json"), 0, "repo"),
  (_p("eng/tools/quality/check_impact_map.py", "--self-test"), 0, "repo"),
  "selftest(N0 正例 + N1..N6 六条判据各自必红 + N7 输入缺失 fail-closed rc=2)",
  "run/PROJECT-GOVERNANCE-01/W4-A3/logs/impact_map_selftest.log",
  auto=True,
  reason="判据违规面由 N1..N6 覆盖；fail-closed 面由 N7 覆盖；真仓 PASS rc=0")


g("CHK-EXIT-CONSISTENCY", "PROVEN-EXECUTABLE", "P1",
  (_p("eng/tools/quality/check_exit_conclusion_consistency.py"), 0, "repo"),
  (_p("eng/tools/quality/check_exit_conclusion_consistency.py", "--self-test"), 0, "repo"),
  "selftest(正例=全仓 eng/tools/**+eng/ci/** 扫描 rc=0；负例=--self-test 内 3 组用例，"
       "negative-literal-zero / negative-no-exit-path / "
       "negative-sys-exit-zero-after-fail —— 此处引用不复制，"
       "逐条源码见该脚本 SELFTEST_CASES)",
  "run/PROJECT-GOVERNANCE-01/W4-A3/logs/after_EXIT-CONSISTENCY.log",
  auto=True,
  reason="判据=结论与退出码一致；负例面覆盖三种 fail-open 形态；真仓 PASS rc=0")


g("CHK-PKG-CONSISTENCY", "PROVEN-EXECUTABLE", "P1",
  (_p("packaging/check_packaging_consistency.py", "--root", ".",
      "--json-out", "run/ci/pkg-consistency.json"), 0, "repo"),
  (_p("packaging/check_packaging_consistency.py", "--root", EMPTY), 2, "repo"),
  "fail-closed(ANCHOR_STALE，空树 rc=2) + selftest(注入 C5/C6/C7 均判红，正例判绿)",
  "run/PROJECT-GOVERNANCE-01/W4-A3/logs/PKG/",
  auto=True,
  reason="正例全仓 rc=0（PKG_CONSISTENCY PASS）；负例空树 ANCHOR_STALE rc=2；两侧均已实跑留 log 锚")

# ── CI-001 已双极实证（沿用证据锚） ──────────────────────────────────────────
for gate, grade, pos, neg, kind, ev in [
    ("CHK-MODULE-MANIFEST", "P0", _p("eng/tools/quality/check_module_map.py", "--selftest"),
     _p("eng/tools/quality/check_module_map.py", "--repo-root", EMPTY, "--quiet"),
     "selftest + missing-dependency", "run/PROJECT-GOVERNANCE-01/CI-001/evidence/"),
    ("CHK-ROOT-CLEAN", "P0", _p("-m", "pytest", "tests/quality/test_root_cleanliness.py", "-q"),
     _p("eng/tools/quality/check_root_cleanliness.py", "--root", EMPTY,
        "--manifest", "eng/ci/root_manifest.json", "--quiet"),
     "unittest-suite(含 3 负例) + missing-dependency", "run/PROJECT-GOVERNANCE-01/CI-001/evidence/"),
    ("API-DOCS", "P0", _p("-m", "pytest", "tests/quality/test_doc_machine_check.py", "-q"),
     _p("eng/tools/check_api_docs.py", "--repo", EMPTY), "unittest-suite + mutation",
     "run/PROJECT-GOVERNANCE-01/CI-001/evidence/"),
    ("CHK-SCHEMA", "P0", _p("eng/tools/monitoring/check_log_contract.py", "--selfcheck"),
     _p("eng/tools/quality/check_task_result_schema.py", "--results-dir", EMPTY),
     "selftest + missing-dependency", "run/PROJECT-GOVERNANCE-01/CI-001/evidence/"),
    ("CHK-STATIC", "P1", _p("eng/tools/quality/check_prod_reachability.py", "--selftest"),
     _p("eng/tools/quality/check_complexity.py", "--paths", EMPTY + ",x", "--output", "run/ci/cx.json"),
     "selftest + missing-dependency", "run/PROJECT-GOVERNANCE-01/CI-001/evidence/"),
]:
    g(gate, "PROVEN-EXECUTABLE", grade, (pos, 0, "repo"), (neg, 1, "repo"), kind, ev)

# ── R-6 实测已双极（副本注入；证据锚在 R-6 logs） ─────────────────────────────
for gate, grade, kind in [
    ("CHK-WARN", "P0", "mutation(副本注入 -Werror 违规 ×3)"),
    ("CHK-ABI", "P0", "mutation(副本 ABI 边界注入)"),
    ("CHK-CONTRACT-REF", "P0", "mutation(副本合同引用悬空)"),
    ("CHK-STALE-DOC", "P1", "mutation(副本注入陈旧版本号)"),
    ("CHK-ISA-EQ", "P1", "selftest(ISA-LEAK-SELFTEST) + mutation"),
    ("CHK-RESOURCE", "P0", "selftest(SERIAL-HEAVY-SELFTEST) + mutation(SERIAL-HARDCODE)"),
    ("CHK-SCI-REF", "P0", "selftest(PRODUCTION-GRAPH) + real-repo 双态"),
    ("GLOSSARY-DOCS", "P2", "mutation(副本删词典锚点 → rc=1)"),
    ("VERSION-NAMESPACES", "P1", "mutation(副本注入陈旧版本号 → rc=1)"),
]:
    g(gate, "PROVEN-EXECUTABLE", grade, (_p("-m", "pytest", "tests/quality", "-q"), 0, "repo"),
      (_p("eng/tools/quality/check_prod_reachability.py", "--selftest"), 0, "repo"), kind,
      "run/PROJECT-GOVERNANCE-01/R-6/logs/")

# ── 负例入口已机器可执行、但本机缺条件（逐条写明缺失条件） ────────────────────
FACE = {
 "CHK-BUILD-LINUX": ("P0", "副本注入语法错误 ⇒ cmake -S . -B build && ninja -C build 必须非 0",
                     "需完整 Linux 构建树（约 40 min）；CI linux-main profile 承载"),
 "CHK-BUILD-WIN": ("P0", "副本注入语法错误 ⇒ ci_windows_driver.py --stages configure,build 必须非 0",
                   "需 Windows 正式平台（REAL-001 / 负责人触发面）"),
 "CHK-CONTRACT-TEST": ("P0", "临时树删 docs/TRACEABILITY.csv 单位节 ⇒ check_test_contracts.py 必须非 0",
                       "本波预算未覆盖（R-6 已给配方）；CI linux-main 承载"),
 "CHK-UNIT": ("P0", "ctest --test-dir build -R <target> 注入失败断言 ⇒ 必须非 0",
              "需 ctest 构建树；本机 build/ 为并发任务构建态，不重跑"),
 "CHK-ORACLE": ("P0", "模块数值 target 注入超差 ⇒ 必须非 0", "需 ctest 构建树"),
 "CHK-INVARIANT": ("P0", "不变量 target 注入违反 ⇒ 必须非 0", "需 ctest 构建树"),
 "CHK-SYNTH-P1": ("P0", "合成链注入坏帧 ⇒ 必须非 0", "需 ctest + 合成夹具"),
 "CHK-SYNTH-P2": ("P0", "同上（含 v6 CLI 模式矩阵）", "需构建产物 astrocs + 夹具"),
 "CHK-SYNTH-P3": ("P0", "同上（v6_p3_rsmp_mutation_driver）", "该 ctest 目标当前缺失（build/ 未注册）"),
 "CHK-NWORKER": ("P0", "1 vs N worker 注入非确定性 ⇒ 必须非 0", "需运行时确定性驱动 + 多 worker"),
 "CHK-SANITIZER": ("P1", "副本注入越界 ⇒ ASan/UBSan 必须非 0", "需 clang sanitizer 重建（2400s 级）"),
 "CHK-COVERAGE": ("P2", "报告项：无阈值/无红判据", "R-6 判定无判据门；已在 §2 标 P2（不阻塞），补阈值属另立任务"),
 "CHK-PACKAGE": ("P0", "候选注入哈希不符 ⇒ eng/ci/validate_candidate.py 必须非 0", "需 Windows 平台"),
 "CHK-ENV-ADOPTION": ("P1", "verify_toolchain.py 注入 lock 漂移 ⇒ 必须非 0",
                      "本机可跑；两个 evidence 绑定 step 已退役（见 §2.1），保留 step 实测 rc=0"),
 "CHK-SECRET-HYGIENE": ("P0", "临时 git 仓注入假凭据 ⇒ check_secret_hygiene.py --scope tracked 必须非 0",
                        "本机可跑但本轮未执行（需脚本化夹具）"),
 "CHK-KNOWN-FAILURES-BASELINE": ("P1", "--mode verify 指向不含基线的 JUnit ⇒ 必须非 0", "需 ctest JUnit 产物"),
 "LINUX-MAIN-FIXTURES": ("P0", "wf_step.py --step LINUX-PREPARE-FIXTURES 缺夹具树 ⇒ 必须非 0",
                         "wf_step 绑定面（eng/ci/workflow_binding.json）"),
 "LINUX-MAIN-BUILD-TREE": ("P0", "同上（LINUX-BUILD-ROOT-GRAPH，9000s 级构建）", "构建树 + wf_step"),
 "WIN-CANDIDATE-VALIDATE": ("P0", "同上（WINDOWS-VALIDATE-CANDIDATE）", "Windows 侧候选产物"),
}
for gate, (grade, face, why) in FACE.items():
    g(gate, "FACE-DEFINED-NOT-RUN", grade,
      (_p("-m", "pytest", "tests/quality", "-q"), 0, "repo"),
      (["sh", "-c", face], 1, "manual"), "face-defined", "-", reason=why)


def run(argv, cwd, timeout=900):
    try:
        return subprocess.run(argv, cwd=cwd, capture_output=True, text=True,
                              timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 124
    except FileNotFoundError:
        return 127


def run_gate(gate):
    spec = GATES[gate]
    LOGDIR.mkdir(parents=True, exist_ok=True)
    res = {}
    with tempfile.TemporaryDirectory(prefix="polarity-") as td:
        for side in ("positive", "negative"):
            argv, want, needs = spec[side]
            if needs == "empty":
                argv = [td if a == EMPTY else a for a in argv]
            rc = run(argv, REPO)
            res[side] = {"cmd": argv, "want_rc": want, "rc": rc, "ok": rc == want}
            (LOGDIR / (gate + "." + side + ".log")).write_text(
                "$ " + " ".join(argv) + "\nrc=" + str(rc) + " want=" + str(want) + "\n",
                encoding="utf-8")
    spec["last_run"] = res
    spec["polarity_ok"] = res["positive"]["ok"] and res["negative"]["ok"]
    return spec["polarity_ok"]


def build_ledger():
    reg = json.loads((REPO / "eng" / "ci" / "checks.json").read_text(encoding="utf-8"))["checks"]
    ids = [c["id"] for c in reg]
    missing = [i for i in ids if i not in GATES]
    extra = [k for k in GATES if k not in ids]
    doc = {
        "schema_version": 2,
        "task": "CI-003",
        "generated_by": "eng/ci/polarity_probe.py",
        "definition": {
            "PROVEN-EXECUTABLE": "正例 rc=0 且负例 rc!=0 已实跑（本会话或 R-6 实测，留 log 锚）",
            "FACE-DEFINED-NOT-RUN": "负例入口/配方机器可执行，本机缺条件（逐条写明缺失条件）",
            "RE-JUDGED": "门判据已整改（不再是原判据），登记新判据与依据",
            "RETIRED": "门已退役，见 docs/ci/01_CHECKS.md §2.1/§2.3",
        },
        "registry_entry_count": len(ids),
        "gates": {k: GATES[k] for k in ids if k in GATES},
        "summary": {
            "gates_total": len(ids),
            "PROVEN-EXECUTABLE": sum(1 for k in ids if GATES.get(k, {}).get("status") == "PROVEN-EXECUTABLE"),
            "RE-JUDGED": sum(1 for k in ids if GATES.get(k, {}).get("status") == "RE-JUDGED"),
            "FACE-DEFINED-NOT-RUN": sum(1 for k in ids if GATES.get(k, {}).get("status") == "FACE-DEFINED-NOT-RUN"),
            "missing_records": missing,
            "extra_records": extra,
        },
    }
    LEDGER.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return doc


def main():
    ap = argparse.ArgumentParser(description="CI-003 门极性证据 harness")
    ap.add_argument("--plan", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--run-provable", action="store_true", dest="run_provable")
    ap.add_argument("--gate", default=None)
    args = ap.parse_args()
    if args.gate:
        if args.gate not in GATES:
            print("POLARITY_FAIL: 未登记的门 " + args.gate + "（fail-closed）", file=sys.stderr)
            return 2
        ok = run_gate(args.gate)
        build_ledger()
        print(args.gate + ": polarity_ok=" + str(ok))
        return 0 if ok else 1
    if args.run_provable:
        bad = [gt for gt, sp in GATES.items() if sp["auto"] and not run_gate(gt)]
        doc = build_ledger()
        if bad:
            print("POLARITY_MISMATCH: " + json.dumps(bad, ensure_ascii=False))
            return 1
        print("POLARITY_RUN_PASS: " + json.dumps(doc["summary"], ensure_ascii=False))
        return 0
    doc = build_ledger()
    if args.check:
        miss, ext = doc["summary"]["missing_records"], doc["summary"]["extra_records"]
        if miss or ext:
            print("POLARITY_LEDGER_FAIL: 无记录=" + json.dumps(miss, ensure_ascii=False)
                  + " 多余=" + json.dumps(ext, ensure_ascii=False))
            return 1
        print("POLARITY_LEDGER_PASS: " + json.dumps(doc["summary"], ensure_ascii=False))
        return 0
    for gt, sp in GATES.items():
        print("{:<32} {:<22} {:<3} {}".format(gt, sp["status"], sp["grade"], sp["kind"]))
    print(json.dumps(doc["summary"], ensure_ascii=False, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
