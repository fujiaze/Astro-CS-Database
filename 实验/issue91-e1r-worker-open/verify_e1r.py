#!/usr/bin/env python3
"""Issue #91 / E1-R：回归核对（相对 E1 已提交证据，不重写 E1 证据）。

1) E1 的 22 例真实 CLI 判决与产物：exit code / `P3-INPUT-*` 码 / FITS 有无 /
   p3_props.bunit / FITS signal DATA sha256 / 有限与 NaN 计数 —— 逐例与
   `实验/issue91-stage3-bunit/evidence/fixed/results.json` 相同。
2) 四组合法 FITS 数值：data_sha256 逐位相同（等价于 bitwise 回归）。
3) 旧 session 入口：status / FITS 有无 / `P3-INPUT-*` 码逐例与 E1 相同。
4) 30 条单位裁决单测：重跑并记录。

用法: python3 实验/issue91-e1r-worker-open/verify_e1r.py
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
E1 = os.path.join(REPO, "实验", "issue91-stage3-bunit", "evidence")
E1R = os.path.join(HERE, "evidence")
# E1 回归矩阵（用 E1 自己的 runner 跑在 e1r_regress tag 下，落在 E1 证据单元内）
REGRESS = os.path.join(E1, "e1r_regress")

NUMERIC_CASES = ["legal_explicit", "legal_bare_adu_sb",
                 "legal_covered_all_faces", "legal_partial_coverage"]


def code_of(text: str) -> str:
    m = re.findall(r"P3-INPUT-[A-Z-]+", text or "")
    return m[0] if m else ""


def main() -> int:
    fails = []
    old = json.load(open(os.path.join(E1, "fixed", "results.json"), encoding="utf-8"))
    new = json.load(open(os.path.join(REGRESS, "results.json"), encoding="utf-8"))
    omap = {r["case"]: r for r in old["results"]}
    nmap = {r["case"]: r for r in new["results"]}

    print(f"== E1 22 例回归（旧 fixed n_fail={old['n_fail']} / 新 n_fail={new['n_fail']}）==")
    numeric_rows = []
    for case, o in omap.items():
        n = nmap.get(case)
        if n is None:
            fails.append(f"missing case {case}")
            continue
        fields = [("exit_code", o["exit_code"], n["exit_code"]),
                  ("codes", o.get("semantic_codes"), n.get("semantic_codes")),
                  ("fits_present", o["fits_present"], n["fits_present"]),
                  ("bunit", (o.get("p3_props") or {}).get("bunit"),
                   (n.get("p3_props") or {}).get("bunit")),
                  ("manifest", (o.get("manifest") or {}).get("status"),
                   (n.get("manifest") or {}).get("status"))]
        if o["fits_present"]:
            of, nf = o.get("fits") or {}, n.get("fits") or {}
            fields += [("data_sha256", of.get("data_sha256"), nf.get("data_sha256")),
                       ("finite_px", of.get("finite_px"), nf.get("finite_px")),
                       ("nan_px", of.get("nan_px"), nf.get("nan_px"))]
        bad = [(k, a, b) for k, a, b in fields if a != b]
        if bad:
            fails.append(f"[e1] {case}: " + "; ".join(f"{k}:{a}!={b}" for k, a, b in bad))
        if case in NUMERIC_CASES and o["fits_present"]:
            numeric_rows.append((case, (o.get("fits") or {}).get("data_sha256"),
                                 (n.get("fits") or {}).get("data_sha256"),
                                 (o.get("fits") or {}).get("data_sha256") ==
                                 (n.get("fits") or {}).get("data_sha256")))
    print(f"   逐例一致: {'PASS' if not any(f.startswith('[e1]') for f in fails) else 'FAIL'}")

    print("== 四组合法 FITS 数值回归（data_sha256 逐位）==")
    for case, a, b, same in numeric_rows:
        print(f"   {case:26s} {str(a)[:16]} == {str(b)[:16]} -> {same}")
        if not same:
            fails.append(f"[numeric] {case} data sha mismatch")

    print("== 旧 session 入口回归 ==")
    so = json.load(open(os.path.join(E1, "fixed", "session_entry.json"), encoding="utf-8"))
    sn = json.load(open(os.path.join(REGRESS, "session_entry.json"), encoding="utf-8"))
    sm_o = {r["case"]: r for r in so["results"]}
    sm_n = {r["case"]: r for r in sn["results"]}
    for case, o in sm_o.items():
        n = sm_n.get(case)
        if n is None:
            fails.append(f"[session] missing {case}")
            continue
        a = (o.get("status"), o.get("published_fits"), code_of(o.get("last_error", "")))
        b = (n.get("status"), n.get("published_fits"), code_of(n.get("last_error", "")))
        same = a == b
        if not same:
            fails.append(f"[session] {case}: {a} != {b}")
        print(f"   {case:24s} {a} -> {'SAME' if same else 'DIFF'}")

    print("== 30 条单位裁决单测 ==")
    unit = subprocess.run([sys.executable,
                           os.path.join(REPO, "eng", "tests", "unit",
                                        "run_p3_input_unit_guard_test.py")],
                          capture_output=True, text=True)
    tail = [l for l in unit.stdout.splitlines() if "passed" in l]
    print(f"   {tail[0] if tail else 'no output'} (rc={unit.returncode})")
    if unit.returncode != 0:
        fails.append("[unit] p3_input_unit_guard_test failed")

    print("== 正常多 worker run 数值不变（E1-R 前/后逐位）==")
    pf = json.load(open(os.path.join(E1R, "prefix", "results.json"), encoding="utf-8"))
    ff = json.load(open(os.path.join(E1R, "fixed", "results.json"), encoding="utf-8"))
    pn = ({r["case"]: r for r in pf["results"]}.get("multi_normal") or {}).get("fits") or {}
    fn = ({r["case"]: r for r in ff["results"]}.get("multi_normal") or {}).get("fits") or {}
    same = pn.get("data_sha256") == fn.get("data_sha256")
    print(f"   multi_normal prefix={str(pn.get('data_sha256'))[:16]} "
          f"fixed={str(fn.get('data_sha256'))[:16]} sum={pn.get('sum')} "
          f"-> {'SAME' if same else 'DIFF'}")
    if not same:
        fails.append("[normal] multi_normal data sha mismatch")

    if fails:
        print("\nFAILURES:")
        for f in fails:
            print("  - " + f)
        return 1
    print("\nall E1-R regression checks PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
