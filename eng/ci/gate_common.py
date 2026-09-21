#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RELEASE-02 fix-gates 共用设施（CI 门禁防复发面）。

本模块只被 eng/ci/check_*.py 消费；不引入任何第三方依赖。

设计口径（对齐 docs/ci/01_CHECKS.md §1 与 ENGINEERING_SPEC.md §8）：
- **fail-closed**：锚点缺失/不可解析/台账非法一律 rc=2（runner error），绝不静默降级；
- **台账不是后门**：每条台账 entry 必须带 id/kind/reason/owner/exit_condition 五字段，
  缺一即判 runner error —— 禁止用空理由静默豁免；
- **能红能绿**：每个检查器提供 --self-test（tempfile 夹具，正例必绿 + 负例必红），
  并接受 --repo 指向夹具根，使同一判据可在真仓与夹具上复跑。
"""
from __future__ import annotations

import json
import pathlib
import re
import sys

LEDGER_SCHEMA = "astrocs.ci-ledger/v1"
LEDGER_REQUIRED_FIELDS = ("id", "kind", "reason", "owner", "exit_condition")


class GateError(Exception):
    """锚点/台账不可用 —— fail-closed，映射为 rc=2。"""


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent.parent.parent


def read_text(path, label=None) -> str:
    p = pathlib.Path(path)
    if not p.is_file():
        raise GateError("ANCHOR_MISSING: %s" % (label or p))
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:  # pragma: no cover - 权限/IO 异常
        raise GateError("ANCHOR_UNREADABLE: %s: %s" % (label or p, exc))


def read_json(path, label=None):
    p = pathlib.Path(path)
    if not p.is_file():
        raise GateError("ANCHOR_MISSING: %s" % (label or p))
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - 任何解析失败都 fail-closed
        raise GateError("ANCHOR_UNPARSABLE: %s: %s" % (label or p, exc))


def load_ledger(path, label) -> dict:
    """加载显式台账；schema/字段/重复 id 任一非法 ⇒ GateError（fail-closed）。"""
    doc = read_json(path, label)
    if not isinstance(doc, dict) or doc.get("ledger_schema") != LEDGER_SCHEMA:
        raise GateError("LEDGER_SCHEMA_INVALID: %s（需 ledger_schema=%s）" % (label, LEDGER_SCHEMA))
    entries = doc.get("entries")
    if not isinstance(entries, list):
        raise GateError("LEDGER_ENTRIES_INVALID: %s" % label)
    out: dict = {}
    for i, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise GateError("LEDGER_ENTRY_NOT_OBJECT: %s entries[%d]" % (label, i))
        for field in LEDGER_REQUIRED_FIELDS:
            value = entry.get(field)
            if not isinstance(value, str) or not value.strip():
                raise GateError(
                    "LEDGER_ENTRY_MISSING_FIELD: %s entries[%d].%s（台账必须写明理由/负责人/解除条件）"
                    % (label, i, field))
        key = entry["id"]
        if key in out:
            raise GateError("LEDGER_DUPLICATE_ID: %s id=%s" % (label, key))
        out[key] = entry
    return out


def strip_comments(text: str) -> str:
    """去 C/C++ 块注释与行注释（保留换行以维持行结构）。"""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def iter_source_files(root: pathlib.Path, suffixes=(".cpp", ".h", ".hpp", ".cc"),
                      skip_parts=("third_party", "tests", "archive", "build")):
    root = pathlib.Path(root)
    if not root.is_dir():
        raise GateError("ANCHOR_MISSING: source root %s" % root)
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in suffixes:
            continue
        rel = path.relative_to(root).as_posix()
        if any(part in path.parts for part in skip_parts):
            continue
        yield path, rel


def write_json(path, doc) -> None:
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def report(verdict: str, check_id: str, findings, extra: dict | None = None,
           json_out=None) -> dict:
    doc = {"check_id": check_id, "verdict": verdict, "findings": list(findings)}
    if extra:
        doc.update(extra)
    if json_out:
        write_json(json_out, doc)
    return doc


def print_findings(check_id: str, findings, limit: int = 80) -> None:
    print("%s_FAIL: %d finding(s)" % (check_id, len(findings)))
    for item in findings[:limit]:
        print("  - %s" % (item if isinstance(item, str) else json.dumps(item, ensure_ascii=False)))
    if len(findings) > limit:
        print("  ... (%d more)" % (len(findings) - limit))


def selftest_main(cases, runner) -> int:
    """cases: [(name, expect_fail(bool), repo_path)]；runner(repo)->findings。"""
    failures = []
    for name, expect_fail, repo in cases:
        try:
            findings = runner(pathlib.Path(repo))
        except GateError as exc:
            failures.append("%s: unexpected GateError: %s" % (name, exc))
            continue
        got_fail = bool(findings)
        if got_fail != expect_fail:
            failures.append("%s: expected_fail=%s got_fail=%s findings=%s"
                            % (name, expect_fail, got_fail, findings[:3]))
        else:
            print("SELFTEST_PASS %s (expect_fail=%s, findings=%d)"
                  % (name, expect_fail, len(findings)))
    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        return 1
    print("SELFTEST_PASS: all cases match expectation")
    return 0
