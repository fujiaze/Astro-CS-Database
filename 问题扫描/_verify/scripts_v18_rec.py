
import json,os,re
ROOT="/workspace/Astro CS Database"
fn=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames_ml.json",encoding="utf-8"))
for fam,q in [("P1PSF","recovery"),("P1STAR","f1_detect_rc"),("P1STAR","f4_oracle_rc"),("P1CAL","const_field_bitwise"),("P1DRZ","perf_parity")]:
    print("%-8s %-24s registered? %s"%(fam,q,q in fn.get(fam,[])))
print()
print("P1PSF names sample:",sorted(fn.get("P1PSF",[]))[:14])
print()
# where is recovery used as a P1PSF_CHECK name?
for f in json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8")):
    if "/p1psf/" not in f: continue
    t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    for i,l in enumerate(t.split(chr(10))):
        if re.search(r'"recovery"',l): print("  ",f+":"+str(i+1),"|",l.strip()[:140])
