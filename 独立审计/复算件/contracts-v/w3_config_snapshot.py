"""W3 evidence collector (read-only): CONFIG_CONTRACT.md numeric snapshot vs machine sources.

Recomputes, straight from the machine fact sources, every count that
docs/contracts/CONFIG_CONTRACT.md states as a number:
  * defaults.json field total / per-group counts / authority_status value set /
    unit-bearing / source-bearing / pending items / enum_target presence
  * config_registry.json#plugin_knobs: rows, distinct docs, owner_class / finding /
    registration distribution
  * filters.json: entries, channel distribution, curve count, lookup keys
  * response_curves/filters.json: entries, per-entry key diff vs packaging copy
  * module.yaml count under lib/**
  * phase_config_* schema property counts per phase (for the "字段总数" family)
Nothing from the repository is imported; only JSON/YAML text is parsed.
"""
import io
import json
import os
import re
import subprocess
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"


def load(rel):
    return json.load(io.open(os.path.join(REPO, rel.replace("/", os.sep)), encoding="utf-8"))


def tracked(prefix):
    out = subprocess.run(["git", "-c", "core.quotePath=false", "-C", REPO, "ls-files", "--", prefix],
                         capture_output=True, text=True, encoding="utf-8", errors="replace")
    return [l for l in out.stdout.splitlines() if l.strip()]


print("=" * 78)
print("A) eng/packaging/config/defaults.json")
d = load("eng/packaging/config/defaults.json")
fields = d.get("fields", [])
print("  declared field_count in file :", d.get("field_count", "<absent>"))
print("  len(fields) actual           :", len(fields))
print("  top-level keys               :", sorted(d))
grp = Counter(f.get("key", "?").split(".")[0] for f in fields)
print("  per-group (key prefix) counts:")
for k in sorted(grp):
    print("     %-16s %d" % (k, grp[k]))
print("  group count (distinct prefix):", len(grp))
print("  authority_status value set   :", sorted({str(f.get("authority_status")) for f in fields}))
print("  authority_status distribution:", dict(Counter(str(f.get("authority_status")) for f in fields)))
print("  fields with unit             :", sum(1 for f in fields if f.get("unit")))
print("  unit == 'unspecified'        :", sum(1 for f in fields if f.get("unit") == "unspecified"))
print("  fields with source           :", sum(1 for f in fields if f.get("source")))
print("  fields with source_ref       :", sum(1 for f in fields if f.get("source_ref")))
print("  fields with enum_target      :", sum(1 for f in fields if f.get("enum_target")))
print("  pending (value is null)      :", [f.get("key") for f in fields if f.get("value") is None])
print("  keys per field (union)       :", sorted({k for f in fields for k in f}))

print()
print("=" * 78)
print("B) eng/packaging/config/config_registry.json#plugin_knobs")
r = load("eng/packaging/config/config_registry.json")
print("  top-level keys               :", sorted(r))
kn = r.get("plugin_knobs") or []
print("  len(plugin_knobs)            :", len(kn))
declared = r.get("totals") or r.get("plugin_knob_totals") or {}
print("  file-declared totals         :", json.dumps(declared, ensure_ascii=False)[:400])
print("  distinct docs                :", len({k.get("doc") for k in kn}))
for facet in ("owner_class", "finding", "registration"):
    print("  %-12s distribution: %s" % (facet, dict(Counter(str(k.get(facet)) for k in kn))))
print("  registration x owner_class   :",
      dict(Counter("%s/%s" % (k.get("registration"), k.get("owner_class")) for k in kn)))
print("  knob keys (union)            :", sorted({x for k in kn for x in k}))
confl = [k.get("doc") + ":" + str(k.get("field")) for k in kn if k.get("finding") == "conflict"]
print("  conflict rows                :", len(confl), confl[:6])

print()
print("=" * 78)
print("C) filters library")
fl = load("eng/packaging/config/filters.json")
print("  top-level keys               :", sorted(fl))
filters = fl.get("filters") or {}
print("  len(filters)                 :", len(filters))
ch = Counter((v or {}).get("channel", "<missing>") for v in filters.values())
print("  channel distribution         :", dict(ch))
print("  empty-channel filter names   :", sorted(k for k, v in filters.items() if (v or {}).get("channel", "") == ""))
lk = fl.get("lookup") or {}
print("  lookup keys                  :", sorted(lk))
print("  lookup                       :", json.dumps({k: lk[k] for k in lk if k != "non_key_examples"}, ensure_ascii=False)[:300])
print("  non_key_examples             :", [e.get("name") for e in lk.get("non_key_examples", [])])

src_rel = "lib/algorithms/photometry/data/response_curves/filters.json"
src = load(src_rel)
sf = src.get("filters") or {}
print("  source copy len(filters)     :", len(sf))
print("  key sets equal               :", set(sf) == set(filters))
diff = []
for name in sorted(set(sf) & set(filters)):
    a, b = sf[name], filters[name]
    ka, kb = set(a or {}), set(b or {})
    if ka != kb:
        diff.append((name, sorted(ka - kb), sorted(kb - ka)))
    else:
        for k in ka:
            if json.dumps(a[k], sort_keys=True) != json.dumps(b[k], sort_keys=True):
                diff.append((name, "value differs on " + k, ""))
print("  per-entry field/value diffs  :", len(diff))
for x in diff[:10]:
    print("     ", x)

print()
print("=" * 78)
print("D) module.yaml surface")
my = [p for p in tracked("lib") if p.endswith("/module.yaml")]
print("  tracked lib/**/module.yaml   :", len(my))
KNOB = {"knobs", "knob", "params", "parameters", "config", "configs", "defaults", "tunables",
        "options", "settings"}
hits = []
tops = Counter()
for p in my:
    txt = io.open(os.path.join(REPO, p.replace("/", os.sep)), encoding="utf-8", errors="replace").read()
    for line in txt.splitlines():
        m = re.match(r"^([A-Za-z0-9_]+):", line)
        if m:
            tops[m.group(1)] += 1
            if m.group(1) in KNOB:
                hits.append((p, m.group(1)))
print("  top-level key hits (union)   :", dict(tops))
print("  knob-key hits                :", len(hits), hits[:5])

print()
print("=" * 78)
print("E) phase_config schema property counts")
for ph in ("normalize", "mosaic", "export"):
    s = load("eng/contracts/schemas/phase_config_%s.schema.json" % ph)
    defs = s.get("$defs", {})
    print("  %-10s top props=%s  $defs=%s" % (
        ph, sorted((s.get("properties") or {}).keys()), sorted(defs)))
    for name, sub in sorted(defs.items()):
        if isinstance(sub, dict) and isinstance(sub.get("properties"), dict):
            print("        $defs/%-22s props=%d required=%d" % (
                name, len(sub["properties"]), len(sub.get("required") or [])))
    for name, sub in sorted(defs.items()):
        if isinstance(sub, dict) and "enum" in sub:
            print("        $defs/%-22s enum n=%d" % (name, len(sub["enum"])))
