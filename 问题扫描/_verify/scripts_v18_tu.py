import os,re,json
ROOT="/workspace/Astro CS Database"
f=ROOT+"/tests/unit/CMakeLists.txt"
lines=open(f,encoding="utf-8",errors="replace").read().split(chr(10))
for i,l in enumerate(lines):
    if re.search(r"ASTROCS_P2002_FAULT|ASTROCS_P3002_FAULT|ASTROCS_RT001_FAULT|ASTROCS_P3PROJ_FAULT|add_test\(NAME p2002|add_test\(NAME p3002|add_test\(NAME rt001|add_test\(NAME p3_projection", l):
        print("%4d: %s"%(i+1,l.rstrip()[:170]))
print("---- set_tests_properties near fault ----")
for i,l in enumerate(lines):
    if "set_tests_properties" in l:
        blk=chr(10).join(lines[i:i+4])
        if "FAULT" in blk: print("%4d: %s"%(i+1, blk.replace(chr(10)," ")[:250]))