import json,re,collections
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
byc=collections.defaultdict(lambda: {"real":[],"self":[]})
for c in d:
    cmd=" ".join(c.get("command",[]))
    for m in re.finditer(r"([\w/.-]+\.py)", cmd):
        script=m.group(1); tail=cmd[m.end():]
        key="self" if re.search(r"--selftest|--selfcheck", tail) else "real"
        byc[script][key].append(c["id"])
print("=== checkers with BOTH self and real CI entries ===")
for k,v in sorted(byc.items()):
    if v["self"] and v["real"]: print("  %-44s self=%s real=%s"%(k,v["self"],v["real"]))
print("=== checkers whose ONLY CI wiring is self-mode ===")
for k,v in sorted(byc.items()):
    if v["self"] and not v["real"]: print("  %-44s self-only=%s"%(k,v["self"]))