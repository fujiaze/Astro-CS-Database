import os,re
ROOT="/workspace/Astro CS Database"
p=os.path.join(ROOT,"tools/quality/deep_ci_driver.py")
txt=open(p,encoding="utf-8",errors="replace").read()
lines=txt.split(chr(10))
for i,l in enumerate(lines):
    if "ctest" in l and ("cmd" in l.lower() or "args" in l.lower() or "\"-R\"" in l or "append" in l):
        print("%4d: %s" % (i+1, l.rstrip()[:150]))
print("---- ctest-full branch ----")
for i,l in enumerate(lines):
    if re.search(r"def .*(ctest_full|_run_ctest|ctest-full)", l):
        for k in range(i, min(i+40,len(lines))): print("%4d: %s"%(k+1,lines[k].rstrip()[:150]))
        print("   ...")
        break