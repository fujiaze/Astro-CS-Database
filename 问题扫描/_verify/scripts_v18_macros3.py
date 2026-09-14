
import json, re, os
ROOT="/workspace/Astro CS Database"
hits=json.load(open(os.path.join(ROOT,"问题扫描/_verify/scripts_v18_macros.json"),encoding="utf-8"))
# for each macro: extract parameter list, then check whether last-ish cond param appears in a boolean-eval position
def params(body):
    m=re.search(r'#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\(([^)]*)\)', body)
    if not m: return None,None
    return m.group(1), [p.strip() for p in m.group(2).split(",")]
rows=[]
for h in hits:
    name,ps=params(h["body"])
    if ps is None:
        rows.append({**h,"kind":"obj-like(no params)"}); continue
    after = h["body"].split(")",1)[1] if ")" in h["body"] else h["body"]
    # remove the define line's macro header
    idx=h["body"].index("(")
    # find end of param list
    depth=0; endpos=None
    for k in range(idx,len(h["body"])):
        if h["body"][k]=="(":depth+=1
        elif h["body"][k]==")":
            depth-=1
            if depth==0: endpos=k; break
    bodytext=h["body"][endpos+1:] if endpos else ""
    # does any param get evaluated in a condition? look for '!(cond)' or 'if (!(cond))' or 'cond'
    evals=[]
    for p in ps:
        if not p: continue
        # patterns where p is used in boolean/negated eval
        pats=[r'!\s*\(\s*'+re.escape(p)+r'\s*\)', r'!\s*'+re.escape(p)+r'\b',
              r'if\s*\(\s*'+re.escape(p), r'\(\s*'+re.escape(p)+r'\s*\)\s*\?']
        ok=any(re.search(x,bodytext) for x in pats)
        evals.append((p,ok))
    rows.append({"file":h["file"],"line":h["line"],"name":h["name"],"params":ps,
                 "eval_map":dict(evals),"body":h["body"]})
json.dump(rows,open(os.path.join(ROOT,"问题扫描/_verify/scripts_v18_macros2.json"),"w"),ensure_ascii=False,indent=1)
# Report macros whose LAST param (typical cond) is NOT evaluated, or which contain ++failures/++failure without cond eval
sus=[]
for r in rows:
    if r.get("params") is None: continue
    ps=[p for p in r["params"] if p]
    if not ps: continue
    # cond is usually named cond/c/x/expr, or second param
    cand=[p for p in ps if re.fullmatch(r'cond|c|x|expr|expression|ok|res|result|v|val|value', p)] or [ps[-1]]
    ceval=all(r["eval_map"].get(p,False) for p in cand)
    hascount=bool(re.search(r'\+\+\s*(failures|n_fail|fails|g_failures|fail_count)|failures\s*\+=', r.get("body","") or json.dumps(r)))
    if not ceval:
        sus.append({**r,"cond_candidates":cand})
print("macros with non-evaluated cond-like param:", len(sus))
for s in sus:
    print("---", s["file"], s["line"], s["name"], s["params"], "condcand:", s["cond_candidates"])
