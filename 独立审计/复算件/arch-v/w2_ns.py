"""AUDIT-06 arch-v W2: two module-id namespaces — build both sets, intersect, and
find who reads which key. Own script; parses YAML/JSON text only, imports nothing
from the repository. Read-only.
"""
import io
import json
import os
import re
import subprocess
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"


def sh(*args):
    return subprocess.run(args, cwd=REPO, capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


def tracked():
    return sh("git", "ls-files").split()


# ---------- side A: registry (module_ports.registry.json) ----------
REG_REL = "lib/infrastructure/pipeline/module_ports.registry.json"
reg = json.load(io.open(os.path.join(REPO, REG_REL), encoding="utf-8"))
reg_ids = sorted({m["module_id"] for m in reg["modules"]})

# ---------- side B: module.yaml module_id ----------
yaml_ids = {}
for f in tracked():
    if f.endswith("module.yaml"):
        txt = io.open(os.path.join(REPO, f), encoding="utf-8").read()
        m = re.search(r"^\s*module_id:\s*(\S+)\s*$", txt, re.M)
        yaml_ids[f] = m.group(1) if m else "<none>"
ys = sorted(set(yaml_ids.values()))

# ---------- side C: production descriptor literals ----------
prod_src = "lib/infrastructure/scheduler/src/module_adapters.cpp"
prod_ids = sorted(set(re.findall(r'"(astrocs\.phase[123]\.[a-z0-9-]+)"',
                                 io.open(os.path.join(REPO, prod_src),
                                         encoding="utf-8").read())))
ir_src = "lib/infrastructure/cli/runtime_client.cpp"
ir_ids = sorted(set(re.findall(r'"(astrocs\.[a-z0-9._-]+)"',
                               io.open(os.path.join(REPO, ir_src),
                                       encoding="utf-8").read())))

short = sorted(i for i in ys if re.match(r"^astrocs\.p[123]\.", i))

print("=== A. registry %s ===" % REG_REL)
print("  count=%d" % len(reg_ids))
for i in reg_ids:
    print("   ", i)
print("\n=== B. module.yaml module_id (tracked) ===")
print("  files=%d  distinct module_id=%d" % (len(yaml_ids), len(ys)))
for f, v in sorted(yaml_ids.items()):
    tag = "SHORT pN" if re.match(r"^astrocs\.p[123]\.", v) else (
        "LONG phaseN" if re.match(r"^astrocs\.phase[123]\.", v) else "OTHER")
    print("   %-52s %-28s %s" % (f, v, tag))
print("  distinct SHORT-form count=%d" % len(short))

print("\n=== C. production code literals ===")
print("  module_adapters.cpp astrocs.phaseN.* distinct=%d" % len(prod_ids))
print("  runtime_client.cpp  (build_pipeline_ir) distinct=%d" % len(ir_ids))
for i in ir_ids:
    print("   ", i)

print("\n=== INTERSECTIONS ===")
a, b = set(reg_ids), set(ys)
print("  registry ∩ module.yaml            = %d %s" % (len(a & b), sorted(a & b)))
print("  registry − module.yaml (only A)    = %d" % len(a - b))
print("  module.yaml − registry (only B)    = %d %s" % (len(b - a), sorted(b - a)))
print("  registry ∩ production-IR literals  = %d / %d" % (len(a & set(ir_ids)), len(a)))
print("  module.yaml-short ∩ registry       = %d" % len(set(short) & a))
print("\n=== normalized comparison (pN<->phaseN, - <-> _, ignore hips_writer/hipses) ===")


def norm(s):
    s = re.sub(r"^astrocs\.phase([123])\.", r"astrocs.p\1.", s)
    return s.replace("-", "_")


na = {norm(x) for x in a}
nb = {norm(x) for x in b}
print("  after name-form normalization: A∩B=%d  onlyA=%s  onlyB=%s"
      % (len(na & nb), sorted(na - nb), sorted(nb - na)))
