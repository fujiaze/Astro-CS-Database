#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-CONFIG-DEFAULTS —— 同一配置键的生产默认值必须一致或显式登记差异。

防的复发缺口：同一逻辑键在不同生产代码路径取不同缺省（如 smoothing_lambda
在 stage2_common.cpp 的 "auto" 缺省 0.1、在 upm.cpp 的缺省 0.0）⇒ 静默双口径。

判据（fail-closed）：
  D1 **配置键集合** = config/templates/*.json 叶子键 ∪ config/defaults.json
     fields[].key 末段 ∪ contracts/schemas/phase_config_*.schema.json 的 config
     属性 ∪ 台账 watch_keys；仅该集合内的键参与比较（避免把局部变量当配置键）；
  D2 集合内某键在生产源码/模板里出现 >=2 个不同字面量缺省 ⇒ finding；
  D3 生产源码零默认字面量命中 ⇒ rc=2（不得空扫描判绿）。

豁免唯一途径：ci/ledgers/config_default_divergences.json 的
'config_default_divergence:<key>' 条目（kind/reason/owner/exit_condition 缺字段即 rc=2）。

用法：
  python3 ci/check_config_defaults.py [--repo ROOT] [--json-out F] [--self-test]
exit 0 = 一致或已登记；exit 1 = 有 finding；exit 2 = 锚点/台账不可用。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-CONFIG-DEFAULTS"
LEDGER = "ci/ledgers/config_default_divergences.json"

_LIT = r"(-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?|true|false|\"[^\"]*\")"
_VALUE_CALL_RE = re.compile(r"\.(?:value|get|get_or|at)\s*\(\s*\"([A-Za-z_][A-Za-z0-9_.]*)\"\s*,\s*" + _LIT)
_ASSIGN_RE = re.compile(
    r"([A-Za-z_][A-Za-z0-9_]*(?:(?:->|\.)[A-Za-z_][A-Za-z0-9_]*)*)(?:->|\.)"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*" + _LIT + r"\s*;")
# 只承认「配置载体」上的赋值缺省，避免把局部变量/结构体临时量当配置默认值。
_CONFIG_RECEIVER_RE = re.compile(r"(?:^|::)(?:[A-Za-z_]*cfg|config|settings|opts|params|options|defaults)$", re.I)


def _norm_lit(lit: str) -> str:
    lit = lit.strip()
    if lit in ("true", "false") or lit.startswith('"'):
        return lit
    try:
        return repr(float(lit))
    except ValueError:
        return lit


def _leaf(key: str) -> str:
    return key.split(".")[-1]


def config_key_set(repo: pathlib.Path, watch_keys):
    keys = set(watch_keys)
    templates = sorted(repo.glob("config/templates/*.json"))
    if not templates:
        raise gc.GateError("ANCHOR_MISSING: config/templates/*.json")
    for path in templates:
        doc = gc.read_json(path, path.relative_to(repo).as_posix())
        stack = [doc.get("config") or {}]
        while stack:
            node = stack.pop()
            if not isinstance(node, dict):
                continue
            for key, value in node.items():
                if isinstance(value, dict):
                    stack.append(value)
                else:
                    keys.add(key)
    defaults = repo / "config/defaults.json"
    if defaults.is_file():
        doc = gc.read_json(defaults, "config/defaults.json")
        for field in doc.get("fields") or []:
            if isinstance(field, dict) and field.get("key"):
                keys.add(_leaf(str(field["key"])))
    for path in sorted(repo.glob("contracts/schemas/phase_config_*.schema.json")):
        doc = gc.read_json(path, path.relative_to(repo).as_posix())
        for name, definition in (doc.get("$defs") or {}).items():
            if not name.endswith("_config") or not isinstance(definition, dict):
                continue
            for key in (definition.get("properties") or {}):
                keys.add(key)
    if not keys:
        raise gc.GateError("ANCHOR_STALE: 配置键集合为空")
    return keys


def scan_defaults(repo: pathlib.Path, key_set):
    seen = {}
    files_with_key = {}
    for path, rel in gc.iter_source_files(repo / "lib"):
        text = path.read_text(encoding="utf-8", errors="replace")
        clean = gc.strip_comments(text)
        for lineno, line in enumerate(clean.splitlines(), start=1):
            for m in _VALUE_CALL_RE.finditer(line):
                key = _leaf(m.group(1))
                if key not in key_set:
                    continue
                value = _norm_lit(m.group(2))
                seen.setdefault(key, {}).setdefault(value, []).append("%s:%d" % (rel, lineno))
                files_with_key.setdefault(key, set()).add(rel)
            for m in _ASSIGN_RE.finditer(line):
                recv = m.group(1).split("->")[-1].split(".")[-1]
                if not _CONFIG_RECEIVER_RE.search(recv):
                    continue
                key = m.group(2)
                if key not in key_set:
                    continue
                value = _norm_lit(m.group(3))
                seen.setdefault(key, {}).setdefault(value, []).append("%s:%d" % (rel, lineno))
                files_with_key.setdefault(key, set()).add(rel)
    if not seen:
        raise gc.GateError("ANCHOR_STALE: 配置键集合内生产源码零默认字面量命中")
    return seen, files_with_key


def template_defaults(repo: pathlib.Path, key_set):
    out = {}
    for path in sorted(repo.glob("config/templates/*.json")):
        doc = gc.read_json(path, path.relative_to(repo).as_posix())
        stack = [doc.get("config") or {}]
        while stack:
            node = stack.pop()
            if not isinstance(node, dict):
                continue
            for key, value in node.items():
                if isinstance(value, dict):
                    stack.append(value)
                elif key in key_set and isinstance(value, (str, int, float, bool)):
                    out.setdefault(key, []).append("%s=%s" % (path.relative_to(repo).as_posix(), value))
    return out


def ledger_watch_keys(ledger):
    keys = []
    for entry in ledger.values():
        for key in entry.get("watch_keys") or []:
            keys.append(key)
    return keys


def evaluate(repo: pathlib.Path):
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    watch = ledger_watch_keys(ledger)
    key_set = config_key_set(repo, watch)
    seen, files_with_key = scan_defaults(repo, key_set)
    templates = template_defaults(repo, key_set)
    findings = []
    divergent = {}
    for key, values in sorted(seen.items()):
        sources = set(values)
        if key in templates:
            for entry in templates[key]:
                sources.add(entry.split("=", 1)[1])
        if len(sources) < 2:
            continue
        detail = {v: values[v] for v in values}
        if key in templates:
            detail["template"] = templates[key]
        divergent[key] = detail
        if ("config_default_divergence:%s" % key) in ledger:
            continue
        findings.append("config_default_divergence:%s values=%s" % (key, sorted(sources)))
    return findings, {"config_key_count": len(key_set), "scanned_key_count": len(seen),
                      "divergent_keys": sorted(divergent), "divergent_detail": divergent,
                      "watched_keys": sorted(watch)}


# --------------------------------------------------------------------------- selftest ----
_WATCH_LEDGER_ENTRY = {"id": "watch:smoothing_lambda", "kind": "watch",
                       "reason": "夹具：smoothing_lambda 未进任何配置类，显式纳入监控",
                       "owner": "fixture", "exit_condition": "夹具", "watch_keys": ["smoothing_lambda"]}


def _write_fixture(root: pathlib.Path, a: str, b: str, extra_entries=None):
    import json
    (root / "lib/modA").mkdir(parents=True, exist_ok=True)
    (root / "lib/modB").mkdir(parents=True, exist_ok=True)
    (root / "config/templates").mkdir(parents=True, exist_ok=True)
    (root / "ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / "lib/modA/a.cpp").write_text(a, encoding="utf-8")
    (root / "lib/modB/b.cpp").write_text(b, encoding="utf-8")
    (root / "config/templates/t.json").write_text(json.dumps({"config": {"precision": "fp64"}}),
                                                  encoding="utf-8")
    entries = [_WATCH_LEDGER_ENTRY] + list(extra_entries or [])
    (root / LEDGER).write_text(json.dumps(
        {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": entries}),
        encoding="utf-8")


def _selftest() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok, "cfg.smoothing_lambda = 0.1;\n", "cfg.smoothing_lambda = 0.1;\n")
        cases.append(("green_consistent", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, "cfg.smoothing_lambda = 0.1;\n", "cfg.smoothing_lambda = 0.0;\n")
        cases.append(("red_divergent_default", True, d_bad))
        d_led = base / "ledgered"
        _write_fixture(d_led, "cfg.smoothing_lambda = 0.1;\n", "cfg.smoothing_lambda = 0.0;\n",
                       extra_entries=[{"id": "config_default_divergence:smoothing_lambda",
                                       "kind": "known_divergence",
                                       "reason": "夹具：生产口径 0.0，stage2 auto 0.1",
                                       "owner": "fixture", "exit_condition": "夹具"}])
        cases.append(("green_ledgered", False, d_led))
        rc = gc.selftest_main(cases, lambda repo: evaluate(repo)[0])
        d_empty = base / "empty"
        _write_fixture(d_empty, "int x;\n", "int y;\n")
        try:
            evaluate(d_empty)
            failures.append("zero_defaults_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS zero_defaults (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-CONFIG-DEFAULTS 生产默认值一致性门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
        return 1
    print("%s_PASS config_keys=%d scanned=%d divergent=%d（均已登记或一致）"
          % (CHECK_ID, extra["config_key_count"], extra["scanned_key_count"],
             len(extra["divergent_keys"])))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())