#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""层1-G：结构事件表（阶段划分的客观信号），只用已固化的 index.jsonl，不再跑 git。
事件类型：
  NEWDIR   首次出现某个顶层/二级目录
  RENAME50 单提交改名或移动 >=50 个文件（目录重构）
  DELETE   一次删除 >=20 个文件（批量移除/归档）
  PACKNEW  新增控制包 marker（版本引入点）
  PACKDEL  删除控制包 marker（版本废止点）
  CI       修改 ci/checks.json 或 ci/known_failures.json（机器门口径变化）
  GATE     修改 CMakePresets / CMakeLists 顶层选项 >=3 处（用文件级近似）
  LEDGER   修改任一 TASK_LEDGER*.csv（控制包执行落账）
  SILENT   日期断档 >=2 天
输出 设计大纲/_evidence/commits/structural_events.csv + 摘要 md"""
import json, os, re, csv
from collections import defaultdict
EV = "/workspace/Astro CS Database/设计大纲/_evidence/commits"
R = "/workspace/Astro CS Database/设计大纲/reports/history"
recs = [json.loads(l) for l in open(os.path.join(EV, "index.jsonl"), encoding="utf-8")]
recs.sort(key=lambda r: r["seq"])
MARKERS = {"00_READ_FIRST.md", "control-pack.json", "MANIFEST.json", "START_PROMPT.txt", "00_AGENT_START_PROMPT.txt", "AUTONOMOUS_ENTRY.md"}
seen_dir = set(); events = []
def key(r, kind, detail, n=0):
    events.append({"seq": r["seq"], "sha8": r["sha"][:8], "date": r["adate"][:10], "kind": kind,
                   "count": n, "detail": detail[:220], "subject": r["subject"][:100]})
prev_date = None
for r in recs:
    tops = set()
    for st, old, new, a, d in r["files"]:
        p = (new or old)
        if p.startswith(("run/", "build/", "out/", "third_party/", "GaiaDR3", "BASS DR3", "testdata/", "AstroCS.wiki/")): continue
        parts = p.split("/")
        tops.add("/".join(parts[:2]) if len(parts) > 1 else p)
    nd = sorted(t for t in tops if t not in seen_dir)
    for t in nd:
        seen_dir.add(t)
        if t.count("/") >= 1 and len(nd) <= 12: key(r, "NEWDIR", "首次出现 " + t, 1)
        elif len(nd) > 12: key(r, "NEWDIR_BATCH", "首次出现 %d 个目录，例: %s" % (len(nd), ", ".join(nd[:6])), len(nd))
    ren = sum(1 for x in r["files"] if x[0] in ("R", "C", "T"))
    if ren >= 50: key(r, "RENAME50", "改名或移动 %d 文件（目录重构）" % ren, ren)
    dele = sum(1 for x in r["files"] if x[0] == "D")
    if dele >= 20: key(r, "DELETE", "删除 %d 文件（批量移除/归档）" % dele, dele)
    pnew = [x for x in r["files"] if x[0] == "A" and (x[2] or "").split("/")[-1] in MARKERS]
    for x in pnew: key(r, "PACKNEW", "新增包文件 " + (x[2] or ""), 1)
    pdel = [x for x in r["files"] if x[0] in ("D",) and (x[2] or "").split("/")[-1] in MARKERS]
    for x in pdel: key(r, "PACKDEL", "删除包文件 " + (x[2] or ""), 1)
    if any((x[2] or x[1]).startswith("ci/") for x in r["files"]): key(r, "CI", "改 ci/ 下文件（机器门口径）", sum(1 for x in r["files"] if (x[2] or x[1]).startswith("ci/")))
    if any((x[2] or x[1]).endswith("CMakePresets.json") for x in r["files"]): key(r, "PRESET", "改 CMakePresets.json", 1)
    led = [x for x in r["files"] if re.search(r"TASK_LEDGER[^/]*\.csv$", x[2] or x[1] or "")]
    if led: key(r, "LEDGER", "改台账 " + ", ".join((x[2] or x[1]).split("/")[-1] for x in led[:4]), len(led))
    dt = r["adate"][:10]
    if prev_date:
        try:
            import datetime
            d0 = datetime.date.fromisoformat(prev_date); d1 = datetime.date.fromisoformat(dt)
            if (d1 - d0).days >= 3: key(r, "SILENT", "距上一提交（拓扑序）%d 天" % (d1 - d0).days, (d1 - d0).days)
        except Exception: pass
    prev_date = dt
os.makedirs(R, exist_ok=True)
with open(os.path.join(EV, "structural_events.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=["seq", "sha8", "date", "kind", "count", "detail", "subject"])
    w.writeheader(); [w.writerow(e) for e in events]
cnt = defaultdict(int)
for e in events: cnt[e["kind"]] += 1
o = ["# 结构事件表（阶段划分的客观信号）", "",
     "由 index.jsonl 机械生成（脚本 _tools/09_structural_events.py）。序号 seq 为拓扑序（上游谱系分组聚合），不是严格日期序。", "",
     "| 事件类型 | 数量 |", "|---|---|"]
for k, v in sorted(cnt.items(), key=lambda x: -x[1]): o.append("| %s | %d |" % (k, v))
o.append("")
o.append("## 控制包引入与废止（PACKNEW / PACKDEL）")
o.append("| seq | sha8 | 日期 | 事件 | 详情 |")
o.append("|---|---|---|---|---|")
for e in events:
    if e["kind"] in ("PACKNEW", "PACKDEL"): o.append("| %d | %s | %s | %s | %s |" % (e["seq"], e["sha8"], e["date"], e["kind"], e["detail"][:90]))
o.append("")
o.append("## 目录重构与批量删除（RENAME50 / DELETE / NEWDIR_BATCH）")
o.append("| seq | sha8 | 日期 | 事件 | 数量 | 详情 | 消息首行 |")
o.append("|---|---|---|---|---|---|---|")
for e in events:
    if e["kind"] in ("RENAME50", "DELETE", "NEWDIR_BATCH"):
        o.append("| %d | %s | %s | %s | %d | %s | %s |" % (e["seq"], e["sha8"], e["date"], e["kind"], e["count"], e["detail"][:70], e["subject"][:60]))
open(os.path.join(R, "structural_events.md"), "w", encoding="utf-8").write("\n".join(o) + "\n")
print("events=%d" % len(events), dict(cnt))
