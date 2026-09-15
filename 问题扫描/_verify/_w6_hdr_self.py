import os, re, subprocess, json, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","lib/*/include/**","modules/*/include/**","providers/*/include/**","runtime/*/include/**"], capture_output=True, text=True).stdout.split(chr(0)) if p]
print("口径 W6-L：模块公共头面 (lib|modules|providers|runtime)*/include/** =", len(files))
inc_re = re.compile(r"^\s*#include\s+\"([^\"]+)\"", re.M)
rows = collections.defaultdict(list)
for f in files:
    txt = open(f, encoding="utf-8", errors="replace").read()
    for m in inc_re.finditer(txt):
        inc = m.group(1)
        ln = txt[:m.start()].count(chr(10)) + 1
        cands = [os.path.normpath(os.path.join(os.path.dirname(f), inc)),
                 os.path.normpath(os.path.join("include", inc)),
                 os.path.normpath(os.path.join("lib/common/include", inc)),
                 os.path.normpath(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(f))), inc))]
        ok = next((c for c in cands if os.path.exists(c)), None)
        kind = "OK" if ok else "UNRESOLVED"
        via = ""
        if ok:
            via = ("自身目录" if cands[0] == ok else ("仓库 include/" if cands[1] == ok else ("lib/common/include" if cands[2] == ok else "同级 src 树"))) 
        rows[(kind, via, f)].append((inc, ln))
print("-- 需借仓库根 include/ 才能解析的模块公共头 include（口径 W6-M）--")
cnt = 0
for (kind, via, f), v in sorted(rows.items()):
    if kind == "OK" and via == "仓库 include/":
        cnt += 1
        print("   %-58s -> %s" % (f, [x[0] for x in v]))
print("   小计:", cnt)
print("-- 无法解析（UNRESOLVED）--")
for (kind, via, f), v in sorted(rows.items()):
    if kind == "UNRESOLVED":
        print("   %-58s -> %s" % (f, v))