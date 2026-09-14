import os, re, json, sys
root = "/workspace/Astro CS Database"
EXCL_DIRS = {".git","build","run","out","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","third_party","node_modules","artifacts","evidence","工程控制"}
# but third_party under astro_image_io is vendored; keep excluded to focus on prod
exts = {".c",".h",".cpp",".hpp",".cc",".py",".md",".json",".yaml",".yml",".csv",".txt",".inc",".ps1",".cmake",".cu",".js",".sh",".in"}
skip_files = {"问题扫描"}
pats = {
 "1.482602218505602": re.compile(r"1\.482602218505602"),
 "1.4826022185": re.compile(r"1\.4826022185(?!05602)"),
 "1.4826(bare)": re.compile(r"1\.4826(?!\d)"),
 "1.482602": re.compile(r"1\.482602(?!\d)"),
 "0.6745": re.compile(r"0\.6745(?!\d)"),
 "0.67448975": re.compile(r"0\.67448"),
 "1.230310": re.compile(r"1\.23031(\d*)"),
}
hits = {k: [] for k in pats}
for dirpath, dirnames, filenames in os.walk(root):
    rel = os.path.relpath(dirpath, root)
    parts = rel.split(os.sep)
    dirnames[:] = [d for d in dirnames if d not in EXCL_DIRS]
    if parts[0] in skip_files: 
        dirnames[:] = []
        continue
    for fn in filenames:
        if os.path.splitext(fn)[1] not in exts: continue
        p = os.path.join(dirpath, fn)
        rp = os.path.relpath(p, root)
        try:
            with open(p, "r", encoding="utf-8", errors="ignore") as f:
                for i, line in enumerate(f, 1):
                    for k, pat in pats.items():
                        if pat.search(line):
                            hits[k].append((rp, i, line.strip()[:150]))
        except Exception as e:
            pass
print("### COUNTS (excl:", ",".join(sorted(EXCL_DIRS)), "问题扫描) ###")
for k in pats:
    print(f"{k}: total={len(hits[k])}")
json.dump(hits, open(os.path.join(root,"问题扫描/_cache/v12_mad.json"),"w"), ensure_ascii=False)
for k in ["1.482602218505602","1.4826022185","1.4826(bare)","1.482602","0.6745","0.67448975"]:
    print("=== "+k+" ===")
    for rp,i,l in hits[k]:
        print(f"  {rp}:{i}: {l[:130]}")
