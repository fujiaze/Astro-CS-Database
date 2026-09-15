
import re
files=[l.rstrip("\n") for l in open("问题扫描/_cache/scope_py.txt",encoding="utf-8") if l.strip()]
skip=("engineering/control","/archive/")
pat=re.compile(r"returncode\s*==\s*0|\brc\s*==\s*0\b")
for f in files:
    if any(s in f for s in skip): continue
    if not f.startswith(("tools/","ci/","runtime/","scripts/","packaging/","lib/")): continue
    try: lines=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception: continue
    for i,L in enumerate(lines,1):
        if pat.search(L):
            nxt="\n".join(lines[i:i+8])
            if not re.search(r"\belse\b", nxt):
                print("%s:%d  %s"%(f,i,L.strip()[:110]))
