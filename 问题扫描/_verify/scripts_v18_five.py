import os,re
ROOT="/workspace/Astro CS Database"
for f,cs in [("lib/plate_solve/cpp/ipv/test/test_kvector.cpp",["failures","n_fail"]),("tests/unit/p1001_real_nodes_test.cpp",["failed","failures"]),("tests/unit/gaia_cat_test.c",["t_failures","t_fail_after"])]:
    p=os.path.join(ROOT,f); lines=open(p,encoding="utf-8",errors="replace").read().split(chr(10))
    print("=== "+f+" ("+str(len(lines))+" lines)")
    for c in cs:
        print("   -- counter "+c+":")
        for i,l in enumerate(lines):
            if re.search(r"\b"+c+r"\b",l): print("      :%d %s"%(i+1,l.strip()[:120]))
