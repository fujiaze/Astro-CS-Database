#!/usr/bin/env python3
"""AUD-501 census3b: PRECISE transitive reachability for checker-like scripts.
A reference counts only if the target appears as: a repo-relative path literal,
'import <stem>'/'from <stem>', or bare '<stem>' module import. Evidence line kept.
Fail-closed: zero candidates or zero dispatch entrypoints -> exit 1.
"""
import json, os, re, sys, glob
from collections import deque

REPO = r"F:/Astro dev/Astro CS Normalization Database"
def p(*a): return os.path.join(REPO, *a)

data = json.load(open(p("eng","ci","checks.json"), encoding="utf-8"))
checks = data.get("checks") or []
if not checks: print("CENSUS-FAIL empty registry"); sys.exit(1)

direct = set()
for c in checks:
    for o in [c] + (c.get("steps") or []):
        for tok in o.get("command", []):
            t = tok.replace("\\","/")
            if re.search(r"\.(py|sh)$", t): direct.add(t)
direct = {d for d in direct if os.path.isfile(p(*d.split("/")))}
if not direct: print("CENSUS-FAIL no dispatch entrypoints resolved"); sys.exit(1)

targets = set()
for pat in ("eng/ci/*.py","eng/tools/**/*.py","eng/ci/steps/*"):
    for f in glob.glob(p(*pat.split("/")), recursive=True):
        rel = os.path.relpath(f, REPO).replace("\\","/")
        if "__pycache__" in rel or rel.startswith("eng/tests/"): continue
        b = os.path.basename(rel)
        if b.startswith("check_") or b.endswith("_probe.py") or b in {
            "run_checks.py","run.py","gate_trust.py","l2_frozen_gate.py","monitor_evidence.py",
            "declared_inputs.py","failclosed_survey.py","validate_registry.py","deep_ci_driver.py",
            "ci_windows_driver.py","polarity_probe.py","resource_monitor.py","reconcile_state.py",
            "verify_ciqa_report.py","ci_repair_round.py","incremental.py","select_candidate.py"}:
            targets.add(rel)

_cache = {}
def txt(rel):
    if rel not in _cache:
        try: _cache[rel] = open(p(*rel.split("/")), encoding="utf-8", errors="replace").read().splitlines()
        except Exception: _cache[rel] = []
    return _cache[rel]

nodes = targets | direct
def find_refs(src):
    lines = txt(src)
    out = {}
    for t in sorted(nodes - {src}):
        stem = os.path.basename(t).rsplit(".",1)[0]
        pat = re.compile(r"(?<![\w/.-])(?:" + re.escape(t) + r"|(?<![\w.])" + re.escape(stem) + r"(?![\w.]))")
        for i, ln in enumerate(lines, 1):
            if pat.search(ln):
                out[t] = (i, ln.strip()[:150]); break
    return out

level = {d: 0 for d in direct}
evid = {}
dq = deque(sorted(direct))
while dq:
    cur = dq.popleft()
    for t,(ln,txtline) in find_refs(cur).items():
        if t not in level:
            level[t] = level[cur]+1
            evid[t] = (cur, level[t], ln, txtline)
            dq.append(t)

wf_dir = p(".github","workflows")
wf_txt = {f: txt(f".github/workflows/{f}") for f in os.listdir(wf_dir)} if os.path.isdir(wf_dir) else {}
def wf_hit(stem):
    for f, ls in wf_txt.items():
        for i,l in enumerate(ls,1):
            if re.search(r"(?<![\w/.-])"+re.escape(stem)+r"(?![\w.])", l): return (f,i,l.strip()[:120])
    return None

test_hits = {}
for f in glob.glob(p("eng","tests","**","*.py"), recursive=True) + glob.glob(p("eng","ci","tests","*.py")):
    rel = os.path.relpath(f, REPO).replace("\\","/")
    for t in targets - {rel}:
        if t in test_hits: continue
        stem = os.path.basename(t).rsplit(".",1)[0]
        for i,l in enumerate(txt(rel),1):
            if re.search(r"(?<![\w/.-])"+re.escape(stem)+r"(?![\w.])", l):
                test_hits[t] = (rel,i,l.strip()[:120]); break

unreach = sorted(targets - set(level))
print("TARGETS(checker-like)", len(targets), "DIRECT", len(direct))
print("REACHABLE_from_registry", len([t for t in targets if t in level]))
print("UNREACHABLE_from_registry", len(unreach))
print()
for t in unreach:
    w = wf_hit(os.path.basename(t).rsplit(".",1)[0]); th = test_hits.get(t)
    print("ORPHAN", t)
    print("   workflow-hit:", w if w else "none")
    print("   test-hit    :", th if th else "none")
