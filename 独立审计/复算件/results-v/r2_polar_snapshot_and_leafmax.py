#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUDIT-06 R2 复算：极区实验 (1) SNAPSHOT.sha256 逐条校验 (2) leafmax_err 统计
(3) 争议字面量在跟踪结果件中的存在性。只读仓库，不执行仓库脚本。"""
import hashlib
import os
import re
import statistics
import subprocess
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = r"F:\Astro dev\Astro CS Normalization Database"
UNIT = os.path.join("实验", "healpix-polar")
RES = os.path.join(REPO, UNIT, "results")

# ---- (1) SNAPSHOT.sha256 ----
snap = os.path.join(RES, "SNAPSHOT.sha256")
lines = [ln for ln in open(snap, encoding="utf-8").read().splitlines() if ln.strip()]
print("SNAPSHOT 条目数 =", len(lines))
ok = bad = miss = 0
for ln in lines:
    h, _, rel = ln.partition("  ")
    rel = rel.strip()
    p = os.path.join(REPO, UNIT, rel.replace("/", os.sep))
    if not os.path.isfile(p):
        tracked = subprocess.run(["git", "-C", REPO, "ls-files", "--error-unmatch",
                                  os.path.join(UNIT, rel)], capture_output=True)
        miss += 1
        print("  MISSING(跟踪=%s): %s" % (tracked.returncode == 0, rel))
        continue
    d = hashlib.sha256(open(p, "rb").read()).hexdigest()
    if d == h.strip():
        ok += 1
    else:
        bad += 1
        print("  HASH-MISMATCH: %s (recorded %s… got %s…)" % (rel, h[:12], d[:12]))
print("校验结果: 命中 %d / 不符 %d / 缺件 %d  ⇒ %d/%d 通过" % (ok, bad, miss, ok, len(lines)))

# snapshot 覆盖度：results/ 与 code/ 下跟踪文件是否都在快照里
tracked = subprocess.run(["git", "-C", REPO, "ls-files", "--", UNIT],
                         capture_output=True, text=True, encoding="utf-8").stdout.splitlines()
in_snap = {ln.split("  ", 1)[1].strip() for ln in lines if "  " in ln}
rc = [t.split("%s/" % UNIT, 1)[1] for t in tracked if t.split("/")[-1].endswith((".cpp", ".h"))]
rr = [t.split("%s/" % UNIT, 1)[1] for t in tracked if t.split("/")[-1].endswith(".out")]
print("跟踪 code 文件 %d 个，其中在快照中 %d 个" % (len(rc), sum(1 for x in rc if x in in_snap)))
print("跟踪 results/*.out %d 个，其中在快照中 %d 个；不在的: %s"
      % (len(rr), sum(1 for x in rr if x in in_snap),
         [x for x in rr if x not in in_snap]))

# ---- (2) leafmax_err 统计 ----
col_stats = {}
for fn in ("t4_pole_full.csv", "t6_polar_200.csv"):
    p = os.path.join(RES, fn)
    if not os.path.isfile(p):
        continue
    rows = [ln.split(",") for ln in open(p, encoding="utf-8").read().splitlines()[1:] if ln.strip()]
    hdr = [ln for ln in open(p, encoding="utf-8").read().splitlines()[0].split(",")]
    i = hdr.index("leafmax_err")
    vals = []
    for r in rows:
        try:
            vals.append(float(r[i]))
        except ValueError:
            pass
    col_stats[fn] = vals
    print("%s: n=%d max=%.4g median=%.4g  (doc 主张 最坏 3.159e-07 / 中位 2.14e-08)"
          % (fn, len(vals), max(vals), statistics.median(vals)))
    # roracle 列（同为逐叶口径候选）
    if "roracle" in hdr:
        j = hdr.index("roracle")
        rv = [float(r[j]) for r in rows if r[j] not in ("", "0") and re.match(r"^[\d.eE+-]+$", r[j])]
        print("   roracle: n=%d max=%.4g median=%.4g" % (len(rv), max(rv), statistics.median(rv)))

# ---- (3) 争议字面量在跟踪结果件中的存在性 ----
tokens = ["6.96e-14", "1.18e-13", "2.15e-14", "2.148e-14", "1.896e-14", "2.14e-08", "441", "n=441"]
files = [f for f in sorted(os.listdir(RES)) if f.endswith((".out", ".csv"))]
for tk in tokens:
    where = []
    for f in files:
        txt = open(os.path.join(RES, f), encoding="utf-8", errors="replace").read()
        if tk in txt:
            where.append(f)
    print("字面量 %-12s 命中 results 文件 %d 个: %s" % (tk, len(where), where[:6]))
