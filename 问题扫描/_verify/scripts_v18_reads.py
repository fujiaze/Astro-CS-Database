
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
NAMES=["ASTROCS_AIO_FAULT","ASTROCS_AIO_SELFCHECK_FAULT","ASTROCS_HASH_FAIL_INJECT",
"ASTROCS_HIPS_DIAG_FAULT","ASTROCS_HIPS_PROV_FAULT","ASTROCS_HIPS_PUBLISH_FAULT","ASTROCS_HIPS_VERIFY_FAULT",
"ASTROCS_P1CAL_FAULT","ASTROCS_P1COS_FAULT","ASTROCS_P1DRZ_FAULT","ASTROCS_P1HIPS_FAULT","ASTROCS_P1HIPS_SELFCHECK_FAULT",
"ASTROCS_P1NOISE_FAULT","ASTROCS_P1PHOT_FAULT","ASTROCS_P1PHOT_SELFCHECK_FAULT","ASTROCS_P1PSF_FAULT",
"ASTROCS_P1SESS_FAULT","ASTROCS_P1SESS_SELFCHECK_FAULT","ASTROCS_P1STAR_FAULT","ASTROCS_P1STAR_SELFCHECK_FAULT_A",
"ASTROCS_P1STAR_SELFCHECK_FAULT_B","ASTROCS_P1WCS_FAULT","ASTROCS_P1WCS_SELFCHECK_FAULT_A","ASTROCS_P1WCS_SELFCHECK_FAULT_B",
"ASTROCS_P2002_FAULT","ASTROCS_P3002_FAULT","ASTROCS_P3PROJ_FAULT","ASTROCS_RT001_FAULT",
"P1CAL_SELFCHECK_FAULT","P1COS_SELFCHECK_FAULT","P1NOISE_SELFCHECK_FAULT","P1PSF_SELFCHECK_FAULT","P1DRZ_SELFCHECK_FAULT"]
res={}
for n in NAMES:
    reads=[];refs=[]
    for f in files:
        p=ROOT+"/"+f[2:]
        try: txt=open(p,encoding="utf-8",errors="replace").read()
        except: continue
        if n not in txt: continue
        for i,l in enumerate(txt.split("\n")):
            if n in l:
                if re.search(r'getenv\s*\(\s*"'+n, l) or ('getenv' in l and n in l): reads.append((f,i+1,l.strip()[:120]))
                else: refs.append((f,i+1,l.strip()[:120]))
    res[n]={"reads":reads,"refs":refs}
json.dump(res,open(ROOT+"/问题扫描/_verify/scripts_v18_reads.json","w"),ensure_ascii=False,indent=1)
for n in NAMES:
    d=res[n]
    prod=[x for x in d["reads"]+d["refs"] if x[0].startswith(("./lib/","./cli/","./include/","./runtime/","./modules/","./providers/"))]
    print("%-36s getenv_reads=%d  prod_hits=%d" % (n, len(d["reads"]), len(prod)))
    for f,ln,t in d["reads"]: print("      READ  ",f,":"+str(ln),"|",t[:110])
    for f,ln,t in prod[:4]: print("      PROD  ",f,":"+str(ln),"|",t[:110])
