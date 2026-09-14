import os,re
root="/workspace/Astro CS Database/问题扫描"
for dp,dn,fn in os.walk(root):
    for f in fn:
        p=os.path.join(dp,f)
        try: s=open(p,encoding="utf-8",errors="ignore").read()
        except Exception: continue
        for i,l in enumerate(s.splitlines(),1):
            if "IPV_PIPELINE" in l:
                print(f"{os.path.relpath(p,root)}:{i}: {l.strip()[:180]}")
