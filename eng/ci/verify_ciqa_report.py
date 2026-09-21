#!/usr/bin/env python3
"""V8-CIQA-001 验收器（控制包 CONTROL_TASK_LEDGER 登记命令的实现）。

校验 evidence/ciqa/CIQA_REPORT.json 的结构契约：
- 必填顶层字段、schema_version、task_id
- attacks 非空且每项含 id/target/method/observation/verdict∈{PASS,FAIL}
- gaps 每项含 id/severity∈{P0,P1,P2,P3}/title/detail
- conclusion 含 verdict∈{PASS,FAIL,PARTIAL} 与 headline
- 一致性：conclusion.verdict 必须与 attacks 败因/缺口严重度自洽
  （存在 FAIL 攻击或 P0/P1 缺口时不得报 PASS）

退出码：0=报告契约合格（不代表 CIQA 结论为 PASS）；2=报告契约不合格。
CIQA 结论本身（FAIL/PASS）由 Owner 与后续 FIX 任务消费，本器只把关结构。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

SCHEMA_VERSION = 1
ATTACK_VERDICTS = {"PASS", "FAIL"}
GAP_SEVERITIES = {"P0", "P1", "P2", "P3"}
CONCLUSION_VERDICTS = {"PASS", "FAIL", "PARTIAL"}

REQUIRED_TOP = ("schema_version", "task_id", "utc", "attacker",
                "attacks", "gaps", "conclusion")
REQUIRED_ATTACK = ("id", "target", "method", "observation", "verdict")
REQUIRED_GAP = ("id", "severity", "title", "detail")
REQUIRED_CONCLUSION = ("verdict", "headline")


def _fail(msg: str) -> int:
    print(f"CIQA-REPORT-REJECT: {msg}")
    return 2


def verify(path: str | Path) -> int:
    p = Path(path)
    if not p.is_file():
        return _fail(f"报告不存在：{p}")
    try:
        report = json.loads(p.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        return _fail(f"JSON 解析失败：{exc}")
    if not isinstance(report, dict):
        return _fail("顶层必须是 object")

    for key in REQUIRED_TOP:
        if key not in report:
            return _fail(f"缺顶层字段 {key}")
    if report["schema_version"] != SCHEMA_VERSION:
        return _fail(f"schema_version={report['schema_version']!r} 预期 {SCHEMA_VERSION}")
    if not isinstance(report["task_id"], str) or not report["task_id"]:
        return _fail("task_id 必须非空字符串")
    if not isinstance(report["utc"], str) or not report["utc"]:
        return _fail("utc 必须非空字符串")

    attacks = report["attacks"]
    if not isinstance(attacks, list) or not attacks:
        return _fail("attacks 必须非空数组（空集不允许 PASS，与 runner 空集纪律一致）")
    attack_ids: set[str] = set()
    fail_count = 0
    for i, atk in enumerate(attacks):
        if not isinstance(atk, dict):
            return _fail(f"attacks[{i}] 必须是 object")
        for key in REQUIRED_ATTACK:
            if key not in atk or not str(atk[key]).strip():
                return _fail(f"attacks[{i}] 缺字段或空值：{key}")
        if atk["verdict"] not in ATTACK_VERDICTS:
            return _fail(f"attacks[{i}].verdict={atk['verdict']!r} "
                         f"必须是 {'/'.join(sorted(ATTACK_VERDICTS))}")
        if atk["id"] in attack_ids:
            return _fail(f"attacks[{i}].id 重复：{atk['id']}")
        attack_ids.add(atk["id"])
        if atk["verdict"] == "FAIL":
            fail_count += 1

    gaps = report["gaps"]
    if not isinstance(gaps, list):
        return _fail("gaps 必须是数组")
    gap_ids: set[str] = set()
    blocking_gaps = 0
    for i, gap in enumerate(gaps):
        if not isinstance(gap, dict):
            return _fail(f"gaps[{i}] 必须是 object")
        for key in REQUIRED_GAP:
            if key not in gap or not str(gap[key]).strip():
                return _fail(f"gaps[{i}] 缺字段或空值：{key}")
        if gap["severity"] not in GAP_SEVERITIES:
            return _fail(f"gaps[{i}].severity={gap['severity']!r} "
                         f"必须是 {'/'.join(sorted(GAP_SEVERITIES))}")
        if gap["id"] in gap_ids:
            return _fail(f"gaps[{i}].id 重复：{gap['id']}")
        gap_ids.add(gap["id"])
        if gap["severity"] in ("P0", "P1"):
            blocking_gaps += 1

    conclusion = report["conclusion"]
    if not isinstance(conclusion, dict):
        return _fail("conclusion 必须是 object")
    for key in REQUIRED_CONCLUSION:
        if key not in conclusion or not str(conclusion[key]).strip():
            return _fail(f"conclusion 缺字段或空值：{key}")
    if conclusion["verdict"] not in CONCLUSION_VERDICTS:
        return _fail(f"conclusion.verdict={conclusion['verdict']!r} "
                     f"必须是 {'/'.join(sorted(CONCLUSION_VERDICTS))}")
    if conclusion["verdict"] == "PASS" and (fail_count or blocking_gaps):
        return _fail(f"结论 PASS 与事实矛盾：{fail_count} 项攻击 FAIL / "
                     f"{blocking_gaps} 项 P0|P1 缺口（禁止假绿）")

    print(f"CIQA-REPORT-OK: task_id={report['task_id']} "
          f"attacks={len(attacks)}(FAIL={fail_count}) gaps={len(gaps)}(P0|P1={blocking_gaps}) "
          f"conclusion={conclusion['verdict']}")
    return 0


def main(argv: list[str]) -> int:
    args = [a for a in argv if not a.startswith("-")]
    if not args:
        return _fail("用法：verify_ciqa_report.py <CIQA_REPORT.json> [--strict]")
    return verify(args[0])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
