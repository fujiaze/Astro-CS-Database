
import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
F = json.load(open(os.path.join(HERE, "_w6_cmake_facts.json"), encoding="utf-8"))
R = json.load(open(os.path.join(HERE, "_w6_reach.json"), encoding="utf-8"))
base = json.load(open("ci/ctest_baseline.json", encoding="utf-8"))
tg = base["targets"]
addtest = set(F["add_test"].keys())
print("口径 W6-K：ci/ctest_baseline.json targets =", len(tg), "| 全仓 add_test 用例名 =", len(addtest))
dang = [t for t in tg if t not in addtest]
print("baseline 名单中在 CMake add_test 面零命中的用例名 =", len(dang))
for t in dang: print("   BASELINE-DANGLING", t)
src = base["sources"]
reach = set(R["reachable"])
print()
print("=== baseline.sources 8 个 CMakeLists 的可达性 ===")
for s in src:
    print("   ", s, "根图可达" if s in reach else "不在根图(不可达)")
names_in_unreach = []
for s in src:
    if s in reach: continue
    for k, v in F["add_test"].items():
        if any(l.rsplit(":", 1)[0] == s for l in v):
            names_in_unreach.append((s, k))
print()
print("=== 来自不可达 CMakeLists 的 baseline 用例名 ===")
for s, k in names_in_unreach: print("   ", s, "->", k, "在baseline名单" if k in tg else "不在baseline")
print()
bset = set(tg)
notrun = [t for t in addtest if t not in bset]
print("在 CMake 注册但不在 baseline 名单 =", len(notrun))
print("样例 30 个:", sorted(notrun)[:30])
