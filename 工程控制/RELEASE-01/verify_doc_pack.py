#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-001 文档包替换核验器（工程控制 / RELEASE-01）

可机器复跑判据（fail-closed：输入缺失即判红）：

  R1 权威文档在位：DOC_PACK_MANIFEST.json 列出的 36 篇全部存在，且
     sha256 == 授权后哈希（授权差异逐条登记在 manifest 的 authorized_edits）
  R2 治理痕迹零残留：36 篇内无治理/任务 ID（GAP-/CFG-/W#-A#/LEDGER-/SC-FIX/
     MOD-/TASK-/ROOT-/GOV-/DOC-/AUD-/BLD-/TST-/E2E-/VIS-/PERF-/DEL-/FIN-）
     且无头部元信息块（首 6 行内的“状态/日期/版本/作者/审核/负责人:”行）
  R3 相对引用可达：36 篇内的 docs/... 相对引用全部指向存在的文件
     （通配形态 docs/.../* 要求至少一个匹配）
  R4 旧版本号零残留：36 篇内无旧世代版本串（V19 / v6 世代 / 0.0.x-alpha 等）；
     发布纪律正文中的 0.1alpha / 0.0.1alpha 属授权文本，不计

用法：
  python3 工程控制/RELEASE-01/verify_doc_pack.py [--json-out <path>]
退出码：0 = PASS；1 = FAIL；2 = 输入缺失或用法错误（fail-closed）
"""

import argparse
import hashlib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MANIFEST = os.path.join(HERE, "DOC_PACK_MANIFEST.json")

RESIDUE_ID = [
    r"\bGAP-\d+", r"\bCFG-\d+", r"\bW\d+-A\d+", r"\bLEDGER-[A-Za-z]+", r"SC-FIX",
    r"\bMOD-\d+", r"\bTASK-\d+", r"\bROOT-\d+", r"\bGOV-\d+",
    r"\bDOC-\d+", r"\bAUD-\d+", r"\bBLD-\d+", r"\bTST-\d+", r"\bE2E-\d+",
    r"\bVIS-\d+", r"\bPERF-\d+", r"\bDEL-\d+", r"\bFIN-\d+",
]
HEADER_META = re.compile(r"^(状态|日期|版本|作者|审核|负责人|文档\s?ID|Commit|commit)\s*[:：]")
STALE_VERSION = [r"\bV19\b", r"\bv6\b", r"\b0\.0\.\d+-?alpha\b", r"\bv0\.\d+"]
REF_MD = re.compile(r"docs/[A-Za-z0-9_./\-]*\.md")
REF_GLOB = re.compile(r"docs/[A-Za-z0-9_./\-]*\*[A-Za-z0-9_./*\-]*")

AUTHORIZED_VERSION_TEXT = ("0.1alpha", "0.0.1alpha")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description="DOC-001 文档包替换核验器")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args(argv)

    if not os.path.isfile(MANIFEST):
        print("DOC001_PACK_MANIFEST_MISSING: %s" % MANIFEST, file=sys.stderr)
        return 2
    man = json.load(open(MANIFEST, encoding="utf-8"))
    files = man.get("files") or {}
    if not files:
        print("DOC001_PACK_MANIFEST_EMPTY", file=sys.stderr)
        return 2

    findings = []
    r1_missing, r1_mismatch = [], []
    r2 = []
    r3_bad = []
    r4 = []
    refs_checked = 0

    for rel in sorted(files):
        rec = files[rel]
        path = os.path.join(ROOT, rel)
        if not os.path.isfile(path):
            r1_missing.append(rel)
            continue
        got = sha256(path)
        if got != rec["repo"]:
            r1_mismatch.append({"path": rel, "expected": rec["repo"], "got": got})
            continue
        text = open(path, encoding="utf-8").read()
        lines = text.splitlines()

        # R2 residue
        for pat in RESIDUE_ID:
            for m in re.finditer(pat, text):
                line_no = text[: m.start()].count("\n") + 1
                r2.append({"path": rel, "line": line_no, "kind": "governance-id", "match": m.group(0)})
        for i, ln in enumerate(lines[:6], start=1):
            if HEADER_META.match(ln.strip()):
                r2.append({"path": rel, "line": i, "kind": "header-meta", "match": ln.strip()[:60]})

        # R3 reachability
        for m in REF_MD.finditer(text):
            ref = m.group(0)
            refs_checked += 1
            if not os.path.isfile(os.path.join(ROOT, ref)):
                r3_bad.append({"path": rel, "ref": ref, "kind": "missing-file"})
        for m in REF_GLOB.finditer(text):
            ref = m.group(0)
            refs_checked += 1
            head, _, tail = ref.partition("*")
            base = os.path.join(ROOT, head)
            d = os.path.dirname(base) if not os.path.isdir(base) else base
            hit = os.path.isdir(d) and any(
                fn.startswith(os.path.basename(head)) for fn in os.listdir(d)
            )
            if not hit:
                r3_bad.append({"path": rel, "ref": ref, "kind": "missing-glob"})

        # R4 stale version
        for pat in STALE_VERSION:
            for m in re.finditer(pat, text):
                line_no = text[: m.start()].count("\n") + 1
                r4.append({"path": rel, "line": line_no, "match": m.group(0)})

    for name, items in (("R1_missing", r1_missing), ("R1_hash_mismatch", r1_mismatch),
                        ("R2_residue", r2), ("R3_unreachable", r3_bad), ("R4_stale_version", r4)):
        findings.extend({"rule": name, **it} if isinstance(it, dict) else {"rule": name, "path": it} for it in items)

    verdict = "PASS" if not findings else "FAIL"
    result = {
        "verifier": "工程控制/RELEASE-01/verify_doc_pack.py",
        "task": "DOC-001",
        "source_zip_sha256": man.get("source_zip_sha256"),
        "authority_files": len(files),
        "authorized_edits": len(man.get("authorized_edits") or []),
        "refs_checked": refs_checked,
        "counts": {
            "R1_missing": len(r1_missing), "R1_hash_mismatch": len(r1_mismatch),
            "R2_residue": len(r2), "R3_unreachable": len(r3_bad), "R4_stale_version": len(r4),
        },
        "findings": findings,
        "verdict": verdict,
    }

    print("DOC-001 文档包替换核验")
    print("  权威文档: %d 篇  授权差异: %d 条  相对引用检查: %d 处"
          % (len(files), result["authorized_edits"], refs_checked))
    for k, v in result["counts"].items():
        print("  %-18s %d" % (k, v))
    for f in findings[:60]:
        print("   - " + json.dumps(f, ensure_ascii=False))
    if len(findings) > 60:
        print("   ... 其余 %d 条见 json" % (len(findings) - 60))
    print("verdict=%s" % verdict)

    if args.json_out:
        out = args.json_out if os.path.isabs(args.json_out) else os.path.join(ROOT, args.json_out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        json.dump(result, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        print("json_out=%s" % os.path.relpath(out, ROOT))

    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
