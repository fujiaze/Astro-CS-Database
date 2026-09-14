import os,re
root="/workspace/Astro CS Database/问题扫描"
keys=["docs/algorithms/IPV_PIPELINE","compute_initial_mag_cut","n_target_cap","0\.2885","α=1.3","m0_offset","IPV_PIPELINE","1\.1107207345395915","AE_C45","bbox_intersects","1\.2 裕量","cos_dec","kQueryCacheCap","307,200,000","307200000","kMon001UtilSampleFrac","0\.70","kTrimMeanToSigma","0\.7316728","1\.230310"]
pat=re.compile("|".join(keys))
hits=0
for dp,dn,fn in os.walk(root):
    if "_cache" in dp or ".git" in dp: continue
    for f in fn:
        p=os.path.join(dp,f)
        try: s=open(p,encoding="utf-8",errors="ignore").read()
        except Exception: continue
        for i,l in enumerate(s.splitlines(),1):
            if pat.search(l):
                rel=os.path.relpath(p,root)
                if rel.startswith("_verify/V12"): continue
                print(f"{rel}:{i}: {l.strip()[:180]}"); hits+=1
print("HITS",hits)
