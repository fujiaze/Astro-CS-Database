#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PROD-SCALE —— 关键算法必须用生产尺度参数跑测试（防尺度裂缝）。

防的复发缺口：合成测试用 O(1) 参数（如 control_ivar=1.0）掩盖生产尺度
（control_ivar 中位 ~5.6e-22）⇒ 权重/数值行为在生产尺度下失真而门禁全绿。

判据（fail-closed）：
  S1 台账 eng/ci/ledgers/prod_scale_params.json#params 声明关键参数的生产尺度
     （name/production/unit/rationale/ratio_floor/ratio_ceil）；
  S2 在测试/源码里扫到该参数的字面量赋值，|value/production| 落在
     [ratio_floor, ratio_ceil] 之外 ⇒ finding（合成尺度）；
  S3 每个参数必须至少有一处生产尺度用例（否则 prod_scale_missing_case）——
     禁止只测合成尺度；
  S4 无参数声明 / 无扫描文件 / 零命中 ⇒ rc=2（不得空扫描判绿）。

豁免唯一途径：eng/ci/ledgers/prod_scale_params.json#entries 的
'prod_scale_exception:<param>:<file>' 条目（kind/reason/owner/exit_condition 缺字段即 rc=2）。

用法：
  python3 eng/ci/check_prod_scale.py [--repo ROOT] [--json-out F] [--self-test]
exit 0 = 生产尺度覆盖或已登记；exit 1 = 有 finding；exit 2 = 锚点/台账不可用。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-PROD-SCALE"
LEDGER = "eng/ci/ledgers/prod_scale_params.json"
SCAN_GLOBS = ("lib/**/tests/**/*.cpp", "lib/**/tests/**/*.h", "eng/tests/**/*.cpp",
              "lib/**/tests/**/*.hpp", "eng/tests/**/*.hpp")
