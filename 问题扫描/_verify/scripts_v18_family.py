
import os,re,json
ROOT="/workspace/Astro CS Database"
MH={"p1hips":"lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp","p2hips":"lib/astro_image_io/tests/p2hips/p2hips_test_main.hpp",
"p1cal":"lib/calibration/tests/p1cal/p1cal_test_main.hpp","p1cos":"lib/cosmetic/tests/p1cos/p1cos_test_main.hpp",
"p1psf":"lib/dynamic_psf/tests/p1psf/p1psf_test_main.hpp","p1drz":"lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp",
"p1sess":"lib/phase1_session/tests/p1sess/p1sess_test_main.hpp","p1phot":"lib/photometric_calib/tests/p1phot/p1phot_test_main.hpp",
"p1noise":"lib/snr_estimator/tests/p1noise/p1noise_test_main.hpp","p1star":"lib/star_detector/tests/p1star/p1star_test_main.hpp",
"aio_abi":"tests/unit/aio_abi_test_main.hpp","p1wcs":"tests/unit/p1wcs/p1wcs_test_main.hpp"}
print("%-9s %-6s %-14s %-9s %-10s %-8s %-9s %s" % ("family","lines","cond-param","faultrep-1st","injected-2nd","cond-3rd","cond-eval?","note"))
for k,f in MH.items():
    txt=open(os.path.join(ROOT,f),encoding="utf-8",errors="replace").read()
    lines=txt.split("\n")
    # collect the main CHECK macro body
    for i,l in enumerate(lines):
        m=re.match(r'\s*#\s*define\s+(\w*CHECK)\s*\(([^)]*)\)',l)
        if not m: continue
        body=[l]; j=i
        while body[-1].rstrip().endswith("\\") and j+1<len(lines): j+=1; body.append(lines[j])
        b="\n".join(body); nm=m.group(1); ps=[x.strip() for x in m.group(2).split(",")]
        cond=[p for p in ps if p in ("cond",)] or ([ps[1]] if len(ps)>1 else [])
        cp=cond[0] if cond else "?"
        fr = "fault_reported" in b and bool(re.search(r'if\s*\(\s*\(?cs\)?\.fault_reported', b))
        inj = bool(re.search(r'\.injected\s*\(', b))
        ceval = bool(re.search(r'!\s*\(\s*'+re.escape(cp)+r'\s*\)', b))
        # is the injected branch BEFORE the cond branch?
        pos_inj = b.find(".injected("); pos_cond = b.find("!("+cp+")")
        order = "inj<cond" if (pos_inj>=0 and pos_cond>=0 and pos_inj<pos_cond) else ("cond-first" if pos_cond>=0 else "no-cond")
        print("%-9s %-6d %-14s %-9s %-10s %-8s %-9s %s" % (k,len(lines),nm,fr,inj,ceval,("YES" if ceval else "NO"),order))
        break
