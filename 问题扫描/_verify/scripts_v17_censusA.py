#!/usr/bin/env python3
# V17 只读普查脚本 A：形态①/②/③ 候选站点粗筛（纯文本扫描，不执行被扫代码）
import os, re, json

ROOT = "/workspace/Astro CS Database"
CORPUS_DIRS = ["tests", "ci", "tools", "lib", "runtime", "providers", "cli", "modules", "cmake", "scripts", "packaging", "engineering"]
EXCLUDE = ["问题扫描", "/run/", "run/", "__pycache__", ".git/", "AstroCS.wiki", "GaiaDR3", "BASS DR3", "astrocs_p1sess_", "third_party"]

def included(p):
    full = "/" + p
    return not any(x in full for x in EXCLUDE)

exts_py = {".py"}
exts_cpp = {".cpp", ".c", ".h", ".hpp"}

PY_SKIP = re.compile(r"os\.path\.exists|\.exists\(\)|skipUnless|skipIf|pytest\.skip|self\.skipTest")
PY_NEG = re.compile(r"assertNotIn|assertNotRegexp|assertIsNone")
PY_GETENV = re.compile(r"os\.environ\.get\(|os\.getenv\(")
CPP_IFS = re.compile(r"std::ifstream|fopen\(")
CPP_NPOS = re.compile(r"== std::string::npos|!= std::string::npos")
CPP_GETENV = re.compile(r"std::getenv\(")

files = []
for d in CORPUS_DIRS:
    for dp, dn, fn in os.walk(os.path.join(ROOT, d)):
        for f in fn:
            p = os.path.relpath(os.path.join(dp, f), ROOT)
            if included(p):
                files.append(p)

print("corpus files:", len(files))

hits = {"py_skip": [], "py_neg": [], "py_getenv": [], "cpp_read": [], "cpp_npos": [], "cpp_getenv": []}
for p in files:
    ext = os.path.splitext(p)[1]
    try:
        text = open(os.path.join(ROOT, p), encoding="utf-8", errors="replace").read()
    except Exception:
        continue
    for i, ln in enumerate(text.splitlines(), 1):
        s = ln.strip()[:160]
        if ext in exts_py:
            if PY_SKIP.search(ln): hits["py_skip"].append(p + ":" + str(i) + ":" + s)
            if PY_NEG.search(ln): hits["py_neg"].append(p + ":" + str(i) + ":" + s)
            if PY_GETENV.search(ln): hits["py_getenv"].append(p + ":" + str(i) + ":" + s)
        elif ext in exts_cpp:
            if CPP_IFS.search(ln): hits["cpp_read"].append(p + ":" + str(i) + ":" + s)
            if CPP_NPOS.search(ln): hits["cpp_npos"].append(p + ":" + str(i) + ":" + s)
            if CPP_GETENV.search(ln): hits["cpp_getenv"].append(p + ":" + str(i) + ":" + s)

for k in hits:
    print(k, len(hits[k]))
out = os.path.join(ROOT, "问题扫描/_verify/scripts_v17_censusA.json")
with open(out, "w", encoding="utf-8") as fh:
    json.dump(hits, fh, ensure_ascii=False, indent=1)
print("written", out)
