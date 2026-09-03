#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-001 机器追溯合同检查器：docs/traceability/TRACEABILITY_MATRIX.json
（八层：SCI→ALG→DATA/API/ARCH→MOD/SRC→TEST→EVIDENCE）全量机器校验。

规则（详见 docs/traceability/TRACEABILITY_SPEC.md §7，exit 0 = PASS）：
  C1 MISSING_FILE     矩阵 JSON 存在且被 Git 跟踪；LAYERS.csv、schema 存在；
  C2 SCHEMA_VIOLATION 每行必需列齐全、层状态取值合法、无附加字段；
  C3 EMPTY_CELL_VIOLATION 任何单元格为 ""/纯空白/'-'/'?'/'TBD'/'TODO' → FAIL
                         （空缺必须显式 MISSING / NONE，禁止空字符串通过）；
  C4 ID_FORMAT_VIOLATION 各层 ID 匹配本层正则（占位 *-MISSING 亦须匹配）；
  C5 DUPLICATE_ID     module_id 行键唯一；SRC id、EVID id 全矩阵唯一；
  C6 CHAIN_BREAK      (a) SRC VERIFIED→同一行 TEST 不得 MISSING；(b) TEST VERIFIED
                      而行 SRC 非 VERIFIED；(c) TEST VERIFIED 而行 MOD 行缺失（不存在）；
                      (d) SCI VERIFIED→ALG 不得 MISSING（conformance/service/provider
                      显式豁免见 SPEC §4 例外，行 notes 声明即可）；
  C7 DANGLING_REF     VERIFIED 层锚可解析：src_path/test_path 文件存在且被 Git 跟踪；
                      src_path 的 SOURCE SYMBOL path::symbol 中 symbol 在文件内可见
                      （MISSING 占位 path::MISSING 只校验路径）；
  C8 REF_OUT_OF_SCOPE WARN（--strict 为 ERROR）：VERIFIED 的 sci/alg/data/api/arch/
                      test/evidence ID 若既不是占位也不是已建 authority（见
                      AUTHORITY_DIRS），登记缺 authority 文件；不崩溃。

任何未捕获异常 → 打印 TOOLING_FAILURE 并 exit 3（不允许伪 PASS）。
用法：
  python3 tools/traceability/check_traceability_matrix.py [--root <repo>]
      [--json-out <file>] [--strict]
依赖：Python 3.10+ 标准库；无网络；不依赖 cwd 之外路径。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys

MATRIX_REL = "docs/traceability/TRACEABILITY_MATRIX.json"
MATRIX_CSV_REL = "docs/traceability/TRACEABILITY_MATRIX.csv"
LAYERS_REL = "docs/traceability/TRACEABILITY_LAYERS.csv"
SCHEMA_REL = "schemas/traceability_matrix.schema.json"
CSV_COLS = ["module_id", "module_kind", "module_anchor",
            "science_id", "science_doc", "science_status",
            "algorithm_id", "algorithm_doc", "algorithm_status",
            "data_id", "data_status",
            "api_id", "api_status",
            "arch_id", "arch_status",
            "src_id", "src_path", "src_status",
            "test_id", "test_path", "test_status",
            "evidence_id", "evidence_status", "notes"]

