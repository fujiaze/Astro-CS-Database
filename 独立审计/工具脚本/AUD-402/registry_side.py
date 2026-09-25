import json
import re
import sys

# Dump the registry-side (config_registry.json plugin_knobs) and defaults-side
# (defaults.json fields) registration surfaces into one comparable TSV, plus a
# machine diff of declared_default vs the value actually in defaults.json.
OUT = r"独立审计/证据/scan402\registry_side.tsv"
REPO = r"F:\Astro dev\Astro CS Normalization Database"

reg = json.load(open(REPO + r"\eng\packaging\config\config_registry.json", encoding="utf-8-sig"))
dfl = json.load(open(REPO + r"\eng\packaging\config\defaults.json", encoding="utf-8-sig"))
defaults = {f["key"]: f for f in dfl["fields"]}

rows = []
for i, k in enumerate(reg["plugin_knobs"]):
    rows.append({
        "src": "config_registry.plugin_knobs[%d]" % i,
        "field": k.get("field"),
        "unit": k.get("unit"),
        "declared_default": json.dumps(k.get("declared_default"), ensure_ascii=False),
        "registration": k.get("registration"),
        "owner_class": k.get("owner_class"),
        "doc": k.get("doc"),
        "line": k.get("line"),
        "conflict": json.dumps(k.get("conflict"), ensure_ascii=False),
        "finding": json.dumps(k.get("finding"), ensure_ascii=False),
        "note": (k.get("note") or "")[:300],
    })
cols = list(rows[0])
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("\t".join(cols) + "\n")
    for r in rows:
        fh.write("\t".join(str(r[c]).replace("\t", " ").replace("\n", " ") for c in cols) + "\n")
print("plugin_knobs:", len(rows))
print("registration values:", collections := {r["registration"] for r in rows})
print("owner_class values:", {r["owner_class"] for r in rows})
# diff against defaults.json
by_leaf = {}
for key, f in defaults.items():
    by_leaf.setdefault(key.split(".")[-1], []).append((key, json.dumps(f["value"], ensure_ascii=False)))
hits = 0
diffs = []
for r in rows:
    leaf = (r["field"] or "").split(".")[-1]
    if leaf in by_leaf:
        hits += 1
        for key, val in by_leaf[leaf]:
            if val != r["declared_default"]:
                diffs.append((r["src"], r["field"], r["declared_default"], key, val))
print("knobs whose leaf also appears in defaults.json:", hits)
print("value-differing pairs:", len(diffs))
for d in diffs[:40]:
    print("   ", d)
