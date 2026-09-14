import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
rows=[]
for f in files:
    if not re.search(r"\.(cpp|c|cc)$", f): continue
    if not (("/tests/" in f) or re.search(r"(_test|selftest|tests_|driver|check|probe)\.(cpp|c|cc)$", f) or "/test_" in f): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if not re.search(r"\bint\s+main\s*\(", txt): continue
    decls=re.findall(r"\b(?:static\s+)?(?:int|unsigned|intmax_t|long|size_t|std::size_t)\s+(\w+)\s*=\s*0\s*;", txt)
    counters={c for c in decls if re.search(r"fail|FAIL|err|ERR",c)}
    if not counters: continue
    allret=re.findall(r"return\s+([^;]{0,120});", txt)
    inret=[c for c in counters if any(re.search(r"\b"+re.escape(c)+r"\b", r) for r in allret)]
    inmain=[c for c in counters if re.search(r"\bint\s+main\b[\s\S]*",txt) and False]
    rows.append({"file":f,"counters":sorted(counters),"in_any_return":sorted(inret),"notin_any_return":sorted(counters-set(inret))})
never=[r for r in rows if not r["in_any_return"]]
print("TUs with fail/err counter + main():",len(rows))
print("  counters that appear in NO return statement anywhere in the TU:",len(never))
for r in never: print("   ",r["file"],"| counters=",r["counters"])