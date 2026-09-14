import os,re
ROOT="/workspace/Astro CS Database"
lines=open(ROOT+"/CMakeLists.txt",encoding="utf-8",errors="replace").read().split(chr(10))
for i,l in enumerate(lines):
    if "check_isa_leak" in l:
        for k in range(max(0,i-6),min(len(lines),i+10)): print("%4d: %s"%(k+1,lines[k].rstrip()[:150]))
        print("---")
print("=== p1003 skip guards ===")
t=open(ROOT+"/tests/cli/test_p1003_drizzle_path.py",encoding="utf-8",errors="replace").read()
for i,l in enumerate(t.split(chr(10))):
    if "skipTest" in l: print("  :%d %s"%(i+1,l.strip()[:130]))
print("=== does check_isa_leak real mode run anywhere (ctest/ci)? ===")
files=os.popen("cd \""+ROOT+"\" && git --no-optional-locks ls-files").read().split(chr(10))
n=0
for f in files:
    if not f or "./" in f: pass
    p=os.path.join(ROOT,f)
    try: s=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if "check_isa_leak" in s and "--binary" in s:
        print("   real-mode args in:",f); n+=1
print("   files with check_isa_leak AND --binary:",n)