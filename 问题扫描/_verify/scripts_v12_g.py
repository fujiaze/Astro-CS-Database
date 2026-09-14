import os,re
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
SCOPE=("lib/","tests/","providers/","cli/","runtime/","modules/","include/","tools/","scripts/","ci/","docs/","contracts/")
EXT={".c",".h",".cpp",".hpp",".py",".md",".json",".yaml",".yml",".inc",".txt"}
groups={
 "P50_90":r"90\.0\b|CPU p50 ?&gt;= ?90|p50 ?&gt;= ?90|>=90%|\b90%",
 "FRAC_70":r"0\.70\b|70% ?样本|>= ?70%|util_samples_pass_frac",
 "WALL5":r"wall_seconds ?[<>]=? ?5|wall ?[<>]=? ?5s|wall<5s|5\.0\b",
 "GATE10":r"kMon002MinWindowSeconds|FROZEN_GATE_MIN_INTERVAL_SECONDS|active_window_seconds|interval ?&lt;=|>= ?10s|超过 ?10 ?秒",
}
for k,pat in groups.items():
    print("=== "+k+" ===")
    n=0
    for f in sorted(tracked):
        if not f.startswith(SCOPE): continue
        if os.path.splitext(f)[1] not in EXT: continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            if re.search(pat,l):
                print(f"  {f}:{i}: {l.strip()[:140]}"); n+=1
                if n>45: break
        if n>45: break
    print("  shown",n); print()
