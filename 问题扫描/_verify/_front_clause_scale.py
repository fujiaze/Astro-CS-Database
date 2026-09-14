import os,re,subprocess
TR=[t for t in subprocess.run(["git","ls-files"],capture_output=True,text=True).stdout.split("\n") if t]
RUN=re.compile(r"(?<![\w/])run/[A-Za-z0-9_./\-]+")
BARE=re.compile(r"[（(]\s*[0-9]{2,5}\s*[-]\s*[0-9]{2,5}\s*[）)]")
MARK=re.compile(r"NEEDS_DECISION|待登记|待复核|待裁决|TODO|FIXME|XXX|OWNER-[0-9]+")
runs={};bare={};mark={}
for t in TR:
    if t.startswith(("evidence/","reports/","artifacts/","问题扫描/")): continue
    if not t.endswith((".md",".c",".cpp",".h",".hpp",".py",".txt",".cmake",".json",".yaml",".yml",".csv",".inc")): continue
    try: txt=open(t,encoding="utf-8",errors="ignore").read()
    except Exception: continue
    a=len(RUN.findall(txt))
    if a: runs[t]=a
    b=len(BARE.findall(txt)) if t.endswith((".md",".h",".hpp",".cpp",".c")) else 0
    if b: bare[t]=b
    mm={}
    for m in MARK.findall(txt): mm[m]=mm.get(m,0)+1
    if mm: mark[t]=mm
print("C-15 规模: tracked 文件引用 run/** 的文件数=",len(runs)," 处数=",sum(runs.values()))
for k,v in sorted(runs.items(),key=lambda x:-x[1])[:8]: print("   %3d %s"%(v,k))
print("   危险子集(docs/contracts/include/AGENTS/宪章):")
for k,v in sorted(runs.items()):
    if k.startswith(("docs/","contracts/","include/","AGENTS","ASTROCS")): print("     %3d %s"%(v,k))
print("C-9 裸行号续锚: 文件数=",len(bare)," 处数=",sum(bare.values()))
for k,v in sorted(bare.items(),key=lambda x:-x[1])[:5]: print("   %3d %s"%(v,k))
tot={}
for f,d in mark.items():
    for k,v in d.items(): tot[k]=tot.get(k,0)+v
print("未决标记按类:",dict(sorted(tot.items(),key=lambda x:-x[1])))
print("含标记文件数=",len(mark))
n=[]
for f in sorted(mark):
    s=subprocess.run(["git","--no-pager","log","-1","--format=%ad","--date=short","--",f],capture_output=True,text=True).stdout.strip()
    if s>="2026-09-13": n.append((s,f))
print("本批(09-13后)有改动的带标记文件数=",len(n))
for s,f in n[:10]: print("   ",s,f)
