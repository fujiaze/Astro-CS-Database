
import json,os,re
ROOT="/workspace/Astro CS Database"
fn=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_faultnames.json",encoding="utf-8"))
for k in ["P1CAL","P1PSF","P1STAR"]:
    print(k, sorted(fn.get(k,[]))[:30]); print()
print("const_field_bitwise in P1CAL?", "const_field_bitwise" in fn.get("P1CAL",[]))
print("recovery in P1PSF?", "recovery" in fn.get("P1PSF",[]))
sc=open(ROOT+"/lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp",encoding="utf-8").read()
print()
print("p1psf/p1cal/p1star registry presence done")
print()
a=open(ROOT+"/lib/star_detector/tests/p1star/p1star_tests_selfcheck.cpp",encoding="utf-8",errors="replace").read().split(chr(10))
print("--- p1star selfcheck 70-115 ---")
for i in range(69,115): print("%4d: %s"%(i+1,a[i].rstrip()[:135]))
