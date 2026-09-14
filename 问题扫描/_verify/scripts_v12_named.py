import os, re, collections
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
def scan(pat, label, scope=("lib/","tests/","providers/","cli/","runtime/","modules/","include/","tools/","scripts/","ci/","docs/","contracts/")):
    print("=== "+label+" ===")
    n=0
    for f in sorted(tracked):
        if not f.startswith(scope): continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            if re.search(pat,l):
                print(f"  {f}:{i}: {l.strip()[:150]}"); n+=1
    print("  TOTAL",n)
scan(r"\bkMadToSigma\b|\bkMadToSigma15\b|\b_MAD_SCALE\b|\bMAD_SCALE\b|\bk_sigma\b|\bMOFFAT4_FWHM_FACTOR\b|\bkMoffat4FwhmFactor\b|\bkFwhmFactor\b", "MAD/FWHM 具名常量定义与使用")
