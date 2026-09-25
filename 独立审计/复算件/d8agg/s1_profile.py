# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤1：底表画像。只读，不写仓库。

用法：cd 产出 && python -B 复算/d8agg/s1_profile.py
（不用 __file__：Windows 下非 ASCII 路径的 abspath 会被 ANSI CP 破坏）
"""
import csv, collections, sys, io, os

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = os.path.join("inventory", "findings_base.csv")

with open(SRC, newline="", encoding="utf-8-sig") as f:
    r = csv.DictReader(f)
    cols = r.fieldnames
    rows = list(r)

print("COLUMNS:", cols)
print("ROWS:", len(rows))
print()
print("--- 来源成稿 分布 ---")
for k, v in collections.Counter(x["来源成稿"] for x in rows).most_common():
    print("%-42s %6d" % (k, v))
print()
print("--- 定级 分布 ---")
for k, v in collections.Counter((x.get("定级") or "(空)") for x in rows).most_common():
    print("%-24s %6d" % (k, v))
print()
print("--- 置信 分布 ---")
for k, v in collections.Counter((x.get("置信") or "(空)") for x in rows).most_common():
    print("%-24s %6d" % (k, v))
print()
print("--- 条目 非空计数 ---", sum(1 for x in rows if (x.get("条目") or "").strip()))
print("--- 对象数 分布 ---")
for k, v in collections.Counter((x.get("对象数") or "(空)") for x in rows).most_common(12):
    print("%-10s %6d" % (k, v))
print()
print("--- 被点名对象 为空 ---", sum(1 for x in rows if not (x.get("被点名对象") or "").strip()))