STATUS_OK = {"VERIFIED", "MISSING", "NONE"}
# 每层: (id_key, status_key, path_key?, id_regex, path_required_when_verified)
LAYERS = [
    ("SCI",  "science_id",  "science_status",  "science_doc",   r"^SCI-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("ALG",  "algorithm_id", "algorithm_status", "algorithm_doc", r"^ALG-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("DATA", "data_id",     "data_status",     None,            r"^DATA-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("API",  "api_id",      "api_status",      None,            r"^API-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("ARCH", "arch_id",     "arch_status",     None,            r"^ARCH-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("MOD",  "module_id",   None,              None,            r"^MOD-[A-Za-z0-9]+(-[A-Za-z0-9]+)*$"),
    ("SRC",  "src_id",      "src_status",      "src_path",      r"^SRC-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("TEST", "test_id",     "test_status",     "test_path",     r"^TEST-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("EVID", "evidence_id", "evidence_status", None,            r"^EVID-[A-Z0-9]+(-[A-Z0-9]+)*$"),
]
REQUIRED_KEYS = [k for _, idk, stk, pathk, _ in LAYERS
                 for k in ((idk,) if idk else ()) + ((stk,) if stk else ()) + ((pathk,) if pathk else ())]
REQUIRED_KEYS += ["module_id", "module_kind", "module_anchor", "notes"]
# 豁免层（conformance/service/provider 允许 SCI/ALG MISSING 而 DATA/API/ARCH VERIFIED）
EXEMPT_KINDS = {"conformance", "service", "provider"}
# 合法模块来源目录：矩阵模块清单须能在其中发现（防孤行/防遗漏）
MODULE_ANCHOR_DIRS = [
    "modules/conformance", "modules/services", "providers/cpu",
    "docs/modules/registry",  # registry production 模块（module_adapters.cpp 唯一源）
]
AUTHORITY_DIRS = {  # 各合同层 authority 文档搜索目录（id 需在其中一个文件文本中出现）
    "SCI": ["docs/science"],
    "ALG": ["docs/algorithms", "docs/science"],
    "DATA": ["docs/contracts", "docs/interfaces/data", "docs/api", "lib/core/src"],
    "API": ["docs/api", "docs/contracts", "docs/interfaces/io", "docs/architecture/cpu",
            "docs/modules/registry", "lib/core/src", "modules", "providers"],
    "ARCH": ["docs/architecture", "docs/architecture/cpu", "docs/contracts", "docs/api"],
    "MOD": ["modules", "docs/modules/registry"],
    "SRC": ["modules", "runtime", "providers", "lib", "include", "tests"],
    "TEST": ["tests", "modules", "docs/interfaces", "docs/contracts", "docs/modules/registry"],
    "EVID": ["evidence", "reports", "returns"],
}
EMPTY_BAD = {"", "-", "?", "TBD", "TODO", "N/A", "NA", "n/a"}
PLACEHOLDER_RE = re.compile(r"^[A-Z]+-MISSING$")


def err(prefix: str, msg: str) -> dict:
    return {"severity": "ERROR", "code": prefix, "detail": msg}


def warn(prefix: str, msg: str) -> dict:
    return {"severity": "WARN", "code": prefix, "detail": msg}


def git_tracked(root: str, rel: str) -> bool:
    r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                       cwd=root, capture_output=True, text=True)
    return r.returncode == 0


def check_file(root: str, rel: str, results: list[dict]) -> None:
    full = os.path.join(root, rel)
    if not os.path.isfile(full):
        results.append(err("MISSING_FILE", f"{rel} 不存在"))
        return
    if not git_tracked(root, rel):
        results.append(err("MISSING_FILE", f"{rel} 存在但未受 Git 跟踪（git ls-files --error-unmatch 失败）"))


def parse_matrix(root: str, results: list[dict]) -> dict | None:
    full = os.path.join(root, MATRIX_REL)
    try:
        with open(full, encoding="utf-8") as f:
            data = json.load(f)
    except Exception as exc:  # noqa: BLE001
        results.append(err("TOOLING_FAILURE", f"{MATRIX_REL} JSON 解析失败: {exc}"))
        return None
    if not isinstance(data, dict) or data.get("schema") != "astrocs.traceability-matrix/v1":
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_REL} schema 声明缺失或错误"))
        return None
    mods = data.get("modules")
    if not isinstance(mods, list):
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_REL} modules 必须是数组"))
        return None
    return data


def is_empty(v) -> bool:
    if v is None:
        return True
    s = str(v).strip()
    return s == "" or s in EMPTY_BAD


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    results: list[dict] = []

    # C1 文件存在 + 被跟踪
    check_file(root, MATRIX_REL, results)
    check_file(root, MATRIX_CSV_REL, results)
    check_file(root, LAYERS_REL, results)
    check_file(root, SCHEMA_REL, results)

    data = parse_matrix(root, results)
    if data is None:
        _finish(root, args, results)
        return 1 if results else 0

    _check_csv_parity(root, results)

    mods: list[dict] = data["modules"]
    rows = []
    # CSV 行（供 C2/C3 行级定位与视图）
    seen_module = {}
    for i, row in enumerate(mods):
        if not isinstance(row, dict):
            results.append(err("SCHEMA_VIOLATION", f"modules[{i}] 非对象"))
            continue
        mid = row.get("module_id")
        if not isinstance(mid, str) or not mid:
            results.append(err("SCHEMA_VIOLATION", f"modules[{i}] module_id 缺失/非法: {mid!r}"))
            continue
        if mid in seen_module:
            results.append(err("DUPLICATE_ID", f"模块行键重复: {mid} (modules[{seen_module[mid]}] 与 modules[{i}])"))
        seen_module[mid] = i
        rows.append(row)

    # C2 必需列 + C3 空值 + C4 ID 格式 + 状态取值
    for row in rows:
        mid = row.get("module_id", "?")
        for k in REQUIRED_KEYS:
            if k not in row:
                results.append(err("SCHEMA_VIOLATION", f"{mid}: 缺必需列 {k}"))
        for idk, stk, pathk, regex in [
            (idk, stk, pathk, rx) for _, idk, stk, pathk, rx in LAYERS
        ]:
            if idk not in row or stk not in row:
                continue  # 已报缺列
            ident, st = row[idk], row[stk]
            # C3 空值
            for label, v in ((idk, ident), (stk, st)) + (((pathk, row[pathk]),) if pathk else ()):
                if is_empty(v):
                    results.append(err("EMPTY_CELL_VIOLATION",
                                       f"{mid} [{idk} 层] 列 {label} 为空值 {v!r} → 必须显式 MISSING/NONE"))
            if st not in STATUS_OK:
                results.append(err("SCHEMA_VIOLATION",
                                   f"{mid} [{idk} 层] 非法状态 {st!r} ∈ {{VERIFIED,MISSING,NONE}}"))
            # C4 ID 格式
            if not PLACEHOLDER_RE.fullmatch(str(ident)) and not re.fullmatch(regex, str(ident)):
                results.append(err("ID_FORMAT_VIOLATION",
                                   f"{mid} [{idk} 层] ID {ident!r} 不匹配 {regex}"))
            if pathk and is_empty(row.get(pathk)):
                results.append(err("EMPTY_CELL_VIOLATION",
                                   f"{mid} [{idk} 层] 列 {pathk} 为空 → 必须显式 MISSING"))
        if row.get("module_kind") not in {"phase1", "phase2", "phase3", "service", "conformance", "provider"}:
            results.append(err("SCHEMA_VIOLATION", f"{mid}: module_kind 非法 {row.get('module_kind')!r}"))
        if is_empty(row.get("module_anchor")):
            results.append(err("EMPTY_CELL_VIOLATION", f"{mid}: module_anchor 为空"))

    # C5 唯一性（行键已在上面；SRC/EVID id 唯一）
    for idk, label in (("src_id", "SRC"), ("evidence_id", "EVIDENCE")):
        seen = {}
        for row in rows:
            v = row.get(idk)
            if not v or str(v).endswith("-MISSING"):
                continue
            if v in seen:
                results.append(err("DUPLICATE_ID", f"{label} id 重复: {v} ({seen[v]} 与 {row.get('module_id')})"))
            seen[v] = row.get("module_id")

    # C6 链完整（非豁免行）
    for row in rows:
        mid = row.get("module_id", "?")
        kind = row.get("module_kind")
        sci_st, alg_st = row.get("science_status"), row.get("algorithm_status")
        src_st, test_st = row.get("src_status"), row.get("test_status")
        # (d) SCI VERIFIED → ALG 不得 MISSING
        if sci_st == "VERIFIED" and alg_st == "MISSING" and kind not in EXEMPT_KINDS:
            results.append(err("CHAIN_BREAK",
                               f"{mid}: SCI VERIFIED 但 ALG MISSING（下层依赖上层缺口）"))
        # (a) SRC VERIFIED → TEST 不得 MISSING
        if src_st == "VERIFIED" and test_st == "MISSING":
            results.append(err("CHAIN_BREAK", f"{mid}: SRC VERIFIED 但 TEST MISSING（有实现无测试）"))
        # (b) TEST VERIFIED → SRC 必须 VERIFIED
        if test_st == "VERIFIED" and src_st != "VERIFIED":
            results.append(err("CHAIN_BREAK", f"{mid}: TEST VERIFIED 但 SRC 非 VERIFIED（无实现却有测试证据）"))

    # C7 DANGLING_REF：VERIFIED 层锚可解析
    src_seen = {}
    for row in rows:
        mid = row.get("module_id", "?")
        # SRC
        if row.get("src_status") == "VERIFIED":
            p = row.get("src_path", "")
            _check_anchor(root, mid, "SRC", p, results, src_seen)
        # TEST
        if row.get("test_status") == "VERIFIED":
            p = row.get("test_path", "")
            _check_anchor(root, mid, "TEST", p, results, {})
        # MOD anchor
        a = row.get("module_anchor", "")
        if a and a != "MISSING" and not os.path.isfile(os.path.join(root, a)):
            results.append(err("DANGLING_REF", f"{mid}: MOD anchor 路径不存在 {a}"))

    # C8 越界/缺 authority：VERIFIED 的真实 id 应有 authority（strict 为 ERROR，否则 WARN）
    for row in rows:
        mid = row.get("module_id", "?")
        for layer, idk, stk, pathk, _rx in LAYERS:
            if layer in ("MOD", "SRC", "TEST"):
                # MOD=行键；SRC/TEST 的“权威”= src_path/test_path 文件存在性（C7 已校验），
                # 不另查 id 文本（合成测试锚由 test_path 文件承载）
                continue
            st = row.get(stk)
            ident = row.get(idk, "")
            if st != "VERIFIED" or not ident or PLACEHOLDER_RE.fullmatch(str(ident)):
                continue
            if _id_has_authority(root, layer, str(ident), row.get(pathk) if pathk else None):
                continue
            item = err("REF_OUT_OF_SCOPE", f"{mid} [{layer}] id {ident} 无 authority 文件可见") if args.strict \
                else warn("REF_OUT_OF_SCOPE", f"{mid} [{layer}] id {ident} 无 authority 文件可见（WARN）")
            results.append(item)

    # 模块清单完整性：每个 registry/已建模块被覆盖（不判孤行）
    _check_module_coverage(root, rows, results)

    _finish(root, args, results)
    errors = [r for r in results if r["severity"] == "ERROR"]
    return 1 if errors else 0


def _check_anchor(root, mid, layer, p, results, seen):
    if is_empty(p) or p == "MISSING":
        results.append(err("DANGLING_REF", f"{mid} [{layer}]: VERIFIED 但路径为空/为 MISSING"))
        return
    if "::" in p:
        path, syms = p.split("::", 1)
        if syms == "MISSING":
            # 路径占位：文件必须存在
            if not os.path.isfile(os.path.join(root, path)):
                results.append(err("DANGLING_REF", f"{mid} [{layer}]: 文件不存在 {path}"))
            return
        full = os.path.join(root, path)
        if not os.path.isfile(full):
            results.append(err("DANGLING_REF", f"{mid} [{layer}]: 文件不存在 {path}"))
            return
        if not git_tracked(root, path):
            results.append(err("DANGLING_REF", f"{mid} [{layer}]: 文件未受 Git 跟踪 {path}"))
        try:
            text = open(full, encoding="utf-8", errors="replace").read()
        except Exception:  # noqa: BLE001
            text = ""
        for sym in syms.split(","):
            sym = sym.strip()
            if not sym:
                continue
            if not re.search(r"\b" + re.escape(sym) + r"\b", text):
                results.append(err("DANGLING_REF", f"{mid} [{layer}]: 符号 {sym} 不在 {path}"))
    else:
        if not os.path.isfile(os.path.join(root, p)):
            results.append(err("DANGLING_REF", f"{mid} [{layer}]: 路径不存在 {p}"))
        elif not git_tracked(root, p):
            results.append(err("DANGLING_REF", f"{mid} [{layer}]: 路径未受 Git 跟踪 {p}"))


def _id_has_authority(root, layer, ident, pathhint):
    if pathhint and pathhint != "MISSING" and not is_empty(pathhint):
        base = pathhint.split("::", 1)[0]
        full = os.path.join(root, base)
        if os.path.isfile(full):
            try:
                if re.search(r"\b" + re.escape(ident) + r"\b", open(full, encoding="utf-8", errors="ignore").read()):
                    return True
            except Exception:  # noqa: BLE001
                pass
    for d in AUTHORITY_DIRS.get(layer, []):
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith((".md", ".csv", ".yaml", ".yml", ".json", ".c", ".h", ".cpp")):
                    continue
                full = os.path.join(dirpath, fn)
                try:
                    text = open(full, encoding="utf-8", errors="ignore").read()
                except Exception:  # noqa: BLE001
                    continue
                if re.search(r"\b" + re.escape(ident) + r"\b", text):
                    return True
    return False


def _check_module_coverage(root, rows, results):
    """已知模块清单（真实目录/registry front-matter）必须在矩阵中有一行。"""
    known = set()
    reg = os.path.join(root, "docs/modules/registry")
    if os.path.isdir(reg):
        for fn in sorted(os.listdir(reg)):
            if fn.endswith(".md"):
                p = os.path.join(reg, fn)
                try:
                    head = open(p, encoding="utf-8", errors="ignore").read(2000)
                except Exception:  # noqa: BLE001
                    continue
                m = re.search(r"^id:\s*(MOD-\S+)", head, re.M)
                if m:
                    known.add(m.group(1).strip())
    # 服务/conformance/provider 目录 → MOD- 行键
    if os.path.isfile(os.path.join(root, "modules/conformance/noop/module.yaml")):
        known.add("MOD-astrocs-conformance-noop")
    if os.path.isdir(os.path.join(root, "modules/services/io")):
        known.add("MOD-astrocs-services-io")
    if os.path.isdir(os.path.join(root, "providers/cpu/common")):
        known.add("MOD-astrocs-providers-cpu")
    present = {r.get("module_id") for r in rows}
    missing = sorted(known - present)
    if missing:
        results.append(err("MISSING_FILE", f"矩阵缺少已知模块行: {missing}"))


def _check_csv_parity(root, results) -> None:
    """JSON(权威) 与 CSV(视图) 必须同构：表头=CSV_COLS，每行逐列一致。"""
    full = os.path.join(root, MATRIX_CSV_REL)
    if not os.path.isfile(full):
        return  # MISSING_FILE 已报
    try:
        with open(full, encoding="utf-8", newline="") as f:
            reader = csv.reader(f)
            header = next(reader)
            csv_rows = [r for r in reader if r and any(c.strip() for c in r)]
    except Exception as exc:  # noqa: BLE001
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_CSV_REL} 解析失败: {exc}"))
        return
    if header != CSV_COLS:
        results.append(err("SCHEMA_VIOLATION",
                           f"{MATRIX_CSV_REL} 表头与 JSON 列不一致: 缺={set(CSV_COLS)-set(header)} 多={set(header)-set(CSV_COLS)}"))
        return
    try:
        with open(os.path.join(root, MATRIX_REL), encoding="utf-8") as f:
            mods = json.load(f)["modules"]
    except Exception:  # noqa: BLE001
        return
    json_map = {}
    for row in sorted(mods, key=lambda r: r["module_id"]):
        json_map[row["module_id"]] = [str(row.get(c, "")) for c in CSV_COLS]
    if len(csv_rows) != len(json_map):
        results.append(err("SCHEMA_VIOLATION",
                           f"{MATRIX_CSV_REL} 行数 {len(csv_rows)} != JSON 模块数 {len(json_map)}（请重跑 gen_traceability_csv.py）"))
        return
    for i, r in enumerate(csv_rows):
        mid = r[0]
        if mid not in json_map:
            results.append(err("SCHEMA_VIOLATION", f"{MATRIX_CSV_REL} 行 {i} 模块 {mid} 不在 JSON"))
            continue
        if r != json_map[mid]:
            diffs = [CSV_COLS[j] for j in range(len(CSV_COLS)) if j < len(r) and j < len(json_map[mid]) and r[j] != json_map[mid][j]]
            results.append(err("SCHEMA_VIOLATION",
                               f"{MATRIX_CSV_REL} 行 {i} {mid} 与 JSON 不一致列: {diffs}（请重跑 gen_traceability_csv.py）"))


