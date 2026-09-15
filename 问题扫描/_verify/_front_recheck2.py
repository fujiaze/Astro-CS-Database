
import os,re,json,collections,subprocess
def git(*a): return subprocess.run(["git","--no-optional-locks"]+list(a),capture_output=True,text=True).stdout
head=git("rev-parse","HEAD").strip()
BASE="521095b8"
changed=set(x for x in git("diff","--name-only",BASE,head,"--","lib","cli","include","runtime","providers","modules","tools","tests","ci","docs","contracts","CMakeLists.txt").split("\n") if x.strip())
tracked=[x for x in git("ls-files").split("\n") if x.strip()]
bidx=collections.defaultdict(list)
for t in tracked: bidx[os.path.basename(t)].append(t)
AN=re.compile(r"([\w./\-]+\.(?:cpp|c|h|hpp|cc|py|json|csv|md|yml|yaml|inc|txt))(?::(\d+))?")
anchors={}
for dp,dn,fn in os.walk("问题扫描/findings"):
    for f in fn:
        if not f.endswith(".md"): continue
        p=os.path.join(dp,f); txt=open(p,encoding="utf-8").read()
        ids=set(re.findall(r"^#{2,4}\s+((?:M\w+|L\d+b?c?d?e?|FD|V\d+)-[A-Z]{1,4}-\d+)\b",txt,re.M))
        for m in AN.finditer(txt):
            fp=m.group(1).lstrip("./"); ln=int(m.group(2) or 0)
            if fp.startswith("问题扫描/"): continue
            rp=fp if os.path.exists(fp) else (bidx.get(os.path.basename(fp)) or [None])
            rp=rp if isinstance(rp,str) else (rp[0] if rp and len(set(rp))==1 else (rp[0] if rp else None))
            k=rp or ("UNRESOLVED:"+fp)
            d=anchors.setdefault(k,{"n":0,"ids":set(),"lines":set()})
            d["n"]+=1; d["ids"]|=ids; d["lines"].add(ln)
c=collections.Counter(); imp_unres=set(); imp_changed=set(); imp_gone=set(); imp_moved=set()
for k,d in anchors.items():
    if k.startswith("UNRESOLVED:"): c["UNRESOLVED"]+=1; continue
    if not os.path.exists(k): c["MISSING"]+=1; continue
    if k in changed: imp_changed|=d["ids"]; c["CHANGED"]+=1
    lines=open(k,encoding="utf-8",errors="replace").read().split("\n"); body="\n".join(lines)
    tot=len(lines); inr=[l for l in d["lines"] if 0<l<=tot]
    for l in inr:
        toks=re.findall(r"[A-Za-z_][A-Za-z0-9_]{5,}",lines[l-1])
        if not toks: continue
        s=max(toks,key=len)
        if s not in body: imp_gone|=d["ids"]; c["SYM_GONE"]+=1; break
    else:
        mv=0
        for l in inr:
            toks=re.findall(r"[A-Za-z_][A-Za-z0-9_]{5,}",lines[l-1])
            if toks:
                s=max(toks,key=len)
                if s in body and lines[l-1].find(s)<0: mv+=1
        if mv: imp_moved|=d["ids"]; c["SYM_MOVED"]+=1
        else: c["OK_INPLACE"]+=1
print("HEAD",head[:8],"| BASE",BASE,"| 变更文件",len(changed),"| 锚点宿主",len(anchors))
print("状态分布",dict(c.most_common()))
u=imp_changed|imp_gone|imp_moved
print("需逐条重验的条目数: 变更宿主",len(imp_changed),"符号消失",len(imp_gone),"符号漂移",len(imp_moved),"合并去重",len(u))
json.dump({"head":head,"base":BASE,"changed":sorted(changed),"verify":sorted(u),"gone":sorted(imp_gone),"moved":sorted(imp_moved),"changed_ids":sorted(imp_changed)},open("问题扫描/_cache/recheck_round1.json","w"),ensure_ascii=False)
print("已落 问题扫描/_cache/recheck_round1.json")
