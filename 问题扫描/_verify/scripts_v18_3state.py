
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
NAMES=["ASTROCS_AIO_FAULT","ASTROCS_AIO_SELFCHECK_FAULT","ASTROCS_HASH_FAIL_INJECT","ASTROCS_HIPS_DIAG_FAULT",
"ASTROCS_HIPS_PROV_FAULT","ASTROCS_HIPS_PUBLISH_FAULT","ASTROCS_HIPS_VERIFY_FAULT","ASTROCS_P1CAL_FAULT",
"ASTROCS_P1COS_FAULT","ASTROCS_P1DRZ_FAULT","ASTROCS_P1HIPS_FAULT","ASTROCS_P1HIPS_SELFCHECK_FAULT","ASTROCS_P1NOISE_FAULT",
"ASTROCS_P1PHOT_FAULT","ASTROCS_P1PHOT_SELFCHECK_FAULT","ASTROCS_P1PSF_FAULT","ASTROCS_P1SESS_FAULT","ASTROCS_P1SESS_SELFCHECK_FAULT",
"ASTROCS_P1STAR_FAULT","ASTROCS_P1STAR_SELFCHECK_FAULT_A","ASTROCS_P1STAR_SELFCHECK_FAULT_B","ASTROCS_P1WCS_FAULT",
"ASTROCS_P1WCS_SELFCHECK_FAULT_A","ASTROCS_P1WCS_SELFCHECK_FAULT_B","ASTROCS_P2002_FAULT","ASTROCS_P3002_FAULT",
"ASTROCS_P3PROJ_FAULT","ASTROCS_RT001_FAULT","P1CAL_SELFCHECK_FAULT","P1COS_SELFCHECK_FAULT","P1NOISE_SELFCHECK_FAULT","P1PSF_SELFCHECK_FAULT"]
PROD=re.compile(r'^\./(lib|cli|include|runtime|providers|modules|cmake|contracts|ci)/')
TEST=re.compile(r'^\./(tests/|lib/[^ ]*/tests/|modules/[^ ]*/tests/)')
def isprod(f):
    return bool(PROD.match(f)) and not TEST.match(f) and '/tests/' not in f and not re.search(r'_test\.(cpp|c|hpp|py)$',f) and 'selfcheck' not in f and 'test_' not in os.path.basename(f)
# CI-driven? checks.json never references ASTROCS_*  -> confirm
cj=open(ROOT+"/ci/checks.json",encoding="utf-8").read()
cienv={n for n in NAMES if n in cj}
# ctest ENVIRONMENT-driven? collect from cmake sources
envstr=""
for f in files:
    if f.endswith("CMakeLists.txt") or f.endswith(".cmake"):
        try: envstr+=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
        except: pass
cmenv={n for n in NAMES if re.search(r'ENVIRONMENT[^\n]*'+n, envstr) or (n+"=" in envstr and "set_tests_properties" in envstr)}
rows=[]
for n in NAMES:
    prod_read=[]; test_read=[]; other=[]
    for f in files:
        p=os.path.join(ROOT,f[2:])
        try: txt=open(p,encoding="utf-8",errors="replace").read()
        except: continue
        if re.search(r'getenv\s*\(\s*"'+n+r'"', txt) or re.search(r'fault_injected\s*\(\s*"'+n+r'"', txt) or re.search(r'"'+n+r'"',txt) and 'setenv' in txt and (n in txt):
            if re.search(r'"'+n+r'"', txt):
                if isprod(f): prod_read.append(f)
                else: test_read.append(f)
        if n in txt and not isprod(f) and n not in [x for x in test_read]:
            pass
    rows.append({"name":n,"prod_reads":prod_read,"test_reads":test_read,
                 "ci_checksjson": n in cienv, "ctest_env": bool(re.search(r'ENVIRONMENT[^\n]{0,200}'+n, envstr))})
json.dump(rows,open(ROOT+"/问题扫描/_verify/scripts_v18_3state.json","w"),ensure_ascii=False,indent=1)
print("%-34s %-6s %-6s %-9s %s" % ("NAME","prod?","ci?","ctestENV","readers"))
for r in rows:
    print("%-34s %-6s %-6s %-9s %s" % (r["name"], "PROD" if r["prod_reads"] else "test-only",
       "CI" if r["ci_checksjson"] else "-", "ENV" if r["ctest_env"] else "-",
       (r["prod_reads"]+r["test_reads"])[:3]))
print()
print("names appearing anywhere in ci/checks.json:", sorted(cienv) or "NONE")
