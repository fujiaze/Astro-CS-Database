#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-FIX-PSF 第 6 项（M3b-F-02）机器门：追溯矩阵「可执行测试目标」三规则门。

对象: docs/traceability/TRACEABILITY_MATRIX.json（权威 JSON）
配套: docs/traceability/TRACEABILITY_MATRIX.csv（gen_traceability_csv.py 生成的视图）

规则（逐 module 断言，任一 FAIL ⇒ 整体 FAIL）:
  RULE-A DOC_ANCHOR      test_path 不得是文档锚（docs/**.md::ID；本门同时把纯 .md 路径判红，
                         二者同类：文档不是可执行测试目标）。
                         依据: PSF.md §13 / SCI-FIX-PSF 第 6 项——文档锚不产出任何可执行证据。
  RULE-B EVID_GAP        test_status == "VERIFIED" ⇒ evidence_status != "MISSING"
                         且 evidence_id != "EVID-MISSING"（禁止「测试已验证」与「证据缺失」并存）。
  RULE-C PATH_MISSING    src_path/test_path 中出现的文件路径必须真实存在于仓库根下；
                         支持 path::symbol（只取第一个 :: 之前）、括号说明（(...) 整段跳过）、
                         ; / + 分隔的复合写法、:LINE 行锚与前置散文（只取含 / 的路径 token）。
                         目录需含 CMakeLists.txt（ctest 套件目录），否则视为路径无效。

输出（stdout，机器可读）:
  PASS <module_id> <RULE> / FAIL <module_id> <RULE>   每 module 每规则一行
  DETAIL <module_id> <RULE> <message>                 失败细节
  TRACEABILITY MATRIX GATE PASS (n modules) 或 FAIL (n modules) 末行
退出码: 0 = 全 PASS；1 = 存在 FAIL；3 = TOOLING_FAILURE（不允许伪 PASS）。

用法:
  python3 tools/check_traceability_matrix.py [--root <repo>] [--matrix <json>]
      [--module MOD-xxx ...] [--quiet] [--json-out <file>]
  python3 tools/check_traceability_matrix.py --self-test \
      --self-test-dir run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/matrix_gate_selftest

依赖: Python 3.10+ 标准库；无网络；不改任何文件（--self-test 只写 --self-test-dir）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

MATRIX_REL = "docs/traceability/TRACEABILITY_MATRIX.json"
RULES = ("RULE-A", "RULE-B", "RULE-C")

# 去掉括号说明 (ctest a/b/c) / (if EXISTS 门卫)
PAREN_RE = re.compile(r"\([^()]*\)")
# 路径 token: 至少含一个 '/'，字符集排除 ':' 以自然截断 :LINE / ::symbol
PATH_TOKEN_RE = re.compile(r"[A-Za-z0-9_.\-]+(?:/[A-Za-z0-9_.\-]+)+")


# --------------------------------------------------------------------------
# 路径抽取
# --------------------------------------------------------------------------
def extract_paths(field: str) -> list[str]:
    """从 src_path/test_path 字段抽取候选文件/目录路径（仓库根相对）。

    例子:
      "lib/algorithms/psf/src/dpsf_psf.cpp::moffat4_fit_tmpl,lm_solve"
          -> ["lib/algorithms/psf/src/dpsf_psf.cpp"]
      "lib/algorithms/psf/tests/p1psf (ctest p1psf_units/p1psf_oracle) + tests/backend/x.py"
          -> ["lib/algorithms/psf/tests/p1psf", "tests/backend/x.py"]
      "生产源 lib/algorithms/noise_snr/cpp/src/noise_model.cpp; tests/unit/CMakeLists.txt:528 add_subdirectory"
          -> ["lib/algorithms/noise_snr/cpp/src/noise_model.cpp", "tests/unit/CMakeLists.txt"]
    """
    if not isinstance(field, str) or not field.strip() or field.strip() == "MISSING":
        return []
    text = PAREN_RE.sub(" ", field)
    seen: list[str] = []
    for seg in re.split(r"[;+]", text):
        seg = seg.split("::", 1)[0]  # 只取第一个 :: 之前（path::symbol 口径）
        for tok in PATH_TOKEN_RE.findall(seg):
            tok = tok.strip().strip(".,")
            if tok and tok not in seen:
                seen.append(tok)
    return seen


def path_status(root: str, rel: str) -> tuple[bool, str]:
    """返回 (是否存在且形态合法, 说明)。"""
    full = os.path.join(root, rel)
    if os.path.isfile(full):
        return True, "file"
    if os.path.isdir(full):
        if os.path.isfile(os.path.join(full, "CMakeLists.txt")):
            return True, "dir(ctest suite)"
        return False, "dir 但无 CMakeLists.txt（不是 ctest 套件目录）"
    return False, "不存在"


# --------------------------------------------------------------------------
# 三规则
# --------------------------------------------------------------------------
def eval_module(root: str, row: dict) -> list[dict]:
    """对单个 module 行施加三规则，返回 [{rule, ok, detail}]。"""
    mid = str(row.get("module_id", "?"))
    out: list[dict] = []

    # RULE-A DOC_ANCHOR
    test_path = row.get("test_path", "")
    docs = [p for p in extract_paths(test_path) if p.lower().endswith(".md")]
    if docs:
        kind = "docs/**.md::ID 文档锚" if "::" in str(test_path) else "纯文档路径"
        out.append({"rule": "RULE-A", "ok": False,
                    "detail": f"test_path 是{kind}: {', '.join(docs)}（文档不是可执行测试目标）"})
    else:
        out.append({"rule": "RULE-A", "ok": True, "detail": ""})

    # RULE-B EVID_GAP
    test_status = row.get("test_status")
    ev_id = str(row.get("evidence_id", ""))
    ev_status = str(row.get("evidence_status", ""))
    if test_status == "VERIFIED" and (ev_status == "MISSING" or ev_id == "EVID-MISSING"):
        out.append({"rule": "RULE-B", "ok": False,
                    "detail": (f"test_status=VERIFIED 但 evidence_id={ev_id!r} / "
                               f"evidence_status={ev_status!r}（验证与证据缺失并存）")})
    else:
        out.append({"rule": "RULE-B", "ok": True, "detail": ""})

    # RULE-C PATH_MISSING
    bad: list[str] = []
    for label, field in (("src_path", row.get("src_path", "")),
                         ("test_path", row.get("test_path", ""))):
        if not isinstance(field, str) or field.strip() in ("", "MISSING"):
            continue
        for rel in extract_paths(field):
            ok, why = path_status(root, rel)
            if not ok:
                bad.append(f"{label}:{rel} ({why})")
    if bad:
        out.append({"rule": "RULE-C", "ok": False, "detail": "路径不可解析: " + "; ".join(bad)})
    else:
        out.append({"rule": "RULE-C", "ok": True, "detail": ""})
    return out


def load_matrix(path: str) -> list[dict]:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    mods = data.get("modules")
    if not isinstance(mods, list):
        raise ValueError(f"{path}: modules 不是数组")
    return mods


def run_gate(root: str, matrix_path: str, only: list[str], quiet: bool) -> tuple[list[dict], int]:
    rows = load_matrix(matrix_path)
    if only:
        want = set(only)
        rows = [r for r in rows if r.get("module_id") in want]
        missing = sorted(want - {r.get("module_id") for r in rows})
        for m in missing:
            print(f"DETAIL {m} RULE-? 模块不在矩阵中")
    results: list[dict] = []
    n_mod = 0
    for row in rows:
        mid = str(row.get("module_id", "?"))
        n_mod += 1
        for r in eval_module(root, row):
            results.append({"module_id": mid, **r})
            if not r["ok"]:
                print(f"FAIL {mid} {r['rule']}")
                print(f"DETAIL {mid} {r['rule']} {r['detail']}")
            elif not quiet:
                print(f"PASS {mid} {r['rule']}")
    return results, n_mod


# --------------------------------------------------------------------------
# --self-test：对故意构造的坏副本跑真实 CLI，证明每条规则都有鉴别力
# --------------------------------------------------------------------------
def _row(mid: str, **kw) -> dict:
    base = {
        "module_id": mid, "module_kind": "phase1",
        "module_anchor": "docs/modules/registry/astrocs.phase1.star-psf.md",
        "src_path": "lib/algorithms/psf/src/dpsf_psf.cpp::moffat4_fit_tmpl",
        "src_status": "VERIFIED",
        "test_id": "TEST-SELFTEST-001", "test_path": "tests/backend/test_psf_moffat_oracle.py",
        "test_status": "VERIFIED",
        "evidence_id": "EVID-SELFTEST-001", "evidence_status": "VERIFIED",
        "notes": "self-test synthetic row",
    }
    base.update(kw)
    return base


def self_test(root: str, outdir: str, matrix_real: str) -> int:
    os.makedirs(outdir, exist_ok=True)
    good_dir = "lib/algorithms/psf/tests/p1psf"
    cases: list[tuple[str, dict, str]] = [
        # (case 名, 行, 期望必红的规则；"NONE" = 必须全绿)
        ("A1_doc_anchor_md_id",
         _row("MOD-selftest-a1", test_path="docs/algorithms/STAR_PSF_ALGORITHMS.md::TEST-PSF-DESIGN-001"),
         "RULE-A"),
        ("A2_doc_only_path",
         _row("MOD-selftest-a2", test_path="docs/algorithms/COSMETIC_ALGORITHMS.md"),
         "RULE-A"),
        ("B1_verified_no_evidence",
         _row("MOD-selftest-b1", evidence_id="EVID-MISSING", evidence_status="MISSING"),
         "RULE-B"),
        ("B2_placeholder_id_only",
         _row("MOD-selftest-b2", evidence_id="EVID-MISSING", evidence_status="VERIFIED"),
         "RULE-B"),
        ("C1_missing_file",
         _row("MOD-selftest-c1", src_path="lib/algorithms/psf/src/__no_such_file__.cpp::x"),
         "RULE-C"),
        ("C2_dir_without_cmakelists",
         _row("MOD-selftest-c2", test_path="docs/traceability"),
         "RULE-C"),
        ("GOOD_control",
         _row("MOD-selftest-good", test_path=good_dir + " (ctest p1psf_units/p1psf_oracle) + "
              "tests/backend/test_psf_moffat_oracle.py"),
         "NONE"),
    ]
    print(f"SELFTEST dir={outdir} cases={len(cases)} (真实 CLI 子进程复算)")
    rc_all = 0
    n_bad_caught = 0
    for name, row, expect in cases:
        copy = os.path.join(outdir, f"matrix_selftest_{name}.json")
        with open(copy, "w", encoding="utf-8") as f:
            json.dump({"schema": "astrocs.traceability-matrix/v1", "modules": [row]}, f,
                      ensure_ascii=False, indent=1)
        proc = subprocess.run(
            [sys.executable, os.path.abspath(__file__), "--root", root, "--matrix", copy,
             "--quiet"],
            capture_output=True, text=True)
        fired = sorted({ln.split()[2] for ln in proc.stdout.splitlines()
                        if ln.startswith("FAIL ")})
        if expect == "NONE":
            ok = proc.returncode == 0 and not fired
        else:
            ok = proc.returncode == 1 and expect in fired
        print(f"SELFTEST {'PASS' if ok else 'FAIL'} {name} expect={expect} fired={fired} rc={proc.returncode}")
        for ln in proc.stdout.splitlines():
            if ln.startswith("DETAIL "):
                print("    " + ln)
        if not ok:
            rc_all = 1
        elif expect != "NONE":
            n_bad_caught += 1
    verdict = "PASS" if rc_all == 0 else "FAIL"
    print(f"TRACEABILITY MATRIX GATE SELFTEST {verdict} "
          f"(cases={len(cases)} bad_caught={n_bad_caught} clean_cases=1)")
    return rc_all


# --------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description="追溯矩阵可执行性门（RULE-A/B/C）")
    ap.add_argument("--root", default=".")
    ap.add_argument("--matrix", default=None, help=f"默认 {MATRIX_REL}")
    ap.add_argument("--module", action="append", default=[], help="只检查指定 module_id（可重复）")
    ap.add_argument("--quiet", action="store_true", help="只打印 FAIL 行")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--self-test-dir", default="run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/matrix_gate_selftest")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    matrix_path = args.matrix or os.path.join(root, MATRIX_REL)

    if args.self_test:
        outdir = args.self_test_dir if os.path.isabs(args.self_test_dir) \
            else os.path.join(root, args.self_test_dir)
        return self_test(root, outdir, matrix_path)

    results, n_mod = run_gate(root, matrix_path, args.module, args.quiet)
    fails = [r for r in results if not r["ok"]]
    verdict = "PASS" if not fails else "FAIL"
    print(f"TRACEABILITY MATRIX GATE {verdict} ({n_mod} modules)")
    if args.json_out:
        try:
            summary = {
                "checker": "tools/check_traceability_matrix.py",
                "matrix": os.path.relpath(matrix_path, root),
                "matrix_sha256": hashlib.sha256(open(matrix_path, "rb").read()).hexdigest(),
                "modules": n_mod,
                "results": len(results),
                "fails": len(fails),
                "fail_detail": fails,
                "verdict": verdict,
            }
            os.makedirs(os.path.dirname(os.path.abspath(args.json_out)) or ".", exist_ok=True)
            with open(args.json_out, "w", encoding="utf-8") as f:
                json.dump(summary, f, ensure_ascii=False, indent=1, sort_keys=True)
                f.write("\n")
            print(f"JSON_OUT {args.json_out}")
        except Exception as exc:  # noqa: BLE001
            print(f"TOOLING_FAILURE: 写 json-out 失败 {exc}", file=sys.stderr)
            return 3
    return 0 if not fails else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print(f"TOOLING_FAILURE: 未捕获异常 {exc!r}", file=sys.stderr)
        raise SystemExit(3)
