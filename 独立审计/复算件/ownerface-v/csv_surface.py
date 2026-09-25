#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Independent read-only analysis of docs/audit/*.csv vs docs/DOCUMENT_INDEX.yaml."""
import csv
import io
import os
import re
import subprocess
import sys
from collections import Counter

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
os.chdir(ROOT)
tracked = set(subprocess.run(["git", "-c", "core.quotePath=false", "ls-files"],
                             capture_output=True, text=True, encoding="utf-8", errors="replace").stdout.split())


def norm(p):
    return p.replace(chr(92), "/")


rows = list(csv.DictReader(io.open("docs/audit/doc_classification.csv", encoding="utf-8-sig")))
print("rows=%d cols=%s" % (len(rows), list(rows[0])))
print("authority vocab:", Counter(r["authority"] for r in rows))
print("status vocab:", Counter(r["status"] for r in rows))
print("notes non-empty rows:", sum(1 for r in rows if r["notes"].strip()))
missing = [r for r in rows if norm(r["doc_path"]) not in tracked]
print("rows whose doc_path NOT tracked at HEAD = %d / %d" % (len(missing), len(rows)))
for m in missing:
    print("   MISSING[%s] %s" % (m["authority"], norm(m["doc_path"])))

mds = sorted(p for p in tracked if p.startswith("docs/") and p.endswith(".md"))
covered = {norm(r["doc_path"]) for r in rows}
print("tracked docs/*.md = %d ; csv distinct paths = %d ; csv covers md = %d"
      % (len(mds), len(covered), len(covered & set(mds))))
print("csv paths that are not .md:", sorted(p for p in covered if not p.endswith(".md"))[:10])

# index vocabulary for the same paths
txt = io.open("docs/DOCUMENT_INDEX.yaml", encoding="utf-8").read()
entries = re.findall(r'- path: "([^"]+)"\n\s+status: ([A-Z_]+)', txt)
idx = {norm(p): s for p, s in entries}
print("index entries parsed = %d" % len(idx))
print("index status vocab:", Counter(idx.values()))
# cross-tab: csv authority vs index status for shared paths
tab = Counter()
for r in rows:
    p = norm(r["doc_path"])
    if p in idx:
        tab[(r["authority"], idx[p])] += 1
print("csv.authority x index.status (shared paths, count):")
for k, v in sorted(tab.items()):
    print("   %-16s x %-22s %d" % (k[0], k[1], v))

# who consumes the three csvs programmatically
print("--- consumers in eng/** ---")
out = subprocess.run(["git", "-c", "core.quotePath=false", "grep", "-rn",
                      "audit/doc_classification\\|audit/inventory.csv\\|risk_verification_T012",
                      "--", "eng/"], capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
print(out.strip() or "(no hits in eng/)")
print("--- consumers anywhere except DOCUMENT_INDEX/reports/evidence ---")
out2 = subprocess.run(["git", "-c", "core.quotePath=false", "grep", "-ln",
                       "doc_classification", "--", "."], capture_output=True, text=True, encoding="utf-8", errors="replace").stdout.split()
print(sorted(x for x in out2 if not x.startswith(("docs/DOCUMENT_INDEX.yaml", "reports/", "artifacts/", "docs/audit/"))))
