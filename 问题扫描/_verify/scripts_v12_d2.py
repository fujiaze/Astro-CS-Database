import os, re
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
SCOPE=("lib/","tests/","providers/","cli/","runtime/","modules/","include/","tools/","scripts/","ci/","docs/","contracts/","engineering/")
def scan(pat,label):
    print("=== "+label+" ===")
    n=0
    for f in sorted(tracked):
        if not f.startswith(SCOPE): continue
        if os.path.splitext(f)[1] not in {".c",".h",".cpp",".hpp",".cc",".py",".md",".yaml",".yml",".json",".inc",".txt",".csv"}: continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            if re.search(pat,l):
                print(f"  {f}:{i}: {l.strip()[:155]}"); n+=1
    print("  TOTAL",n); print()
scan(r"0\.73167", "trimmed-mean→σ 常数 0.7316728 vs 0.7316727929211932")
scan(r"4\.685|_TUKEY_C|1\.253|1\.0253", "Tukey c / 中位数 SE 乘数")
scan(r"1\.2\s*\*|\*\s*1\.2\b|bbox.*1\.2|1\.2.*bbox|kPad|pad_frac|margin", "1.2 bbox 裕量（宽匹配）")
