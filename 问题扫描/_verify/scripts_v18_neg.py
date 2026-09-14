
import os,re
ROOT="/workspace/Astro CS Database"
t=open(ROOT+"/tools/monitoring/check_log_contract.py",encoding="utf-8",errors="replace").read()
lines=t.split(chr(10))
i=[k for k,l in enumerate(lines) if re.search(r"def check_schema_self",l)][0]
for k in range(i,min(i+30,len(lines))): print("%4d: %s"%(k+1,lines[k].rstrip()[:140]))
print()
print("=== any NEGATIVE case inside selfcheck (expect reject)? ===")
j=[k for k,l in enumerate(lines) if re.search(r"def selfcheck",l)][0]
body=chr(10).join(lines[j:j+35])
print("  mentions bad/invalid/负例/must_reject:",re.findall(r"bad_|invalid|负例|必拒|expect_fail|neg", body))
