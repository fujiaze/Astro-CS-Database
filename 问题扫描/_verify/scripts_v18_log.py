
import os,re
ROOT="/workspace/Astro CS Database"
txt=open(ROOT+"/tools/monitoring/check_log_contract.py",encoding="utf-8",errors="replace").read()
lines=txt.split(chr(10))
print("lines:",len(lines))
i=[k for k,l in enumerate(lines) if re.search(r"def .*selfcheck",l)]
if i:
    a=i[0]
    for k in range(a,min(a+40,len(lines))): print("%4d: %s"%(k+1,lines[k].rstrip()[:150]))