_LIT = r"(-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)"
_CONST_DEF_RE = re.compile(
    r"\b(?:const|constexpr)\s+[A-Za-z_][A-Za-z0-9_:<>,\s\*&]*?\b"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*" + _LIT + r"\s*;")
_DEFINE_RE = re.compile(r"#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+" + _LIT)
_ASSIGN_VAL_RE = r"(?:([A-Za-z_][A-Za-z0-9_]*)|" + _LIT + r")"


def parse_params(repo: pathlib.Path):
    doc = gc.read_json(repo / LEDGER, "ledger/prod_scale_params.json")
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    params = doc.get("params")
    if not isinstance(params, list) or not params:
        raise gc.GateError("LEDGER_PARAMS_MISSING: %s#params" % LEDGER)
    out = []
    for i, param in enumerate(params):
        for field in ("name", "production", "unit", "rationale"):
            if param.get(field) in (None, ""):
                raise gc.GateError("LEDGER_PARAM_FIELD: params[%d].%s" % (i, field))
        out.append({
            "name": param["name"],
            "production": float(param["production"]),
            "unit": param["unit"],
            "rationale": param["rationale"],
            "ratio_floor": float(param.get("ratio_floor", 1e-6)),
            "ratio_ceil": float(param.get("ratio_ceil", 1e6)),
        })
    return out, ledger


def scan_usages(repo: pathlib.Path, params):
    files = []
    for pattern in SCAN_GLOBS:
        files.extend(repo.glob(pattern))
    files = sorted({p for p in files if p.is_file()})
    if not files:
        raise gc.GateError("ANCHOR_MISSING: 无测试/源码扫描文件（%s）" % (SCAN_GLOBS,))
    usages = {p["name"]: [] for p in params}
    for path in files:
        rel = path.relative_to(repo).as_posix()
        clean = gc.strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        const_map = {}
        for m in _CONST_DEF_RE.finditer(clean):
            const_map[m.group(1)] = float(m.group(2))
        for m in _DEFINE_RE.finditer(clean):
            const_map[m.group(1)] = float(m.group(2))
        for lineno, line in enumerate(clean.splitlines(), start=1):
            for param in params:
                name = param["name"]
                pat = (r"\b" + re.escape(name) + r"\s*=\s*" + _ASSIGN_VAL_RE +
                       r"\s*(?:/\s*([A-Za-z_][A-Za-z0-9_]*)\s*)?;")
                for m in re.finditer(pat, line):
                    ident, lit, divisor = m.group(1), m.group(2), m.group(3)
                    if lit is not None:
                        value = float(lit)
                    elif ident in const_map:
                        value = const_map[ident]
                    else:
                        continue
                    if divisor:
                        if divisor not in const_map or const_map[divisor] == 0.0:
                            continue
                        value = value / const_map[divisor]
                    usages[name].append((rel, lineno, value))
    return usages, [p.relative_to(repo).as_posix() for p in files]


def evaluate(repo: pathlib.Path):
    params, ledger = parse_params(repo)
    usages, files = scan_usages(repo, params)
    findings = []
    detail = {}
    for param in params:
        name = param["name"]
        prod = abs(param["production"])
        synthetic = []
        production_scale = []
        for rel, lineno, value in usages[name]:
            ratio = abs(value) / prod if prod else float("inf")
            if param["ratio_floor"] <= ratio <= param["ratio_ceil"]:
                production_scale.append("%s:%d=%g" % (rel, lineno, value))
            else:
                synthetic.append("%s:%d=%g(ratio=%.3g)" % (rel, lineno, value, ratio))
        detail[name] = {"production_scale": production_scale, "synthetic_scale": synthetic}
        if not usages[name]:
            findings.append("prod_scale_no_usage:%s" % name)
            continue
        if not production_scale:
            findings.append("prod_scale_missing_case:%s（无生产尺度用例）" % name)
        for entry in synthetic:
            rel = entry.split(":")[0]
            key = "prod_scale_exception:%s:%s" % (name, rel)
            if key in ledger:
                continue
            findings.append("%s (%s)" % (key, entry))
    return findings, {"params": [p["name"] for p in params],
                      "scan_file_count": len(files), "detail": detail}


# --------------------------------------------------------------------------- selftest ----
def _write_fixture(root: pathlib.Path, body: str, entries=None, params=None):
    import json
    (root / "lib/algo/tests").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / "lib/algo/tests/t.cpp").write_text(body, encoding="utf-8")
    doc = {
        "ledger_schema": gc.LEDGER_SCHEMA,
        "ledger_id": "prod-scale-params",
        "params": params if params is not None else [{"name": "control_ivar", "production": 5.6e-22,
                              "unit": "ADU^-2", "rationale": "fixture"}],
        "entries": list(entries or []),
    }
    (root / LEDGER).write_text(json.dumps(doc), encoding="utf-8")


def _selftest() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok, "o.control_ivar = 5.6e-22;\n")
        cases.append(("green_prod_scale", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, "o.control_ivar = 1.0;\no.control_ivar = 5.6e-22;\n")
        cases.append(("red_synthetic_scale", True, d_bad))
        d_missing = base / "missing_case"
        _write_fixture(d_missing, "o.control_ivar = 1.0;\n")
        cases.append(("red_missing_prod_case", True, d_missing))
        d_led = base / "ledgered"
        _write_fixture(d_led, "o.control_ivar = 1.0;\no.control_ivar = 5.6e-22;\n",
                       entries=[{"id": "prod_scale_exception:control_ivar:lib/algo/tests/t.cpp",
                                 "kind": "synthetic_exception",
                                 "reason": "夹具：另有生产尺度用例", "owner": "fixture",
                                 "exit_condition": "夹具"}])
        cases.append(("green_ledgered", False, d_led))
        rc = gc.selftest_main(cases, lambda repo: evaluate(repo)[0])
        d_noparam = base / "noparam"
        _write_fixture(d_noparam, "x=1;\n", params=[])
        try:
            evaluate(d_noparam)
            failures.append("no_params_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS no_params (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-PROD-SCALE 生产尺度参数门")
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
    print("%s_PASS params=%s scan_files=%d（均有生产尺度用例）"
          % (CHECK_ID, extra["params"], extra["scan_file_count"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())