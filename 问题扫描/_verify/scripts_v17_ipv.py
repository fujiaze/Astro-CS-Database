import json
d = json.load(open("ci/checks.json"))
for i, c in enumerate(d["checks"]):
    s = json.dumps(c, ensure_ascii=False)
    if "ipv" in s.lower() or "IPV" in s:
        print(i, c.get("id"), "|", " ".join(c.get("command") or [])[:220], "| profiles:", c.get("profiles"), "| env:", c.get("environment"))
        for k in c:
            if "env" in k.lower(): print("    ", k, c[k])