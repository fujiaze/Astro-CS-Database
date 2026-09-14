
import os,re
ROOT="/workspace/Astro CS Database"
SC=["lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp","lib/calibration/tests/p1cal/p1cal_tests_selfcheck.cpp",
"lib/cosmetic/tests/p1cos/p1cos_tests_selfcheck.cpp","lib/dynamic_psf/tests/p1psf/p1psf_tests_selfcheck.cpp",
"lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp","lib/phase1_session/tests/p1sess/p1sess_tests_selfcheck.cpp",
"lib/photometric_calib/tests/p1phot/p1phot_tests_selfcheck.cpp","lib/snr_estimator/tests/p1noise/p1noise_tests_selfcheck.cpp",
"lib/star_detector/tests/p1star/p1star_tests_selfcheck.cpp","tests/unit/aio_abi_selfcheck.cpp","tests/unit/p1wcs/p1wcs_tests_selfcheck.cpp"]
for f in SC:
    p=os.path.join(ROOT,f); txt=open(p,encoding="utf-8",errors="replace").read(); lines=txt.split("\n")
    # find int main( block and print first 12 lines
    for i,l in enumerate(lines):
        if re.match(r'\s*int\s+main\s*\(', l):
            print("=== "+f+(" :%d"%(i+1)))
            for k in range(i, min(i+14,len(lines))):
                print("   %d: %s" % (k+1, lines[k].rstrip()))
            break
    print()
