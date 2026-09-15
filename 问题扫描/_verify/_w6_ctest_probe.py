import json, os, re, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
base = json.load(open("ci/ctest_baseline.json", encoding="utf-8"))
print("ctest_baseline keys:", list(base.keys()))
print("sources:", json.dumps(base["sources"], ensure_ascii=False)[:400])
t = base["targets"]
print("targets type:", type(t), "len:", len(t))
if isinstance(t, list) and t:
    print("first item:", json.dumps(t[0], ensure_ascii=False)[:300])
elif isinstance(t, dict):
    print("keys:", list(t.keys())[:20])

# 门命令里 ctest 的用法
ctest_cmd = [c["id"] for c in checks if "ctest" in " ".join(c["command"])]
print("命令含 ctest 的门数:", len(ctest_cmd))
for c in checks:
    j = " ".join(c["command"]) if isinstance(c["command"], list) else str(c["command"])
    if "ctest" in j:
        m = re.findall(r"-R\s+\S+|-E\s+\S+", j)
        print("  ", c["id"], "| profiles:", ",".join(c["profiles"]), "| -R:", m[:3], "| ctest_targets:", (c.get("ctest_targets") or [])[:3])