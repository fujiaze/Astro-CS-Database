#!/usr/bin/env python3
# W6 只读：可达性闭包 + ctest 注册面 + checks.json 对账
import json, os, re, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
F = json.load(open(os.path.join(HERE, "_w6_cmake_facts.json"), encoding="utf-8"))

# 建立 add_subdirectory 边：dir(CMakeLists) -> 子目录 CMakeLists
edges = collections.defaultdict(set)
for parent, locs in F["add_subdirectory"].items():
    for loc in locs:
        pf = loc.rsplit(":", 1)[0]
        pdir = os.path.dirname(pf)
        parent_norm = parent.replace("${CMAKE_CURRENT_SOURCE_DIR}", ".")
        if "${" in parent_norm:
            continue
        # 规整路径
        for guess in [parent, parent + "/CMakeLists.txt"]:
            cand = os.path.normpath(os.path.join(pdir, guess))
            if cand in F["add_test"] or os.path.exists(cand):
                pass
        tgt = os.path.normpath(os.path.join(pdir, parent_norm))
        if not tgt.endswith("CMakeLists.txt"):
            tgt = os.path.normpath(os.path.join(tgt, "CMakeLists.txt"))
        edges[pf].add(tgt)

reachable = set()
stack = ["CMakeLists.txt"]
missing = []
while stack:
    cur = stack.pop()
    if cur in reachable:
        continue
    reachable.add(cur)
    for nxt in sorted(edges.get(cur, [])):
        if os.path.exists(nxt):
            stack.append(nxt)
        else:
            missing.append((cur, nxt))

allfiles = subprocess.run(["git","--no-optional-locks","ls-files","*CMakeLists.txt"], capture_output=True, text=True).stdout.split()
unreach = [f for f in allfiles if f not in reachable]
print("=== 口径 W6-D：add_subdirectory 可达闭包（根 CMakeLists 起，静态） ===")
print("CMakeLists 总数:", len(allfiles), "根图可达:", len(reachable), "不可达:", len(unreach))
print("-- add_subdirectory 指向不存在的目标（潜在 configure 失败点） --")
for a,b in missing: print("   MISSING", a, "->", b)
print("-- 不可达（不在根图）的 CMakeLists --")
for f in unreach: print("   UNREACH", f)

print()
print("=== 口径 W6-C：add_test 用例名 × 可达性 ===")
inr = [k for k,v in F["add_test"].items() if all(l.rsplit(":",1)[0] in reachable for l in v)]
out = [k for k,v in F["add_test"].items() if any(l.rsplit(":",1)[0] not in reachable for l in v)]
print("用例名总数:", len(F["add_test"]), "根图内:", len(inr), "含不可达来源:", len(out))
for k in sorted(out): print("   TEST-NOT-IN-ROOT-GRAPH", k, F["add_test"][k])

json.dump({"reachable": sorted(reachable), "unreachable": sorted(unreach),
           "addtest_unreachable": sorted(out)}, open(os.path.join(HERE, "_w6_reach.json"),"w"), indent=1, ensure_ascii=False)
