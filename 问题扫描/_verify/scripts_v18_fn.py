
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
MACROS=["P1HIPS_CHECK","P1HIPS_CHECK_MSG","P1HIPS_CHECK_EQ","P1CAL_CHECK","P1CAL_CHECK_EQ","P1COS_CHECK","P1COS_CHECK_EQ",
"P1PSF_CHECK","P1PSF_CHECK_MSG","P1DRZ_CHECK","P1DRZ_CHECK_MSG","P1SESS_CHECK","P1SESS_CHECK_MSG","P1PHOT_CHECK","P1PHOT_CHECK_MSG","P1PHOT_CHECK_NEAR",
"P1NOISE_CHECK","P1WCS_CHECK","P1WCS_CHECK_NEAR","AIO_CHECK","AIO_CHECK_MSG","P1STAR_CHECK","P1STAR_CHECK_EQ","P2H_CHECK"]
from collections import defaultdict
fam=defaultdict(set)   # macro family -> fault names
sites=defaultdict(list)
pat=re.compile(r'\b('+'|'.join(sorted(MACROS,key=len,reverse=True))+r')\s*\(')
def split_args(s):
    out=[];d=0;cur=""
    for ch in s:
        if ch in "([{":d+=1
        if ch in ")]}":d-=1
        if ch=="," and d==0:
            out.append(cur.strip());cur=""
        else: cur+=ch
    if cur.strip(): out.append(cur.strip())
    return out
for f in files:
    if not re.search(r'\.(cpp|hpp|c|h)$',f): continue
    p=ROOT+"/"+f[2:]
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    for i,l in enumerate(txt.split("\n")):
        m=pat.search(l)
        if not m: continue
        name=m.group(1)
        inner=l[m.end():]
        depth=1; buf=""
        for ch in inner:
            if ch=="(":depth+=1
            elif ch==")":
                depth-=1
                if depth==0: break
            buf+=ch
        args=split_args(buf)
        fn=None
        if name.endswith("_EQ") or "P2H" in name: pass
        elif name.endswith("_NEAR") and "WCS" in name: fn=args[4] if len(args)>4 else None
        elif "CHECK_MSG" in name or "CHECK_NEAR" in name: fn=args[2] if len(args)>2 else None
        else: fn=args[2] if len(args)>2 else (args[-1] if len(args)>=3 else None)
        if fn:
            mm=re.search(r'"([^"]+)"',fn)
            if mm: fam[name.rsplit("_CHECK",1)[0]].add(mm.group(1)); sites[name].append((f,i+1,mm.group(1)))
        else:
            if len(args)>=3 and re.search(r'nullptr|NULL',args[2] if len(args)>2 else ""):
                fam[name].add("<nullptr>")
json.dump({k:sorted(v) for k,v in fam.items()}, open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames.json","w"),ensure_ascii=False,indent=1)
tot=set()
for k,v in fam.items():
    real={x for x in v if x!="<nullptr>"}
    tot|=real
    print("%-14s names=%3d  %s" % (k, len(real), sorted(real)[:14]))
print("TOTAL distinct fault names (all families):", len(tot))
print("total CHECK sites with a literal fault name:", sum(len(v) for v in sites.values()))
