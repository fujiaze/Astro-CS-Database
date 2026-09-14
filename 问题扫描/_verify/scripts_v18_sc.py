
import os,re,json
ROOT="/workspace/Astro CS Database"
MH="""lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp
lib/calibration/tests/p1cal/p1cal_tests_selfcheck.cpp
lib/cosmetic/tests/p1cos/p1cos_tests_selfcheck.cpp
lib/dynamic_psf/tests/p1psf/p1psf_tests_selfcheck.cpp
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp
lib/phase1_session/tests/p1sess/p1sess_tests_selfcheck.cpp
lib/photometric_calib/tests/p1phot/p1phot_tests_selfcheck.cpp
lib/snr_estimator/tests/p1noise/p1noise_tests_selfcheck.cpp
lib/star_detector/tests/p1star/p1star_tests_selfcheck.cpp
tests/unit/aio_abi_selfcheck.cpp
tests/unit/p1wcs/p1wcs_tests_selfcheck.cpp"""
out={}
for f in MH.strip().split("\n"):
    p=os.path.join(ROOT,f)
    txt=open(p,encoding="utf-8",errors="replace").read()
    lines=txt.split("\n")
    d={}
    d["lines"]=len(lines)
    d["execve"]=bool(re.search(r'\bexecve\s*\(',txt))
    d["posix_spawn"]=bool(re.search(r'posix_spawn',txt))
    d["system"]=bool(re.search(r'\bsystem\s*\(|popen',txt))
    d["fork"]=bool(re.search(r'\bfork\s*\(',txt))
    d["fault_env"]=sorted(set(re.findall(r'ASTROCS_[A-Z0-9_]*FAULT[A-Z0-9_]*',txt)))
    d["selfcheck_env"]=sorted(set(re.findall(r'[A-Z0-9_]*SELFCHECK[A-Z0-9_]*',txt)))
    d["child_env_null"]=bool(re.search(r'child_env\s*\[\s*\]\s*=\s*\{',txt))
    m=re.search(r'const\s+std::string\s+name\s*=\s*\w+\s*\?\s*\w+\s*:\s*"([^"]+)"',txt)
    d["default_fault"]=m.group(1) if m else None
    d["rc_check"]=[x.strip() for x in re.findall(r'if\s*\(\s*child_rc[^)]*\)',txt)]
    d["baseline"]=[x.strip()[:80] for x in re.findall(r'"(SELFCHECK[^"]*)"',txt)]
    d["pass_re"]=re.findall(r'SELFCHECK PASS',txt)
    out[f]=d
json.dump(out,open(os.path.join(ROOT,"问题扫描/_verify/scripts_v18_selfcheck.json"),"w"),ensure_ascii=False,indent=1)
for f,d in out.items():
    print("===",f)
    for k,v in d.items():
        if k in ("lines","pass_re") : continue
        print("   ",k,"=",v)
