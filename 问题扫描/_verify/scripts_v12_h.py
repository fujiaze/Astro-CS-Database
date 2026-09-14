import os,re
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
groups={
 "Mon002_32_used":r"memory_growth_limit_mb_per_s|kAllocGrowthUnboundedMbPerS",
 "Frac70_used":r"kMon001UtilSampleFrac|utilization_p75_low|UtilizationP75Low|p75",
 "P50_90_used":r"kCpuP50MinPercent|cpu_p50_percent",
 "Alpha2885_tests":r"0\.2885|m_lim_alpha_prior",
 "m0_consts_tests":r"compute_initial_mag_cut|m_lim_m0_offset|13\.0",
 "Cache307_tests":r"307200000|307,200,000|QUERY_CACHE_MAX_BYTES|kQueryCacheCap",
 "Reclaim32MiB":r"kAllocReclaimResidualTolBytes|kAllocMinReclaimFrac|33554432",
}
for k,pat in groups.items():
    print("=== "+k+" ===")
    n=0
    for f in sorted(tracked):
        if not f.startswith(("lib/","cli/","tests/","tools/","docs/","contracts/","ci/","scripts/","providers/","runtime/")): continue
        if os.path.splitext(f)[1] not in {".c",".h",".cpp",".hpp",".py",".md",".json",".yaml",".yml",".inc",".txt",".csv"}: continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            if re.search(pat,l):
                print(f"  {f}:{i}: {l.strip()[:135]}"); n+=1
                if n>=34: break
        if n>=34: break
    print("  shown",n); print()
