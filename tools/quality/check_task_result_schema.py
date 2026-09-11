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

# F-CI-002-03 (CI-002, owner 裁决锚 2026-09-11 "归档线证据不受现行 schema 复验"):
# ARCHIVED_SUPERSEDED 历史线(ACTIVITY_STATE 登记: V6.1 REWORK = ARCHIVED_SUPERSEDED,
# 仅作历史参照, 交付面由宪章对齐包接管)的 TASK_RESULT 属旧 schema 时代历史证据,
# 禁止改写证据文件本身; 现行 schema 复验只约束活跃线。命中下列前缀的文件显式
# 跳过(SCHEMA_CHECK_ARCHIVED_SKIP 逐条列出 + archived_skipped 计数, 非静默);
# 归档线集合变更以 ACTIVITY_STATE 登记为准, 修改本清单须 owner 授权。
ARCHIVED_EVIDENCE_PREFIXES = ("evidence/v6_1_rework/",)


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
                        # F-CI-002-02(CI-002): 唯一事实源=contracts/schemas/
                        # (目录规范 2026-09-09 后根 schemas/ 不存在, 旧默认路径
                        # 使本检查 fail-closed exit2, Windows run 34601822033 实证)
                        default=Path(__file__).resolve().parents[2] / "contracts" / "schemas" / "task_result.schema.json")
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
    archived_skipped: list[str] = []
    repo_root = Path(__file__).resolve().parents[2]
    for result_path in sorted(args.results_dir.glob("*/TASK_RESULT.json")):
        # F-CI-002-03: 归档线前缀排除(显式呈现, 非静默跳过)。
        try:
            rel_posix = result_path.resolve().relative_to(repo_root).as_posix()
        except ValueError:
            rel_posix = result_path.as_posix()
        if rel_posix.startswith(ARCHIVED_EVIDENCE_PREFIXES):
            print(f"SCHEMA_CHECK_ARCHIVED_SKIP: {rel_posix}")
            archived_skipped.append(rel_posix)
            continue
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
        print(f"SCHEMA_CHECK_FAIL total_failures={failures} checked={checked} "
              f"archived_skipped={len(archived_skipped)}")
        return 1
    print(f"SCHEMA_CHECK_PASS checked={checked} "
          f"archived_skipped={len(archived_skipped)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
