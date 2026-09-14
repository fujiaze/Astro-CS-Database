# -*- coding: utf-8 -*-
"""V15 普查器 C：C++ 侧「读源文件文本 + 子串命中即通过」的守卫清单（①类），
   按读取对象是否 = 仓库源文件/配置 分类，并检查断言与不变量的蕴含关系线索。"""
import os, re, json
ROOT = "/workspace/Astro CS Database"
SKIP_DIRS = {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports",
             "工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描",
             "third_party","testdata","CS","Database","graph",".git","astrocs_p1sess_neg",
             "astrocs_p1sess_perf","astrocs_p1sess_props","astrocs_p1sess_test"}
READ = re.compile(r'(read_file\(|readText|ifstream|std::fopen|istreambuf_iterator)')
SRC_PATH = re.compile(r'(ASTROCS_REPO|lib/|include/|cli/|tests/|cmake/|CMakeLists)')
FIND = re.compile(r'\.find\(\s*("([^"]*)"|std::string|f *\+|node)')

rows = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
    for fn in filenames:
        if not fn.endswith((".cpp",".c",".h",".hpp")): continue
        p = os.path.join(dirpath, fn)
        r = "./" + os.path.relpath(p, ROOT).replace(os.sep,"/")
        ok = r.startswith("./tests/") or r.startswith("./tools/") or r.startswith("./ci/")
        if not ok:
            if not (r.startswith("./lib/") and (("/tests/" in r) or "_test." in r or "selftest" in r)): continue
        try: lines = open(p, encoding="utf-8", errors="replace").read().split("\n")
        except Exception: continue
        for i, ln in enumerate(lines):
            s = ln.strip()
            if s.startswith("//") or s.startswith("*"): continue
            if ".find(" not in s and "npos" not in s: continue
            win = "\n".join(lines[max(0,i-20):i+1])
            if not READ.search(win): continue
            srcish = bool(SRC_PATH.search(win))
            rows.append({"f": r, "l": i+1, "src": srcish, "s": s[:180]})

print("C++ find-on-read-text 命中:", len(rows), " 其中疑似读**仓库源文件/配置**:", sum(1 for x in rows if x["src"]))
byfile={}
for x in rows:
    if x["src"]: byfile.setdefault(x["f"],[]).append(x)
for f in sorted(byfile):
    print(f"{len(byfile[f]):4d}  {f}")
json.dump(rows, open(os.path.join(ROOT,"问题扫描/_verify/scripts_v15_cpp_text.json"),"w",encoding="utf-8"), ensure_ascii=False, indent=1)
