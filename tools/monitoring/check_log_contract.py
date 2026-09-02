#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-001 结构化日志合同机器检查器（tools/monitoring 域，owner SA-LOG-08）。

检查内容（exit 0 = PASS，任何违例 => 非 0 且输出 machine JSON verdict=FAIL）：
  1. schema 自检：log_event_v1.schema.json 为合法 JSON Schema draft-07 子集，
     必需字段集合 == log_event.py REQUIRED_FIELDS（合同单一事实源一致性）；
  2. JSONL 行校验：逐行解析 JSON、命中 schema 的 required/properties 规则；
     缺字段/未知字段/枚举外值/类型错/pattern 错 => FAIL；
  3. seq 单调性：全文件 seq 严格递增、首事件=1、无空洞；
  4. error 载荷：level=error 必须含 error{source,symbol,status}；非 error 不得携带；
  5. 脱敏规则：诊断文本中绝对路径/家目录/凭据样例 => <redacted>（--verify-redact）；
  6. 大小上限：单行（含换行）<= MAX_LINE_BYTES(4096)。

本检查器是 LOG-001 小型验证工具，不是生产 logger（LOG-002 接监控）。
零第三方依赖（仅 Python 标准库），可在任意控制节点复跑。

用法：
  python3 tools/monitoring/check_log_contract.py --selfcheck
  python3 tools/monitoring/check_log_contract.py --jsonl <file> [--verify-redact]
  python3 tools/monitoring/check_log_contract.py --stdin
  （--schema 可选，默认取 runtime/logging/log_event_v1.schema.json）
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

try:  # 允许从仓库根或任意 cwd 运行
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
    from runtime.logging.log_event import (  # type: ignore
        MAX_LINE_BYTES, REPO_ROOT, REQUIRED_FIELDS, SCHEMA_ID, line_size_bytes, redact)
except Exception:  # pragma: no cover - 仅在独立运行异常时兜底
    MAX_LINE_BYTES = 4096
    REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
    REQUIRED_FIELDS = [
        "schema", "seq", "ts", "run", "task", "node", "module", "phase",
        "commit", "host", "level", "event", "units", "elapsed", "diagnostic",
    ]
    SCHEMA_ID = "astrocs.log.event.v1"

    def line_size_bytes(line: str) -> int:
        return len(line.encode("utf-8", "replace"))

    def redact(text: str) -> str:
        return text

DEFAULT_SCHEMA = REPO_ROOT / "runtime" / "logging" / "log_event_v1.schema.json"

_JSON_NUMBER = (int, float)
_JSON_PRIM = (str, int, float, bool, type(None))


class ValidationError(Exception):
    pass


# ── 迷你 JSON Schema draft-07 子集校验器 ───────────────────────────────────────
# 仅实现本合同 schema 使用的关键字：type/required/properties/additionalProperties/
# enum/const/pattern/minimum/maximum/maxLength/minItems/items。任何其他关键字
# 保守拒绝（未知关键字=违例，防止 schema 与实现漂移）。

_SUPPORTED_KEYWORDS = {
    "type", "required", "properties", "additionalProperties", "enum", "const",
    "pattern", "minimum", "maximum", "maxLength", "minItems", "items",
    "$schema", "$id", "title", "description",
}


def _check_type(value: Any, typ: str, path: str, errs: List[str]) -> None:
    if typ == "string":
        if not isinstance(value, str):
            errs.append(f"{path}: 期望 string，实际 {type(value).__name__}")
    elif typ == "integer":
        if not isinstance(value, int) or isinstance(value, bool):
            errs.append(f"{path}: 期望 integer，实际 {type(value).__name__}")
    elif typ == "number":
        if not isinstance(value, _JSON_NUMBER) or isinstance(value, bool):
            errs.append(f"{path}: 期望 number，实际 {type(value).__name__}")
    elif typ == "boolean":
        if not isinstance(value, bool):
            errs.append(f"{path}: 期望 boolean，实际 {type(value).__name__}")
    elif typ == "object":
        if not isinstance(value, dict):
            errs.append(f"{path}: 期望 object，实际 {type(value).__name__}")
    elif typ == "array":
        if not isinstance(value, list):
            errs.append(f"{path}: 期望 array，实际 {type(value).__name__}")
    elif typ == "null":
        if value is not None:
            errs.append(f"{path}: 期望 null，实际 {type(value).__name__}")
    else:
        errs.append(f"{path}: 未知 type {typ!r}")


