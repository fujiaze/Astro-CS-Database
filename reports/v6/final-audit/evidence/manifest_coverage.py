#!/usr/bin/env python3
import json, csv, os
REPO="/workspace/Astro CS Database"
PKG=os.path.join(REPO,"工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915")
man=json.load(open(os.path.join(PKG,"TASK_MANIFEST.json")))
tasks=man["tasks"]
led={}
with open(os.path.join(PKG,"TASK_LEDGER.csv")) as f:
    for row in csv.DictReader(f): led[row["task_id"]]=row
print("manifest tasks:",len(tasks)," ledger rows:",len(led))
mid=[t["id"] for t in tasks]; lid=list(led.keys())
print("in manifest not ledger:", set(mid)-set(lid))
print("in ledger not manifest:", set(lid)-set(mid))
missing_ev=[]
for t in tasks:
    tid=t["id"]
    if tid not in led: continue
    ev=led[tid]["evidence"]
    paths=[p for p in ev.split(";") if p.strip()]
    for p in paths:
        ap=os.path.join(REPO,p.strip())
        if not os.path.exists(ap):
            missing_ev.append((tid,p.strip()))
print("missing evidence paths (not on disk now):", len(missing_ev))
for t,p in missing_ev: print("   MISSING",t,p)
# task spec files
spec_missing=[t["id"] for t in tasks if not os.path.exists(os.path.join(PKG,t["spec"]))]
print("missing task spec files:", spec_missing)
# write_scope existence check for code tasks
for t in tasks:
    ws=t.get("write_scope",[])
    for s in ws:
        ap=os.path.join(REPO,s.rstrip("/"))
        if not os.path.exists(ap):
            print("   scope path does not exist:",t["id"],s)
