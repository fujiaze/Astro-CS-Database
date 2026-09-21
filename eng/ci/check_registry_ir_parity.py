#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-REGISTRY-IR-PARITY —— 生产注册表 ↔ Pipeline IR 节点集合双向一致门。

防的复发缺口（B 类）：**注册了但不在管线**（模块 descriptor 已 register_module，
但 build_pipeline_ir 的 preset→IR 里没有该节点 ⇒ 该模块永远不被调度，静默死代码）。

判据（双向集合，fail-closed）：
  P1 registered_not_in_ir：register_phase_modules() 里 register_module 的每个
     descriptor 的 module_id 必须出现在 build_pipeline_ir() 的节点 module_id 集合；
  P2 ir_not_registered：IR 里每个节点 module_id 必须有已注册 descriptor；
  P3 任一侧为空（解析不到）⇒ 判红（不得把「解析不到」当「一致」）。

锚点（硬编码引用，缺失即 rc=2 并点名 ANCHOR_MISSING）：
  - lib/infrastructure/scheduler/src/module_adapters.cpp（生产注册面）
  - lib/infrastructure/cli/runtime_client.cpp（生产 IR 面）
  - eng/ci/ledgers/registry_ir_parity.json（显式台账：已知差异 + 理由/负责人/解除条件）

用法：
  python3 eng/ci/check_registry_ir_parity.py [--repo ROOT] [--json-out F] [--self-test]
exit 0 = 一致；exit 1 = 有 finding；exit 2 = 锚点/台账不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-REGISTRY-IR-PARITY"
REGISTER_CPP = "lib/infrastructure/scheduler/src/module_adapters.cpp"
IR_CPP = "lib/infrastructure/cli/runtime_client.cpp"
LEDGER = "eng/ci/ledgers/registry_ir_parity.json"


def _brace_body(text: str, marker: str):
    idx = text.find(marker)
    if idx < 0:
        return None
    start = text.find("{", idx)
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None


def parse_descriptor_module_ids(adapters_text: str) -> dict:
    out = {}
    for m in re.finditer(r"ModuleDescriptor\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{", adapters_text):
        fn = m.group(1)
        window = adapters_text[m.end():m.end() + 4000]
        mid = re.search(r"\.module_id\s*=\s*\"([^\"]+)\"", window)
        if mid:
            out[fn] = mid.group(1)
    return out


def parse_registered(adapters_text: str) -> set:
    body = _brace_body(adapters_text, "Result<void> register_phase_modules(")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: register_phase_modules not found in %s" % REGISTER_CPP)
    desc_ids = parse_descriptor_module_ids(adapters_text)
    registered = set()
    for fn in set(re.findall(r"([A-Za-z_][A-Za-z0-9_]*_descriptor)\s*\(\s*\)", body)):
        if fn in desc_ids:
            registered.add(desc_ids[fn])
    if not registered:
        raise gc.GateError("ANCHOR_STALE: no registered descriptors parsed from %s" % REGISTER_CPP)
    return registered


def parse_ir(ir_text: str) -> set:
    body = _brace_body(ir_text, "std::string build_pipeline_ir(")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: build_pipeline_ir not found in %s" % IR_CPP)
    clean = gc.strip_comments(body)
    nodes = set(re.findall(r"\"(astrocs\.phase[123]\.[a-z0-9\-]+)\"", clean))
    if not nodes:
        raise gc.GateError("ANCHOR_STALE: no IR node module_id parsed from %s" % IR_CPP)
    return nodes


def collect(repo: pathlib.Path):
    adapters = gc.read_text(repo / REGISTER_CPP, REGISTER_CPP)
    ir_text = gc.read_text(repo / IR_CPP, IR_CPP)
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    return parse_registered(adapters), parse_ir(ir_text), ledger


def evaluate(repo: pathlib.Path):
    registered, ir_nodes, ledger = collect(repo)
    findings = []
    for mid in sorted(registered - ir_nodes):
        key = "registered_not_in_ir:%s" % mid
        if key in ledger:
            continue
        findings.append(key)
    for mid in sorted(ir_nodes - registered):
        key = "ir_not_registered:%s" % mid
        if key in ledger:
            continue
        findings.append(key)
    return findings, {"registered_count": len(registered), "ir_node_count": len(ir_nodes),
                      "registered_not_in_ir": sorted(registered - ir_nodes),
                      "ir_not_registered": sorted(ir_nodes - registered)}


