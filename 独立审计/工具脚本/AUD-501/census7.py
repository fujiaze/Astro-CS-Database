#!/usr/bin/env python3
"""AUD-501 census7: scan-face vs zero-face-guard per dispatched checker.
For each dispatched script: count directory/glob scan faces, and detect whether the
file contains a zero-hit guard (finding raised when a scan face yields nothing).
Screening tool: report is a candidate list, NOT a verdict; each row keeps line anchors.
Fail-closed: zero scripts -> exit 1.
"""
import ast, json, os, re, sys
from collections import defaultdict

REPO = r"F:/Astro dev/Astro CS Normalization Database"
def P(*a): return os.path.join(REPO, *a)
d = json.load(open(P("eng","ci","checks.json"), encoding="utf-8"))
if not d.get("checks"): print("CENSUS-FAIL"); sys.exit(1)

units = defaultdict(set)
for c in d["checks"]:
    for o in [c] + (c.get("steps") or []):
        for tok in o.get("command", []):
            t = tok.replace("\\","/")
            if re.search(r"\.(py|sh)$", t): units[t].add(o.get("id") or c["id"])
scripts = [s for s in sorted(units) if os.path.isfile(P(*s.split("/")))]
if not scripts: print("CENSUS-FAIL no scripts"); sys.exit(1)

GUARD = re.compile(r"零命中|空面|零对象|无任何|不得为空|扫描面|EMPTY|no files|not any |len\(\w+\) == 0|is empty|零语料|无对象|zero[- ]hit")
SCANF = re.compile(r"\.(glob|rglob|iterdir)\(|os\.walk\(|glob\.glob\(|Path\([^)]*\)\s*/")

rows = []
for rel in scripts:
    src = open(P(*rel.split("/")), encoding="utf-8", errors="replace").read()
    lines = src.splitlines()
    faces = [(i, l.strip()[:110]) for i, l in enumerate(lines, 1) if SCANF.search(l)]
    guards = [(i, l.strip()[:110]) for i, l in enumerate(lines, 1) if GUARD.search(l)]
    rows.append((rel, len(faces), len(guards), faces[:2], guards[:2]))

noscans = [r for r in rows if r[1] == 0]
unguarded = [r for r in rows if r[1] >= 1 and r[2] == 0]
print("DISPATCHED_CHECKERS_WITH_SCANS", len([r for r in rows if r[1]]), "of", len(scripts))
print("scripts with >=1 scan face and ZERO zero-face-guard text:", len(unguarded))
for rel, f, g, fl, gl in unguarded:
    print(f"  UNGUARDED {rel:56s} faces={f:3d} guardhits={g}")
    for i, l in fl[:2]: print(f"        scan@{i}: {l}")
print()
print("== guard-positive examples (anchor proof that the pattern is detectable) ==")
for rel, f, g, fl, gl in [r for r in rows if r[2]][:5]:
    print(f"  {rel} faces={f} guards={g}")
    for i, l in gl[:2]: print(f"        guard@{i}: {l}")
print()
print("== scripts with no scan face (read specific files only) ==", len(noscans))
