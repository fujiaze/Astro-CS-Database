import os,re
root="/workspace/Astro CS Database/问题扫描"
for dp,dn,fn in os.walk(root):
    if "_cache" in dp: continue
    for f in fn:
        p=os.path.join(dp,f)
        try: s=open(p,encoding="utf-8",errors="ignore").read()
        except Exception: continue
        for i,l in enumerate(s.splitlines(),1):
            if "docs_machine_consistency" in l or "snr_constants" in l:
                rel=os.path.relpath(p,root)
                if rel.startswith("_verify/scripts") or rel.startswith("_verify/_v"): continue
                print(f"{rel}:{i}: {l.strip()[:175]}")
