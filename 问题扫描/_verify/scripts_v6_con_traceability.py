# -*- coding: utf-8 -*-
"""V6 独立复算：CON-TRACEABILITY 判据（逐行照抄
tools/quality/contracts/check_traceability.py :26-:67 的规则，输入改为
git show 取指定 rev 的 docs/TRACEABILITY.csv blob；不写文件、不改索引）。
目的：验证 c3452d48 删除 4 行后核心覆盖门是否被触发。"""
import csv, io, subprocess, sys

REV = sys.argv[1] if len(sys.argv) > 1 else "HEAD"

def blob(rev, path):
    return subprocess.run(["git", "--no-optional-locks", "show", rev + ":" + path],
                          capture_output=True, text=True).stdout

tracked = set(x for x in subprocess.run(
    ["git", "--no-optional-locks", "ls-tree", "-r", "--name-only", REV],
    capture_output=True, text=True).stdout.split("\n") if x)

rows = list(csv.DictReader(io.StringIO(blob(REV, "docs/TRACEABILITY.csv"))))
findings, status = [], "PASS"

ids = [r["requirement_id"].strip() for r in rows]
dup = [x for x in set(ids) if ids.count(x) > 1]
if dup:
    findings.append(("TRACE-DUP", ",".join(sorted(dup)))); status = "FAIL"

for r in rows:
    doc = r["authority_doc"].strip()
    if doc and doc not in tracked:
        findings.append(("TRACE-MISSING-DOC", r["requirement_id"] + " -> " + doc)); status = "FAIL"

core = ["CAL", "PSF", "PHOT", "NOISE", "DRZ", "UPM", "REJ", "INT", "ACR"]
titles = " ".join(r["requirement_id"] for r in rows).upper()
if not any(k in titles for k in ("WCS", "AST")):
    findings.append(("TRACE-CORE-MISSING", "WCS")); status = "FAIL"
for kw in core:
    if kw not in titles:
        findings.append(("TRACE-CORE-MISSING", kw)); status = "FAIL"

print("REV=%s rows=%d status=%s exit=%d" % (REV, len(rows), status, 0 if status == "PASS" else 1))
for f in findings:
    print("   ", f[0], f[1])
