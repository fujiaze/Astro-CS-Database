import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
print("=== (1) assertion macros that compile to nothing under a guard ===")
for f in files:
    if not f.endswith((".h",".hpp",".c",".cpp")): continue
    p=os.path.join(ROOT,f[2:])
    try: lines=open(p,encoding="utf-8",errors="replace").read().split(chr(10))
    except: continue
    for i,l in enumerate(lines):
        m=re.match(r"\s*#\s*define\s+(\w*(CHECK|ASSERT|EXPECT|VERIFY|REQUIRE)\w*)\s*(\([^)]*\))?\s*((?:\S+\s*)*)", l)
        if m and re.fullmatch(r"\s*(/\*.*\*/)?\s*", m.group(4) or ""):
            print("   EMPTY-BODY", f+":"+str(i+1), l.strip()[:110])
print()
print("=== (2) PASS/FAIL_REGULAR_EXPRESSION on ctest targets ===")
for f in files:
    if not (f.endswith("CMakeLists.txt") or f.endswith(".cmake")): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    for i,l in enumerate(txt.split(chr(10))):
        if "REGULAR_EXPRESSION" in l: print("   ",f+":"+str(i+1),l.strip()[:150])
print()
print("=== (3) SKIP_RETURN_CODE ===")
for f in files:
    if not (f.endswith("CMakeLists.txt") or f.endswith(".cmake")): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    for i,l in enumerate(txt.split(chr(10))):
        if "SKIP_RETURN_CODE" in l: print("   ",f+":"+str(i+1),l.strip()[:150])