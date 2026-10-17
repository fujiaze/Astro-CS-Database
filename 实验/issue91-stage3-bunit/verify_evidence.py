#!/usr/bin/env python3
"""Issue #91：入口一致性 + 失败发布状态复核（E1-B / E1-C）。

1) 入口一致性：同一输入文件在
     * scheduler 入口（真实 CLI `acsd export`，phase3 IR 五节点链）
     * 正式 session 入口（p3_session_run，生产 C API）
   必须给出**同一 verdict**（accept/reject）与同一机器错误码（拒绝时）。
   修复前证据取自 evidence/base，修复后取自 evidence/fixed。

2) 失败发布状态：拒绝用例不得产出 FITS、不得出现 status=complete 的完成清单、
   不得残留 staging 目录/半成品。

用法: python3 实验/issue91-stage3-bunit/verify_evidence.py
"""
from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, "evidence")
WORK = os.path.join(HERE, "work")

STATUS_TO_KIND = {0: "accept", 1: "reject", 3: "reject", 4: "reject"}


def code_of(text: str) -> str:
    m = re.findall(r"P3-INPUT-[A-Z-]+", text or "")
    return m[0] if m else ""


def main() -> int:
    fails = []

    for tag in ("base", "fixed"):
        cli = json.load(open(os.path.join(EVID, tag, "results.json"), encoding="utf-8"))
        ses = json.load(open(os.path.join(EVID, tag, "session_entry.json"), encoding="utf-8"))
        cli_map = {r["case"]: r for r in cli["results"]}
        ses_map = {r["case"]: r for r in ses["results"]}

        print(f"== 入口一致性 [{tag}] ==")
        for case, s in ses_map.items():
            c = cli_map.get(case)
            if c is None:
                continue
            cli_state = "accept" if c["exit_code"] == 0 else "reject"
            ses_state = "accept" if s.get("status") == 0 else "reject"
            cli_code = (c.get("semantic_codes") or [""])[0]
            ses_code = code_of(s.get("last_error", ""))
            same = (cli_state == ses_state) and (cli_state == "accept" or cli_code == ses_code)
            if not same:
                # 只有修复后要求一致；修复前的不一致本身就是缺陷证据
                if tag == "fixed":
                    fails.append(f"[{tag}] entry parity {case}: cli={cli_state}/{cli_code} "
                                 f"session={ses_state}/{ses_code}")
            print(f"  {case:24s} cli={cli_state:6s} {cli_code or '-':44s} "
                  f"session={ses_state:6s} {ses_code or '-':44s} "
                  f"{'SAME' if same else 'DIFF'}")

    print("\n== 失败发布状态（修复后）==")
    fixed = json.load(open(os.path.join(EVID, "fixed", "results.json"), encoding="utf-8"))
    for r in fixed["results"]:
        if r["kind_expected"] == "ok":
            continue
        case = r["case"]
        out = os.path.join(WORK, "out", "fixed", case)
        entries = sorted(os.listdir(out)) if os.path.isdir(out) else []
        staging = [e for e in entries if "staging" in e.lower() or e.endswith(".tmp")]
        complete = False
        for e in entries:
            if e.startswith("acsd_run_") and e.endswith(".json"):
                m = json.load(open(os.path.join(out, e), encoding="utf-8"))
                if m.get("status") == "complete":
                    complete = True
        ok = (not r["fits_present"]) and (not complete) and (not staging)
        print(f"  {case:26s} fits={r['fits_present']} manifest="
              f"{(r.get('manifest') or {}).get('status')} staging={staging or 'none'} "
              f"-> {'PASS' if ok else 'FAIL'}")
        if not ok:
            fails.append(f"[fixed] publish state {case}: fits={r['fits_present']} "
                         f"complete={complete} staging={staging}")

    if fails:
        print("\nFAILURES:")
        for f in fails:
            print("  - " + f)
        return 1
    print("\nall parity/publish-state checks PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
