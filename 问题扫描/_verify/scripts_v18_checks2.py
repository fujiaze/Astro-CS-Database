
import json,re,os
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
for c in d:
    if "CTEST" in c.get("id","") or "ctest" in json.dumps(c).lower():
        pass
print("=== all CTEST* checks ===")
for c in d:
    if c.get("id","").startswith("CTEST"):
        print(" ",c["id"],"| profiles:",c.get("profiles"),"| waivable:",c.get("waivable"),"| env:",c.get("env") or c.get("environment"),"| cmd:",json.dumps(c.get("command"))[:200])
print()
print("=== keys used across checks ===")
ks=set()
for c in d: ks|=set(c.keys())
print(sorted(ks))
