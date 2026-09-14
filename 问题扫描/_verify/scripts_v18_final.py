
import os,re
ROOT="/workspace/Astro CS Database"
print("=== A) sibling ISA tests test_04/test_05 bodies ===")
for f in ["tests/backend/test_isa_avx.py","tests/backend/test_isa_avx2_fma.py","tests/backend/test_isa_avx512.py","tests/backend/test_isa_variants.py","tests/backend/test_isa_bit_manip.py"]:
    p=os.path.join(ROOT,f)
    if not os.path.exists(p):
        print("  MISS",f); continue
    lines=open(p,encoding="utf-8",errors="replace").read().split(chr(10))
    for i,l in enumerate(lines):
        if re.search(r"def test_0[45]", l):
            print("  >> "+f+":"+str(i+1)+" "+l.strip()[:100])
            for k in range(i+1,min(i+16,len(lines))):
                s=lines[k]
                if s.strip() and (len(s)-len(s.lstrip()))<8: break
                if re.search(r"assert|continue|^\s+for |True", s):
                    print("       %4d| %s"%(k+1,s.strip()[:125]))
print()
print("=== B) PRODUCTION-GRAPH mentions tree-wide ===")
out=os.popen('cd "'+ROOT+'" && git --no-optional-locks grep -n "PRODUCTION-GRAPH" -- . 2>/dev/null').read()
print(out[:2500])
