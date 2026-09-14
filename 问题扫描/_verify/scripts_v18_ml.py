
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
MACS=r"P1HIPS_CHECK(?:_MSG|_EQ)?|P1CAL_CHECK(?:_EQ)?|P1COS_CHECK(?:_EQ)?|P1PSF_CHECK(?:_MSG|_EQ)?|P1DRZ_CHECK(?:_MSG)?|P1SESS_CHECK(?:_MSG|_EQ)?|P1PHOT_CHECK(?:_MSG|_NEAR)?|P1NOISE_CHECK(?:_EQ)?|P1STAR_CHECK(?:_EQ)?|AIO_CHECK(?:_MSG)?|P1WCS_CHECK(?:_NEAR)?"
pat=re.compile(r"\b("+MACS+r")\s*\(")
def split_args(s):
    out=[];d=0;cur=""
    for ch in s:
        if ch in "([{": d+=1
        elif ch in ")]}": d-=1
        if ch=="," and d==0: out.append(cur.strip()); cur=""
        else: cur+=ch
    if cur.strip(): out.append(cur.strip())
    return out
fam={}; sites=0; single=0
for f in files:
    if not re.search(r"\.(cpp|hpp)$",f): continue
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    t=re.sub(r"^[ \t]*#[ \t]*define[^\n]*","",t,flags=re.M)   # drop macro defs
    for m in pat.finditer(t):
        mac=m.group(1); famkey=mac.split("_CHECK")[0]
        # balanced paren scan across newlines
        d=1; buf=""
        for ch in t[m.end():]:
            if ch=="(": d+=1
            elif ch==")":
                d-=1
                if d==0: break
            buf+=ch
        args=split_args(buf)
        idx=3 if mac.endswith("_NEAR") else 2
        if len(args)<=idx: continue
        mm=re.search(r'"([^"]+)"',args[idx])
        if mm:
            fam.setdefault(famkey,set()).add(mm.group(1)); sites+=1
            if chr(10) not in buf: single+=1
tot=set()
for k,v in fam.items(): tot|=v
print("MULTI-LINE-AWARE  distinct injection names:",len(tot),"  CHECK sites w/ literal name:",sites,"  of which single-line:",single)
print("per family:",{k:len(v) for k,v in sorted(fam.items())})
json.dump({k:sorted(v) for k,v in fam.items()},open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames_ml.json","w"),ensure_ascii=False,indent=1)
# three-state again
occ={}
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    for n in tot:
        if chr(34)+n+chr(34) in t: occ.setdefault(n,set()).add(f)
DRV=re.compile(r"selfcheck|CMakeLists\.txt|(^|/)ci/")
cm=[]; sc=[]; none=[]
for n in sorted(tot):
    fs=occ.get(n,set())
    d=[f for f in fs if DRV.search(f)]
    cml=[f for f in d if ("CMakeLists" in f or f.startswith("./ci/"))]
    scl=[f for f in d if "selfcheck" in f]
    if cml: cm.append((n,cml))
    elif scl: sc.append((n,scl))
    else: none.append(n)
print()
print("三态（多行口径）: CI/CMake ENVIRONMENT 点名 = %d | 仅 selfcheck TU 提及 = %d | 无人驱动 = %d"%(len(cm),len(sc),len(none)))
for n,c in cm: print("   CI/ENV:",n,c)
print("   selfcheck-only names:",[n for n,_ in sc])
