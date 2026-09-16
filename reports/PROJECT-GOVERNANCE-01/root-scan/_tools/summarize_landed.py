#!/usr/bin/env python3
"""ROOT-004: summarize landed shard PSVs (verdict x priority x category). Read-only."""
import csv, glob, os, collections, json
ROOT="/workspace/Astro CS Database"
files=sorted(glob.glob(os.path.join(ROOT,"reports/PROJECT-GOVERNANCE-01/root-scan/shards/*.psv")))
ver=collections.Counter(); vp=collections.Counter(); vc=collections.Counter(); per={}; bad=[]; ids=[]
for f in files:
    name=os.path.basename(f)[:-4]; rows=[]
    for ln in open(f,encoding="utf-8").read().split("\n"):
        if not ln.strip(): continue
        p=ln.split("|")
        if len(p)!=10: bad.append((name,len(p))); continue
        rows.append(p)
    rows=rows[1:]  # drop header
    per[name]=len(rows)
    for r in rows:
        ids.append(r[0]); ver[r[6]]+=1; vp[(r[6],r[2])]+=1; vc[(r[6],r[1])]+=1
print("shards_landed:",len(files),"entries:",len(ids),"| malformed_rows_skipped:",collections.Counter(n for n,_ in bad))
print("VERDICT:",dict(ver))
print("VERDICT x PRIORITY:",dict(vp))
print("VERDICT x CATEGORY:",dict(vc))
print("P0 land:",{k:v for k,v in vp.items() if k[1]=="P0"})
print("dup IDs:",[k for k,v in collections.Counter(ids).items() if v>1][:10])
json.dump({"per_shard":per,"verdict":dict(ver),"verdict_priority":{str(k):v for k,v in vp.items()},"verdict_category":{str(k):v for k,v in vc.items()}},
          open(os.path.join(ROOT,"reports/PROJECT-GOVERNANCE-01/root-scan/_gen/landed_stats.json"),"w",encoding="utf-8"),ensure_ascii=False,indent=1)
