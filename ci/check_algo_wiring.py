#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-ALGO-WIRING —— 已实现算法/关键 API 的生产调用图可达性门。

防的复发缺口（C 类）：**算法/API 已实现但不在生产调用图**（源码在、module.yaml
登记了 source_symbols/node_operations，但既没绑到 register_phase_modules 的
descriptor operation、也没链进生产二进制）⇒ 静默死代码、门禁抓不到。

判据（两条，均 fail-closed）：
  W1 registry_op_unbound：module_ports.registry.json 的每个
     (module_id, operation, entry) 必须在 register_phase_modules() 的
     descriptor 绑定表里出现（模块↔真实 operation 唯一绑定）；
  W2 dormant_symbol：每个 lib/**/module.yaml 的 source_symbols 里
     **函数型**符号必须出现在生产二进制符号表（nm -C），否则视为不可达实现。

豁免唯一途径：ci/ledgers/dormant_algorithms.json 显式登记
（每条带 kind/reason/owner/exit_condition，缺字段即 rc=2）。

锚点（缺失即 rc=2）：
  - lib/infrastructure/pipeline/module_ports.registry.json
  - lib/infrastructure/scheduler/src/module_adapters.cpp
  - lib/**/module.yaml（至少一份）
  - 生产二进制（--binary 或自动发现 build/astrocs / run/ci/build-gcc-release/astrocs）

用法：
  python3 ci/check_algo_wiring.py [--repo ROOT] [--binary PATH] [--json-out F] [--self-test]
exit 0 = 全部可达或已登记；exit 1 = 有 finding；exit 2 = 锚点/台账/工具不可用。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-ALGO-WIRING"
REGISTRY = "lib/infrastructure/pipeline/module_ports.registry.json"
ADAPTERS = "lib/infrastructure/scheduler/src/module_adapters.cpp"
LEDGER = "ci/ledgers/dormant_algorithms.json"
FUNCTIONAL_SYMBOL_RE = re.compile(r"^[a-z_][a-z0-9_]*$")


def _brace_body(text: str, marker: str):
    idx = text.find(marker)
    if idx < 0:
        return None
    start = text.find("{", idx)
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None


def parse_registry_ops(registry_doc: dict) -> set:
    ops = set()
    for module in registry_doc.get("modules", []):
        mid = module.get("module_id")
        for op in module.get("operations", []):
            ops.add((mid, op.get("operation"), op.get("entry")))
    if not ops:
        raise gc.GateError("ANCHOR_STALE: %s 无 operations" % REGISTRY)
    return ops


def parse_production_bindings(adapters_text: str) -> set:
    body = _brace_body(adapters_text, "Result<void> register_phase_modules(")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: register_phase_modules not found in %s" % ADAPTERS)
    desc_ids = {}
    for m in re.finditer(r"ModuleDescriptor\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{", adapters_text):
        mid = re.search(r"\.module_id\s*=\s*\"([^\"]+)\"", adapters_text[m.end():m.end() + 4000])
        if mid:
            desc_ids[m.group(1)] = mid.group(1)
    bindings = set()
    pattern = re.compile(
        r"\{\s*([A-Za-z_][A-Za-z0-9_]*_descriptor)\s*\(\s*\)\s*,\s*\{[^{}]*?\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*\}\s*\}")
    for m in pattern.finditer(body):
        fn, op, entry = m.group(1), m.group(2), m.group(3)
        if fn in desc_ids:
            bindings.add((desc_ids[fn], op, entry))
    if not bindings:
        raise gc.GateError("ANCHOR_STALE: no descriptor operation bindings parsed from %s" % ADAPTERS)
    return bindings


def parse_manifest_symbols(repo: pathlib.Path) -> list:
    try:
        import yaml  # noqa: PLC0415
    except ImportError as exc:  # pragma: no cover
        raise gc.GateError("PREREQUISITE_MISSING: python3:yaml (%s)" % exc)
    manifests = sorted(repo.glob("lib/**/module.yaml"))
    if not manifests:
        raise gc.GateError("ANCHOR_MISSING: no lib/**/module.yaml")
    out = []
    for path in manifests:
        try:
            doc = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
        except Exception as exc:  # noqa: BLE001
            raise gc.GateError("ANCHOR_UNPARSABLE: %s: %s" % (path, exc))
        mid = doc.get("module_id") or doc.get("id") or str(path)
        for sym in doc.get("source_symbols") or []:
            name = str(sym).split("(")[0].strip()
            if FUNCTIONAL_SYMBOL_RE.match(name):
                out.append((mid, name, path.relative_to(repo).as_posix()))
    return out


