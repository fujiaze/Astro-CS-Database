# -*- coding: utf-8 -*-
import re, csv, os
ROOT="/workspace/Astro CS Database"
lines=open(os.path.join(ROOT,"tests/runtime/test_rt005_plan_estimator.py"),encoding="utf-8").read().split("\n")
seg="\n".join(lines[469:480])
print("--- 原文 470-480 ---"); print(seg)
m=re.search(r'_KNOWN_KERNELS = \{(.*?)\n\}', "\n".join(lines), re.S)
known=set(re.findall(r'"([^"]+)"', m.group(1)))
alt=set(re.findall(r'([a-z][a-z0-9]*-[a-z0-9\-]+)', seg[seg.index("finditer"):]))
print("\nknown(%d)=%s"%(len(known),sorted(known)))
print("alt(%d)   =%s"%(len(alt),sorted(alt)))
print("两集合相同?", known==alt)
inc=open(os.path.join(ROOT,"lib/backend_host/backend_table.inc"),encoding="utf-8").read()
real=re.findall(r'ACS_KERNEL_ENTRY\("([^"]+)",\s*"([^"]+)"', inc)
print("\n.inc 真实条目数:", len(real))
print(".inc kernel 名:", sorted(n for _,n in real))
print(".inc ALG id:", sorted({a for a,_ in real}))
print("known - inc:", known - {n for _,n in real}, " inc-known:", {n for _,n in real}-known)

rows=list(csv.reader(open(os.path.join(ROOT,"evidence/v6_1_rework/TASK_LEDGER.csv"),encoding="utf-8")))
hdr=rows[0]
print("\nTASK_LEDGER 表头:", hdr)
for r_ in rows[1:]:
    if r_ and r_[0]=="P3-006":
        print("P3-006 行:", dict(zip(hdr,r_)))
PY3=None
