
import json,re,os
ROOT="/workspace/Astro CS Database"
cj=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
for c in cj:
    if c["id"] in ("UT-CLI",): print("UT-CLI:",json.dumps(c,ensure_ascii=False)[:400])
print()
kf=json.load(open(ROOT+"/ci/known_failures.json",encoding="utf-8"))
print("known_failures keys:",list(kf)[:6])
arr=kf.get("entries") or kf.get("known_failures") or kf
if isinstance(arr,dict): arr=arr.get("entries",[])
for e in arr:
    s=json.dumps(e,ensure_ascii=False)
    if "UT-CLI" in s or "phase123" in s or "PRODUCTION-GRAPH" in s: print("  KF:",s[:300])