def load_symbol_table(binary: pathlib.Path) -> set:
    if shutil.which("nm") is None:
        raise gc.GateError("PREREQUISITE_MISSING: nm")
    proc = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True, timeout=300)
    if proc.returncode != 0:
        raise gc.GateError("NM_FAILED: %s: %s" % (binary, proc.stderr.strip()[:200]))
    table = set()
    for line in proc.stdout.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3:
            table.add(parts[2].strip())
    if not table:
        raise gc.GateError("NM_EMPTY: %s（不得把空符号表当通过）" % binary)
    return table


def load_symbols_file(path: pathlib.Path) -> set:
    table = set()
    for line in gc.read_text(path, str(path)).splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3:
            table.add(parts[2].strip())
        elif line.strip():
            table.add(line.strip())
    if not table:
        raise gc.GateError("NM_EMPTY: %s" % path)
    return table


def find_binary(repo: pathlib.Path):
    for rel in ("build/astrocs", "run/ci/build-gcc-release/astrocs"):
        p = repo / rel
        if p.is_file():
            return p
    return None


def symbol_present(name: str, table: set) -> bool:
    if name in table:
        return True
    return any(sym.startswith(name + "(") or sym.startswith(name + "<") for sym in table)


def evaluate(repo: pathlib.Path, binary=None, symbols_file=None):
    registry_doc = gc.read_json(repo / REGISTRY, REGISTRY)
    adapters = gc.read_text(repo / ADAPTERS, ADAPTERS)
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    registry_ops = parse_registry_ops(registry_doc)
    bindings = parse_production_bindings(adapters)
    bound_pairs = {(mid, op) for (mid, op, _entry) in bindings}

    findings = []
    for (mid, op, entry) in sorted(registry_ops):
        if (mid, op) in bound_pairs:
            continue
        key = "registry_op_unbound:%s:%s" % (mid, op)
        if key in ledger:
            continue
        findings.append("%s (entry=%s)" % (key, entry))

    manifest_symbols = parse_manifest_symbols(repo)
    if symbols_file is not None:
        table = load_symbols_file(pathlib.Path(symbols_file))
    else:
        binary = pathlib.Path(binary) if binary else find_binary(repo)
        if binary is None or not binary.is_file():
            raise gc.GateError(
                "ANCHOR_MISSING: production binary not found（--binary 指定；"
                "不得在无生产产物时判绿）")
        table = load_symbol_table(binary)
    dormant = []
    for (mid, name, rel) in manifest_symbols:
        if symbol_present(name, table):
            continue
        dormant.append((mid, name, rel))
        key = "dormant_symbol:%s:%s" % (mid, name)
        if key in ledger:
            continue
        findings.append("%s (%s)" % (key, rel))
    return findings, {
        "registry_op_count": len(registry_ops),
        "production_binding_count": len(bindings),
        "manifest_functional_symbol_count": len(manifest_symbols),
        "dormant_symbols": ["%s:%s@%s" % t for t in dormant],
        "symbol_source": str(symbols_file) if symbols_file else str(binary),
    }


# --------------------------------------------------------------------------- selftest ----
_FIXTURE_REGISTRY_GREEN = {
    "registry_schema": "astrocs.module-ports-registry/v1",
    "modules": [
        {"module_id": "astrocs.phase2.coverage", "phase": "phase2",
         "operations": [{"operation": "compute_coverage", "entry": "astrocs_phase2_coverage_v1",
                         "ports": []}]},
    ],
}
_FIXTURE_REGISTRY_RED = {
    "registry_schema": "astrocs.module-ports-registry/v1",
    "modules": [
        {"module_id": "astrocs.phase2.coverage", "phase": "phase2",
         "operations": [{"operation": "compute_coverage", "entry": "astrocs_phase2_coverage_v1",
                         "ports": []}]},
        {"module_id": "astrocs.phase2.dead", "phase": "phase2",
         "operations": [{"operation": "dead_op", "entry": "astrocs_phase2_dead_v1", "ports": []}]},
    ],
}
_FIXTURE_ADAPTERS = """
ModuleDescriptor p2_coverage_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.coverage";
  return d;
}
Result<void> register_phase_modules(ModuleRegistry& registry) {
  const std::pair<ModuleDescriptor, int> p2_nodes[] = {
      {p2_coverage_descriptor(), {0, "compute_coverage", "astrocs_phase2_coverage_v1"}},
  };
  return Result<void>::success();
}
"""
_FIXTURE_MANIFEST = """
module_id: astrocs.p2.coverage
source_symbols:
  - p2_coverage_build
  - p2_coverage_free
  - P3WcsStatus
"""
_FIXTURE_SYMBOLS_OK = "0000000000001000 T p2_coverage_build\n0000000000002000 T p2_coverage_free\n"
_FIXTURE_SYMBOLS_BAD = "0000000000001000 T p2_coverage_build\n"


