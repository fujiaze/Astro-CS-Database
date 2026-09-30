#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""合同台账的 schema 校验与加载。

台账用于登记「合同声明但生产零消费」这类需要显式挂账的事实。每条 entry 必须写明
五字段：id、kind、reason、owner、exit_condition —— 缺任一字段即报错，不允许用空理由
静默豁免。schema 不匹配、entry 非对象、id 重复同样报错：台账的读者据此 fail-closed，
不得在台账不可用时降级为「无异常」。

本模块只依赖标准库，不引入第三方依赖。
"""
from __future__ import annotations

import json
import pathlib

LEDGER_SCHEMA = "astrocs.ci-ledger/v1"
LEDGER_REQUIRED_FIELDS = ("id", "kind", "reason", "owner", "exit_condition")


class LedgerError(Exception):
    """台账不可用：schema 不符、字段缺失或 id 重复。调用方据此 fail-closed。"""


def read_json(path, label=None) -> dict:
    label = label or str(path)
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def load_ledger(path, label) -> dict:
    """加载显式台账，返回 id -> entry 映射。

    schema 字段不符、entries 非列表、entry 非对象、五字段任一缺失或非非空字符串、
    id 重复，任一情形都抛 LedgerError。
    """
    doc = read_json(path, label)
    if not isinstance(doc, dict) or doc.get("ledger_schema") != LEDGER_SCHEMA:
        raise LedgerError(
            "LEDGER_SCHEMA_INVALID: %s（需 ledger_schema=%s）" % (label, LEDGER_SCHEMA))
    entries = doc.get("entries")
    if not isinstance(entries, list):
        raise LedgerError("LEDGER_ENTRIES_INVALID: %s" % label)
    out: dict = {}
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise LedgerError("LEDGER_ENTRY_NOT_OBJECT: %s entries[%d]" % (label, index))
        for field in LEDGER_REQUIRED_FIELDS:
            value = entry.get(field)
            if not isinstance(value, str) or not value.strip():
                raise LedgerError(
                    "LEDGER_ENTRY_MISSING_FIELD: %s entries[%d].%s"
                    "（台账必须写明理由/负责人/解除条件）" % (label, index, field))
        key = entry["id"]
        if key in out:
            raise LedgerError("LEDGER_DUPLICATE_ID: %s id=%s" % (label, key))
        out[key] = entry
    return out


def load_ledger_from_repo(repo_root, relative_path) -> dict:
    """按仓内相对路径加载台账。"""
    return load_ledger(pathlib.Path(repo_root) / relative_path, relative_path)


if __name__ == "__main__":  # pragma: no cover - 手工自检入口
    import sys
    if len(sys.argv) != 2:
        print("用法: python3 ledger_schema.py <台账.json>", file=sys.stderr)
        raise SystemExit(2)
    try:
        entries = load_ledger(sys.argv[1], sys.argv[1])
    except LedgerError as exc:
        print("FAIL: %s" % exc, file=sys.stderr)
        raise SystemExit(1)
    print("PASS: %d 条台账 entry 全部合规" % len(entries))
