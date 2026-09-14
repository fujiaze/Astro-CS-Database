#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""层1-H：模块谱系溯源表——对当前仍存在的目录，算出"由哪条提交引入、最后一次由谁改动、共被多少提交触及"。
只用 index.jsonl + 当前 worktree 目录清单，不再跑 git log。
输出 设计大纲/_evidence/commits/module_provenance.csv"""
import json, os, csv
from collections import defaultdict
REPO = "/workspace/Astro CS Database"
EV = os.path.join(REPO, "设计大纲/_evidence/commits")
ROOTS = ("lib", "cli", "include", "tests", "providers", "runtime", "modules", "contracts",
         "tools", "ci", "scripts", "packaging", "docs", "engineering", "evidence", "artifacts", "reports")
recs = [json.loads(l) for l in open(os.path.join(EV, "index.jsonl"), encoding="utf-8")]
recs.sort(key=lambda r: r["seq"])
first = {}; last = {}; ct = defaultdict(int); lines = defaultdict(lambda: [0, 0])
for r in recs:
    touched = set()
    for st, old, new, a, d in r["files"]:
        for p in (old, new):
            if not p: continue
            parts = p.split("/")
            if parts[0] not in ROOTS: continue
            k = "/".join(parts[:2]) if len(parts) > 1 else parts[0]
            touched.add(k)
    for k in touched:
        ct[k] += 1
        if k not in first: first[k] = (r["seq"], r["sha"][:8], r["adate"][:10], r["subject"][:80])
        last[k] = (r["seq"], r["sha"][:8], r["adate"][:10])
        if r["files"]:
            for st, old, new, a, d in r["files"]:
                p = new or old
                if p.startswith(k + "/") or p == k: lines[k][0] += a; lines[k][1] += d
rows = []
for k in sorted(set(list(ct.keys()))):
    top = k.split("/")[0]
    now = os.path.isdir(os.path.join(REPO, k))
    nf = 0
    if now:
        for root, dirs, files in os.walk(os.path.join(REPO, k)):
            nf += len(files)
            if nf > 20000: break
    f0 = first.get(k); l0 = last.get(k)
    rows.append({"路径": k, "层级": ("目录" if now else ("目录或文件" if os.path.exists(os.path.join(REPO, k)) else "已不在树中")), "当前存在": "是" if now else "否", "现存文件数": nf if now else "",
                 "被提交触及次数": ct[k], "累计增行": lines[k][0], "累计删行": lines[k][1],
                 "引入_seq": f0[0] if f0 else "", "引入_sha8": f0[1] if f0 else "", "引入日期": f0[2] if f0 else "",
                 "引入消息": (f0[3] if f0 else ""), "最后改动_seq": l0[0] if l0 else "", "最后改动_sha8": l0[1] if l0 else "",
                 "最后改动日期": l0[2] if l0 else ""})
rows.sort(key=lambda r: (-int(r["被提交触及次数"] or 0)))
with open(os.path.join(EV, "module_provenance.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
alive = [r for r in rows if r["当前存在"] == "是"]
gone = [r for r in rows if r["当前存在"] != "是"]
print("目录条目 %d（现存 %d / 已消失 %d）" % (len(rows), len(alive), len(gone)))
print("\n最活跃 12 个目录：")
for r in rows[:12]:
    print(" %-28s 现存=%s 提交=%s 引入=C%s %s(%s) 最后=C%s %s" % (
        r["路径"], r["当前存在"], r["被提交触及次数"], r["引入_seq"], r["引入_sha8"], r["引入日期"], r["最后改动_seq"], r["最后改动_sha8"]))
print("\n已消失但历史存在过 10 个：")
for r in gone[:10]:
    print(" %-28s 提交=%s 引入=C%s(%s) 最后=C%s(%s)" % (r["路径"], r["被提交触及次数"], r["引入_seq"], r["引入日期"], r["最后改动_seq"], r["最后改动日期"]))
