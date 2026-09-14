import json,re
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
ids=[c["id"] for c in d]
print("any id matching real counterparts?")
for pat in ["LOG-CONTRACT","ISA-LEAK","PROD-REACH","PRODUCTION-GRAPH","SERIAL-HEAVY"]:
    print("  ",pat,"->",[i for i in ids if pat in i])
print()
for c in d:
    if c["id"] in ("PRODUCTION-GRAPH","ISA-LEAK-SELFTEST","PROD-REACH-SELFTEST","LOG-CONTRACT-SELFCHECK"):
        print(json.dumps(c,ensure_ascii=False)[:600]); print()