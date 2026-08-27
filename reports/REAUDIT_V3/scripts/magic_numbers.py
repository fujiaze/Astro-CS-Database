#!/usr/bin/env python3
"""§13 magic-number scan in the scientific core (no numpy): count hardcoded numeric literals."""
import os, re, collections, json

REPO = "/home/lighthouse/Astro CS Database"
files = [
    "lib/phase2/src/upm.cpp",
    "lib/phase2/src/sampler.cpp",
    "lib/phase2/src/rejection.cpp",
    "lib/phase2/src/integrate.cpp",
    "lib/phase2/tools/stage2.cpp",
    "lib/healpix_db/healpix_drizzle/drizzle_engine.cpp",
    "lib/calibration/src/ac_api.cpp",
]
lit_re = re.compile(r'(?<![A-Za-z0-9_])(\d+\.\d+[fFlL]?|\.\d+[fFlL]?|0x[0-9a-fA-F]+|\d+[fFlL]?)(?![A-Za-z0-9_])')
result = {}
total = 0
for rel in files:
    p = os.path.join(REPO, rel)
    if not os.path.isfile(p):
        continue
    txt = open(p, encoding="utf-8", errors="ignore").read()
    # strip comments and strings crudely
    code = re.sub(r'//.*', '', txt)
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'"(?:[^"\\]|\\.)*"', '', code)
    code = re.sub(r"'(?:[^'\\]|\\.)*'", '', code)
    lits = lit_re.findall(code)
    cnt = collections.Counter(lits)
    result[rel] = {"count": len(lits), "top": cnt.most_common(12)}
    total += len(lits)
    print(rel, "->", len(lits), "literals")
    for k, v in cnt.most_common(8):
        print("   ", k, "x", v)
print()
print("TOTAL literals in science core:", total)
json.dump(result, open(os.path.join(open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip(), "package", "13_static_quality", "magic_number_scan.json"), "w"), indent=2, ensure_ascii=False)
print("written magic_number_scan.json")
