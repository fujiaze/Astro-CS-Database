
import os,re,json,subprocess
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
PAT=re.compile(r'\b(?:ASTROCS_)?[A-Z0-9]+(?:_[A-Z0-9]+)*_(?:FAULT|DIAG|PROV|VERIFY|INJECT)[A-Z0-9_]*\b')
FAULTEXACT=re.compile(r'\bASTROCS_[A-Za-z0-9_]*FAULT[A-Za-z0-9_]*\b')
INJECT=re.compile(r'\b[A-Za-z0-9_]*INJECT[A-Za-z0-9_]*\b')
from collections import defaultdict
occ=defaultdict(list)   # name -> [(file,line,kind)]
for f in files:
    p=ROOT+"/"+f[2:]
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except Exception: continue
    for i,l in enumerate(txt.split("\n")):
        names=set(PAT.findall(l)) | set(FAULTEXACT.findall(l))
        for n in INJECT.findall(l):
            if re.search(r'FAULT|INJECT',n): names.add(n)
        for n in names:
            if len(n)<6: continue
            occ[n].append((f,i+1,l.strip()[:160]))
json.dump({k:v for k,v in occ.items()}, open(ROOT+"/问题扫描/_verify/scripts_v18_names.json","w"), ensure_ascii=False)
# classify env-var names (uppercase with underscore, look like env) vs C identifiers
def is_envish(n): return n.isupper() and n.startswith(("ASTROCS_","ACS_","P1","AIO","HIPS"))
envnames=sorted([n for n in occ if is_envish(n)])
print("distinct names:",len(occ)," env-like:",len(envnames))
for n in envnames:
    kinds=set()
    for f,ln,txt in occ[n]:
        if 'getenv' in txt or 'env' in txt.lower(): kinds.add('read')
        if 'add_test' in txt or 'ENVIRONMENT' in txt: kinds.add('cmake')
    fl=sorted(set(f for f,_,_ in occ[n]))
    print("%-42s occ=%3d files=%d  %s" % (n, len(occ[n]), len(fl), fl[:3]))
