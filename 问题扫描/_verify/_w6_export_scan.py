import os, re, subprocess, json, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z"], capture_output=True, text=True).stdout.split(chr(0)) if p]
EXCL = ("问题扫描/", "工程控制/", "设计大纲/", "evidence/", "reports/", "docs/archive/", "AstroCS.wiki/")
tracked = [f for f in files if not f.startswith(EXCL)]
print("口径 W6-F：受跟踪文件总数", len(files), "| 排除前缀", list(EXCL), "后扫描面", len(tracked))
pats = {
  "WINDOWS_EXPORT_ALL_SYMBOLS": r"WINDOWS_EXPORT_ALL_SYMBOLS",
  "ENABLE_EXPORTS": r"ENABLE_EXPORTS",
  "version-script": r"version-script",
  "DEF_file": r"\\.def\\b",
  "visibility_hidden": r"VISIBILITY_PRESET|fvisibility",
  "EXPORTS_macro": r"ASTROCS_EXPORT|ASTROCS_ABI_SHARED|__declspec\\(dllexport\\)",
}
hits = collections.defaultdict(list)
for f in tracked:
    if not (f.endswith(".txt") or f.endswith(".cmake") or f.endswith(".h") or f.endswith(".in") or f.endswith(".py") or f.endswith(".c") or f.endswith(".cpp") or f.endswith(".def")):
        continue
    try:
        txt = open(f, encoding="utf-8", errors="replace").read()
    except Exception:
        continue
    for k, p in pats.items():
        for m in re.finditer(p, txt):
            ln = txt[:m.start()].count(chr(10)) + 1
            hits[k].append(f + ":" + str(ln))
for k in pats:
    v = hits[k]
    print("###", k, "命中", len(v))
    for x in sorted(set(v)): print("    ", x)