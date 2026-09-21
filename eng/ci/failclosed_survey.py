#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/failclosed_survey.py — 全门禁 fail-closed 普查（缺失证据 / 坏证据 / 无输出）。

权威依据
  * ENGINEERING_SPEC.md §10：检查器在输入缺失、路径不存在、依赖不可用时判红；
    「文件不存在」按「无违规」通过视为假绿；
  * docs/ci/CI_SPEC.md §4/§7：红灯必须阻塞，不得静默跳过；
  * GATE-501 任务书步骤 5：全部门禁逐项验证三种注入均判红，出普查表。

做法（不是重新实现一遍判据，而是驱动真判定函数）
  对 eng/ci/checks.json 的**每个执行单元**（顶层项 + step），在临时沙箱仓库里
  直接调用 eng/ci/run_checks.py 的 evidence_verdict（run_checks / run.py 共用
  同一证据面判定）做三种注入：
    A 缺失证据：单元声明的 outputs 在沙箱中都不存在 -> 期望判红
    B 坏证据  ：requires_monitor 单元写入一份**违反 L2 冻结判据**的监控证据
               -> 期望判红（无监控证据面的单元记 N/A）
    C 无输出  ：outputs 为空且 stdout/stderr 全空的静默成功 -> 期望判红
  「适用面全部判红」才算该单元通过；不适用面显式记 N/A（不冒充已覆盖）。

用法:
  python3 eng/ci/failclosed_survey.py --json-out run/ci/failclosed-survey/survey.json
  python3 eng/ci/failclosed_survey.py --md-out artifacts/evidence/release-05/FAILCLOSED_SURVEY.md
  python3 eng/ci/failclosed_survey.py --self-test
exit 0 = 全部适用面判红；1 = 存在适用面判绿（假绿风险）；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_checks as RC  # noqa: E402  (真判定函数单一实现点)

REPO = pathlib.Path(__file__).resolve().parents[2]
REGISTRY = "eng/ci/checks.json"
RED_VERDICTS = set(RC.FAIL_VERDICTS)
N_A = "N/A"

BAD_EVIDENCE = {
    "duration_seconds": 389.278, "poll_interval": 0.2,
    "cpu_samples": [{"t": 0.0, "cpu_percent": 100.0}],
    "frozen_gate": {"verdict": "pass", "violations": [], "recorded": [],
                    "metrics": {"effective_cpus": 16, "allocated": 16,
                                "interval_seconds": 389.278,
                                "avg_utilization": 0.245275,
                                "p50_utilization": 0.038825,
                                "sample_pass_fraction": 0.069272,
                                "max_low_window_seconds": 142.049,
                                "utilization_evaluated": True}},
}


def units(registry: dict) -> list:
    out = []
    for c in registry.get("checks", []):
        steps = c.get("steps")
        if isinstance(steps, list) and steps:
            for s in steps:
                out.append((c["id"], s["id"], s))
        else:
            out.append((c["id"], c["id"], c))
    return out


def _sandbox(repo_root: pathlib.Path) -> pathlib.Path:
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="failclosed-survey-"))
    (tmp / "eng" / "contracts").mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo_root / "eng" / "contracts" / "resource_gate_v1.json",
                 tmp / "eng" / "contracts" / "resource_gate_v1.json")
    return tmp


def _verdict(step: dict, root: pathlib.Path, stdout_tail: str) -> str:
    got = RC.evidence_verdict(step, root, stdout_tail, "")
    return got[0] if got else RC.V_PASS


