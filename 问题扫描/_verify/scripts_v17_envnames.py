import json, re
hits = json.load(open("问题扫描/_verify/scripts_v17_censusA.json"))
print("py_getenv count:", len(hits["py_getenv"]))
names = {}
pat = re.compile(r"(?:os\.environ\.get|os\.getenv)\(\s*[\x27\"]([A-Za-z0-9_]+)[\x27\"]")
for h in hits["py_getenv"]:
    p, n, s = h.split(":", 2)
    for m in pat.findall(s):
        names.setdefault(m, []).append(p + ":" + n)
for k in sorted(names):
    print(k, len(names[k]), names[k][:5])