# --------------------------------------------------------------------------- selftest ----
_FIXTURE_ADAPTERS = """
ModuleDescriptor phase2_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.resample";
  return d;
}
ModuleDescriptor p2_coverage_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.coverage";
  return d;
}
Result<void> register_phase_modules(ModuleRegistry& registry) {
  auto d2 = phase2_descriptor();
  registry.register_module(d2);
  const std::pair<ModuleDescriptor, int> p2_nodes[] = {
      {p2_coverage_descriptor(), {0}},
  };
  for (const auto& [d, spec] : p2_nodes) { registry.register_module(d); }
  return Result<void>::success();
}
"""
_FIXTURE_IR_OK = """
std::string build_pipeline_ir(const std::vector<int>& phases, const std::string& config_json, std::string* err) {
  const std::vector<std::tuple<std::string, std::string>> chain = {
      {"coverage", "astrocs.phase2.coverage"},
      {"resample", "astrocs.phase2.resample"},
  };
  return "{}";
}
"""
_FIXTURE_IR_BAD = """
std::string build_pipeline_ir(const std::vector<int>& phases, const std::string& config_json, std::string* err) {
  const std::vector<std::tuple<std::string, std::string>> chain = {
      {"coverage", "astrocs.phase2.coverage"},
  };
  return "{}";
}
"""
_LEDGER_EMPTY = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": []}


def _write_fixture(root: pathlib.Path, ir_text: str, ledger=None):
    (root / "lib/infrastructure/scheduler/src").mkdir(parents=True, exist_ok=True)
    (root / "lib/infrastructure/cli").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / REGISTER_CPP).write_text(_FIXTURE_ADAPTERS, encoding="utf-8")
    (root / IR_CPP).write_text(ir_text, encoding="utf-8")
    import json
    (root / LEDGER).write_text(json.dumps(ledger or _LEDGER_EMPTY), encoding="utf-8")


def _selftest() -> int:
    import json
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok, _FIXTURE_IR_OK)
        cases.append(("green_parity", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, _FIXTURE_IR_BAD)
        cases.append(("red_registered_not_in_ir", True, d_bad))
        ledger = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture",
                  "entries": [{"id": "registered_not_in_ir:astrocs.phase2.resample",
                               "kind": "known_divergence", "reason": "fixture 已知项",
                               "owner": "fixture", "exit_condition": "fixture 移除"}]}
        d_led = base / "ledgered"
        _write_fixture(d_led, _FIXTURE_IR_BAD, ledger)
        cases.append(("green_ledgered", False, d_led))
        d_extra = base / "extra"
        _write_fixture(d_extra, _FIXTURE_IR_OK.replace(
            '"astrocs.phase2.resample"', '"astrocs.phase2.ghost"'))
        cases.append(("red_ir_not_registered", True, d_extra))
        d_missing = base / "missing"
        _write_fixture(d_missing, _FIXTURE_IR_OK)
        (d_missing / IR_CPP).unlink()
        try:
            evaluate(d_missing)
            failures.append("missing_anchor_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS missing_anchor (fail-closed GateError)")
        # 负例 fail-closed：台账缺 reason ⇒ GateError
        d_badledger = base / "badledger"
        _write_fixture(d_badledger, _FIXTURE_IR_BAD, {"ledger_schema": gc.LEDGER_SCHEMA,
                      "ledger_id": "fixture", "entries": [{"id": "x", "kind": "k"}]})
        try:
            evaluate(d_badledger)
            failures.append("bad_ledger_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS bad_ledger (fail-closed GateError)")
        rc = gc.selftest_main(cases, lambda repo: evaluate(repo)[0])
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-REGISTRY-IR-PARITY 注册表↔IR 双向一致门")
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
    print("%s_PASS registered=%d ir_nodes=%d（双向差集空）"
          % (CHECK_ID, extra["registered_count"], extra["ir_node_count"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
