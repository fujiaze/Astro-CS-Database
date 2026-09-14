import os, re
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
SCOPE=("lib/","tests/","providers/","cli/","runtime/","modules/","include/","tools/","scripts/","ci/","docs/","contracts/","engineering/","packaging/",".github/")
pats = {
 "CPU85": r"0\.85\b|85\.0\b|\b85%|85 ?percent|_min_utilization|MIN_AVG_UTIL",
 "CPU60": r"0\.60\b|60\.0\b|\b60%|WINDOW_MIN_UTIL",
 "WIN10": r"10\.0\b|\b10s\b|连续 ?10|WINDOW_SECONDS|MIN_INTERVAL_SECONDS",
 "W5"  : r"wall\s*>=\s*5|>= ?5s|\b5s\b|0\.05",
}
for k,pat in pats.items():
    print("=== "+k+" ===")
    n=0
    for f in sorted(tracked):
        if not f.startswith(SCOPE): continue
        if os.path.splitext(f)[1] not in {".c",".h",".cpp",".hpp",".py",".md",".json",".yaml",".yml",".inc",".ps1",".sh",".txt"}: continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            if re.search(pat,l):
                if f.startswith("docs/archive/") or "/archive/" in f: continue
                print(f"  {f}:{i}: {l.strip()[:150]}"); n+=1
                if n>90: break
    print("  TOTAL(shown)",n); print()
