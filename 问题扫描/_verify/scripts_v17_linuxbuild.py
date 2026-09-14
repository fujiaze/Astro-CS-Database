import json
d = json.load(open("ci/checks.json"))
checks = d["checks"]
for i, c in enumerate(checks):
    s = json.dumps(c, ensure_ascii=False)
    if "LINUX-BUILD-ROOT-GRAPH" in s:
        print(i, c.get("id"), "| keys:", sorted(c.keys()))
        for k, v in c.items():
            if "LINUX-BUILD" in json.dumps(v, ensure_ascii=False):
                print("   ", k, "=", json.dumps(v, ensure_ascii=False)[:300])