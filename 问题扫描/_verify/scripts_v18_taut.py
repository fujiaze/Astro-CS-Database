import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
hits=[]
for f in files:
    if not f.endswith(".py"): continue
    p=os.path.join(ROOT,f[2:])
    try: lines=open(p,encoding="utf-8",errors="replace").read().split(chr(10))
    except: continue
    for i,l in enumerate(lines):
        if re.search(r"assertTrue\(\s*True\s*\)", l) or re.search(r"assertIsNotNone\(\s*\w+\s*\)", l) and False:
            hits.append((f,i+1,l.strip()[:120]))
print("assertTrue(True) 站点:",len(hits))
for h in hits: print("   ",h[0]+":"+str(h[1]),"|",h[2])
print()
# 2) same-file write-then-read of the SAME path variable
wr=[]
for f in files:
    if not f.endswith(".py"): continue
    if "test" not in os.path.basename(f) and "/tests/" not in f: continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    for m in re.finditer(r"(\w+)\s*=\s*os\.path\.join\([^)]*\)\s*(?:#.*?)?\n", txt):
        var=m.group(1)
        w=bool(re.search(r"open\(\s*"+var+r"\s*,\s*.w", txt)) or bool(re.search(var+r"\)\.write", txt)) or bool(re.search(r"writer\("+var, txt)) or bool(re.search(r"with open\(\s*"+var, txt))
        a=bool(re.search(r"(isfile|exists)\(\s*"+var, txt)) or bool(re.search(r"assertIn\([^)]*\b"+var+r"\b", txt))
        if w and a:
            ln=txt[:m.start()].count(chr(10))+1
            wr.append((f,ln,var)); break
print("同文件内 变量路径 先写后断言存在/命中 的测试文件:",len(wr))
for x in wr[:25]: print("   ",x[0]+":"+str(x[1]),"var=",x[2])