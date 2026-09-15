import os,re,json
ROOT="问题扫描/findings"
ID=re.compile(r"^#{2,4}\s+((?:M\w+|L\d+b?c?d?e?|FD|V\d+|W\d+)-[A-Z]{1,4}\d*-\d+)\b(.*)$")
PRIO=re.compile(r"(?:优先级|priority)[^A-Za-z0-9]{0,4}(P[0-3])",re.I)
move=[];files=0;total=0
for dp,dn,fn in os.walk(ROOT):
    for f in fn:
        if not f.endswith(".md"): continue
        p=os.path.join(dp,f); files+=1
        m=re.search(r"/(p[0-3])/[^/]+$",p.replace(os.sep,"/"))
        if not m: continue
        dirp=m.group(1).upper()
        lines=open(p,encoding="utf-8").read().split("\n")
        idx=[i for i,l in enumerate(lines) if ID.match(l)]
        for k,i in enumerate(idx):
            j=idx[k+1] if k+1<len(idx) else len(lines)
            blk="\n".join(lines[i:j]); total+=1
            mm=PRIO.search(blk)
            if mm and mm.group(1).upper()!=dirp:
                move.append((ID.match(lines[i]).group(1),dirp,mm.group(1).upper(),p))
print("扫描文件",files,"条目",total,"目录/正文不一致",len(move))
agg={}
for a,b,c,d in move: agg[(b,c)]=agg.get((b,c),0)+1
print("按 (目录级 -> 正文级) 汇总:",agg)
for a,b,c,d in move[:40]: print("   ",a,b,"->",c,d)
json.dump([{"id":a,"dir":b,"declared":c,"file":d} for a,b,c,d in move],open("问题扫描/_cache/prio_mismatch.json","w"),ensure_ascii=False,indent=1)