def _write_fixture(root: pathlib.Path, ledger=None, registry=None):
    import json
    (root / "lib/infrastructure/pipeline").mkdir(parents=True, exist_ok=True)
    (root / "lib/infrastructure/scheduler/src").mkdir(parents=True, exist_ok=True)
    (root / "lib/algorithms/coverage").mkdir(parents=True, exist_ok=True)
    (root / "ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / REGISTRY).write_text(json.dumps(registry or _FIXTURE_REGISTRY_GREEN), encoding="utf-8")
    (root / ADAPTERS).write_text(_FIXTURE_ADAPTERS, encoding="utf-8")
    (root / "lib/algorithms/coverage/module.yaml").write_text(_FIXTURE_MANIFEST, encoding="utf-8")
    (root / LEDGER).write_text(json.dumps(
        ledger or {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": []}),
        encoding="utf-8")


def _selftest() -> int:
    import json
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok)
        cases.append(("green_wired", False, d_ok))
        # 负例 W2：p2_coverage_free 不在符号表 ⇒ 红
        cases.append(("red_dormant_symbol", True, d_ok))
        # 负例 W1：registry 的 dead_op 未绑定 ⇒ 红（单独夹具，避免污染绿例）
        d_dead = base / "dead"
        _write_fixture(d_dead, registry=_FIXTURE_REGISTRY_RED)
        cases.append(("red_unbound_op", True, d_dead))
        # 负例：台账承载已知项 ⇒ 绿
        ledger = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": [
            {"id": "registry_op_unbound:astrocs.phase2.dead:dead_op", "kind": "dormant",
             "reason": "fixture", "owner": "fixture", "exit_condition": "fixture"},
            {"id": "dormant_symbol:astrocs.p2.coverage:p2_coverage_free", "kind": "dormant",
             "reason": "fixture", "owner": "fixture", "exit_condition": "fixture"},
        ]}
        d_led = base / "ledgered"
        _write_fixture(d_led, ledger)
        cases.append(("green_ledgered", False, d_led))

        def runner(repo, symbols):
            return evaluate(repo, symbols_file=symbols)[0]

        # 正例用 OK 符号表；负例用 BAD 符号表（写盘成文件，模拟 nm 输出）
        sym_ok = base / "symbols_ok.txt"
        sym_bad = base / "symbols_bad.txt"
        sym_ok.write_text(_FIXTURE_SYMBOLS_OK, encoding="utf-8")
        sym_bad.write_text(_FIXTURE_SYMBOLS_BAD, encoding="utf-8")
        for name, expect_fail, repo in cases:
            symbols = sym_bad if name == "red_dormant_symbol" else sym_ok
            findings = runner(repo, symbols)
            got = bool(findings)
            if got != expect_fail:
                failures.append("%s: expect_fail=%s got=%s %s" % (name, expect_fail, got, findings[:3]))
            else:
                print("SELFTEST_PASS %s (expect_fail=%s, findings=%d)" % (name, expect_fail, len(findings)))
        # fail-closed：无二进制且无 symbols file ⇒ GateError
        try:
            evaluate(d_ok)
            failures.append("no_binary_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS no_binary (fail-closed GateError)")
        # fail-closed：台账缺字段 ⇒ GateError
        d_badledger = base / "badledger"
        _write_fixture(d_badledger, {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "x",
                                     "entries": [{"id": "y", "kind": "z"}]})
        try:
            evaluate(d_badledger, symbols_file=_FIXTURE_SYMBOLS_OK)
            failures.append("bad_ledger_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS bad_ledger (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    print("SELFTEST_PASS: all cases match expectation")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-ALGO-WIRING 算法生产可达性门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--binary", default=None)
    ap.add_argument("--symbols-file", default=None, help="离线符号表（自证/夹具用；CI 用 nm）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo, args.binary, args.symbols_file)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
        return 1
    print("%s_PASS registry_ops=%d bound=%d dormant_symbols=%d"
          % (CHECK_ID, extra["registry_op_count"], extra["production_binding_count"],
             len(extra["dormant_symbols"])))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())