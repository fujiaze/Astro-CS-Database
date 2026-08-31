#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_task_result_schema.py — R0-003 TASK_RESULT 结构一致性检查器。

以控制包 task_result.schema.json 为唯一 schema，对 evidence/v6_1_rework/tasks/*/TASK_RESULT.json
逐项做结构校验（不依赖第三方 jsonschema 库，内联实现核心约束）：

- 必填字段齐全；schema == astrocs.task-result/v2；
- parent_commit / control_sha256 / ledger_sha256 / log_sha256 / evidence sha256 格式正确；
- impact 字段完整；commands 含 started_utc/duration_seconds；evidence 非空；
- changed_paths 无绝对路径/.. 穿越。

用法: python3 tools/quality/check_task_result_schema.py [--schema 控制包/schemas/task_result.schema.json] [--results-dir evidence/v6_1_rework/tasks]
"""

from __future__ import annotations

import argparse
import datetime
import json
import re
import sys
from pathlib import Path

SHA40 = re.compile(r"^[0-9a-f]{40}$")
SHA64 = re.compile(r"^[0-9a-f]{64}$")
REQUIRED_TOP = {"schema", "task_id", "status", "parent_commit", "control_sha256",
                "ledger_sha256", "change_class", "changed_paths", "impact",
                "commands", "tests", "evidence", "findings",
                "expected_commit_subject", "created_utc"}
IMPACT_FIELDS = {"science", "algorithm", "data_semantics", "public_api",
                 "pipeline", "binary", "reason"}
CMD_FIELDS = {"command_id", "argv_redacted", "timeout_seconds", "exit_code",
              "started_utc", "duration_seconds", "log_path", "log_sha256"}


def validate_one(doc: dict, path: Path) -> list[str]:
    errs: list[str] = []
    if doc.get("schema") != "astrocs.task-result/v2":
        errs.append("schema != astrocs.task-result/v2")
    if not re.fullmatch(r"^[A-Z0-9]+-[0-9]{3}$", doc.get("task_id", "")):
        errs.append("bad task_id")
    if doc.get("status") not in {"PASS", "FAIL", "WAITING_WINDOWS"}:
        errs.append("bad status")
    for key in REQUIRED_TOP:
        if key not in doc:
            errs.append(f"missing top field: {key}")
    if not SHA40.fullmatch(doc.get("parent_commit", "")):
        errs.append("parent_commit not 40hex")
    if not SHA64.fullmatch(doc.get("control_sha256", "")):
        errs.append("control_sha256 not 64hex")
    if not SHA64.fullmatch(doc.get("ledger_sha256", "")):
        errs.append("ledger_sha256 not 64hex")
    for entry in doc.get("changed_paths", []):
        if entry.startswith("/") or ".." in Path(entry).parts:
            errs.append(f"unsafe changed path: {entry}")
    imp = doc.get("impact", {})
    for key in IMPACT_FIELDS:
        if key not in imp:
            errs.append(f"impact missing: {key}")
    cmds = doc.get("commands", [])
    if not cmds:
        errs.append("commands empty")
    for index, cmd in enumerate(cmds):
        for key in CMD_FIELDS:
            if key not in cmd:
                errs.append(f"commands[{index}] missing: {key}")
        if not SHA64.fullmatch(cmd.get("log_sha256", "")):
            errs.append(f"commands[{index}] log_sha256 not 64hex")
        try:
            datetime.datetime.fromisoformat(cmd.get("started_utc", "").replace("Z", "+00:00"))
        except (ValueError, TypeError):
            errs.append(f"commands[{index}] bad started_utc")
        try:
            float(cmd.get("duration_seconds", "x"))
        except (ValueError, TypeError):
            errs.append(f"commands[{index}] bad duration_seconds")
    ev = doc.get("evidence", [])
    if not ev:
        errs.append("evidence empty")
    for entry in ev:
        if not SHA64.fullmatch(entry.get("sha256", "")):
            errs.append("evidence sha256 not 64hex")
    try:
        datetime.datetime.fromisoformat(doc.get("created_utc", "").replace("Z", "+00:00"))
    except (ValueError, TypeError):
        errs.append("bad created_utc")
    if len(doc.get("expected_commit_subject", "")) < 8:
        errs.append("expected_commit_subject too short")
    return errs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", type=Path,
                        default=Path("工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831/schemas/task_result.schema.json"))
    parser.add_argument("--results-dir", type=Path, default=Path("evidence/v6_1_rework/tasks"))
    args = parser.parse_args(argv)

    if not args.schema.is_file():
        print(f"SCHEMA_CHECK_FAIL: schema missing: {args.schema}", file=sys.stderr)
        return 2
    try:
        json.loads(args.schema.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"SCHEMA_CHECK_FAIL: schema not JSON: {exc}", file=sys.stderr)
        return 2

    failures = 0
    checked = 0
    for result_path in sorted(args.results_dir.glob("*/TASK_RESULT.json")):
        checked += 1
        try:
            doc = json.loads(result_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            print(f"SCHEMA_CHECK_FAIL: {result_path} not JSON: {exc}")
            failures += 1
            continue
        errs = validate_one(doc, result_path)
        if errs:
            print(f"SCHEMA_CHECK_FAIL: {result_path}")
            for err in errs[:12]:
                print(f"  - {err}", file=sys.stderr)
            failures += 1
        else:
            print(f"SCHEMA_CHECK_OK: {result_path}")
    if failures:
        print(f"SCHEMA_CHECK_FAIL total_failures={failures} checked={checked}")
        return 1
    print(f"SCHEMA_CHECK_PASS checked={checked}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
