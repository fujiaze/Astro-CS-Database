
import os,re
ROOT="/workspace/Astro CS Database"
print("=== A) p1drz: group fn returns failures? ===")
t=open(ROOT+"/lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp",encoding="utf-8",errors="replace").read()
lines=t.split(chr(10))
for i,l in enumerate(lines):
    if re.search(r"failures|run_all_groups|getenv|injected|return", l):
        print("  :%d %s"%(i+1,l.strip()[:135]))
print()
print("=== B) p1psf group runner return ===")
t2=open(ROOT+"/lib/dynamic_psf/tests/p1psf/p1psf_test_main.hpp",encoding="utf-8",errors="replace").read().split(chr(10))
for i,l in enumerate(t2):
    if re.search(r"failures|return", l) and i>110: print("  :%d %s"%(i+1,l.strip()[:135]))
