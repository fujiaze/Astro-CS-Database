#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""OWNER-PACK-001 汇总器校验器。

对负责人审核包做机器可读的一致性 / 诚实性检查，并支持 --self-test 负向检查：
  1) 状态被写成 RELEASED            -> 必须红
  2) 漏掉任一 BLOCKER (B-01..B-05)   -> 必须红
  3) SO-05 被写成已签字              -> 必须红

用法：
  python3 verify_owner_package.py                 # 校验真实审核包（应绿 rc=0）
  python3 verify_owner_package.py --self-test      # 跑上述 3 组篡改负向检查
  python3 verify_owner_package.py --self-test --out negative_checks.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve()
REPO = HERE.parents[4]  # artifacts/v6/owner-package/checks/*.py -> repo root
assert (REPO / "artifacts").is_dir(), f"repo root resolution failed: {REPO}"
PKG = REPO / "artifacts" / "v6" / "owner-package"
STATUS = PKG / "package_status.json"
DECISIONS = PKG / "owner_decisions.json"
EVIDENCE = PKG / "evidence_index.json"
REPORT = REPO / "reports" / "v6" / "owner-package" / "OWNER-PACK-001_REVIEW_PACKAGE.md"

BLOCKERS = ["B-01", "B-02", "B-03", "B-04", "B-05"]
MAJOR = ["M-01", "M-02", "M-03", "M-04", "M-05"]
MINOR = ["m-01", "m-02", "m-03", "m-04", "m-05"]
INFO = ["i-01", "i-02", "i-03", "i-04", "i-05", "i-06", "i-07", "i-08"]
DENY = ("不得", "禁止", "未", "不是", "无", "NOT_", "非", "不可", "不能", "必红", "篡改", "负向", "NEGATIVE")


def _positive_released(text: str):
    """返回正文中正面宣称 RELEASED 的行（排除 NOT_RELEASED / 禁止性语句）。"""
    hits = []
    for i, line in enumerate(text.splitlines(), 1):
        if re.search(r"(?<!NOT_)\bRELEASED\b", line) and not any(d in line for d in DENY):
            hits.append(i)
    return hits


def validate_text(report_md: str, status: dict, decisions: dict, evidence: dict | None = None):
    fails = []

    # A. 状态与发布口径
    if status.get("package_status") != "NOT_READY":
        fails.append(f"package_status != NOT_READY ({status.get('package_status')!r})")
    if status.get("release_status") != "NOT_RELEASED":
        fails.append(f"release_status != NOT_RELEASED ({status.get('release_status')!r})")
    if status.get("released_claim") is not False:
        fails.append("released_claim 必须为 false")
    if status.get("announce_release") is not False:
        fails.append("announce_release 必须为 false")
    if status.get("version_bumped") is not False:
        fails.append("version_bumped 必须为 false")
    hits = _positive_released(report_md)
    if hits:
        fails.append(f"正文出现正面 RELEASED 宣称（行 {hits}）")
    if "NOT_READY" not in report_md or "NOT_RELEASED" not in report_md:
        fails.append("正文缺少 NOT_READY / NOT_RELEASED 字面量")

    # B. §14.5 六步
    s145 = status.get("section_14_5", [])
    if len(s145) != 6:
        fails.append(f"§14.5 步骤数 != 6 ({len(s145)})")
    if any(x.get("verdict") == "PASS" for x in s145):
        fails.append("§14.5 出现 PASS（本包无 PASS）")
    if status.get("section_14_5_summary", {}).get("PASS", 1) != 0:
        fails.append("§14.5 summary PASS != 0")

    # C. BLOCKER / MAJOR 完整性
    got_b = [b.get("id") for b in status.get("blockers", [])]
    for b in BLOCKERS:
        if b not in got_b:
            fails.append(f"漏掉 BLOCKER {b}")
    if len(got_b) != 5:
        fails.append(f"BLOCKER 条数 != 5 ({len(got_b)})")
    for m in MAJOR:
        if m not in status.get("major", []):
            fails.append(f"漏掉 MAJOR {m}")
    for m in MINOR:
        if m not in status.get("minor", []):
            fails.append(f"漏掉 MINOR {m}")
    for m in INFO:
        if m not in status.get("info", []):
            fails.append(f"漏掉 INFO {m}")
    for b in BLOCKERS:
        if b not in report_md:
            fails.append(f"正文缺少 {b}")

    # D. SO-05 不得写成已签字
    so05 = status.get("so05", {})
    if so05.get("signoff_status") != "PENDING_OWNER_SIGNOFF":
        fails.append(f"SO-05 signoff_status != PENDING_OWNER_SIGNOFF ({so05.get('signoff_status')!r})")
    if so05.get("hard_fail") not in (False,):
        fails.append("SO-05 hard_fail 必须为 false（未自行裁决）")
    if so05.get("signed") is True:
        fails.append("SO-05 被写成已签字")

    # E. 12 项裁决结构
    items = decisions.get("items", [])
    if len(items) != 12:
        fails.append(f"owner_decisions 条目 != 12 ({len(items)})")
    required = ("facts", "evidence_pointers", "decision_needed", "options", "default_if_no_decision")
    for it in items:
        for k in required:
            if not it.get(k):
                fails.append(f"{it.get('id')} 缺字段 {k}")
        if it.get("status", "AWAITING_OWNER_DECISION") != "AWAITING_OWNER_DECISION":
            fails.append(f"{it.get('id')} status 非 AWAITING_OWNER_DECISION")
    ids = [it.get("id") for it in items]
    exp = [f"OD-{i:02d}" for i in range(1, 13)]
    if ids != exp:
        fails.append(f"owner_decisions id 序列异常：{ids}")

    # F. 证据索引 sha256 重算（PENDING_RECOMPUTE 除外）
    if evidence:
        for e in evidence.get("entries", []):
            h = e.get("sha256", "")
            if not re.fullmatch(r"[0-9a-f]{64}", h):
                continue
            p = REPO / e["path"]
            if not p.exists():
                fails.append(f"证据缺失：{e['path']}")
                continue
            actual = hashlib.sha256(p.read_bytes()).hexdigest()
            if actual != h:
                fails.append(f"sha256 不符：{e['path']}")
    return fails


