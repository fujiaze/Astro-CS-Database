
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
cand=[]
W=re.compile(r"open\(|\.write_text\(|\.write_bytes\(|csv\.writer\(")
A=re.compile(r"isfile|\.exists\(|assertEqual\(os")
for f in files:
    if not f.endswith(".py"): continue
    if "test" not in os.path.basename(f) and "/tests/" not in f: continue
    p=os.path.join(ROOT,f[2:])
    try: lines=open(p,encoding="utf-8",errors="replace").read().split("\n")
    except: continue
    i=0
    while i<len(lines):
        m=re.match(r"\s*def (\w+)\s*\(", lines[i])
        if not m:
            i+=1; continue
        ind=len(lines[i])-len(lines[i].lstrip()); j=i+1; body=[]
        while j<len(lines):
            l=lines[j]
            if l.strip() and (len(l)-len(l.lstrip()))<=ind and not l.strip().startswith("#"): break
            body.append(l); j+=1
        b="\n".join(body)
        hasw=bool(W.search(b)); hasA=bool(A.search(b))
        if hasw and hasA:
            ex=[x.strip()[:80] for x in re.findall(r"isfile\([^)]{0,70}|exists\([^)]{0,70}", b)][:3]
            cand.append({"file":f,"fn":m.group(1),"line":i+1,"exists":ex})
        i=j
print("py test functions that WRITE then ASSERT-EXISTS:",len(cand))
for c in cand: print("  ",c["file"]+":"+str(c["line"]),c["fn"],"|",c["exists"])
