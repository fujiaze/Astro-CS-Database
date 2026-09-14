import json
d = json.load(open("ci/checks.json"))
checks = d["checks"]
for c in checks:
    if c.get("id") in ("UT-BACKEND","UT-CLI","ABI-BOUNDARY","SERIAL-HARDCODE"):
        print(json.dumps(c, ensure_ascii=False, indent=1)[:1500])
        print("----")