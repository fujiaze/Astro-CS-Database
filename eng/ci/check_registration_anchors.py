#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_registration_anchors.py —— 「登记项必须仍有对象」门（C 类孤儿登记面）。

规范依据
  - ENGINEERING_SPEC.md §8 规则 4：「悬空即缺陷 —— 索引指向的文件/章节必须存在，
    文档引用的代码路径必须真实，CI 检查悬空引用」；
  - ENGINEERING_SPEC.md §10「锚存活：检查器硬编码引用的文件/目录必须存在，失效时
    报 ANCHOR_STALE」；
  - docs/ci/01_CHECKS.md §1「锚存活」「fail-closed（scanned == 0 ⇒ rc != 0）」；
  - AGENTS.md §7：run/ 是临时产物面（gitignore、不入库）⇒ 登记册把对象登记到 run/
    等于把登记项登记到一个**必然消失**的对象上。

判据（exit 0 = PASS / 1 = FAIL / 2 = fail-closed / 3 = TOOLING_FAILURE）
  R1 contract_live_ref_stale   eng/contracts/** 的登记 JSON 里，**活引用键**的值若是
                               仓库源路径形态（lib/ eng/ docs/ testdata/ gaia/），必须存在。
  R2 registry_run_local_ref    eng/ci/** 的登记 JSON 里，任何指向 run/ dist/ build/ 的
                               引用都是「登记到临时面」⇒ 判红（AGENTS.md §7）。
  棘轮（样板 = eng/ci/check_mutation_gates.py 的 gone_artifacts）：已确证的存量违规必须
  逐 (rule, file, class) 登记于 eng/ci/ledgers/registration_anchor_ledger.json，
  条目**只减不增**；新增违规判 REGISTRATION_ANCHOR_STALE / _GROWN，
  台账条目不再匹配任何当前违规判 LEDGER_STALE（产物回来了/修好了 ⇒ 必须删条目）。

不覆盖（如实声明，防口径误读）
  * 键名白名单口径：只扫 LIVE_KEYS 里的键名（path/doc_ref/implementations/entries/
    evidence/anchor/...）。新增键名需同步登记，否则本门**不覆盖**该字段。
  * 历史/迁移面豁免：键路径含 migration/history/retired/from/to/proposals 等标记的
    引用按「历史坐标」处理（迁移的 from→to、退役对象的 canonical_schema 允许已消失）。
  * 样例面豁免：examples/ negative/ fixtures/ proposals/ templates/ 子目录。
  * 另有主面豁免：已登记于 eng/tools/doccheck/dangling_ledger.json（DOC-INDEX 跨域台账）
    的 (file, token) 视为**另有主**，本门不重复计数（单一归属，避免两个棘轮互相牵制）。
  * 专属门面排除：eng/ci/checks.json（CHK-REGISTRY-VALIDATE + CHK-PATH-DOMAIN-ANCHORS）、
    eng/ci/mutation_gates.json（CHK-MUTATION-GATES 自带棘轮）、
    eng/ci/ctest_baseline.json（冻结时点快照，维护面 --write-baseline）、
    eng/ci/id_migration_map.json / eng/ci/impact_map.json（迁移/影响面，各有专属门）。

用法
  python3 eng/ci/check_registration_anchors.py [--root .] [--json-out run/ci/registration-anchors/report.json]
  python3 eng/ci/check_registration_anchors.py --self-test
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gate_common  # noqa: E402  （eng/ci 共用设施：台账 schema / fail-closed 口径）

CHECK_ID = "CHK-REGISTRATION-ANCHORS"
LEDGER_REL = "eng/ci/ledgers/registration_anchor_ledger.json"
DOC_INDEX_LEDGER_REL = "eng/tools/doccheck/dangling_ledger.json"

PATH_FILE_RE = re.compile(r"^(?:[A-Za-z0-9_.\-]+/)+[A-Za-z0-9_.\-]+$")
PATH_DIR_RE = re.compile(r"^(?:[A-Za-z0-9_.\-]+/)+$")
SRC_TOP = ("lib/", "eng/", "docs/", "testdata/", "gaia/", "实验/", "工程控制/")
RUNTIME_TOP = ("run/", "dist/", "build/")
TEMPLATE_MARK = ("path/to", "%s", "<", ">", "$", "__no_such", "...")

# 活引用键白名单（值 = 仓库内必须存在的对象路径）
LIVE_KEYS = {
    "path", "doc_ref", "schema_ref", "canonical_schema", "schema", "source", "target",
    "file", "ref", "contract", "authority", "anchors", "anchor", "implementations",
    "implementation", "spec", "doc", "schema_path", "registry", "matrix", "ledger",
    "evidence",
}
# 历史/迁移/退役坐标：允许指向已消失对象
HISTORY_KEY_MARK = ("migration", "migrated", "history", "historical", "former",
                    "previous", "from", "old", "retired", "superseded", "proposal",
                    "at_base_commit", "gone", "deleted")
SAMPLE_PATH_MARK = ("/examples/", "/negative/", "/fixtures/", "/proposals/", "/templates/")

# 已有专属门的登记册（本门不接管）
EXCLUDED_CI_FILES = {
    "eng/ci/checks.json",
    "eng/ci/mutation_gates.json",
    "eng/ci/ctest_baseline.json",
    "eng/ci/id_migration_map.json",
    "eng/ci/impact_map.json",
}


def _is_historical(keypath: str) -> bool:
    low = keypath.lower()
    return any(mark in low for mark in HISTORY_KEY_MARK)


def _iter_strings(node, keypath: str, key: str, out: list) -> None:
    if isinstance(node, dict):
        for k, val in node.items():
            _iter_strings(val, "%s.%s" % (keypath, k), k, out)
    elif isinstance(node, list):
        for idx, val in enumerate(node):
            _iter_strings(val, "%s[%d]" % (keypath, idx), key, out)
    elif isinstance(node, str):
        out.append((keypath, key, node.strip()))


def _scan_class(value: str, keypath: str):
    """返回 'source' / 'runtime' / None。"""
    if not (PATH_FILE_RE.match(value) or PATH_DIR_RE.match(value)):
        return None
    if any(mark in value for mark in TEMPLATE_MARK):
        return None
    if _is_historical(keypath):
        return None
    if value.startswith(RUNTIME_TOP):
        return "runtime"
    if value.startswith(SRC_TOP):
        return "source"
    return None


def _json_files(root: Path, subdir: str, excluded=frozenset()):
    base = root / subdir
    if not base.is_dir():
        raise gate_common.GateError("ANCHOR_MISSING: 扫描面目录 %s" % subdir)
    out = []
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = [d for d in dirnames if d != "__pycache__"]
        for name in filenames:
            if not name.endswith(".json"):
                continue
            rel = (Path(dirpath) / name).relative_to(root).as_posix()
            if rel in excluded:
                continue
            out.append(rel)
    return sorted(out)


def _doc_index_exempt(root: Path) -> set:
    """DOC-INDEX 跨域台账里已登记的 (file, token) ⇒ 另有主，本门不重复计数。

    台账缺失/不可解析 ⇒ **不放宽**：不施加任何豁免（本门更严），并在摘要里留痕。
    """
    path = root / DOC_INDEX_LEDGER_REL
    if not path.is_file():
        return set()
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return set()
    out = set()
    for entry in doc.get("entries", []) or []:
        if isinstance(entry, dict):
            out.add((str(entry.get("file", "")), str(entry.get("token", ""))))
    return out


def scan(root: Path) -> tuple:
    """返回 (findings, stats)。findings = [(rule, file, cls, keypath, value)]"""
    findings = []
    stats = {"contract_files": 0, "ci_files": 0, "refs_checked": 0, "exempt_doc_index": 0}

    exempt = _doc_index_exempt(root)

    contract_files = _json_files(root, "eng/contracts")
    stats["contract_files"] = len(contract_files)
    for rel in contract_files:
        posix = "/" + rel + "/"
        if any(mark in posix for mark in SAMPLE_PATH_MARK):
            continue
        doc = gate_common.read_json(root / rel, rel)
        strings = []
        _iter_strings(doc, "$", None, strings)
        for keypath, key, value in strings:
            if key not in LIVE_KEYS:
                continue
            cls = _scan_class(value, keypath)
            if cls != "source":
                continue
            stats["refs_checked"] += 1
            if (root / value).exists():
                continue
            if (rel, value) in exempt:
                stats["exempt_doc_index"] += 1
                continue
            findings.append(("R1", rel, "source_ref", keypath, value))

    ci_files = _json_files(root, "eng/ci", EXCLUDED_CI_FILES)
    stats["ci_files"] = len(ci_files)
    for rel in ci_files:
        posix = "/" + rel + "/"
        if any(mark in posix for mark in SAMPLE_PATH_MARK):
            continue
        doc = gate_common.read_json(root / rel, rel)
        strings = []
        _iter_strings(doc, "$", None, strings)
        for keypath, key, value in strings:
            if key not in LIVE_KEYS:
                continue
            cls = _scan_class(value, keypath)
            if cls != "runtime":
                continue
            stats["refs_checked"] += 1
            findings.append(("R2", rel, "run_local_ref", keypath, value))

    if stats["contract_files"] + stats["ci_files"] == 0:
        raise gate_common.GateError("SCAN_FACE_EMPTY: 登记 JSON 扫描面为空（fail-closed）")
    return findings, stats


def _group(findings):
    groups = {}
    for rule, rel, cls, keypath, value in findings:
        gid = "%s|%s|%s" % (rule, rel, cls)
        groups.setdefault(gid, []).append((keypath, value))
    return groups


def evaluate(root: Path, ledger_path: Path) -> tuple:
    findings, stats = scan(root)
    ledger = gate_common.load_ledger(ledger_path, LEDGER_REL)
    groups = _group(findings)

    problems = []
    for gid, items in sorted(groups.items()):
        current = [value for _kp, value in items]
        entry = ledger.get(gid)
        if entry is None:
            problems.append("REGISTRATION_ANCHOR_STALE %s: %d 条违规整组未登记（登记项已无对象）例: %s = %s"
                            % (gid, len(items), items[0][0], items[0][1]))
            continue
        values = entry.get("values")
        if not isinstance(values, list) or not all(isinstance(v, str) for v in values):
            # 台账不是后门（gate_common 口径）：结构非法 ⇒ runner error，不给结论。
            raise gate_common.GateError(
                "LEDGER_SHAPE_INVALID %s: 台账条目必须有字符串数组 values" % gid)
        for value in sorted(set(current) - set(values)):
            problems.append("REGISTRATION_ANCHOR_STALE %s: 未登记的具体对象 %s（棘轮只减不增，"
                            "新悬空对象必须显式登记并写明 owner/解除条件）" % (gid, value))
    for gid, entry in sorted(ledger.items()):
        values = entry.get("values") if isinstance(entry.get("values"), list) else []
        if gid not in groups:
            problems.append("LEDGER_STALE %s: 台账条目已不再匹配任何违规（修好了 ⇒ 必须删条目，只减不增）"
                            % gid)
            continue
        current = {value for _kp, value in groups[gid]}
        for value in sorted(set(values) - current):
            problems.append("LEDGER_STALE %s: 台账登记的对象 %s 已不再是违规（修好了 ⇒ 必须删该登记）"
                            % (gid, value))

    stats["ledger_entries"] = len(ledger)
    stats["violation_groups"] = len(groups)
    stats["violations"] = len(findings)
    return problems, stats, findings


def run_gate(root: Path, ledger_path: Path) -> int:
    problems, stats, findings = evaluate(root, ledger_path)
    if problems:
        gate_common.print_findings(CHECK_ID, problems)
        return 1
    print("%s_PASS refs_checked=%d contract_files=%d ci_files=%d violations=0 ledger_entries=%d"
          % (CHECK_ID, stats["refs_checked"], stats["contract_files"], stats["ci_files"],
             stats["ledger_entries"]))
    return 0


# ─────────────────────────── self-test ───────────────────────────

LEDGER_HEAD = {
    "ledger_schema": gate_common.LEDGER_SCHEMA,
    "ledger_id": "registration-anchors",
    "purpose": "self-test fixture",
}


def _write_fixture(base: Path, *, contract_ref="lib/ok/impl.cpp", ci_ref=None,
                   ledger_entries=(), doc_index_entries=(), make_files=("lib/ok/impl.cpp",),
                   write_ledger=True):
    (base / "eng" / "contracts" / "data").mkdir(parents=True, exist_ok=True)
    (base / "eng" / "ci" / "ledgers").mkdir(parents=True, exist_ok=True)
    for rel in make_files:
        p = base / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("x\n", encoding="utf-8")
    contract = {"schema_version": 1, "implementations": [{"path": contract_ref}]}
    (base / "eng" / "contracts" / "data" / "c.json").write_text(
        json.dumps(contract), encoding="utf-8")
    ci = {"schema_version": 1}
    if ci_ref:
        ci["evidence"] = [ci_ref]
    (base / "eng" / "ci" / "r.json").write_text(json.dumps(ci), encoding="utf-8")
    if doc_index_entries:
        p = base / DOC_INDEX_LEDGER_REL
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps({"entries": [{"file": f, "token": t}
                                             for f, t in doc_index_entries]}), encoding="utf-8")
    if write_ledger:
        doc = dict(LEDGER_HEAD)
        doc["entries"] = list(ledger_entries)
        (base / LEDGER_REL).write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")


def _entry(gid, values, **over):
    e = {"id": gid, "kind": "registration_anchor", "reason": "self-test",
         "owner": "self-test", "exit_condition": "self-test",
         "values": list(values)}
    e.update(over)
    return e


def self_test() -> int:
    cases = []

    def case(name, expect, **kw):
        cases.append((name, expect, kw))

    R1C = "R1|eng/contracts/data/c.json|source_ref"
    R2C = "R2|eng/ci/r.json|run_local_ref"
    case("pos_clean", 0)
    case("neg_contract_ref_stale", 1, contract_ref="lib/gone/impl.cpp", ledger_entries=[])
    case("pos_contract_ref_registered", 0, contract_ref="lib/gone/impl.cpp",
         ledger_entries=[_entry(R1C, ["lib/gone/impl.cpp"])])
    case("neg_contract_ref_swapped", 1, contract_ref="lib/other_gone/impl.cpp",
         ledger_entries=[_entry(R1C, ["lib/gone/impl.cpp"])])
    case("neg_run_ref_unregistered", 1, ci_ref="run/gone/log.txt")
    case("pos_run_ref_registered", 0, ci_ref="run/gone/log.txt",
         ledger_entries=[_entry(R2C, ["run/gone/log.txt"])])
    case("neg_ledger_value_unregistered", 1, ci_ref="run/gone/log.txt",
         ledger_entries=[_entry(R2C, [])])
    case("neg_ledger_stale", 1, ledger_entries=[_entry(R2C, ["run/gone/log.txt"])])
    case("neg_ledger_values_not_list", 2, ci_ref="run/gone/log.txt",
         ledger_entries=[{"id": R2C, "kind": "k", "reason": "r", "owner": "o",
                          "exit_condition": "e", "values": "run/gone/log.txt"}])
    case("pos_doc_index_exempt", 0, contract_ref="docs/gone/SPEC.md",
         doc_index_entries=[("eng/contracts/data/c.json", "docs/gone/SPEC.md")])
    case("neg_doc_index_not_exempting_other", 1, contract_ref="docs/gone/SPEC.md",
         doc_index_entries=[("eng/contracts/data/c.json", "docs/other.md")])
    case("neg_ledger_missing_field", 2, ci_ref="run/gone/log.txt",
         ledger_entries=[{"id": R2C, "kind": "k", "reason": "",
                          "owner": "o", "exit_condition": "e",
                          "values": ["run/gone/log.txt"]}])
    case("neg_ledger_missing_file", 2, write_ledger=False)

    failures = []
    for name, expect, kw in cases:
        with tempfile.TemporaryDirectory(prefix="astrocs_reganchor_") as tmp:
            base = Path(tmp)
            _write_fixture(base, **kw)
            try:
                problems, _stats, _f = evaluate(base, base / LEDGER_REL)
                rc = 1 if problems else 0
            except gate_common.GateError:
                rc = 2
            if rc != expect:
                failures.append("%s: expect rc=%d got rc=%d" % (name, expect, rc))
            else:
                print("SELFTEST_PASS %s rc=%d" % (name, rc))

    # 空扫描面（目录被移空）⇒ fail-closed rc=2
    with tempfile.TemporaryDirectory(prefix="astrocs_reganchor_") as tmp:
        base = Path(tmp)
        (base / "eng" / "ci" / "ledgers").mkdir(parents=True, exist_ok=True)
        (base / LEDGER_REL).write_text(json.dumps(dict(LEDGER_HEAD, entries=[])), encoding="utf-8")
        try:
            evaluate(base, base / LEDGER_REL)
            rc = 0
        except gate_common.GateError:
            rc = 2
        if rc != 2:
            failures.append("neg_empty_face: expect rc=2 got rc=%d" % rc)
        else:
            print("SELFTEST_PASS neg_empty_face rc=2")

    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        return 1
    print("SELFTEST_PASS: %d cases + empty-face, all match expectation" % len(cases))
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=CHECK_ID)
    ap.add_argument("--root", default=".")
    ap.add_argument("--ledger", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = Path(args.root).resolve()
    ledger_path = Path(args.ledger) if args.ledger else root / LEDGER_REL
    try:
        rc = run_gate(root, ledger_path)
    except gate_common.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    except Exception as exc:  # noqa: BLE001
        print("%s_TOOLING_FAILURE: %r" % (CHECK_ID, exc), file=sys.stderr)
        return 3
    if args.json_out:
        try:
            _problems, stats, findings = evaluate(root, ledger_path)
            gate_common.write_json(args.json_out, {
                "check_id": CHECK_ID, "stats": stats,
                "findings": [{"rule": r, "file": f, "class": c, "key": k, "value": v}
                             for r, f, c, k, v in findings]})
            print("JSON_OUT %s" % args.json_out)
        except gate_common.GateError:
            pass
    return rc


if __name__ == "__main__":
    sys.exit(main())