def survey(repo_root: pathlib.Path, registry: dict) -> dict:
    rows = []
    for parent, uid, step in units(registry):
        root = _sandbox(repo_root)
        try:
            outputs = [str(x) for x in (step.get("outputs") or [])]
            waivable = bool(step.get("waivable"))
            face_a = face_b = face_c = N_A
            if outputs:
                face_a = _verdict(step, root, "ok")
            if step.get("requires_monitor"):
                target = None
                for rel in outputs:
                    if rel.endswith(".json"):
                        target = rel
                        break
                if target is None:
                    face_b = "N/A(未声明监控证据 JSON 路径)"
                else:
                    # 先把其余声明产物铺成占位（让 B 面只检验监控证据本身，
                    # 不被 A 面的 missing_output 抢先命中），再在监控证据路径
                    # 注入违反 L2 冻结判据的证据。
                    for rel in outputs:
                        if any(ch in rel for ch in "*?["):
                            # glob 产物：materialize 其字面前缀目录，使 glob 面可命中
                            prefix = rel
                            for ch in "*?[":
                                prefix = prefix.split(ch)[0]
                            prefix = prefix.rstrip("/")
                            if prefix:
                                (root / prefix).mkdir(parents=True, exist_ok=True)
                            continue
                        p = root / rel
                        if pathlib.Path(rel).name.count(".") == 0:
                            p.mkdir(parents=True, exist_ok=True)
                        else:
                            p.parent.mkdir(parents=True, exist_ok=True)
                            p.write_text("{}" if rel.endswith(".json") else "x",
                                         encoding="utf-8")
                    p = root / target
                    p.parent.mkdir(parents=True, exist_ok=True)
                    p.write_text(json.dumps(BAD_EVIDENCE, ensure_ascii=False),
                                 encoding="utf-8")
                    face_b = _verdict(step, root, "ok")
            if not outputs and not waivable:
                if uid in RC.SILENT_OK_UNITS:
                    # 显式登记的"按设计静默成功"单元（run_checks.SILENT_OK_UNITS）：
                    # 不是未发现的假绿，但也不冒充已覆盖 —— 表内显式标注豁免来源。
                    face_c = "N/A(SILENT_OK_UNITS 显式登记)"
                else:
                    face_c = _verdict(step, root, "")
            applicable = [f for f in (face_a, face_b, face_c)
                          if f != N_A and not str(f).startswith("N/A")]
            ok = bool(applicable) and all(f in RED_VERDICTS for f in applicable)
            rows.append({
                "parent": parent, "unit": uid, "outputs": len(outputs),
                "requires_monitor": bool(step.get("requires_monitor")),
                "waivable": waivable, "missing_evidence": face_a,
                "bad_evidence": face_b, "no_output": face_c,
                "applicable_faces": len(applicable), "ok": ok,
            })
        finally:
            shutil.rmtree(root, ignore_errors=True)
    covered = [r for r in rows if r["applicable_faces"] > 0]
    failures = [r for r in covered if not r["ok"]]
    return {
        "registry": REGISTRY,
        "units": len(rows),
        "units_with_applicable_face": len(covered),
        "units_without_applicable_face": [r["unit"] for r in rows
                                          if r["applicable_faces"] == 0],
        "failures": [r["unit"] for r in failures],
        "rows": rows,
    }


