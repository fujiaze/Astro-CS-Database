import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z"], capture_output=True, text=True).stdout.split(chr(0)) if p]
print("tracked files:", len(files))
targets = ["packaging/verify_install_tree.py", "verify_install_tree", "install-tree.contract.json", "packaging/astrocs.product.json", "astrocs.product.windows.json.in"]
hits = {t: [] for t in targets}
for f in files:
    if f.startswith("问题扫描/") or f.startswith("工程控制/") or f.startswith("设计大纲/"):
        continue
    try:
        txt = open(f, encoding="utf-8", errors="replace").read()
    except Exception:
        continue
    for t in targets:
        if t in txt:
            hits[t].append(f)
for t in targets:
    print("=== ", t, " 引用文件数:", len(hits[t]))
    for h in hits[t][:40]: print("     ", h)