import re,os
ROOT="/workspace/Astro CS Database"
for f in ["tools/quality/check_pipeline_graph.py","tools/quality/check_prod_reachability.py","tools/quality/check_isa_leak.py"]:
    txt=open(os.path.join(ROOT,f),encoding="utf-8",errors="replace").read()
    lines=txt.split(chr(10))
    print("="*70); print(f, " total lines:",len(lines))
    # find selftest function
    for i,l in enumerate(lines):
        if re.search(r"def .*selftest|def .*self_test", l):
            print("  def at :%d  %s"%(i+1,l.strip()))
            for k in range(i, min(i+34,len(lines))):
                print("     %4d: %s"%(k+1, lines[k].rstrip()[:130]))
            break
    m=re.search(r"--selftest.{0,120}", txt, re.S)
    if m: print("  FLAG:", m.group(0).replace(chr(10)," ")[:170])
