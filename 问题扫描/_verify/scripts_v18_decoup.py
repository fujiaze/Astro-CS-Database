import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
hits=[]
for f in files:
    if not re.search(r"\.(cpp|c|cc)$", f): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if not re.search(r"\bint\s+main\s*\(", txt): continue
    decls=re.findall(r"\b(?:static\s+)?(?:int|unsigned|long|size_t|std::size_t)\s+(\w+)\s*=\s*0\s*;", txt)
    counters={c for c in decls if re.search(r"fail|FAIL|err|ERR",c)}
    if not counters: continue
    # unconditional return 0 in main AND at least one counter incremented somewhere
    mi=re.search(r"\bint\s+main\s*\([^)]*\)\s*(?:noexcept\s*)?\{", txt)
    if not mi: continue
    d=0;st=None;en=None
    for k in range(mi.end()-1,len(txt)):
        if txt[k]=="{":
            if d==0: st=k
            d+=1
        elif txt[k]=="}":
            d-=1
            if d==0 and st is not None: en=k; break
    if st is None or en is None: continue
    mb=txt[st:en+1]
    mret=re.findall(r"return\s+([^;]{0,140});", mb)
    uses=re.findall(r"\b("+"|".join(re.escape(c) for c in counters)+r")\b", mb) if counters else []
    cnt_incr=len(re.findall(r"(?:\+\+|--)\s*(?:"+ "|".join(re.escape(c) for c in counters)+r")\b|\b(?:"+ "|".join(re.escape(c) for c in counters)+r")\s*(?:\+\+|--)|(?:\+=|-=)\s*1", txt))
    zero_only=all(r.strip()=="0" for r in mret) and len(mret)>0
    never_used_in_main=[c for c in counters if not re.search(r"\b"+re.escape(c)+r"\b", mb)]
    if (zero_only and never_used_in_main) or (never_used_in_main and cnt_incr>0):
        hits.append({"file":f,"counters":sorted(counters),"never_in_main":sorted(never_used_in_main),"returns":sorted(set(mret)),"incr_sites":cnt_incr})
print("CANDIDATES fail/err counter never read in main() despite being incremented:",len(hits))
for h in hits[:40]:
    print("   ",h["file"],"| ctr=",h["counters"],"| not-in-main=",h["never_in_main"],"| main returns=",h["returns"],"| incr~",h["incr_sites"])