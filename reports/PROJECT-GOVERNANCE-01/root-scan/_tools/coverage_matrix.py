#!/usr/bin/env python3
# coverage_matrix.py - ROOT-004 W2 coverage merger
import os, re, sys, glob, collections
root="/workspace/Astro CS Database"
plan={}
for l in open(os.path.join(root,"reports/PROJECT-GOVERNANCE-01/root-scan/_gen/coverage_plan.tsv"),encoding="utf-8"):
    l=l.rstrip(chr(10))
    if not l.strip(): continue
    ax,f=l.split(chr(9),1); plan.setdefault(f,set()).add(ax)
covers={}; row=re.compile(r"^(.+?)"+chr(9)+r"((?:OK|FINDING:|NA:).*)$")
for md in sorted(glob.glob(os.path.join(root,"reports/PROJECT-GOVERNANCE-01/root-scan/audit/w2/*.md"))):
    ax=os.path.basename(md)[:-3]; n=0
    for l in open(md,encoding="utf-8"):
        m=row.match(l.rstrip(chr(10)))
        if m:
            f=m.group(1).strip().strip("-").strip().strip(chr(96)).strip()
            if (chr(47) in f or chr(46) in f) and not f.startswith("|"):
                covers.setdefault(f,set()).add(ax); n+=1
    print("axis",ax,"manifest rows parsed",n)
missing=sorted(set(plan)-set(covers)); extra=sorted(set(covers)-set(plan))
print("planned",len(plan),"covered",len(covers),"missing",len(missing),"extra",len(extra))
out=["# COVERAGE_MATRIX - ROOT-004 文件级覆盖合并表（自动生成）","",
 "计划 "+str(len(plan))+" 文件；进入任一轴覆盖清单 "+str(len(covers))+"；缺 "+str(len(missing))+"；计划外新增 "+str(len(extra))+"。","",
 "## 缺口（必须补交/补派）",""]
out+= ["- "+f+" （计划轴 "+",".join(sorted(plan[f]))+"）" for f in missing] or ["（空）"]
out+=["","## 计划外新增（窗口内并行入库，收口后补扫）",""]
out+=["- "+f for f in extra] or ["（空）"]
dst=os.path.join(root,"reports/PROJECT-GOVERNANCE-01/root-scan/audit/COVERAGE_MATRIX.md")
open(dst,"w",encoding="utf-8").write(chr(10).join(out)+chr(10))
open(os.path.join(root,"reports/PROJECT-GOVERNANCE-01/root-scan/_gen/coverage_missing.tsv"),"w",encoding="utf-8").write(chr(10).join(missing)+chr(10))
print("WROTE",dst)
sys.exit(0 if not missing else 1)