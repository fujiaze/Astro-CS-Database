#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-101 文档包替换核验器（工程控制 / RELEASE-02）

可机器复跑判据（fail-closed：输入缺失即判红）：

  R1 权威文档在位：DOC_PACK_MANIFEST.json 列出的 36 篇全部存在，且 sha256 等于
     授权后哈希（授权差异逐条登记在 manifest 的 authorized_edits）
  R2 治理痕迹零残留：36 篇内无治理/任务 ID（GAP-/CFG-/W#-A#/LEDGER-/SC-FIX/
     MOD-/TASK-/ROOT-/GOV-/DOC-/AUD-/BLD-/TST-/E2E-/VIS-/PERF-/DEL-/FIN-）
     且无头部元信息块（首 6 行内的“状态/日期/版本/作者/审核/负责人:”行）
  R3 相对引用可达：36 篇内的 docs/... 相对引用全部指向存在的文件
     （通配形态 docs/.../* 要求至少一个匹配）
  R4 旧版本号零残留：36 篇内无**旧世代**版本串（V19 / v6 世代 / 其它 0.0.x-alpha）；
     发布纪律正文中的授权版本串（AUTHORIZED_VERSION_TEXT）不计

用法：
  python3 工程控制/RELEASE-02/verify_doc_pack.py [--json-out <path>]
  python3 工程控制/RELEASE-02/verify_doc_pack.py --self-test
退出码：0 = PASS；1 = FAIL；2 = 输入缺失或用法错误（fail-closed）

修订记录（RELEASE-02 / DOC-101）：
  初版照搬 RELEASE-01 核验器，R4 定义了 AUTHORIZED_VERSION_TEXT 却**从未使用**，
  导致授权版本串 `0.0.1alpha`（本包 ACCEPTANCE_SPEC/ASTROCS_DESIGN/ENGINEERING_SPEC
  的发布纪律正文）被误判为旧世代残留（5 处假阳性）。本版按 CONTROL_PACK 00_README
  §3a 第 2 条「门禁判据不合理则改进门禁本身，配正负例与 --self-test」落实例外，
  并把 R2/R3/R4 判据抽成可单测的 scan_text()。
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

# 授权发布版本串：出现在发布纪律正文中属授权文本，R4 不计（见模块 docstring 修订记录）。
AUTHORIZED_VERSION_TEXT = ("0.1alpha", "0.0.1alpha")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _line_of(text, pos):
    return text[:pos].count("\n") + 1


def scan_text(rel, text, check_refs=True):
    """对单篇文档执行 R2/R3/R4 判据，返回 findings 列表（可单测）。"""
    out = []
    for pat in RESIDUE_ID:
        for m in re.finditer(pat, text):
            out.append({"rule": "R2_residue", "path": rel, "line": _line_of(text, m.start()),
                        "kind": "governance-id", "match": m.group(0)})
    for i, ln in enumerate(text.splitlines()[:6], start=1):
        if HEADER_META.match(ln.strip()):
            out.append({"rule": "R2_residue", "path": rel, "line": i, "kind": "header-meta",
                        "match": ln.strip()[:60]})

    if check_refs:
        for m in REF_MD.finditer(text):
            ref = m.group(0)
            if not os.path.isfile(os.path.join(ROOT, ref)):
                out.append({"rule": "R3_unreachable", "path": rel, "ref": ref, "kind": "missing-file"})
        for m in REF_GLOB.finditer(text):
            ref = m.group(0)
            head, _, tail = ref.partition("*")
            base = os.path.join(ROOT, head)
            d = os.path.dirname(base) if not os.path.isdir(base) else base
            hit = os.path.isdir(d) and any(
                fn.startswith(os.path.basename(head)) for fn in os.listdir(d)
            )
            if not hit:
                out.append({"rule": "R3_unreachable", "path": rel, "ref": ref, "kind": "missing-glob"})

    for pat in STALE_VERSION:
        for m in re.finditer(pat, text):
            tok = m.group(0)
            if tok in AUTHORIZED_VERSION_TEXT:   # R4 例外：授权发布版本串
                continue
            out.append({"rule": "R4_stale_version", "path": rel, "line": _line_of(text, m.start()),
                        "match": tok})
    return out


def self_test():
    """正负例自检：判据必须能红能绿（防恒真/恒假）。"""
    cases = []
    # R4 正例（应零命中）：授权版本串
    cases.append(("R4-authorized-version", "RELEASE.md",
                  "发布预览版 0.0.1alpha\n--version 输出 0.1alpha\n", True))
    # R4 负例（必须命中）：旧世代版本串
    cases.append(("R4-stale-version", "OLD.md", "旧版 0.0.9-alpha\nV19 世代\n", False))
    # R2 正例（应零命中）：合法检查项 ID（CHK-* 不是治理痕迹）
    cases.append(("R2-clean", "CI.md", "| CHK-E2E-REPRO | ... |\n| CHK-EXIT-CONSISTENCY | ... |\n", True))
    # R2 负例（必须命中）：治理 ID 与头部元信息
    cases.append(("R2-residue", "GOV.md", "见 GAP-033 与 CFG-001\n状态: 已审\n", False))
    # R3 正例（应零命中）：可达引用
    cases.append(("R3-reachable", "REF.md", "见 docs/ci/CI_SPEC.md\n", True))
    # R3 负例（必须命中）：不可达引用
    cases.append(("R3-dead", "REF2.md", "见 docs/definitely/missing.md\n", False))

    bad = []
    for name, rel, text, expect_clean in cases:
        got = scan_text(rel, text, check_refs=True)
        ok = (len(got) == 0) if expect_clean else (len(got) > 0)
        print("  [%s] %-24s findings=%d expect=%s"
              % ("PASS" if ok else "FAIL", name, len(got),
                 "clean" if expect_clean else "hit"))
        if not ok:
            bad.append(name)
    print("self-test: %d/%d" % (len(cases) - len(bad), len(cases)))
    return 0 if not bad else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="DOC-101 文档包替换核验器")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    if not os.path.isfile(MANIFEST):
        print("DOC101_PACK_MANIFEST_MISSING: %s" % MANIFEST, file=sys.stderr)
        return 2
    man = json.load(open(MANIFEST, encoding="utf-8"))
    files = man.get("files") or {}
    if not files:
        print("DOC101_PACK_MANIFEST_EMPTY", file=sys.stderr)
        return 2
    auth = {e["path"]: e for e in (man.get("authorized_edits") or [])}

    findings = []
    r1_missing, r1_mismatch = [], []
    refs_checked = 0
    r2_n = r3_n = r4_n = 0

    for rel in sorted(files):
        rec = files[rel]
        want = rec.get("repo") or rec.get("sha256")
        path = os.path.join(ROOT, rel)
        if not os.path.isfile(path):
            r1_missing.append(rel)
            continue
        got = sha256(path)
        exp = auth.get(rel, {}).get("repo_sha256") or want
        if got != exp:
            r1_mismatch.append({"path": rel, "expected": exp, "got": got})
            continue
        text = open(path, encoding="utf-8").read()
        refs_checked += len(REF_MD.findall(text)) + len(REF_GLOB.findall(text))
        for f in scan_text(rel, text, check_refs=True):
            findings.append(f)
            if f["rule"] == "R2_residue": r2_n += 1
            elif f["rule"] == "R3_unreachable": r3_n += 1
            elif f["rule"] == "R4_stale_version": r4_n += 1

    for name, items in (("R1_missing", r1_missing), ("R1_hash_mismatch", r1_mismatch)):
        for it in items:
            findings.append({"rule": name, **it} if isinstance(it, dict) else {"rule": name, "path": it})

    verdict = "PASS" if not findings else "FAIL"
    result = {
        "verifier": "工程控制/RELEASE-02/verify_doc_pack.py",
        "task": "DOC-101",
        "source_zip_sha256": man.get("source_zip_sha256"),
        "authority_files": len(files),
        "authorized_edits": len(man.get("authorized_edits") or []),
        "refs_checked": refs_checked,
        "counts": {
            "R1_missing": len(r1_missing), "R1_hash_mismatch": len(r1_mismatch),
            "R2_residue": r2_n, "R3_unreachable": r3_n, "R4_stale_version": r4_n,
        },
        "findings": findings,
        "verdict": verdict,
    }

    print("DOC-101 文档包替换核验")
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