
import os,re,json
ROOT="/workspace/Astro CS Database"
print("=== (b) P1WCS_CHECK_NEAR call sites: faultname arg ===")
n=0; nul=0
for f in json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8")):
    if not re.search(r"\.(cpp|hpp)$",f): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if "P1WCS_CHECK_NEAR" not in txt: continue
    for i,l in enumerate(txt.split(chr(10))):
        if "P1WCS_CHECK_NEAR" in l and "define" not in l:
            n+=1
            if re.search(r"nullptr|NULL", l): nul+=1; print("   NULL:",f+":"+str(i+1),l.strip()[:120])
print("   sites=",n," with nullptr=",nul)
print()
print("=== (c) is tests/monitoring/test_log_contract.py registered? ===")
for idx in ["tests/test_index.csv","ci/ctest_baseline.json"]:
    p=os.path.join(ROOT,idx)
    if not os.path.exists(p): print("  MISS",idx); continue
    s=open(p,encoding="utf-8",errors="replace").read()
    hits=[l.strip()[:160] for l in s.split(chr(10)) if "log_contract" in l or "monitor_contract" in l]
    print("  ",idx,"hits:",len(hits))
    for h in hits[:6]: print("      ",h)
print()
print("=== (d) p1drz: names in CHECK sites vs k_injections table ===")
fn=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames.json",encoding="utf-8"))
drz=set(fn.get("P1DRZ",[]))
sc=open(ROOT+"/lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp",encoding="utf-8",errors="replace").read()
tbl=set(re.findall(r'\{"(\w+)"\}', sc))
print("   registry names (P1DRZ_CHECK 3rd arg):",sorted(drz))
print("   k_injections table:",sorted(tbl))
print("   in registry but NEVER driven:",sorted(drz-tbl))
print("   in table but not registry:",sorted(tbl-drz))
