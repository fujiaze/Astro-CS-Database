
import os,re,json
ROOT="/workspace/Astro CS Database"
fn=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames.json",encoding="utf-8"))
SC={"p1hips":("lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp",["P1HIPS"]),
"p1cal":("lib/calibration/tests/p1cal/p1cal_tests_selfcheck.cpp",["P1CAL"]),
"p1cos":("lib/cosmetic/tests/p1cos/p1cos_tests_selfcheck.cpp",["P1COS"]),
"p1psf":("lib/dynamic_psf/tests/p1psf/p1psf_tests_selfcheck.cpp",["P1PSF"]),
"p1drz":("lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp",["P1DRZ"]),
"p1sess":("lib/phase1_session/tests/p1sess/p1sess_tests_selfcheck.cpp",["P1SESS"]),
"p1phot":("lib/photometric_calib/tests/p1phot/p1phot_tests_selfcheck.cpp",["P1PHOT"]),
"p1noise":("lib/snr_estimator/tests/p1noise/p1noise_tests_selfcheck.cpp",["P1NOISE"]),
"p1star":("lib/star_detector/tests/p1star/p1star_tests_selfcheck.cpp",["P1STAR"]),
"aio_abi":("tests/unit/aio_abi_selfcheck.cpp",["AIO"]),
"p1wcs":("tests/unit/p1wcs/p1wcs_tests_selfcheck.cpp",["P1WCS"])}
print("%-9s %-8s %-8s %-8s %s"%("family","registry","driven","undriven","driven-but-not-in-registry"))
tot_reg=tot_dr=0
for k,(f,fams) in SC.items():
    reg=set()
    for fm in fams: reg|=set(fn.get(fm,[]))
    txt=open(os.path.join(ROOT,f),encoding="utf-8",errors="replace").read()
    # hardcoded names: in string literals that look like fault names, or defaults
    lits=set(re.findall(r'"([a-z][a-z0-9_]{4,})"', txt))
    lits={x for x in lits if "_" in x or len(x)>6}
    drv=lits&reg
    dflt=re.findall(r'fault \? ?\w+ : "\w+"|"[a-z0-9_]+"\s*;\s*//', txt)
    tot_reg+=len(reg); tot_dr+=len(drv)
    print("%-9s %-8d %-8d %-8d %s"%(k,len(reg),len(drv),len(reg-drv),sorted(drv-reg)[:5]))
print("TOTALS registry-names=%d  names-mentioned-in-some-selfcheck=%d"%(tot_reg,tot_dr))
