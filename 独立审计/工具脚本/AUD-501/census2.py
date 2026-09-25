#!/usr/bin/env python3
"""AUD-501 census v2: registry structure + command targets + guards.
Read-only, fail-closed on empty input.
"""
import json, os, re, sys, glob
from collections import Counter, defaultdict

REPO = r"F:/Astro dev/Astro CS Normalization Database"
REG = os.path.join(REPO, "eng/ci/checks.json")

def fail(msg):
    print("CENSUS-FAIL:", msg); sys.exit(1)

if not os.path.isfile(REG):
    fail("registry missing")
data = json.load(open(REG, encoding="utf-8"))
checks = data.get("checks")
if not checks:
    fail("checks empty")

units = []            # execution units actually dispatched
for c in checks:
    for s in (c.get("steps") or [None]):
        if s is None:
            units.append((c["id"], "single", c, c["id"]))
        else:
            units.append((s.get("id"), "step", s, c["id"]))

print("TOP_LEVEL", len(checks), "UNITS", len(units))

# --- ID uniqueness ---
ids = Counter(u[0] for u in units)
dups = {k: v for k, v in ids.items() if v > 1}
print("DUP_UNIT_IDS", len(dups), dups if len(dups) < 25 else "")
topids = Counter(c["id"] for c in checks)
print("DUP_TOP_IDS", {k: v for k, v in topids.items() if v > 1})

# --- command target resolution (scan BOTH parent and step commands) ---
cmd_objs = [c for c in checks] + [s for c in checks for s in (c.get("steps") or [])]
referenced = set()
for o in cmd_objs:
    for tok in o.get("command", []):
        t = tok.replace("\\", "/")
        if "/" in t and re.search(r"\.(py|sh|cpp|c|json|exe|bat)$", t):
            referenced.add(t)
        if t.endswith(".py") or t.endswith(".sh"):
            referenced.add(t)

disk_checkers = set()
for pat in ("eng/ci/*.py", "eng/tools/quality/*.py", "eng/tools/*/*.py"):
    for p in glob.glob(os.path.join(REPO, pat)):
        rel = os.path.relpath(p, REPO).replace("\\", "/")
        b = os.path.basename(rel)
        if "__pycache__" in rel or "/tests/" in rel:
            continue
        if b.startswith("check_") or b.endswith("_probe.py") or b in (
            "run_checks.py", "run.py", "gate_trust.py", "l2_frozen_gate.py",
            "monitor_evidence.py", "declared_inputs.py", "failclosed_survey.py",
            "resource_monitor.py", "validate_registry.py", "polarity_probe.py",
            "deep_ci_driver.py", "ci_windows_driver.py"):
            disk_checkers.add(rel)

def referenced_hit(rel):
    base = os.path.basename(rel)
    return rel in referenced or base in {os.path.basename(x) for x in referenced}

orphans = sorted(d for d in disk_checkers if not referenced_hit(d))
print("DISK_CHECKER_FILES", len(disk_checkers))
print("ORPHANS(not in any registry command token)", len(orphans))
for o in orphans: print("  ORPHAN", o)

missing = sorted(t for t in referenced if t.startswith(("eng/", "lib/")) and not os.path.isfile(os.path.join(REPO, t)))
print("COMMAND_TARGETS_MISSING_ON_DISK", len(missing))
for m in missing: print("  MISSING", m)

# --- guards / semantics census ---
no_inputs = [u for u in units if not u[2].get("inputs")]
print("UNITS_WITHOUT_inputs", len(no_inputs), "/", len(units))
print("UNITS_WITH_inputs", [u[0] for u in units if u[2].get("inputs")])
print("WAIVABLE_MISSING_UNITS", [u[0] for u in units if "waivable" not in u[2]])
print("WAIVABLE_TRUE_UNITS", [u[0] for u in units if u[2].get("waivable") is True])
print("FINGERPRINT_UNITS", [u[0] for u in units if u[2].get("fingerprint")])
print("READS_RUN_RESULTS_UNITS", [u[0] for u in units if u[2].get("reads_run_results")])
print("HEAVY_UNITS", [u[0] for u in units if u[2].get("heavy")])
print("HEAVY_NO_MONITOR", [u[0] for u in units if u[2].get("heavy") and not u[2].get("requires_monitor")])
print("REQ_MONITOR_UNITS", [u[0] for u in units if u[2].get("requires_monitor")])
print("PREREQ_TOOL_UNITS", [(u[0], u[2]["prerequisite_tools"]) for u in units if u[2].get("prerequisite_tools")])
print("CTEST_TARGET_TOP", sum(1 for c in checks if c.get("ctest_targets")),
      "UNITS_WITH_CTEST_TARGETS", [u[0] for u in units if u[2].get("ctest_targets")])
print("NO_changed_paths_UNITS", len([u for u in units if not u[2].get("changed_paths") and not u[3] and False]))
np = [u[0] for u in units if not (u[2].get("changed_paths") or (next((c.get("changed_paths") for c in checks if c["id"] == u[3]), None)))]
print("UNITS_without_inherited_changed_paths", len(np), np[:40])
print("EMPTY_OUTPUTS_NONWAIVABLE", [u[0] for u in units if not u[2].get("outputs") and u[2].get("waivable") is False])
print("OUTPUTS_COUNT_zero_total", sum(1 for u in units if not u[2].get("outputs")))
