#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""层1-F：把控制包台账里的任务号与提交消息中出现的编号做机械对接（包 <-> 历史 的交叉证据）。
局限（须在报告中声明）：只依据提交消息文本里能被正则捕获的编号，不等于工作是否真实发生。
输出 _evidence/packs/pack_commit_link.csv 与 pack_commit_link.md"""
import json, os, re, csv
from collections import defaultdict
D = "/workspace/Astro CS Database/设计大纲/_evidence"
recs = [json.loads(l) for l in open(os.path.join(D, "commits/index.jsonl"), encoding="utf-8")]
cmap = defaultdict(list)
for r in recs:
    for t in r.get("ids") or []:
        for v in {t, t.replace("_", "-"), re.sub(r"^([A-Z]+)0*", r"\1-", t).replace("--", "-")}:
            cmap[v].append((r["seq"], r["sha"][:8], r["adate"][:10]))
inst = json.load(open(os.path.join(D, "packs/pack_instances.json"), encoding="utf-8"))
by = defaultdict(set)
for it in inst:
    for lg in (it.get("ledgers") or []):
        for i in (lg.get("all_ids") or []): by[it["identity"]].add(i.strip())
    z = (it.get("zip") or {}).get("ledger")
    if z and z.get("file"): pass
def variants(tid):
    out = {tid, tid.replace("_", "-")}
    m = re.match(r"^([A-Z]+)[-_]?0*(\d+)", tid)
    if m: out.add("%s-%s" % (m.group(1), m.group(2).zfill(3)))
    return out
rows = []
for ident, tids in sorted(by.items()):
    hit = {}; tot = 0; seqs = set()
    for t in tids:
        for v in variants(t):
            if v in cmap:
                hit[t] = len(cmap[v]); tot += len(cmap[v]); seqs.update(s for s, _, _ in cmap[v]); break
    rows.append({"身份": ident, "台账任务号数": len(tids), "在提交消息中出现的号数": len(hit),
                 "命中率": round(100.0 * len(hit) / max(1, len(tids)), 1), "提及这些号的提交数": len(seqs),
                 "命中号样例": ", ".join(sorted(hit)[:12]), "未命中号样例": ", ".join(sorted(set(tids) - set(hit))[:12])})
rows.sort(key=lambda r: -r["台账任务号数"])
with open(os.path.join(D, "packs/pack_commit_link.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
o = ["# 控制包任务号 与 提交消息编号 的机械对接", "",
     "方法：从包内 TASK_LEDGER 取任务号全集，在 1988 条提交消息的编号抽取结果（正则）中查同名或规范化变体。",
     "局限：提交消息未写编号的情况不计入；此表只说明" + chr(8220) + "包与提交在编号口径上是否对得上" + chr(8220) + "，不说明工作完成度。", ""]
o.append("| 身份 | 台账号数 | 消息中出现 | 命中率% | 相关提交数 | 未命中样例 |")
o.append("|---|---|---|---|---|---|")
for r in rows:
    o.append("| %s | %d | %d | %s | %d | %s |" % (r["身份"][:44], r["台账任务号数"], r["在提交消息中出现的号数"], r["命中率"], r["提及这些号的提交数"], r["未命中号样例"][:60]))
open(os.path.join(D, "packs/pack_commit_link.md"), "w", encoding="utf-8").write("\n".join(o) + "\n")
print("packs_with_ledger=%d" % len(rows))
for r in rows[:12]: print("%-46s 号=%-4d 命中=%-4d %s%%" % (r["身份"][:46], r["台账任务号数"], r["在提交消息中出现的号数"], r["命中率"]))
