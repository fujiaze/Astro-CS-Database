
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
fn=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames.json",encoding="utf-8"))
allnames=set()
for v in fn.values(): allnames|=set(v)
# where does P1PSF appear?
cnt=0
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    if "P1PSF_CHECK" in t: cnt+=1; print("P1PSF_CHECK file:",f,t.count("P1PSF_CHECK"))
print("P1PSF files:",cnt)
print()
# build: name -> files containing the quoted literal
occ={}
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    for n in allnames:
        if chr(34)+n+chr(34) in t: occ.setdefault(n,set()).add(f)
# classify: registered-in-CHECK-file only  vs also mentioned in a driver (selfcheck cpp / CMakeLists / ci/)
drv=re.compile(r"selfcheck|CMakeLists|^[^]*/ci/|^\./ci/|checks\.json")
rows=[]
for n in allnames:
    fs=occ.get(n,set())
    drivers=[f for f in fs if drv.search(f)]
    rows.append((n,sorted(fs),sorted(drivers)))
nodrv=[r for r in rows if not r[2]]
print("injection names total:",len(allnames))
print("names with a quoted-literal occurrence SOMEWHERE:",len(occ))
print("names with NO occurrence in any selfcheck TU / CMakeLists / ci/:",len(nodrv))
print("names appearing ONLY in their own CHECK file:",sum(1 for r in rows if len(r[1])==1))
print()
print("sample of names with zero driver mentions (10):")
for r in nodrv[:10]: print("   ",r[0],r[1][:2])
json.dump({"all":sorted(allnames),"no_driver":sorted(x[0] for x in nodrv),"only_one_file":sorted(r[0] for r in rows if len(r[1])==1)},open(ROOT+"/问题扫描/_verify/scripts_v18_driven.json","w"),ensure_ascii=False,indent=1)