def load_real():
    status = json.loads(STATUS.read_text(encoding="utf-8"))
    decisions = json.loads(DECISIONS.read_text(encoding="utf-8"))
    evidence = json.loads(EVIDENCE.read_text(encoding="utf-8")) if EVIDENCE.exists() else None
    md = REPORT.read_text(encoding="utf-8")
    return md, status, decisions, evidence


def self_test():
    md, status, decisions, evidence = load_real()
    results = []

    def one(name, mutate):
        s = json.loads(json.dumps(status))
        d = json.loads(json.dumps(decisions))
        m = mutate(s, d, md)
        fails = validate_text(m, s, d, evidence)
        red = len(fails) >= 1
        results.append({"mutation": name, "detected": red, "fail_count": len(fails), "fails": fails[:6]})

    def mut_released(s, d, m):
        s["package_status"] = "RELEASED"
        s["released_claim"] = True
        return m + "\n\nPACKAGE RELEASED for production.\n"

    def mut_drop_blocker(s, d, m):
        s["blockers"] = [b for b in s["blockers"] if b["id"] != "B-03"]
        return m.replace("B-03", "B-XX")

    def mut_so05_signed(s, d, m):
        s["so05"]["signoff_status"] = "SIGNED"
        s["so05"]["signed"] = True
        return m

    one("status_written_as_RELEASED", mut_released)
    one("drop_BLOCKER_B-03", mut_drop_blocker)
    one("SO05_written_as_signed", mut_so05_signed)

    # 原始（未篡改）必须绿
    base_fails = validate_text(md, status, decisions, evidence)
    results.append({"mutation": "BASELINE_UNMUTATED", "detected": False, "fail_count": len(base_fails), "fails": base_fails[:6]})
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--out")
    args = ap.parse_args()

    if args.self_test:
        results = self_test()
        neg = [r for r in results if r["mutation"] != "BASELINE_UNMUTATED"]
        base = next(r for r in results if r["mutation"] == "BASELINE_UNMUTATED")
        all_red = all(r["detected"] for r in neg)
        base_green = base["fail_count"] == 0
        ok = all_red and base_green
        payload = {
            "schema": "astrocs.v6.owner-package.negative-checks/v1",
            "task": "OWNER-PACK-001",
            "baseline_green": base_green,
            "baseline_fails": base["fails"],
            "each_mutation_detected": all_red,
            "checks": results,
            "verdict": "NEGATIVE_CHECKS_PASS" if ok else "NEGATIVE_CHECKS_FAIL",
        }
        text = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
        if args.out:
            Path(args.out).write_text(text, encoding="utf-8")
        print(text)
        print(f"verdict={payload['verdict']} baseline_green={base_green} each_mutation_detected={all_red}")
        return 0 if ok else 1

    md, status, decisions, evidence = load_real()
    fails = validate_text(md, status, decisions, evidence)
    if fails:
        print("OWNER_PACKAGE_CHECK_FAIL:")
        for f in fails:
            print("  -", f)
        return 1
    print("OWNER_PACKAGE_CHECK_PASS: status=NOT_READY, NOT_RELEASED, 5 BLOCKER, 12 decisions, SO-05 unsigned")
    return 0


if __name__ == "__main__":
    sys.exit(main())
