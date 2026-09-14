import os, re, json
root="/workspace/Astro CS Database"
EXCL={".git","build","run","out","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","node_modules","artifacts","evidence","工程控制","问题扫描","reports"}
exts={".c",".h",".cpp",".hpp",".cc",".py",".md",".json",".yaml",".yml",".csv",".txt",".inc",".cu",".sh",".ps1",".in"}
pats={
 "MAX_STARS_200000": r"200000|200_000",
 "wlcount_343": r"\b343\b",
 "buf511": r"\b511\b|\b512\b",
 "upm_grid8": r"grid\s*[=!<>]+\s*8|\b8\s*[x×]\s*8\b",
 "reclaim_0.5_32MiB": r"32\s*\*\s*1024\s*\*\s*1024|33554432|0\.5\b",
 "cpu_0.85_0.60": r"0\.85|0\.60|85\.0|60\.0|kCpuMeanMinPercent|kCpuLowDip",
 "margin_0.25": r"0\.25\b",
 "margin_1.2": r"1\.2\b",
}
out={}
for dp,dn,fn in os.walk(root):
    rel=os.path.relpath(dp,root); parts=rel.split(os.sep)
    dn[:]=[d for d in dn if d not in EXCL]
    if parts[0] in EXCL: dn[:]=[]; continue
    for f in fn:
        if os.path.splitext(f)[1] not in exts: continue
        p=os.path.join(dp,f); rp=os.path.relpath(p,root)
        try:
            with open(p,encoding="utf-8",errors="ignore") as fh:
                for i,l in enumerate(fh,1):
                    for k,pat in pats.items():
                        if re.search(pat,l):
                            out.setdefault(k,[]).append((rp,i,l.strip()[:160]))
        except Exception: pass
json.dump(out,open(os.path.join(root,"问题扫描/_cache/v12_scan2.json"),"w"),ensure_ascii=False)
for k in pats: print(k, len(out.get(k,[])))
