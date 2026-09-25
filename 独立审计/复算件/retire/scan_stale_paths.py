# -*- coding: utf-8 -*-
"""AUD-404 复算 5：lib/ 与 docs/ 陈旧路径名聚类（改名后登记面残留）。"""
import collections
import json
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
tracked = set(x for x in subprocess.run(
    ["git", "-C", REPO, "-c", "core.quotepath=false", "ls-files"],
    capture_output=True, text=True, encoding="utf-8", errors="replace"
).stdout.split("\n") if x)

PREFIX = r"(?:lib|docs|eng)"
PATH_RE = re.compile(r"(?<![\w./-])(" + PREFIX + r"/[\w./\-]+\.(?:hpp|cpp|csv|cc|md|json|yaml|yml|registry|txt|py|c|h))")
TXT_RE = re.compile(r"(?<![\w./-])(" + PREFIX + r"/[\w\-]+(?:/[\w\-]+)*)")

# 只扫"登记面"：合同/门/注册表/接口台账 + module.yaml + 跟踪的 lib|docs 注释
surf = [f for f in tracked if re.match(
    r"^(eng/ci/|eng/contracts/|eng/tools/quality/|docs/architecture/|"
    r"docs/contracts/|docs/traceability/|docs/modules/registry/|"
    r"docs/DOCUMENT_INDEX\.yaml|docs/TRACEABILITY\.csv)", f)]
surf += [f for f in tracked if f.endswith("module.yaml")]
surf += [f for f in tracked if re.match(r"^lib/.*\.(h|cpp|c|hpp)$", f)]

miss_src = collections.defaultdict(set)
for s in surf:
    text = open(REPO + "/" + s, encoding="utf-8", errors="replace").read()
    for tok in set(PATH_RE.findall(text)):
        t = tok.rstrip(".,;:、）)")
        if t in tracked or "*" in t:
            continue
        if any(x.startswith(t + "/") or x == t for x in tracked):
            continue
        # 目录前缀存活则不算
        parts = t.split("/")
        for k in range(len(parts), 1, -1):
            if any(x.startswith("/".join(parts[:k]) + "/") for x in tracked):
                break
        else:
            miss_src[t].add(s)

top = collections.Counter()
for t in miss_src:
    top["/".join(t.split("/")[:3])] += 1
print("陈旧 lib/docs/eng 路径名种类:", len(miss_src))
print("按前 3 段聚类 top30:")
for k, v in top.most_common(30):
    print(f"  {v:4d}  {k}")
FIXTURE = re.compile(r"/fixtures?/|_test\.py$|/tests/test_|dead\.cpp|foo\.cpp|NOPE|does_not_exist")
print("\n全部陈旧名（标注来源；FIXTURE=故意负例夹具）：")
for t, ss in sorted(miss_src.items(), key=lambda kv: (kv[0].split("/")[0], kv[0])):
    src = sorted(ss)
    tag = "FIXTURE" if all(FIXTURE.search(x) for x in src) else "REAL   "
    print(f"  {tag} {t}")
    for x in src:
        print(f"          <- {x}")
json.dump({t: sorted(v) for t, v in miss_src.items()},
          open(r"独立审计/复算件/retire"
               r"\stale_paths.json", "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
