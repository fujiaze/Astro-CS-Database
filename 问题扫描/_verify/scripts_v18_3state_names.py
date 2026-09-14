
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
MACS=["P1HIPS_CHECK_MSG","P1HIPS_CHECK","P1HIPS_CHECK_EQ","P1CAL_CHECK_EQ","P1CAL_CHECK","P1COS_CHECK_EQ","P1COS_CHECK",
"P1PSF_CHECK_MSG","P1PSF_CHECK","P1PSF_CHECK_EQ","P1DRZ_CHECK_MSG","P1DRZ_CHECK","P1SESS_CHECK_MSG","P1SESS_CHECK","P1SESS_CHECK_EQ",
"P1PHOT_CHECK_MSG","P1PHOT_CHECK","P1PHOT_CHECK_NEAR","P1NOISE_CHECK_EQ","P1NOISE_CHECK","P1STAR_CHECK_EQ","P1STAR_CHECK",
"AIO_CHECK_MSG","AIO_CHECK","P1WCS_CHECK_NEAR","P1WCS_CHECK"]
pat=re.compile(r'\b('+'|'.join(MACS)+r')\s*\(')
def split_args(s):
    out=[];d=0;cur=""
    for ch in s:
        if ch in "([{": d+=1
        elif ch in ")]}": d-=1
        if ch=="," and d==0: out.append(cur.strip()); cur=""
        else: cur+=ch
    if cur.strip(): out.append(cur.strip())
    return out
fam={}
sites={}
for f in files:
    if not re.search(r"\.(cpp|hpp)$",f): continue
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    if "#define" in t and "P1PSF_CHECK" in f: pass
    for i,l in enumerate(t.split(chr(10))):
        if "#define" in l: continue
        m=pat.search(l)
        if not m: continue
        mac=m.group(1)
        famkey=mac.split("_CHECK")[0]
        depth=1; buf=""
        for ch in l[m.end():]:
            if ch=="(": depth+=1
            elif ch==")":
                depth-=1
                if depth==0: break
            buf+=ch
        args=split_args(buf)
        idx = 3 if (mac.endswith("_NEAR")) else 2
        if len(args)<=idx: continue
        mm=re.search(r'"([^"]+)"',args[idx])
        if mm:
            fam.setdefault(famkey,set()).add(mm.group(1)); sites.setdefault(famkey,0)
            sites[famkey]+=1
allnames=set()
for k,v in fam.items(): allnames|=v
print("families:",{k:len(v) for k,v in sorted(fam.items())})
print("TOTAL distinct registered injection names:",len(allnames),"  CHECK sites w/ literal name:",sum(sites.values()))
occ={}
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    for n in allnames:
        if chr(34)+n+chr(34) in t: occ.setdefault(n,set()).add(f)
DRV=re.compile(r"selfcheck|CMakeLists\.txt|(^|/)ci/|checks\.json|baseline")
res={"ci_or_cmake_driven":[],"test_selfcheck_driven":[],"nobody_driven":[]}
for n in sorted(allnames):
    fs=occ.get(n,set())
    d=[f for f in fs if DRV.search(f)]
    sc=[f for f in d if "selfcheck" in f]
    cm=[f for f in d if ("CMakeLists" in f or "/ci/" in f or "checks.json" in f)]
    if cm: res["ci_or_cmake_driven"].append((n,cm))
    elif sc: res["test_selfcheck_driven"].append((n,sc))
    else: res["nobody_driven"].append(n)
print()
print("三态（按名，口径=带引号字面量在全仓 tracked 文件出现处）：")
print("  A. CI/checks.json 或 ctest ENVIRONMENT 点名驱动 :",len(res["ci_or_cmake_driven"]))
for n,c in res["ci_or_cmake_driven"]: print("      ",n,c)
print("  B. 仅被某个 *_selfcheck* TU 字面提及（测试驱动）  :",len(res["test_selfcheck_driven"]))
for n,c in res["test_selfcheck_driven"][:30]: print("      ",n,"|",os.path.basename(c[0]))
print("  C. 无人驱动（只在注册它的 CHECK 行出现）        :",len(res["nobody_driven"]))
json.dump(res,open(ROOT+"/问题扫描/_verify/scripts_v18_3state_names.json","w"),ensure_ascii=False,indent=1)
print()
print("names with zero quoted occurrence outside their own file:",len([n for n in allnames if len(occ.get(n,set()))<=1]))