def validate_subschema(instance: Any, schema: Dict[str, Any], path: str,
                       errs: List[str]) -> None:
    for kw in schema:
        if kw not in _SUPPORTED_KEYWORDS:
            errs.append(f"{path}: schema 含未支持关键字 {kw!r}（保守拒绝）")
            continue
    typ = schema.get("type")
    if typ:
        _check_type(instance, typ, path, errs)
    if typ == "object" and isinstance(instance, dict):
        req = schema.get("required", [])
        for key in req:
            if key not in instance:
                errs.append(f"{path}: 缺必需字段 {key!r}")
        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for key in instance:
                if key not in props:
                    errs.append(f"{path}: 未知字段 {key!r}（additionalProperties=false）")
        for key, subschema in props.items():
            if key in instance:
                validate_subschema(instance[key], subschema, f"{path}.{key}", errs)
    if typ == "array" and isinstance(instance, list):
        items = schema.get("items")
        if items and isinstance(items, dict):
            for i, it in enumerate(instance):
                validate_subschema(it, items, f"{path}[{i}]", errs)
        mini = schema.get("minItems")
        if isinstance(mini, int) and len(instance) < mini:
            errs.append(f"{path}: 数组长度 {len(instance)} < minItems {mini}")
    if isinstance(instance, str):
        pat = schema.get("pattern")
        if pat is not None:
            if not re.fullmatch(pat, instance):
                errs.append(f"{path}: 值 {instance!r} 不匹配 pattern {pat!r}")
        ml = schema.get("maxLength")
        if isinstance(ml, int) and len(instance) > ml:
            errs.append(f"{path}: 长度 {len(instance)} > maxLength {ml}")
    if isinstance(instance, _JSON_NUMBER) and not isinstance(instance, bool):
        mn = schema.get("minimum")
        if isinstance(mn, (int, float)) and instance < mn:
            errs.append(f"{path}: 值 {instance} < minimum {mn}")
        mx = schema.get("maximum")
        if isinstance(mx, (int, float)) and instance > mx:
            errs.append(f"{path}: 值 {instance} > maximum {mx}")
    if "enum" in schema:
        if instance not in schema["enum"]:
            errs.append(f"{path}: 值 {instance!r} 不在 enum {schema['enum']}")
    if "const" in schema:
        if instance != schema["const"]:
            errs.append(f"{path}: 值 {instance!r} != const {schema['const']!r}")


