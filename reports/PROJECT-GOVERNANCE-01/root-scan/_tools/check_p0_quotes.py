#!/usr/bin/env python3
"""ROOT-004: P0 quote-liveness check (substance alive vs gone). Read-only."""
import os, re, json, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..","..","..",".."))
GEN  = os.path.join(ROOT,"reports","PROJECT-GOVERNANCE-01","root-scan","_gen")
PRUNE = {"run","build","out","artifacts","evidence","graph","worktrees","Testing","logs","third_party","设计大纲",".git","node_modules","__pycache__",".pytest_cache",".cache"}
index={}
for dp,dns,fns in os.walk(ROOT):
    dns[:]=[d for d in dns if d not in PRUNE]
    for fn in fns: index.setdefault(fn,[]).append(os.path.relpath(os.path.join(dp,fn),ROOT))
def resolve(p):
    full=os.path.join(ROOT,p)
    if os.path.isfile(full): return p
    c=index.get(os.path.basename(p),[])
    return sorted(c,key=len)[0] if c else None
def norm(s):
    s=re.sub(r"[`*_]","",s); s=re.sub(r"\s+"," ",s)
    return s.strip()
EXT = "cpp|hpp|h|c|py|md|json|csv|txt|jsonl|in|sh|cmake|yaml|yml"
QUOTE=re.compile(r"[（(]([A-Za-z0-9_][A-Za-z0-9_./\-]*\.(?:" + EXT + r"))(?::(\d+)(?:\s*[-–]\s*\d+)?)?[）)]([^\n]{10,400})")
dig=json.load(open(os.path.join(GEN,"p0_digest.json"),encoding="utf-8"))
out=[]
for d in dig:
    txt=open(os.path.join(GEN,"p0_sections",d["id"]+".md"),encoding="utf-8").read()
    recs=[]; seen=set()
    for m in QUOTE.finditer(txt):
        p,l1,q=m.group(1),m.group(2),norm(m.group(3))
        if p.startswith(("run/","build/","out/","reports/","evidence/","artifacts/")): continue
        q=q.strip(" ->…|")
        if len(q)<12: continue
        key=(p,q[:40])
        if key in seen: continue
        seen.add(key)
        rp=resolve(p)
        if not rp: recs.append({"ref":p,"line":l1,"q":q[:110],"verdict":"NOFILE"}); continue
        content=open(os.path.join(ROOT,rp),encoding="utf-8",errors="replace").read().replace("\r\n","\n")
        cn=norm(content)
        probe=q[:70]
        found = probe in cn
        if not found:
            tk=[t for t in re.findall(r"[A-Za-z_][A-Za-z0-9_]{3,}|[0-9]+\.[0-9]+",probe)]
            hit=sum(1 for t in tk if t in cn)
            found = bool(tk) and hit/len(tk)>=0.7
        recs.append({"ref":p,"line":l1,"q":probe,"verdict":"ALIVE" if found else "GONE","file":rp})
    out.append({"id":d["id"],"cat":d["cat"],"n":len(recs),"alive":sum(1 for r in recs if r["verdict"]=="ALIVE"),
                "gone":[r for r in recs if r["verdict"]=="GONE"],"nofile":[r for r in recs if r["verdict"]=="NOFILE"],"recs":recs})
json.dump(out,open(os.path.join(GEN,"p0_quote_liveness.json"),"w",encoding="utf-8"),ensure_ascii=False,indent=1)
print("entries:",len(out),"| quotes checked:",sum(o["n"] for o in out),"| ALIVE:",sum(o["alive"] for o in out),
      "| GONE:",sum(len(o["gone"]) for o in out),"| NOFILE:",sum(len(o["nofile"]) for o in out))
print("entries with >=1 GONE quote:",sum(1 for o in out if o["gone"]))
lo,hi=int(sys.argv[1]),int(sys.argv[2])
for o in out[lo:hi]:
    g=o["gone"]
    print("%-11s %-14s quotes=%-2d alive=%-2d gone=%-2d %s" % (o["id"],o["cat"],o["n"],o["alive"],len(g),
        ("| "+ " ;; ".join("%s: %s"%(r["ref"].split("/")[-1], r["q"][:80]) for r in g[:2])) if g else ""))
