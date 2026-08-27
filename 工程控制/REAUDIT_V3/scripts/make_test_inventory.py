#!/usr/bin/env python3
"""Build the full test inventory (Control §7) from the repository: all test sources + discovered
executables + current status per module (phase2 run this session; others BLOCKED-not-built)."""
import os, re, csv, subprocess, collections

REPO = "/home/lighthouse/Astro CS Database"
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "05_tests")

rows = []
fields = ["test_id","path","module","type","status","notes"]

# test cpp files
for dp, _, fns in os.walk(os.path.join(REPO, "lib")):
    if "/third_party/" in dp or "/archive/" in dp or "/build/" in dp or "/_deps/" in dp:
        continue
    for fn in fns:
        if fn.endswith((".cpp", ".c")) and ("test" in fn.lower() or "fuzz" in fn.lower() or "driver" in fn.lower()):
            rel = os.path.relpath(os.path.join(dp, fn), REPO)
            parts = rel.split(os.sep)
            module = parts[1] if len(parts) > 1 else "?"
            # only include under tests/ dirs or with test/driver naming
            if "/tests/" not in rel and not re.search(r"test|driver|fuzz", fn):
                continue
            rows.append({"test_id": os.path.basename(fn), "path": rel, "module": module,
                         "type": "C++ test/driver", "status": "BLOCKED-NOT_BUILT_THIS_ROUND",
                         "notes": "module not built/run on Linux this round (Windows-centric builds; see build_matrix.csv)"})

# python tests
for dp, _, fns in os.walk(os.path.join(REPO, "lib")):
    if "/third_party/" in dp or "/archive/" in dp:
        continue
    for fn in fns:
        if fn.startswith("test_") and fn.endswith(".py"):
            rel = os.path.relpath(os.path.join(dp, fn), REPO)
            parts = rel.split(os.sep)
            module = parts[1] if len(parts) > 1 else "?"
            rows.append({"test_id": fn, "path": rel, "module": module,
                         "type": "python", "status": "BLOCKED-NOT_RUN_THIS_ROUND",
                         "notes": "requires built DLLs / Windows env"})

# phase2 executables that were actually run this round
for fn in ["phase2_synthetic_gate", "phase2_ivar_wiring"]:
    rows.append({"test_id": fn, "path": "lib/phase2/tests", "module": "phase2",
                 "type": "gtest executable", "status": "RUN (ctest phase2 release+debug)",
                 "notes": "90 tests: 79 PASS / 1 FAIL / 10 SKIP; see test_execution_matrix.csv"})

with open(os.path.join(OUT, "test_inventory_full.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader()
    w.writerows(rows)

from collections import Counter
c = Counter(r["module"] for r in rows)
print("test_inventory_full.csv rows=" + str(len(rows)))
print("by module:", dict(c))
print("phase2-run tests:", sum(1 for r in rows if r["status"].startswith("RUN")))
print("blocked-not-built:", sum(1 for r in rows if r["status"].startswith("BLOCKED")))
