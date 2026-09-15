#!/usr/bin/env python3
# W6 只读：ci/checks.json × ctest 注册面 × workflows 三面对账
import json, os, re, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
F = json.load(open(os.path.join(HERE, "_w6_cmake_facts.json"), encoding="utf-8"))
R = json.load(open(os.path.join(HERE, "_w6_reach.json"), encoding="utf-8"))
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
baseline = json.load(open("ci/ctest_baseline.json", encoding="utf-8"))

reach = set(R["reachable"])
testsite = {k: v for k, v in F["add_test"].items()}
root_tests = sorted(k for k, v in testsite.items() if all(l.rsplit(":", 1)[0] in reach for l in v))
print("口径 W6-B: checks.json 门数 =", len(checks), "| 口径 W6-C: 根图内 add_test 用例名 =", len(root_tests))
print("baseline json keys:", list(baseline.keys())[:10])

# 1) checks 里引用的 ctest 目标名
ref = collections.defaultdict(list)
for c in checks:
    for t in (c.get("ctest_targets") or []):
        ref[t].append(c["id"])
dangling = {t: v for t, v in ref.items() if t not in testsite}
print("checks.ctest_targets 引用名总数 =", len(ref), "| 在 add_test 中零命中 =", len(dangling))
for t, v in sorted(dangling.items()):
    print("   DANGLING-CTEST-TARGET", t, "<-", v)

# 2) 未被任何门引用的根图用例
never = [t for t in root_tests if t not in ref]
print("根图用例中未被任何 checks ctest_targets 引用 =", len(never), "/", len(root_tests))

# 3) baseline 名单 vs 注册面
bl = baseline.get("tests") or baseline.get("baseline") or []
print("baseline 类型:", type(bl), "长度:", len(bl) if hasattr(bl, "__len__") else "-")
if isinstance(bl, list) and bl and isinstance(bl[0], dict):
    print("baseline item keys:", sorted(bl[0].keys()))
    names = [x.get("name") or x.get("test") for x in bl]
else:
    names = list(bl)
bset = set(n for n in names if isinstance(n, str))
print("baseline 名单大小:", len(bset))
b_dangling = sorted(n for n in bset if n not in testsite)
print("baseline 名单中不在 add_test 注册面 =", len(b_dangling))
for n in b_dangling: print("   BASELINE-NOT-REGISTERED", n)
b_never = sorted(n for n in bset if n in testsite and n not in ref)
print("baseline 在册但无门引用:", len(b_never))

json.dump({"root_tests": root_tests, "gate_refs": {k: v for k, v in ref.items()},
           "never_ref": never, "baseline_names": sorted(bset),
           "baseline_dangling": b_dangling},
          open(os.path.join(HERE, "_w6_ci_facts.json"), "w"), indent=1, ensure_ascii=False)
