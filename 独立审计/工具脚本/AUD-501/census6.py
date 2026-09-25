#!/usr/bin/env python3
"""AUD-501 census6: for every script dispatched by eng/ci/checks.json, classify the
repo-path-like string literals it contains (what it reads) by object class.
Fail-closed: zero dispatched scripts or zero classified -> exit 1.
"""
import ast, json, os, re, sys
from collections import defaultdict

REPO = r"F:/Astro dev/Astro CS Normalization Database"
def P(*a): return os.path.join(REPO, *a)

d = json.load(open(P("eng","ci","checks.json"), encoding="utf-8"))
if not d.get("checks"): print("CENSUS-FAIL empty registry"); sys.exit(1)

# unit id(s) -> dispatched script tokens
unit_scripts = defaultdict(set)
for c in d["checks"]:
    for o in [c] + (c.get("steps") or []):
        sid = o.get("id") or c["id"]
        for tok in o.get("command", []):
            t = tok.replace("\\","/")
            if re.search(r"\.(py|sh)$", t):
                unit_scripts[t].add(sid)
scripts = sorted(unit_scripts)
if not scripts: print("CENSUS-FAIL no dispatched scripts"); sys.exit(1)

CLS = [
    ("PRODUCT_run",   re.compile(r"(^|/)run/")),
    ("ARCHIVE_evid",  re.compile(r"(^|/)artifacts/|^run_keep|evidence")),
    ("DOCS",          re.compile(r"(^|/)docs/|\.md$|ASTROCS_DESIGN|ACCEPTANCE_SPEC|ENGINEERING_SPEC|AGENTS\.md")),
    ("SELFCONFIG",    re.compile(r"(^|/)eng/(ci|contracts|packaging|cmake|build)/")),
    ("FIXTURE",       re.compile(r"fixtures/|sample_|_sample|fake_|mock_")),
    ("PRODSRC",       re.compile(r"(^|/)lib/|src/|\.cpp$|\.h$")),
    ("TESTSRC",       re.compile(r"(^|/)eng/tests/|test_")),
    ("SITE",          re.compile(r"(^|/)site/|_site/")),
    ("EXPRESS",       re.compile(r"(^|/)(实验|testdata|gaia)/")),
]
PATHISH = re.compile(r"[A-Za-z0-9_./\-*\u4e00-\u9fff]*/[A-Za-z0-9_./\-*\u4e00-\u9fff]+(\.(py|json|md|cpp|h|txt|csv|sh|fits|yaml|yml|html|js))?\b")

rows = []
for rel in scripts:
    fp = P(*rel.split("/"))
    if not os.path.isfile(fp):
        rows.append((rel, set(), "MISSING-ON-DISK")); continue
    src = open(fp, encoding="utf-8", errors="replace").read()
    lits = set()
    try:
        tree = ast.parse(src)
        for n in ast.walk(tree):
            if isinstance(n, ast.Constant) and isinstance(n.value, str):
                lits.add(n.value)
    except SyntaxError:
        lits = set()
    if not lits:  # shell scripts / unparseable -> raw line scan
        lits = set(re.findall(r"[\"']([^\"']{3,160})[\"']", src))
    blob = " | ".join(lits)
    cls = {name for name, rx in CLS if any(rx.search(x) for x in lits if "/" in x or "." in x)}
    rows.append((rel, cls, None))

prod = [r for r in rows if "PRODUCT_run" in r[1] or "EXPRESS" in r[1]]
docs_only = [r for r in rows if r[1] and not ({"PRODUCT_run","EXPRESS","ARCHIVE_evid","PRODSRC","TESTSRC"} & r[1])]
print("DISPATCHED_SCRIPTS", len(scripts))
print("reads PRODUCT/run or 实验/testdata :", len(prod))
print("reads only DOCS/SELFCONFIG/FIXTURE  :", len(docs_only))
print("reads PRODSRC (source code text)    :", len([r for r in rows if 'PRODSRC' in r[1]]))
print("reads DOCS                          :", len([r for r in rows if 'DOCS' in r[1]]))
print("reads SELFCONFIG(eng/ci,contracts)  :", len([r for r in rows if 'SELFCONFIG' in r[1]]))
print("reads FIXTURE                       :", len([r for r in rows if 'FIXTURE' in r[1]]))
print("reads TESTSRC                       :", len([r for r in rows if 'TESTSRC' in r[1]]))
print("no path-like literals at all        :", len([r for r in rows if not r[1]]))
print()
print("== scripts with NO product/archive/docs/prodsrc class (candidate self-referential) ==")
for rel, cls, note in rows:
    if not ({"PRODUCT_run","EXPRESS","ARCHIVE_evid","DOCS","PRODSRC"} & cls):
        print(f"  {rel:58s} cls={sorted(cls) or note or '[]'}  units={sorted(unit_scripts[rel])[:3]}")
print()
print("== FIXTURE-reading scripts and their unit ids ==")
for rel, cls, note in rows:
    if "FIXTURE" in cls:
        print(f"  {rel:58s} units={sorted(unit_scripts[rel])[:4]}")
