#!/usr/bin/env python3
"""D1 gap recompute: DA-all universe minus anything registered (any form) in AUD-101-*.md."""
import sys, os, io, re

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

BASE = r"产出/"
INV = os.path.join(BASE, "inventory")
RAW = os.path.join(BASE, "raw")

RELEASE05 = [
    "ACCEPT-501", "ARCH-501", "ARCH-502", "ARCH-503", "ARCH-504", "ARCH-505",
    "BLD-501", "CLEAN-501", "CONTRACT-501", "DOC-501",
]

with open(os.path.join(INV, "DA-all.txt"), encoding="utf-8") as f:
    universe = [l.strip().replace("\\", "/") for l in f if l.strip()]

print("universe lines:", len(universe), "unique:", len(set(universe)))

# load all AUD-101-*.md ledgers
ledgers = sorted(n for n in os.listdir(RAW)
                 if n.startswith("AUD-101-") and n.endswith(".md"))
print("ledger files:", len(ledgers))
blob_parts = []
for n in ledgers:
    with open(os.path.join(RAW, n), encoding="utf-8", errors="replace") as fh:
        blob_parts.append(fh.read())
blob = "\n".join(blob_parts)
print("total ledger chars:", len(blob))

def forms(p):
    out = {p}
    out.add(os.path.basename(p))
    # package-relative forms: strip leading dirs progressively
    segs = p.split("/")
    for k in range(1, len(segs)):
        out.add("/".join(segs[k:]))
    return out

missing = []
for p in universe:
    hit = None
    for form in sorted(forms(p), key=len, reverse=True):
        if form and form in blob:
            hit = form
            break
    if hit is None:
        missing.append(p)

print("\n--- missing (no form appears anywhere in ledgers):", len(missing))
for p in missing:
    print(p)

rel = [p for p in missing if p.startswith("工程控制/RELEASE-05/tasks/")]
print("\nRELEASE-05 tasks among missing:", len(rel))
for p in rel:
    print("  ", p)

# now subtract the 10 known-registered RELEASE-05 tasks explicitly
excl = {f"工程控制/RELEASE-05/tasks/{n}.md" for n in RELEASE05}
gap = [p for p in missing if p not in excl]
print("\n=== FINAL GAP after subtracting 10 RELEASE-05 tasks:", len(gap))
for p in gap:
    print(p)

WORK = os.path.join(BASE, "复算", "d1gap3")
with open(os.path.join(WORK, "gap.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(gap) + "\n")