# ── 检查项 ─────────────────────────────────────────────────────────────────────
def check_schema_self(root: pathlib.Path, schema_path: Optional[pathlib.Path],
                      ) -> Tuple[bool, List[str], Dict[str, Any]]:
    """schema 自检：JSON 合法、必需字段集合与参考实现 REQUIRED_FIELDS 一致。"""
    errs: List[str] = []
    p = schema_path or DEFAULT_SCHEMA
    try:
        schema = json.loads(p.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        return False, [f"schema 文件 {p} 无法解析: {e}"], {}
    if not isinstance(schema, dict):
        return False, ["schema 顶层不是 object"], {}
    if schema.get("$id") != SCHEMA_ID:
        errs.append(f"schema $id {schema.get('$id')!r} != 合同 {SCHEMA_ID!r}")
    if schema.get("type") != "object":
        errs.append("schema 顶层 type 必须为 object")
    req = schema.get("required", [])
    if req != REQUIRED_FIELDS:
        errs.append(
            f"schema required {req} != 参考实现 REQUIRED_FIELDS {REQUIRED_FIELDS}")
    return (not errs), errs, schema


def validate_jsonl_line(line: str, lineno: int, schema: Dict[str, Any],
                        errs: List[str]) -> Optional[Dict[str, Any]]:
    stripped = line.rstrip("\n").rstrip("\r")
    if not stripped:
        errs.append(f"第 {lineno} 行：空行（禁止；每行必须为一个事件对象）")
        return None
    try:
        obj = json.loads(stripped)
    except json.JSONDecodeError as e:
        errs.append(f"第 {lineno} 行：JSON 解析失败: {e}")
        return None
    if not isinstance(obj, dict):
        errs.append(f"第 {lineno} 行：不是 JSON object")
        return None
    validate_subschema(obj, schema, f"L{lineno}", errs)
    return obj


def check_seq(objs: List[Dict[str, Any]], errs: List[str]) -> None:
    prev = 0
    for i, o in enumerate(objs):
        seq = o.get("seq")
        if not isinstance(seq, int) or isinstance(seq, bool):
            errs.append(f"seq 非整数（事件 #{i + 1}）")
            continue
        if seq != prev + 1:
            errs.append(
                f"seq 不连续：事件 #{i + 1} seq={seq}，期望 {prev + 1}"
                "（要求首事件=1、严格递增、无空洞）")
        prev = seq if isinstance(seq, int) and not isinstance(seq, bool) else prev


def check_error_payload(objs: List[Dict[str, Any]], errs: List[str]) -> None:
    for i, o in enumerate(objs):
        level = o.get("level")
        err = o.get("error")
        if level == "error":
            if not isinstance(err, dict):
                errs.append(f"事件 #{i + 1}：level=error 必须携带 error 对象")
                continue
            for sub in ("source", "symbol", "status"):
                if sub not in err or not isinstance(err[sub], str) or not err[sub]:
                    errs.append(f"事件 #{i + 1}：error.{sub} 缺失或为空")
        elif err is not None:
            errs.append(f"事件 #{i + 1}：非 error 级不得携带 error 载荷")


def check_redact(objs: List[Dict[str, Any]], errs: List[str]) -> None:
    """脱敏生效样例：任意 diagnostic 中不得残留敏感模式原文。"""
    probes = [
        "/home/alice/astrocs/run/out.fits",
        "/Users/bob/Documents/catalog.fits",
        "C:\\Users\\mallory\\astrocs\\data.fits",
        "\\\\srv\\share\\hips\\tile.fits",
        "password=super-secret-1",
        "https://user:pass@example.com/token",
    ]
    for p in probes:
        r = redact(p)
        if r != "<redacted>":
            errs.append(f"脱敏规则失效：{p!r} -> {r!r}（应为 <redacted>）")
    for i, o in enumerate(objs):
        diag = o.get("diagnostic")
        if isinstance(diag, str):
            for probe in ("/home/", "/Users/", "C:\\", "password=",
                          "Bearer ", "://"):
                if probe in diag:
                    errs.append(
                        f"事件 #{i + 1} diagnostic 含未脱敏敏感模式 {probe!r}: {diag!r}")
                    break


def check_line_size(lines: List[str], errs: List[str]) -> None:
    for i, ln in enumerate(lines, start=1):
        if line_size_bytes(ln) > MAX_LINE_BYTES:
            errs.append(
                f"第 {i} 行大小 {line_size_bytes(ln)}B > 上限 {MAX_LINE_BYTES}B")


def run_check(jsonl: Optional[str], schema_path: Optional[pathlib.Path],
              verify_redact: bool) -> Tuple[bool, List[str], Dict[str, Any]]:
    errs: List[str] = []
    meta: Dict[str, Any] = {}
    ok, schema_errs, schema = check_schema_self(REPO_ROOT, schema_path)
    meta["schema"] = str(schema_path or DEFAULT_SCHEMA)
    meta["schema_selfcheck"] = "PASS" if ok else "FAIL"
    errs += [f"schema 自检: {e}" for e in schema_errs]
    if not ok:
        return False, errs, meta
    lines: List[str] = []
    objs: List[Dict[str, Any]] = []
    if jsonl:
        lines = pathlib.Path(jsonl).read_text(encoding="utf-8").splitlines(keepends=True)
        meta["lines"] = len(lines)
        for i, ln in enumerate(lines, start=1):
            o = validate_jsonl_line(ln, i, schema, errs)
            if o is not None:
                objs.append(o)
        check_seq(objs, errs)
        check_error_payload(objs, errs)
        check_line_size(lines, errs)
        if verify_redact:
            check_redact(objs, errs)
    meta["checks"] = ["schema_selfcheck", "jsonl_schema", "seq_monotonic",
                      "error_payload"]
    if verify_redact:
        meta["checks"].append("redact_rules")
    if jsonl:
        meta["checks"].append("line_size")
    return (not errs), errs, meta


def selfcheck(schema_path: Optional[pathlib.Path]) -> Tuple[bool, List[str], Dict[str, Any]]:
    """--selfcheck：无样本文件也可独立验证检查器可运行（PASS 语义）。"""
    errs: List[str] = []
    meta: Dict[str, Any] = {}
    ok, schema_errs, schema = check_schema_self(REPO_ROOT, schema_path)
    meta["schema"] = str(schema_path or DEFAULT_SCHEMA)
    meta["schema_selfcheck"] = "PASS" if ok else "FAIL"
    errs += [f"schema 自检: {e}" for e in schema_errs]
    if not ok:
        return False, errs, meta
    # 用参考实现生成最小合法行，验证整条链路
    try:
        sys.path.insert(0, str(REPO_ROOT))
        from runtime.logging.log_event import LogEvent  # type: ignore
        ev = LogEvent(seq=1, ts="2026-09-02T00:00:00Z", run="r1", level="info",
                      event="start", diagnostic="自检：检查器链路正常",
                      phase="runtime", module="core", commit="0" * 40,
                      host="ctrl-node", units="", elapsed=0.0)
        line = ev.to_jsonl()
    except Exception as e:  # noqa: BLE001
        return False, [f"selfcheck 参考实现调用失败: {e}"], meta
    o = validate_jsonl_line(line, 1, schema, errs)
    if o is not None:
        check_seq([o], errs)
        check_error_payload([o], errs)
        check_redact([o], errs)
        check_line_size([line], errs)
    meta["checks"] = ["schema_selfcheck", "jsonl_schema", "seq_monotonic",
                      "error_payload", "redact_rules", "line_size"]
    meta["selfcheck_line"] = line.strip()
    return (not errs), errs, meta


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="LOG-001 结构化日志合同检查器")
    ap.add_argument("--schema", type=pathlib.Path, default=None,
                    help="log_event_v1.schema.json 路径（默认 runtime/logging/）")
    ap.add_argument("--jsonl", type=str, default=None, help="待校验 JSONL 文件")
    ap.add_argument("--stdin", action="store_true", help="从 stdin 读取 JSONL")
    ap.add_argument("--verify-redact", action="store_true",
                    help="校验脱敏规则生效")
    ap.add_argument("--selfcheck", action="store_true", help="自检模式")
    args = ap.parse_args(argv)

    if args.stdin:
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False) as tf:
            tf.write(sys.stdin.read())
            tmp = tf.name
        try:
            ok, errs, meta = run_check(tmp, args.schema, args.verify_redact)
        finally:
            pathlib.Path(tmp).unlink(missing_ok=True)
    elif args.selfcheck:
        ok, errs, meta = selfcheck(args.schema)
    else:
        ok, errs, meta = run_check(args.jsonl, args.schema, args.verify_redact)
    verdict = "PASS" if ok else "FAIL"
    out = {
        "tool": "check_log_contract.py",
        "contract": "astrocs.log.event.v1",
        "task": "LOG-001",
        "verdict": verdict,
        "meta": meta,
        "errors": errs[:200],
        "error_count": len(errs),
    }
    print(json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
