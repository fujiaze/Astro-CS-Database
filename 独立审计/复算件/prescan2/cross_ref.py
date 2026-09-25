#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D4 第二路交付件 3：与既有四路判读（AUD-402-判读-A1/A2/A3/BD1.csv）交叉。"""
import csv
import io
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
RAW = r"独立审计/证据"
HERE = os.path.dirname(os.path.abspath(__file__))
FILES = ["AUD-402-判读-A1.csv", "AUD-402-判读-A2.csv", "AUD-402-判读-A3.csv",
         "AUD-402-判读-BD1.csv"]
BT = chr(96)

mine = [r[1] for r in list(csv.reader(io.open(os.path.join(HERE, "D4-分叉行筛选.csv"),
                                              encoding="utf-8-sig")))[1:]]
covered = {}
for f in FILES:
    p = os.path.join(RAW, f)
    if not os.path.isfile(p):
        print("MISSING", f)
        continue
    rows = list(csv.reader(io.open(p, encoding="utf-8-sig")))
    keys = [(r[0].strip().replace(BT, ""), r) for r in rows[1:] if r and r[0].strip()]
    hit = []
    for m in mine:
        for k, r in keys:
            kl = k.lower()
            if kl == m.lower() or kl.endswith("." + m.lower()) or ("." + m.lower() + ".") in kl \
                    or kl.replace(".", "_") == m.replace(".", "_").lower():
                hit.append((m, k))
                covered.setdefault(m, []).append(f.replace("AUD-402-判读-", "").replace(".csv", ""))
                break
    print("### %s  判读行 %d ; 与本次分叉键交集 %d" % (f, len(keys), len(hit)))
    for m, k in hit:
        print("    %-24s <- %s" % (m, k[:70]))

print()
print("=== 分叉键覆盖情况 ===")
new = [m for m in mine if m not in covered]
for m in mine:
    print("%-26s %s" % (m, ("已判:" + ",".join(sorted(set(covered[m])))) if m in covered else "新暴露"))
print("已判 %d / 新暴露 %d / 合计筛出 %d" % (len(mine) - len(new), len(new), len(mine)))

# 四路里已判但与本次筛出键不同名的相关条目（按备注里的多侧字样回捞）
print()
print("=== 四路中带「多侧/六侧/四侧/两侧并列」字样的判读条目（键名对照） ===")
for f in FILES:
    p = os.path.join(RAW, f)
    if not os.path.isfile(p):
        continue
    rows = list(csv.reader(io.open(p, encoding="utf-8-sig")))
    n = 0
    for r in rows[1:]:
        if not r or not r[0].strip():
            continue
        blob = " ".join(r)
        if any(w in blob for w in ("多侧", "六侧", "四侧", "三侧", "两侧并列", "五侧")):
            n += 1
            print("  %-14s %-34s" % (f.replace("AUD-402-判读-", "").replace(".csv", ""),
                                     r[0].strip().replace(BT, "")[:34]))
    print("  —— %s 计 %d 条带多侧并列字样" % (f, n))
