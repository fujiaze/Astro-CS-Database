
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
tot=0; self_tot=0
for f in files:
    if not (f.endswith("CMakeLists.txt") or f.endswith(".cmake")): continue
    t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    for l in t.split(chr(10)):
        if re.search(r"\badd_test\s*\(", l):
            tot+=1
            if "selfcheck" in l.lower(): self_tot+=1
print("in-tree add_test() calls:",tot,"  with 'selfcheck' in the line:",self_tot)
# exclusion prefix count
EXCL=["./run/","./build/","./out/","./worktrees/","./Testing/","./logs/","./artifacts/","./evidence/","./reports/","./工程控制/","./GaiaDR3/","./GaiaDR3SP/","./BASS DR3/","./AstroCS.wiki/","./__pycache__/","./问题扫描/","./third_party/","./testdata/","./CS/","./Database/","./graph/","./.git/","./astrocs_p1sess_neg/","./astrocs_p1sess_perf/","./astrocs_p1sess_props/","./astrocs_p1sess_test/","./设计大纲/"]
print("exclusion prefixes:",len(EXCL))
print("note: ./graph/ excludes the tracked graph/ dir if it exists:", os.path.isdir(ROOT+"/graph"))
for d in ["graph","CS","Database"]:
    print("   dir exists?",d,os.path.isdir(os.path.join(ROOT,d)))
