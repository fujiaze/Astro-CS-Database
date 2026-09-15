
import re
files=[l.rstrip("\n") for l in open("问题扫描/_cache/scope_py.txt",encoding="utf-8") if l.strip()]
pat=re.compile(r"returncode != 0|assertNotEqual\([^,]*returncode, 0\)|not .*returncode == 0")
for f in files:
    if "engineering/control" in f or "/archive/" in f: continue
    try: lines=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception: continue
    for i,L in enumerate(lines):
        if pat.search(L):
            blk="\n".join(lines[max(0,i-3):i+6])
            # narrow: negative expectation (必败/should fail/非零) with NO code/message qualifier on same logical stmt
            if re.search(r"必败|必须非零|must fail|应失败|拒绝", blk+L) and not re.search(r"==\s*[1-9]\d*\b", blk):
                print("### %s:%d"%(f,i+1))
                print("\n".join("  %d| %s"%(j+1,lines[j]) for j in range(max(0,i-3),min(i+6,len(lines)))))
