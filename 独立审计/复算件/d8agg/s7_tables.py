# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤7：总览用的三张表片段。"""
import sys, csv, collections
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

p = list(csv.DictReader(open("复算/d8agg/out_pending_l2.csv", newline="", encoding="utf-8-sig")))
by = collections.defaultdict(lambda: [set(), 0])
for r in p:
    by[r["来源成稿"]][0].add(r["对象"])
    by[r["来源成稿"]][1] += int(r["提及行数"] or 0)
print("### 待第二层（按来源成稿）")
print("| 来源成稿（无第②层复核件） | 对象数 | 提及行数 |")
print("|---|---:|---:|")
tot = 0
for k in sorted(by):
    print("| `%s` | %d | %d |" % (k, len(by[k][0]), by[k][1]))
    tot += len(by[k][0])
print("| **合计** | **%d** | **%d** |" % (tot, len(p)))
print()
ag = list(csv.DictReader(open("复算/d8agg/out_tasks_objagg.csv", newline="", encoding="utf-8-sig")))
for wv in ("W2", "W3", "W4"):
    rs = [r for r in ag if r["波次"] == wv]
    print("### %s：%d 个双层确认对象（未成套，清单留本件）" % (wv, len(rs)))
    print("| 对象 | L1行数 | 复核小节（判定） |")
    print("|---|---:|---|")
    for r in sorted(rs, key=lambda x: -int(x["L1行数"]))[:40]:
        print("| `%s` | %s | %s |" % (r["对象"], r["L1行数"], (r["复核小节"] or "")[:90]))
    print()