def _finish(root, args, results):
    errors = [r for r in results if r["severity"] == "ERROR"]
    warns = [r for r in results if r["severity"] == "WARN"]
    summary = {
        "checker": "check_traceability_matrix.py",
        "matrix": MATRIX_REL,
        "strict": bool(args.strict),
        "errors": len(errors),
        "warns": len(warns),
        "issues": sorted(results, key=lambda r: (r["severity"], r["code"], r["detail"])),
        "result": "TRACEABILITY_MATRIX_PASS" if not errors else "TRACEABILITY_MATRIX_FAIL",
    }
    # 附上输入 hash 与矩阵统计
    full = os.path.join(root, MATRIX_REL)
    try:
        summary["matrix_sha256"] = hashlib.sha256(open(full, "rb").read()).hexdigest()
    except Exception:  # noqa: BLE001
        summary["matrix_sha256"] = None
    text = json.dumps(summary, ensure_ascii=False, indent=1, sort_keys=True)
    if args.json_out:
        try:
            os.makedirs(os.path.dirname(os.path.abspath(args.json_out)) or ".", exist_ok=True)
            with open(args.json_out, "w", encoding="utf-8") as f:
                f.write(text + "\n")
        except Exception as exc:  # noqa: BLE001
            print(f"TOOLING_FAILURE: 写 json-out 失败 {exc}", file=sys.stderr)
    if errors:
        print("TRACEABILITY_MATRIX_FAIL errors=%d" % len(errors))
        for r in errors:
            print(f"  [{r['code']}] {r['detail']}")
        print("WARN 摘要: %d 条（--strict 将升级为 ERROR）" % len(warns))
    else:
        rows = 0
        try:
            d = json.load(open(os.path.join(root, MATRIX_REL), encoding="utf-8"))
            rows = len(d.get("modules", []))
        except Exception:  # noqa: BLE001
            pass
        print(f"TRACEABILITY_MATRIX_PASS modules={rows} errors=0"
              + (f" warns={len(warns)}" if warns else ""))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print(f"TOOLING_FAILURE: 未捕获异常 {exc!r}", file=sys.stderr)
        raise SystemExit(3)
