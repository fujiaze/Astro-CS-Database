import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
rows=[]
for f in files:
    if not re.search(r"\.(cpp|c|cc)$", f): continue
    if not (("/tests/" in f) or re.search(r"(_test|selftest|tests_)\.(cpp|c|cc)$", f) or "/test_" in f): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    mi=re.search(r"\bint\s+main\s*\(", txt)
    if not mi: continue
    d=0; start=None; end=None
    for k in range(mi.end(),len(txt)):
        if txt[k]=="{":
            if d==0: start=k
            d+=1
        elif txt[k]=="}":
            d-=1
            if d==0 and start is not None: end=k; break
    if start is None or end is None: continue
    mainbody=txt[start:end+1]
    cnt=re.findall(r"\b([A-Za-z_]\w*(?:failures|failure|fails|n_fail)\w*)\b", mainbody)
    decls=re.findall(r"\b(?:int|std::size_t|size_t|long|unsigned)\s+(\w+)\s*=\s*0\s*;", txt)
    counters=set(cnt)|set(decls)
    counters={c for c in counters if re.search(r"fail|FAIL",c)}
    if not counters: continue
    rets=re.findall(r"return\s+([^;]{0,90});", mainbody)
    used=[c for c in counters if any(re.search(r"\b"+re.escape(c)+r"\b", r) for r in rets)]
    rows.append({"file":f,"counters":sorted(counters),"returns":sorted(set(r.strip()[:60] for r in rets)),"used":sorted(used),"mlines":mainbody.count(chr(10))})
bad=[r for r in rows if not r["used"]]
print("C++ test TUs with failure-counter + parseable main():",len(rows))
print("  counters NEVER appearing in any main() return:",len(bad))
for r in bad: print("   ",r["file"],"| counters=",r["counters"],"| returns=",r["returns"][:4])
json.dump(rows,open(ROOT+"/问题扫描/_verify/scripts_v18_exit.json","w"),ensure_ascii=False,indent=1)