
import json,re,os
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))
items = d if isinstance(d,list) else d.get("checks",d)
print("type:",type(d).__name__, "keys:", list(d)[:8] if isinstance(d,dict) else len(d))
arr = d if isinstance(d,list) else None
if arr is None:
    for k,v in d.items():
        if isinstance(v,list): arr=v; print("list key:",k,len(v)); break
envhit=[]
for c in arr:
    s=json.dumps(c,ensure_ascii=False)
    for m in re.findall(r'ASTROCS_[A-Z0-9_]+', s):
        envhit.append((c.get("id"), m))
from collections import defaultdict
g=defaultdict(set)
for cid,m in envhit: g[m].add(cid)
print("--- ASTROCS_* env referenced in ci/checks.json ---")
for m in sorted(g): print("  %-40s %s" % (m, sorted(g[m])))
# find checks with 'FAULT' or 'SELFCHECK' in id
print("--- checks whose id/command mentions FAULT/SELFCHECK/INJECT ---")
for c in arr:
    if re.search(r'FAULT|SELFCHECK|INJECT', (c.get("id","")+json.dumps(c,ensure_ascii=False)), re.I):
        cmd=c.get("command") or c.get("cmd")
        print("  ",c.get("id"), "| profiles:",c.get("profiles"), "| waivable:",c.get("waivable"), "| cmd:", (json.dumps(cmd,ensure_ascii=False)[:220] if cmd else None))
