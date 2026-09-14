#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""独立校验层 2/3 产出：
 A. 历史侧：每个分片报告是否存在、manifest 主责 seq 是否被点名、重叠区是否被处理
 B. 包侧：49 个包身份是否都在 P-Gxx 报告里出现；STAGE/总览是否齐
输出到终端摘要 + 设计大纲/reports/_selfcheck.md"""
import os, re, csv, json, glob
R = "/workspace/Astro CS Database/设计大纲/reports"
EV = "/workspace/Astro CS Database/设计大纲/_evidence"
def rd(p):
    try: return open(p, encoding="utf-8").read()
    except Exception: return ""
missing_slices = []; slice_stats = []; seq_total = 0; seq_hit = 0; overlap_total = 0; overlap_hit = 0
for mf in sorted(glob.glob(os.path.join(EV, "commits/slices/S*/manifest.csv"))):
    sid = os.path.basename(os.path.dirname(mf))
    rp = os.path.join(R, "history/slices/H-%s.md" % sid)
    txt = rd(rp)
    prim = ov = ph = oh = 0
    for row in csv.DictReader(open(mf, encoding="utf-8")):
        if row["角色"] == "主责":
            prim += 1
            if re.search(r"(?<!\d)0*%s(?!\d)" % int(row["seq"]), txt) or row["sha8"] in txt: ph += 1
        else:
            ov += 1
            if re.search(r"(?<!\d)0*%s(?!\d)" % int(row["seq"]), txt) or row["sha8"] in txt: oh += 1
    seq_total += prim; seq_hit += ph; overlap_total += ov; overlap_hit += oh
    slice_stats.append((sid, len(txt), prim, ph, ov, oh))
    if not txt: missing_slices.append(sid)
inv = list(csv.DictReader(open(os.path.join(EV, "packs/pack_inventory.csv"), encoding="utf-8")))
idents = sorted({r["身份"] for r in inv})
ptxt = "".join(rd(p) for p in glob.glob(os.path.join(R, "packs/P-*.md")))
pack_missing = [i for i in idents if i not in ptxt and i.replace(" ", "") not in ptxt.replace(" ", "")]
stages = sorted(glob.glob(os.path.join(R, "history/STAGE-*.md")))
pstages = sorted(glob.glob(os.path.join(R, "packs/STAGE-*.md")))
lines = ["# 前台自检（脚本 08_check_reports.py 生成）", ""]
lines.append("## 历史分片")
lines.append("- 分片报告缺失: %s" % (", ".join(missing_slices) or "无"))
lines.append("- 主责条目覆盖: %d/%d = %.2f%%" % (seq_hit, seq_total, 100.0 * seq_hit / max(1, seq_total)))
lines.append("- 重叠条目被处理: %d/%d = %.2f%%" % (overlap_hit, overlap_total, 100.0 * overlap_hit / max(1, overlap_total)))
lines.append("- 阶段报告: %d 份; 总览: %s" % (len(stages), "有" if os.path.exists(os.path.join(R, "history/00_OVERVIEW.md")) else "缺"))
lines.append("- 抽查报告: %s" % ("有" if os.path.exists(os.path.join(R, "history/spotcheck.md")) else "缺"))
lines.append("")
lines.append("## 控制包")
lines.append("- 身份总数 %d；未在组报告中出现: %s" % (len(idents), ", ".join(pack_missing[:12]) or "无"))
lines.append("- 组报告 %d 份; 代际报告 %d 份; 总览: %s; 覆盖核对: %s; digest 复核: %s" % (
    len(glob.glob(os.path.join(R, "packs/P-*.md"))), len(pstages),
    "有" if os.path.exists(os.path.join(R, "packs/00_OVERVIEW.md")) else "缺",
    "有" if os.path.exists(os.path.join(R, "packs/coverage.md")) else "缺",
    "有" if os.path.exists(os.path.join(R, "packs/digest_verify.md")) else "缺"))
lines.append("")
lines.append("## 分片明细")
lines.append("| 片 | 报告字节 | 主责 | 已覆盖 | 重叠 | 重叠已处理 |")
lines.append("|---|---|---|---|---|---|")
for sid, b, p, ph, o, oh in slice_stats: lines.append("| %s | %d | %d | %d | %d | %d |" % (sid, b, p, ph, o, oh))
os.makedirs(R, exist_ok=True)
open(os.path.join(R, "_selfcheck.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines[:24]))
print("... 明细见 reports/_selfcheck.md")
