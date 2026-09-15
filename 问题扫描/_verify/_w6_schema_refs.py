import os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z"], capture_output=True, text=True).stdout.split(chr(0)) if p]
EXCL = ("问题扫描/", "工程控制/", "设计大纲/", "evidence/", "reports/", "docs/archive/", "AstroCS.wiki/")
tracked = [f for f in files if not f.startswith(EXCL)]
needles = ["astrocs-product.schema", "install-tree-contract.schema", "dependency-lock.schema", "preset-contract", "product_version", "0.11.0-alpha.1", "0.11.0-alpha.2", "astrocs.product.json"]
for n in needles:
    print("#### ", n)
    for f in tracked:
        try: txt = open(f, encoding="utf-8", errors="replace").read()
        except Exception: continue
        if n in txt:
            cnt = txt.count(n)
            print("     ", f, "x"+str(cnt))