#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
层2 覆盖率校验：核对每个分片报告是否为其 manifest 里的每条"主责"提交写了正文条目。
用法: python3 04_check_slice_coverage.py [S01 S02 ...]
输出: 设计大纲/reports/history/coverage.csv + 控制台摘要
"""
import os, re, sys, csv
REPO = "/workspace/Astro CS Database"
SL = os.path.join(REPO, "设计大纲/_evidence/commits/slices")
RPT = os.path.join(REPO, "设计大纲/reports/history/slices")
want = sys.argv[1:]
rows = []; miss_total = 0
sids = sorted(d for d in os.listdir(SL) if re.match(r"^S\d+$", d))
for sid in sids:
    if want and sid not in want: continue
    mf = os.path.join(SL, sid, "manifest.csv")
    if not os.path.exists(mf): continue
    prim = []
    for r in csv.DictReader(open(mf, encoding="utf-8")):
        if r["角色"] == "主责": prim.append(r)
    rf = os.path.join(RPT, "H-%s.md" % sid)
    txt = open(rf, encoding="utf-8").read() if os.path.exists(rf) else ""
    # 逐条正文已下沉到证据层（11_split_slice_reports.py），覆盖核对同时看报告树三段与证据层条目
    gf = os.path.join(REPO, "设计大纲/_evidence/commits/逐条详析/H-%s.md" % sid)
    gtxt = open(gf, encoding="utf-8").read() if os.path.exists(gf) else ""
    both = txt + "\n" + gtxt
    covered = []
    for r in prim:
        seq = r["seq"]; sha = r["sha8"]
        hit = bool(re.search(r"(?<!\d)0*%s(?!\d)" % int(seq), both)) or (sha in both)
        if hit: covered.append(seq)
    miss = [r["seq"] for r in prim if r["seq"] not in covered]
    miss_total += len(miss)
    rows.append({"slice": sid, "报告存在": "是" if txt else "否", "报告字节": len(txt.encode("utf-8")) if txt else 0,
                 "主责条数": len(prim), "已覆盖": len(covered), "缺失": len(miss),
                 "缺失seq": ",".join(miss[:25]) + ("…" if len(miss) > 25 else ""),
                 "重叠条数": sum(1 for r in csv.DictReader(open(mf, encoding="utf-8")) if r["角色"] == "重叠复核")})
os.makedirs(RPT, exist_ok=True)
with open(os.path.join(REPO, "设计大纲/reports/history/coverage.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ["slice"]); w.writeheader(); w.writerows(rows)
tot_p = sum(r["主责条数"] for r in rows); tot_c = sum(r["已覆盖"] for r in rows)
print("分片 %d 个；主责提交 %d；已覆盖 %d；缺失 %d；覆盖率 %.2f%%" % (len(rows), tot_p, tot_c, miss_total, 100.0 * tot_c / max(1, tot_p)))
for r in rows:
    if r["报告存在"] == "否" or r["缺失"]:
        print("  %-5s 报告=%s 主责=%d 缺=%d %s" % (r["slice"], r["报告存在"], r["主责条数"], r["缺失"], r["缺失seq"][:70]))
