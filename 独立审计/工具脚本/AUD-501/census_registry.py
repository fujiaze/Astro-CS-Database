#!/usr/bin/env python3
"""AUD-501 deterministic census of eng/ci/checks.json + on-disk checkers.
Read-only. Fail-closed: empty/missing registry -> non-zero exit.
"""
import json, os, re, sys, glob

REPO = r"F:/Astro dev/Astro CS Normalization Database"
REG = os.path.join(REPO, "eng/ci/checks.json")

def fail(msg):
    print("CENSUS-FAIL:", msg)
    sys.exit(1)

if not os.path.isfile(REG):
    fail("registry missing " + REG)
with open(REG, encoding="utf-8") as f:
    data = json.load(f)
checks = data.get("checks")
if not checks:
    fail("registry 'checks' empty or absent")

# ---- collect top-level items and their steps ----
top = checks
units = []  # execution units: (id, kind, obj)
for c in checks:
    steps = c.get("steps")
    if steps:
        for s in steps:
            units.append((s.get("id"), "step", s, c.get("id")))
    else:
        units.append((c.get("id"), "single", c, c.get("id")))

print("TOTAL_TOP_LEVEL_ITEMS", len(top))
print("TOTAL_EXECUTION_UNITS", len(units))

# ---- profile distribution (per execution unit) ----
from collections import Counter
prof = Counter()
for uid, kind, o, parent in units:
    for p in o.get("profiles", []):
        prof[p]+=1
print("PROFILE_DIST_UNITS", dict(prof))

# waivable distribution
waiv = Counter(o.get("waivable") for _,_,o,_ in units)
print("WAIVABLE_DIST_UNITS", {str(k):v for k,v in waiv.items()})
waiv_top = Counter(c.get("waivable") for c in top)
print("WAIVABLE_DIST_TOP", {str(k):v for k,v in waiv_top.items()})

# inputs declared?
with_inputs = sum(1 for _,_,o,_ in units if o.get("inputs"))
print("UNITS_WITH_declared_inputs", with_inputs, "/", len(units))
with_opt = sum(1 for _,_,o,_ in units if o.get("optional_inputs"))
print("UNITS_WITH_optional_inputs", with_opt)
no_inputs_nonwaivable = [uid for uid,_,o,_ in units if not o.get("inputs") and o.get("waivable") is False]
print("UNITS_no_inputs_and_not_waivable", len(no_inputs_nonwaivable))

# ---- on-disk checker scripts referenced by commands ----
# find tokens in commands that look like repo script paths
script_token = re.compile(r"^(eng|lib|scripts|tools)/.*\.(py|sh|cpp|c)$")
referenced = set()
for uid, kind, o, parent in units:
    for tok in o.get("command", []):
        if script_token.match(tok.replace("\\","/")):
            referenced.add(tok.replace("\\","/"))
    for s in o.get("steps", []) or []:
        for tok in s.get("command", []):
            if script_token.match(tok.replace("\\","/")):
                referenced.add(tok.replace("\\","/"))

# all on-disk checker scripts under eng/ci and eng/tools/quality
disk_checkers = set()
for pat in ("eng/ci/check_*.py","eng/ci/*.py","eng/tools/quality/check_*.py","eng/tools/quality/*.py"):
    for p in glob.glob(os.path.join(REPO, pat)):
        rel = os.path.relpath(p, REPO).replace("\\","/")
        base = os.path.basename(rel)
        if base.startswith("check_") or base in ("run_checks.py","run.py","polarity_probe.py","resource_monitor.py","gate_trust.py","l2_frozen_gate.py","failclosed_survey.py","monitor_evidence.py","validate_registry.py","declared_inputs.py"):
            disk_checkers.add(rel)

# referenced-from-full-registry (including nested steps already handled)
orphans = sorted(d for d in disk_checkers if d not in referenced)
print("DISK_CHECKER_SCRIPTS", len(disk_checkers))
print("REFERENCED_SCRIPT_TARGETS", len(referenced))
print("ORPHAN_CHECKERS_not_referenced_by_any_command", len(orphans))
for o in orphans:
    print("  ORPHAN", o)

# registry commands referencing a script target that does NOT exist on disk
missing_targets = set()
for tok in referenced:
    if not os.path.isfile(os.path.join(REPO, tok)):
        missing_targets.add(tok)
print("REFERENCED_BUT_MISSING_ON_DISK", len(missing_targets))
for m in sorted(missing_targets):
    print("  MISSING", m)

# ---- command shape: which entries point at run_checks meta vs direct script ----
print("---COMMAND ENTRY POINTS (unit id -> first script token or interpreter)---")
ep = Counter()
for uid, kind, o, parent in units:
    cmd = o.get("command", [])
    key = "?"
    for tok in cmd:
        if script_token.match(tok.replace("\\","/")):
            key = tok.replace("\\","/"); break
    else:
        key = cmd[0] if cmd else "(empty)"
    ep[key]+=1
for k,v in sorted(ep.items(), key=lambda x:-x[1]):
    print(f"  {v:4d}  {k}")
