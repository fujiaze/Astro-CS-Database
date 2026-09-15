
import re
files=[l.rstrip("\n") for l in open("问题扫描/_cache/scope_py.txt",encoding="utf-8") if l.strip()]
skip=("engineering/control","/archive/","问题扫描")
# lines that check rc against 0 near a subprocess of astrocs/binary
for f in files:
    if any(s in f for s in skip): continue
    try: txt=open(f,encoding="utf-8",errors="replace").read()
    except Exception: continue
    if not re.search(r"astrocs|fixture|\.exe|EXE|BIN", txt): continue
    lines=txt.splitlines()
    for i,L in enumerate(lines):
        if re.search(r"(rc|returncode|exit_code|code)\s*(!=|==)\s*0", L) or re.search(r"assert\s+\w*\.returncode", L) or re.search(r"if\s+not\s+\w*\.returncode\s*==\s*0", L):
            ctx="\n".join(lines[max(0,i-6):i+4])
            if re.search(r"subprocess|run_bin|RUN|exe|EXE|BIN", ctx):
                print("%s:%d  %s"%(f,i+1,L.strip()[:110]))
