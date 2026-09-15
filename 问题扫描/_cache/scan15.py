
import re
files=[l.rstrip("\n") for l in open("问题扫描/_cache/scope_py.txt",encoding="utf-8") if l.strip()]
for f in files:
    if "engineering/control" in f or "/archive/" in f: continue
    try: txt=open(f,encoding="utf-8",errors="replace").read()
    except Exception: continue
    if "Popen" not in txt: continue
    lines=txt.splitlines()
    for i,L in enumerate(lines):
        if "Popen" in L:
            print("### %s:%d"%(f,i+1))
            print("\n".join("    %d| %s"%(j+1,lines[j]) for j in range(i, min(i+16,len(lines)))))
