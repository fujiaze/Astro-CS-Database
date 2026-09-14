
import subprocess, os, re, json
ROOT="/workspace/Astro CS Database"
out=subprocess.run(["git","--no-optional-locks","-C",ROOT,"ls-files"],capture_output=True,text=True).stdout
tracked=("./"+l for l in out.split("\n") if l.strip())
EXCL=["./run/","./build/","./out/","./worktrees/","./Testing/","./logs/","./artifacts/","./evidence/","./reports/",
"./工程控制/","./GaiaDR3/","./GaiaDR3SP/","./BASS DR3/","./AstroCS.wiki/","./__pycache__/","./问题扫描/",
"./third_party/","./testdata/","./CS/","./Database/","./graph/","./.git/",
"./astrocs_p1sess_neg/","./astrocs_p1sess_perf/","./astrocs_p1sess_props/","./astrocs_p1sess_test/","./设计大纲/"]
files=[f for f in tracked if not any(f.startswith(e) for e in EXCL)]
print("tracked after exclusions:", len(files))
# main.hpp family
mh=[f for f in files if f.endswith("_test_main.hpp")]
print("in-tree *_test_main.hpp:", len(mh))
for f in sorted(mh): print("  ",f)
json.dump(files, open(ROOT+"/问题扫描/_verify/scripts_v18_files.json","w"))
