#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-CONFIG-CONSUMED —— 生产配置键必须被生产代码读取（防死键 no-op）。

防的复发缺口：配置模板/schema 里声明了键，但生产代码从不读取 ⇒ 用户以为设了值
生效，实际 no-op（静默失效）。

判据（fail-closed）：
  C1 每个 config/templates/*.json#config 的叶子键，必须在 lib/** 生产源码里
     以带引号 token 出现（JSON 键读取面），或由 ci/ledgers/dead_config_keys.json
     显式登记（带 consumed_as 别名 / 理由 / 负责人 / 解除条件）；
  C2 模板键集合解析为空 / 模板目录缺失 ⇒ rc=2（不得把「解析不到」当「无死键」）。

锚点：config/templates/*.json（至少一份）、lib/** 生产源码、台账。

用法：
  python3 ci/check_config_consumed.py [--repo ROOT] [--json-out F] [--self-test]
exit 0 = 全部被读或已登记；exit 1 = 有死键；exit 2 = 锚点/台账不可用。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-CONFIG-CONSUMED"
TEMPLATE_GLOB = "config/templates/*.json"
LEDGER = "ci/ledgers/dead_config_keys.json"


def template_keys(repo: pathlib.Path):
    """返回 [(key_path, leaf, template_rel)]，key_path 为 config 内相对路径。"""
    out = []
    templates = sorted(repo.glob(TEMPLATE_GLOB))
    if not templates:
        raise gc.GateError("ANCHOR_MISSING: %s" % TEMPLATE_GLOB)
    for path in templates:
        doc = gc.read_json(path, path.relative_to(repo).as_posix())
        config = doc.get("config")
        if not isinstance(config, dict):
            raise gc.GateError("ANCHOR_STALE: %s 无 config 对象" % path)
        rel = path.relative_to(repo).as_posix()
        stack = [("", config)]
        while stack:
            prefix, node = stack.pop()
            for key, value in node.items():
                path_key = prefix + key
                if isinstance(value, dict):
                    stack.append((path_key + ".", value))
                else:
                    out.append((path_key, key, rel))
    if not out:
        raise gc.GateError("ANCHOR_STALE: 模板 config 零叶子键")
    return out


def production_blob(repo: pathlib.Path) -> str:
    chunks = []
    for path, _rel in gc.iter_source_files(repo / "lib"):
        chunks.append(path.read_text(encoding="utf-8", errors="replace"))
    if not chunks:
        raise gc.GateError("ANCHOR_MISSING: lib/** 无生产源码")
    return "\n".join(chunks)


def evaluate(repo: pathlib.Path):
    keys = template_keys(repo)
    blob = production_blob(repo)
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    findings = []
    dead = []
    for path_key, leaf, rel in keys:
        consumed = ('"%s"' % leaf) in blob
        if consumed:
            continue
        dead.append("%s (%s)" % (path_key, rel))
        key = "dead_config_key:%s" % path_key
        if key in ledger:
            continue
        findings.append("%s (%s) — 生产源码零命中" % (key, rel))
    return findings, {"template_key_count": len(keys), "dead_keys": dead,
                      "templates": sorted({rel for _p, _l, rel in keys})}


# --------------------------------------------------------------------------- selftest ----
_FIXTURE_TEMPLATE = {
    "phase_name": "mosaic",
    "config": {
        "output_dir": "path",
        "precision": "fp64",
        "algorithm_weight_mode": "point_information",
        "wcs": {"projection": "tan", "center_deg": [0, 0], "s_out_deg": 0.001},
    },
    "inputs": [{"product": "x"}],
}
_FIXTURE_SRC_OK = 'cfg.value("output_dir", "x"); cfg.value("precision", "fp64");\n' \
                  'cfg.value("algorithm_weight_mode", "p"); cfg.value("projection","tan");\n' \
                  'cfg.value("center_deg", 0); cfg.value("s_out_deg", 0);\n'
_FIXTURE_SRC_BAD = 'cfg.value("output_dir", "x"); cfg.value("precision", "fp64");\n' \
                   'cfg.value("algorithm_weight_mode", "p"); cfg.value("projection","tan");\n'


def _write_fixture(root: pathlib.Path, src: str, ledger=None):
    import json
    (root / "config/templates").mkdir(parents=True, exist_ok=True)
    (root / "lib/prod").mkdir(parents=True, exist_ok=True)
    (root / "ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / "config/templates/mosaic.phase_config.json").write_text(
        json.dumps(_FIXTURE_TEMPLATE), encoding="utf-8")
    (root / "lib/prod/consumer.cpp").write_text(src, encoding="utf-8")
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
        _write_fixture(d_ok, _FIXTURE_SRC_OK)
        cases.append(("green_all_consumed", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, _FIXTURE_SRC_BAD)
        cases.append(("red_dead_keys", True, d_bad))
        ledger = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": [
            {"id": "dead_config_key:wcs.center_deg", "kind": "dead_key",
             "reason": "夹具：别名 center", "owner": "fixture", "exit_condition": "夹具",
             "consumed_as": "center"},
            {"id": "dead_config_key:wcs.s_out_deg", "kind": "dead_key",
             "reason": "夹具：别名 s_out", "owner": "fixture", "exit_condition": "夹具",
             "consumed_as": "s_out"},
        ]}
        d_led = base / "ledgered"
        _write_fixture(d_led, _FIXTURE_SRC_BAD, ledger)
        cases.append(("green_ledgered", False, d_led))
        rc = gc.selftest_main(cases, lambda repo: evaluate(repo)[0])
        # fail-closed：模板缺失
        d_missing = base / "missing"
        _write_fixture(d_missing, _FIXTURE_SRC_OK)
        (d_missing / "config/templates/mosaic.phase_config.json").unlink()
        try:
            evaluate(d_missing)
            failures.append("missing_template_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS missing_template (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-CONFIG-CONSUMED 生产配置键消费门")
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
    print("%s_PASS template_keys=%d dead_keys=%d（均已登记）"
          % (CHECK_ID, extra["template_key_count"], len(extra["dead_keys"])))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
