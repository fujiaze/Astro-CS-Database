import json, os, re, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
decl = {c["id"]: c.get("prerequisite_tools") for c in checks if c.get("prerequisite_tools")}
print("声明 prerequisite_tools 的门数:", len(decl))
vals = collections.Counter()
for k, v in decl.items():
    for x in v: vals[x] += 1
print("声明值集合:", dict(vals))
for k, v in sorted(decl.items()): print("  ", k, v)