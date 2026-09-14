
import os,re,json
ROOT="/workspace/Astro CS Database"
def show(title, pat, pathfilter):
    print("### "+title)
    n=0
    for dp,dns,fns in os.walk(os.path.join(ROOT,pathfilter)):
        for fn in fns:
            p=os.path.join(dp,fn)
            try: t=open(p,encoding="utf-8",errors="replace").read()
            except: continue
            for i,l in enumerate(t.split(chr(10))):
                if re.search(pat,l):
                    print("   ",os.path.relpath(p,ROOT)+":"+str(i+1),"|",l.strip()[:130]); n+=1
    print("   L1 hits:",n); print()
show("L1: const_field_bitwise anywhere under lib/calibration", r"const_field_bitwise", "lib/calibration")
show("L1: const_field_bitwise anywhere under lib/cosmetic", r"const_field_bitwise", "lib/cosmetic")
show("L1b: const_field_bitwise tree-wide (tracked subset)", r"const_field_bitwise", ".")
show("L2: P1CAL_CHECK with non-null name in p1cal core", r"P1CAL_CHECK\s*\([^,]+,[^,]+,\s*\"", "lib/calibration")
