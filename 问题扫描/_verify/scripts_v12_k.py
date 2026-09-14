import os, re, json
root="/workspace/Astro CS Database"
EXCL={".git","build","run","out","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","node_modules","artifacts","evidence","工程控制","问题扫描","reports"}
exts={".c",".h",".cpp",".hpp",".cc",".py",".md",".json",".yaml",".yml",".inc",".cu"}
# K family: 6.0 13.0 1.5 2.0 as a group -> find files where several of them co-occur
pat = re.compile(r"\b(6\.0|13\.0|1\.5|2\.0)\b")
hits={}
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
                    if re.search(r"13\.0", l):
                        hits.setdefault(rp,[]).append((i,l.strip()[:170]))
        except Exception: pass
for rp,v in hits.items():
    print("###",rp)
    for i,l in v[:14]: print("   ",i,l)
