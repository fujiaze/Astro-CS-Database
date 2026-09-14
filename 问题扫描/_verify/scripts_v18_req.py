
import os,re
ROOT="/workspace/Astro CS Database"
t=open(ROOT+"/tools/monitoring/check_log_contract.py",encoding="utf-8",errors="replace").read()
for m in re.finditer(r"REQUIRED_FIELDS\s*=\s*\[[^\]]*\]", t, re.S): print("checker REQUIRED_FIELDS @line",t[:m.start()].count(chr(10))+1,"\n",m.group(0)[:400])
le=open(ROOT+"/runtime/logging/log_event.py",encoding="utf-8",errors="replace").read()
for m in re.finditer(r"REQUIRED_FIELDS\s*=\s*\[[^\]]*\]", le, re.S): print("\nproducer REQUIRED_FIELDS @line",le[:m.start()].count(chr(10))+1,"\n",m.group(0)[:400])
import json
# compare
a=re.search(r"REQUIRED_FIELDS\s*=\s*\[([^\]]*)\]",t,re.S).group(1)
b=re.search(r"REQUIRED_FIELDS\s*=\s*\[([^\]]*)\]",le,re.S).group(1)
fa=[x.strip().strip(chr(34)) for x in a.split(",") if x.strip()]
fb=[x.strip().strip(chr(34)) for x in b.split(",") if x.strip()]
print("\nchecker==producer (as ordered list):",fa==fb,"  as set:",set(fa)==set(fb)," len",len(fa),len(fb))
print("schema file required:")
s=json.load(open(ROOT+"/runtime/logging/log_event_v1.schema.json",encoding="utf-8"))
print("  ",s.get("required"))
print("  schema==producer set:",set(s.get("required",[]))==set(fb))
