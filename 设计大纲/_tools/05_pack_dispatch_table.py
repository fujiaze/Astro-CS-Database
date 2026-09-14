#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""层1-D：把 pack_instances.json 汇成派工表（按代际/时期分组，标出容器与副本），
输出 _evidence/packs/dispatch_groups.csv + pack_lineage_table.md"""
import json, os, re, csv
from collections import defaultdict
P = "/workspace/Astro CS Database/设计大纲/_evidence/packs"
inst = json.load(open(os.path.join(P, "pack_instances.json"), encoding="utf-8"))
by = defaultdict(list)
for it in inst: by[it["identity"]].append(it)
def dates(items):
    ds = []
    for it in items:
        ge = it.get("git_events") or {}
        for k in ("first", "last"):
            v = ge.get(k)
            if isinstance(v, list) and len(v) > 1 and v[1]: ds.append(str(v[1])[:10])
        tl = ge.get("touch_last")
        if tl: ds.append(str(tl).split(" ")[1][:10] if " " in str(tl) else "")
    ds = [d for d in ds if d]
    return (min(ds), max(ds)) if ds else ("", "")
def ledger_stats(items):
    rows = 0; dist = {}; ids = set()
    for it in items:
        for lg in (it.get("ledgers") or []):
            rows = max(rows, lg.get("rows") or 0)
            for k, v in (lg.get("status_dist") or {}).items(): dist[k] = dist.get(k, 0) + v
            for i in (lg.get("all_ids") or []): ids.add(i)
        z = (it.get("zip") or {}).get("ledger")
        if z: rows = max(rows, z.get("rows") or 0)
    return rows, dist, sorted(ids)
DATE_RE = re.compile(r"20(2\d)(\d\d)(\d\d)")
def stamp(ident, items):
    m = DATE_RE.findall(ident)
    if m: return "20%s-%s-%s" % m[0]
    for it in items:
        md = re.search(r"20\d{6}", it["path"])
        if md: 
            s = md.group(0); return s[:4] + "-" + s[4:6] + "-" + s[6:]
    return ""
rows = []
for ident, items in sorted(by.items()):
    forms = sorted({it["form"] for it in items})
    mx = max(it["nfile"] for it in items)
    nested = max((len(it.get("nested_packs") or []) for it in items), default=0)
    d0, d1 = dates(items)
    lr, dist, lids = ledger_stats(items)
    fams = sorted({re.match(r"^[A-Z]+", i).group(0) for i in lids if re.match(r"^[A-Z]+", i)})
    rows.append({"身份": ident, "日期戳": stamp(ident, items), "首次": d0, "末次": d1, "实例数": len(items),
                 "形态": ",".join(f[:4] for f in forms), "最大文件数": mx, "子包数": nested,
                 "台账行数": lr, "状态分布": json.dumps(dist, ensure_ascii=False)[:120],
                 "任务号族": ",".join(fams[:12]), "代表路径": sorted(items, key=lambda x: -x["nfile"])[0]["path"][:90]})
rows.sort(key=lambda r: (r["日期戳"] or r["首次"] or "9999", r["身份"]))
with open(os.path.join(P, "dispatch_groups.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
o = ["# 控制包身份总表（脚本生成，按日期戳/首次提交排序）", ""]
o.append("| 身份 | 日期戳 | 首次 | 末次 | 实例 | 形态 | 最大文件 | 子包 | 台账行 | 任务号族 |")
o.append("|---|---|---|---|---|---|---|---|---|---|")
for r in rows:
    o.append("| %s | %s | %s | %s | %d | %s | %d | %d | %s | %s |" % (
        r["身份"][:46], r["日期戳"], r["首次"], r["末次"], r["实例数"], r["形态"], r["最大文件数"], r["子包数"], r["台账行数"], r["任务号族"][:40]))
open(os.path.join(P, "pack_lineage_table.md"), "w", encoding="utf-8").write("\n".join(o) + "\n")
print("rows=%d" % len(rows))
for r in rows: print("%-12s %-46s inst=%d forms=%-24s ledger=%s fam=%s" % (r["日期戳"], r["身份"][:46], r["实例数"], r["形态"], r["台账行数"], r["任务号族"][:26]))
