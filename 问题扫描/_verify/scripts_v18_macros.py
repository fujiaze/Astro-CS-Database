
import os, re, json, sys
ROOT = "/workspace/Astro CS Database"
EXCL = ["./run/","./build/","./out/","./worktrees/","./Testing/","./logs/","./artifacts/","./evidence/","./reports/",
        "./工程控制/","./GaiaDR3/","./GaiaDR3SP/","./BASS DR3/","./AstroCS.wiki/","./__pycache__/","./问题扫描/",
        "./third_party/","./testdata/","./CS/","./Database/","./graph/","./.git/",
        "./astrocs_p1sess_neg/","./astrocs_p1sess_perf/","./astrocs_p1sess_props/","./astrocs_p1sess_test/","./设计大纲/"]
def skip(rel):
    return any(rel.startswith(e) for e in EXCL)
exts = (".c",".cc",".cpp",".cxx",".h",".hpp",".hxx",".inl",".inc",".cmake",".txt",".py",".sh",".ps1")
files=[]
for dp,dns,fns in os.walk(ROOT):
    dns[:] = [d for d in dns if not d.startswith(".git")]
    rel = os.path.relpath(dp, ROOT)
    relp = "./"+(rel if rel==". " .strip() and rel!="." else "").strip()+"/"
    for f in fns:
        rp = os.path.join(dp,f); r = "./"+os.path.relpath(rp,ROOT)
        if skip(r): continue
        if f.endswith(exts): files.append(r)
print("scanned files:", len(files))
# gather #define of assertion-like macros with multi-line continuation
defre = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)\b')
hits=[]
for r in files:
    try:
        txt = open(os.path.join(ROOT,r[2:]),encoding="utf-8",errors="replace").read()
    except Exception as e:
        continue
    lines = txt.split("\n")
    i=0
    while i < len(lines):
        m = defre.match(lines[i])
        if m:
            name=m.group(1)
            body=lines[i]
            j=i
            while body.rstrip().endswith("\\") and j+1<len(lines):
                j+=1; body += "\n" + lines[j]
            if re.search(r'(CHECK|ASSERT|EXPECT|VERIFY|REQUIRE|ENSURE)', name) and '(' in body.split('define')[-1][:60]:
                hits.append({"file":r,"line":i+1,"name":name,"body":body})
            i=j+1; continue
        i+=1
print("assertion-like macro defs:", len(hits))
json.dump(hits, open(os.path.join(ROOT,"问题扫描/_verify/scripts_v18_macros.json"),"w"), ensure_ascii=False, indent=1)
from collections import Counter
c=Counter(h["name"] for h in hits)
for n,k in c.most_common(60): print(k,n)
