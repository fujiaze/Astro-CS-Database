
import re,os,json
ROOT="/workspace/Astro CS Database"
MH="""lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp
lib/astro_image_io/tests/p2hips/p2hips_test_main.hpp
lib/calibration/tests/p1cal/p1cal_test_main.hpp
lib/cosmetic/tests/p1cos/p1cos_test_main.hpp
lib/dynamic_psf/tests/p1psf/p1psf_test_main.hpp
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp
lib/phase1_session/tests/p1sess/p1sess_test_main.hpp
lib/photometric_calib/tests/p1phot/p1phot_test_main.hpp
lib/snr_estimator/tests/p1noise/p1noise_test_main.hpp
lib/star_detector/tests/p1star/p1star_test_main.hpp
tests/unit/aio_abi_test_main.hpp
tests/unit/p1wcs/p1wcs_test_main.hpp
""".strip().split("\n")
res={}
for f in MH:
    p=os.path.join(ROOT,f)
    if not os.path.exists(p):
        print("MISSING",f); continue
    txt=open(p,encoding="utf-8",errors="replace").read().split("\n")
    out=[]
    for i,l in enumerate(txt):
        if re.match(r'\s*#\s*define', l) and re.search(r'CHECK|ASSERT', l):
            body=[l]; j=i
            while body[-1].rstrip().endswith("\\") and j+1<len(txt):
                j+=1; body.append(txt[j])
            out.append({"line":i+1,"text":"\n".join(x.rstrip() for x in body)})
    res[f]=out
json.dump(res, open(os.path.join(ROOT,"问题扫描/_verify/scripts_v18_mh.json"),"w"), ensure_ascii=False, indent=1)
for f,out in res.items():
    print("=== "+f)
    for o in out:
        print("  :%d"%o["line"])
        for ln in o["text"].split("\n"): print("     "+ln)
