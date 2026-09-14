import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
hits=[]
for f in files:
    if not f.endswith(".py"): continue
    if not (f.startswith("./tools/") or f.startswith("./ci/")): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if re.search(r"selfcheck|self_check|--selftest|def _self", txt, re.I):
        lines=txt.split(chr(10))
        for i,l in enumerate(lines):
            if re.search(r"selfcheck|self_check|selftest", l, re.I):
                hits.append((f,i+1,l.strip()[:130]))
print("tools/ci selfcheck mentions:",len(hits))
from collections import Counter
c=Counter(h[0] for h in hits)
for k,v in c.most_common(30): print("  %3d  %s"%(v,k))