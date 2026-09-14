import json
d = json.load(open("ci/checks.json"))
checks = d["checks"] if isinstance(d, dict) and "checks" in d else d
import re
gate_files = {}
for c in checks:
    cmd = " ".join(c.get("command") or [])
    for m in re.findall(r"([A-Za-z0-9_/]+\.py)", cmd):
        gate_files.setdefault(m.split("/")[-1], []).append((c.get("id") or c.get("name"), cmd[:120]))
for k in sorted(gate_files):
    print(k, gate_files[k][:2])