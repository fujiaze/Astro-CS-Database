
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
# macro -> index of the fault/name argument
SPEC={
 "P1HIPS_CHECK":2,"P1HIPS_CHECK_MSG":2,"P1HIPS_CHECK_EQ":None,
 "P1CAL_CHECK":2,"P1CAL_CHECK_EQ":2,
 "P1COS_CHECK":2,"P1COS_CHECK_EQ":2,
 "P1PSF_CHECK":2,"P1PSF_CHECK_MSG":2,"P1PSF_CHECK_EQ":None,
 "P1DRZ_CHECK":2,"P1DRZ_CHECK_MSG":2,
 "P1SESS_CHECK":2,"P1SESS_CHECK_MSG":2,"P1SESS_CHECK_EQ":None,
 "P1PHOT_CHECK":2,"P1PHOT_CHECK_MSG":2,"P1PHOT_CHECK_NEAR":4,
 "P1NOISE_CHECK":2,"P1NOISE_CHECK_EQ":None,
 "P1STAR_CHECK":2,"P1STAR_CHECK_EQ":3,
 "AIO_CHECK":2,"AIO_CHECK_MSG":2,
 "P1WCS_CHECK":2,"P1WCS_CHECK_NEAR":4,
 "P2H_CHECK":None,
}
names=sorted(SPEC,key=len,reverse=True)
pat=re.compile(r"\b("+"|".join(names)+r")\s*\(")
def split_args(s):
    out=[];d=0;cur=""
    for ch in s:
        if ch in "([{": d+=1
        elif ch in ")]}": d-=1
        if ch=="," and d==0: out.append(cur.strip()); cur=""
        else: cur+=ch
    if cur.strip(): out.append(cur.strip())
    return out
fam={}; sites=0; varnames=[]
for f in files:
    if not re.search(r"\.(cpp|hpp)$",f): continue
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    t=re.sub(r"^[ \t]*#[ \t]*define[^\n]*","",t,flags=re.M)
    for m in pat.finditer(t):
        mac=m.group(1); idx=SPEC[mac]
        if idx is None: continue
        d=1; buf=""
        for ch in t[m.end():]:
            if ch=="(": d+=1
            elif ch==")":
                d-=1
                if d==0: break
            buf+=ch
        args=split_args(buf)
        if len(args)<=idx: continue
        a=args[idx]; mm=re.search(r'"([^"]+)"',a)
        fk=mac.split("_CHECK")[0]
        if mm:
            fam.setdefault(fk,set()).add(mm.group(1)); sites+=1
        elif re.match(r"^[A-Za-z_]\w*$",a):
            varnames.append((f,t[:m.start()].count(chr(10))+1,mac,a))
tot=set()
for v in fam.values(): tot|=v
print("=== corrected census (HEAD d8726fd7, 2026-09-15) ===")
print("distinct registered fault NAMES:",len(tot))
print("CHECK sites carrying a literal name:",sites)
print("per family:",{k:len(v) for k,v in sorted(fam.items())})
print()
print("sites where the name argument is a VARIABLE not a literal:",len(varnames))
seen=set()
for f,l,mac,a in varnames:
    if (f,a) in seen: continue
    seen.add((f,a)); print("   ",f+":"+str(l),mac,"name-arg =",a)
occ={}
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    for n in tot:
        if chr(34)+n+chr(34) in t: occ.setdefault(n,set()).add(f)
DRV=re.compile(r"selfcheck|CMakeLists\.txt|(^|/)ci/")
cm=[];sc=[];no=[]
for n in sorted(tot):
    fs=occ.get(n,set()); d=[f for f in fs if DRV.search(f)]
    c=[x for x in d if ("CMakeLists" in x or x.startswith("./ci/"))]
    s=[x for x in d if "selfcheck" in x]
    (cm if c else sc if s else no).append((n,(c or s)))
print()
print("三态：CI/CMake 点名 %d | 仅 selfcheck TU 点名 %d | 无人驱动 %d | 合计 %d"%(len(cm),len(sc),len(no),len(cm)+len(sc)+len(no)))
print("  selfcheck-driven:",[x for x,_ in sc])
print("  names with zero quoted hit outside their own file:",len([n for n in tot if len(occ.get(n,set()))<=1]))
json.dump({"families":{k:sorted(v) for k,v in fam.items()},"total":sorted(tot),"sc":[x for x,_ in sc],"none":[x for x in no]},open(ROOT+"/问题扫描/_verify/scripts_v18_final_census.json","w"),ensure_ascii=False,indent=1)