def render_md(result: dict) -> str:
    lines = [
        "# 全门禁 fail-closed 普查表（缺失证据 / 坏证据 / 无输出）",
        "",
        "生成命令：python3 eng/ci/failclosed_survey.py --md-out artifacts/evidence/release-05/FAILCLOSED_SURVEY.md",
        "（判据函数 = eng/ci/run_checks.py::evidence_verdict，run.py / run_checks.py 共用；",
        "权威 ENGINEERING_SPEC.md §10 + docs/ci/CI_SPEC.md §9。）",
        "",
        "- 执行单元总数：**%d**；有适用注入面的：**%d**；" % (
            result["units"], result["units_with_applicable_face"]),
        "- 适用面全部判红（通过）：**%d**；" % (
            result["units_with_applicable_face"] - len(result["failures"])),
        "- 判绿（假绿风险）：**%d**；" % len(result["failures"]),
        "- 无适用注入面（waivable 显式登记或无证据面）：%d" % len(
            result["units_without_applicable_face"]),
        "",
        "| 执行单元 | 父项 | 声明 outputs | requires_monitor | waivable | A 缺失证据 | B 坏证据 | C 无输出 | 结论 |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for r in result["rows"]:
        verdict = "通过" if r["ok"] else (
            "无适用面" if r["applicable_faces"] == 0 else "**假绿风险**")
        lines.append(
            "| %s | %s | %d | %s | %s | %s | %s | %s | %s |" % (
                r["unit"], r["parent"], r["outputs"],
                "是" if r["requires_monitor"] else "否",
                "是" if r["waivable"] else "否",
                r["missing_evidence"], r["bad_evidence"], r["no_output"], verdict))
    lines += [
        "",
        "说明：A 面适用于声明了 outputs 的单元（缺失即 FAIL(missing_output)）；",
        "B 面适用于 requires_monitor 单元（注入违反 L2 冻结判据的证据，期望 FAIL(monitor_gate_missing)）；",
        "C 面适用于 outputs 为空且非 waivable 的单元（静默成功即 FAIL(empty_outputs)）。",
        "",
    ]
    return "\n".join(lines)


def self_test() -> int:
    cases = []
    registry = json.loads((REPO / REGISTRY).read_text(encoding="utf-8"))
    good = survey(REPO, registry)
    cases.append(("G1", "真实注册表普查可跑通且无假绿",
                  not good["failures"] and good["units"] > 50,
                  "units=%d failures=%s" % (good["units"], good["failures"][:3])))
    fake = {"checks": [
        {"id": "T-OK", "outputs": ["run/x.json"], "waivable": False,
         "requires_monitor": False, "command": ["python3", "-c", "pass"]},
        {"id": "T-BAD", "outputs": [], "waivable": False,
         "requires_monitor": False, "command": ["python3", "-c", "pass"]},
    ]}
    res = survey(REPO, fake)
    by = {r["unit"]: r for r in res["rows"]}
    cases.append(("G2", "合成注册表：A/C 面各自判红",
                  by["T-OK"]["ok"] and by["T-BAD"]["ok"],
                  json.dumps(by, ensure_ascii=False)[:200]))
    orig = RC.evidence_verdict
    try:
        RC.evidence_verdict = lambda *a, **k: None  # 恒绿注入
        res2 = survey(REPO, fake)
        cases.append(("N1", "判定函数恒绿注入 -> 普查报假绿",
                      len(res2["failures"]) == 2, str(res2["failures"])))
    finally:
        RC.evidence_verdict = orig
    passed = sum(1 for _, _, ok, _ in cases if ok)
    for tag, name, ok, detail in cases:
        print("  [%s] %s: %s" % (tag, name, "OK" if ok else "MISMATCH " + detail))
    print("FAILCLOSED_SURVEY_SELFTEST: cases=%d passed=%d failed=%d"
          % (len(cases), passed, len(cases) - passed))
    if passed != len(cases):
        print("FAILCLOSED_SURVEY_SELFTEST_FAIL", file=sys.stderr)
        return 1
    print("FAILCLOSED_SURVEY_SELFTEST_PASS: 普查自身能红能绿（恒绿注入必被抓）")
    return 0


def main(argv: list | None = None) -> int:
    ap = argparse.ArgumentParser(description="全门禁 fail-closed 普查")
    ap.add_argument("--json-out", default=None, metavar="PATH")
    ap.add_argument("--md-out", default=None, metavar="PATH")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    try:
        registry = json.loads((REPO / REGISTRY).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print("FAILCLOSED_SURVEY_FAIL: 注册表不可用（fail-closed）：%s" % exc,
              file=sys.stderr)
        return 2
    result = survey(REPO, registry)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding="utf-8")
    if args.md_out:
        out = pathlib.Path(args.md_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(render_md(result), encoding="utf-8")
    for r in result["rows"]:
        if not r["ok"] and r["applicable_faces"] > 0:
            print("  假绿风险 %s: A=%s B=%s C=%s" % (
                r["unit"], r["missing_evidence"], r["bad_evidence"], r["no_output"]))
    if result["failures"]:
        print("FAILCLOSED_SURVEY_FAIL: %d 个执行单元存在适用面判绿" % len(result["failures"]))
        return 1
    print("FAILCLOSED_SURVEY_PASS: %d 个执行单元；%d 个有适用注入面且全部判红；"
          "%d 个无适用面（显式登记）" % (
              result["units"], result["units_with_applicable_face"],
              len(result["units_without_applicable_face"])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
