# -*- coding: utf-8 -*-
"""AUD-404 复算 2：CHK-PROD-WIRING 棘轮基线条目存活核对。

对 eng/ci/prod_wiring_baseline.json 的 209 条 finding_id 逐条判其锚符号/锚路径
是否仍存在于跟踪代码面（排除注释与文档）。stale = 基线仍冻结但锚已不在代码里。
"""
import collections
import json
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:/Astro dev/Astro CS Normalization Database"

d = json.load(open(REPO + "/eng/ci/prod_wiring_baseline.json", encoding="utf-8"))
ids = d["finding_ids"]
print("baseline finding_ids =", len(ids), " finding_count =", d.get("finding_count"))
print(collections.Counter(i.split(":")[0] for i in ids).most_common())
print(collections.Counter(":".join(i.split(":")[:2]) for i in ids).most_common(20))


def git_grep(pattern, pathspec):
    r = subprocess.run(
        ["git", "-C", REPO, "-c", "core.quotepath=false", "grep", "-l",
         "-e", pattern, "--"] + pathspec,
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    return [l for l in (r.stdout or "").split("\n") if l]


# 代码面 = lib + eng（排除 docs/ artifacts/ reports/ 与登记 JSON 自身）
CODE = ["lib", "eng"]
stale = []
for i in ids:
    parts = i.split(":")
    sym = parts[-1]
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", sym):
        continue
    hits = git_grep(r"\b%s\b" % sym, CODE)
    if not hits:
        stale.append(i)

print("\nstale（锚符号在 lib/eng 代码面 0 命中）:", len(stale))
for s in stale:
    print("  ", s)
json.dump(stale, open(r"独立审计/复算件/retire"
                      r"\prod_wiring_stale.json", "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
