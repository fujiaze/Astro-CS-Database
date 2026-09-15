
import os,re,json,collections,subprocess
ROOT="."
# 1) 隔夜变更规模
base="56b5e662"
head=subprocess.run(["git","--no-optional-locks","rev-parse","HEAD"],capture_output=True,text=True).stdout.strip()
names=subprocess.run(["git","--no-optional-locks","diff","--name-only",base,head,"--","lib","cli","include","runtime","providers","modules","tools","tests","ci","docs","contracts"],capture_output=True,text=True).stdout.split("\n")
changed=set(x for x in names if x.strip())
print("HEAD",head[:8],"变更文件(生产+测试+合同+docs面)",len(changed))
byext=collections.Counter(os.path.splitext(x)[1] for x in changed)
print("按扩展名",dict(byext.most_common(8)))
dirs=collections.Counter(x.split("/")[0] for x in changed)
print("顶层分布",dict(dirs.most_common(10)))
# 2) 抽锚点
AN=re.compile(r"([\w./\-]+\.(?:cpp|c|h|hpp|cc|py|json|csv|md|yml|yaml|inc|txt))(?::(\d+))?")
anchors={}
for dp,dn,fn in os.walk("问题扫描/findings"):
    for f in fn:
        if not f.endswith(".md"): continue
        p=os.path.join(dp,f)
        txt=open(p,encoding="utf-8").read()
        ids=set(re.findall(r"^#{2,4}\s+((?:M\w+|L\d+b?c?d?e?|FD|V\d+)-[A-Z]{1,4}-\d+)\b",txt,re.M))
        for m in AN.finditer(txt):
            fp=m.group(1); ln=int(m.group(2) or 0)
            if fp.startswith("问题扫描/"): continue
            key=fp
            d=anchors.setdefault(key,{"n":0,"ids":set(),"lines":set()})
            d["n"]+=1; d["ids"]|=ids; d["lines"].add(ln)
print("锚点宿主文件数",len(anchors))
# 3) 三态判定
def repo_path(fp):
    cands=[fp, fp.replace("./","")]
    for c in cands:
        if os.path.exists(c): return c
    return None
res=[]
for fp,d in anchors.items():
    rp=repo_path(fp)
    if rp is None:
        res.append((fp,"MISSING_FILE",d)); continue
    try: lines=open(rp,encoding="utf-8",errors="replace").read().split("\n")
    except Exception: res.append((fp,"UNREADABLE",d)); continue
    tot=len(lines)
    inrange=[l for l in d["lines"] if 0<l<=tot]
    # 符号：取锚点行里最长的标识符，看是否仍在全文件
    syms=set()
    for l in inrange:
        toks=re.findall(r"[A-Za-z_][A-Za-z0-9_]{5,}",lines[l-1] or "")
        if toks: syms.add(max(toks,key=len))
    body="\n".join(lines)
    gone=[s for s in syms if s not in body]
    moved=[]
    for l in inrange:
        toks=re.findall(r"[A-Za-z_][A-Za-z0-9_]{5,}",lines[l-1] or "")
        if toks:
            s=max(toks,key=len)
            if s in body and lines[l-1].find(s)<0: moved.append(s)
    st="OK_INPLACE"
    if d["n"] and rp in changed: st="CHANGED_FILE"
    if moved: st="SYMBOL_MOVED"
    if gone and len(gone)==len(syms) and syms: st="SYMBOL_GONE"
    res.append((fp,st,d))
cnt=collections.Counter(x[1] for x in res)
print("三态统计",dict(cnt.most_common()))
print("受影响条目数（锚点宿主已变或符号消失）")
worst=set(["MISSING_FILE","SYMBOL_GONE","SYMBOL_MOVED","CHANGED_FILE"])
imp=set()
for fp,st,d in res:
    if st in worst: imp|=d["ids"]
print("涉及条目数",len(imp),"（账本共 682 条）")
json.dump({"head":head,"changed":sorted(changed),"anchors":{k:[v[1],sorted(v[0] if False else [])] for k,v in []},"impact":sorted(imp),"states":{k:[s,sorted(list(dd["ids"]))[:60]] for k,s,dd in res if s in worst}},open("问题扫描/_cache/recheck_round1.json","w"),ensure_ascii=False)
print("明细已落 问题扫描/_cache/recheck_round1.json")
