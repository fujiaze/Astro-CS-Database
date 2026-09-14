
import os,re
ROOT="/workspace/Astro CS Database"
txt=open(ROOT+"/tools/quality/contracts/check_full_integration.py",encoding="utf-8",errors="replace").read()
lines=txt.split(chr(10))
print("total lines:",len(lines))
for i,l in enumerate(lines):
    if re.search(r"HARDCODE_THREAD_DEBT_CAP|debt_count|FORBID-HARDCODE-THREADS", l):
        print("   %4d: %s"%(i+1,l.rstrip()[:150]))
print()
print("--- context around the debt_count verdict ---")
idx=[i for i,l in enumerate(lines) if "debt_count" in l]
if idx:
    a=max(0,idx[0]-12); b=min(len(lines),idx[-1]+14)
    for k in range(a,b): print("%4d: %s"%(k+1,lines[k].rstrip()[:150]))
