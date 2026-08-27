import re, os
D = "/tmp/control_doc"
files = sorted(os.listdir(D))
print(files[:20])
# find the V2 control md
for f in files:
    if f.endswith(".md") or f.endswith(".txt"):
        p = os.path.join(D, f)
        try:
            txt = open(p, encoding="utf-8", errors="replace").read()
        except: continue
        if "9.2" in txt or "跨层" in txt:
            print("===", f, "size", len(txt))
