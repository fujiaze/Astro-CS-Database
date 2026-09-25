import sys, os, json, re
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
p=os.path.join(root,"eng/packaging/config/filters.json")
raw=open(p,encoding="utf-8").read().splitlines()
lib=json.load(open(p,encoding="utf-8"))["filters"]
src=json.load(open(os.path.join(root,"lib/algorithms/photometry/data/response_curves/filters.json"),encoding="utf-8"))
blocks=[]; cur=None; i=1; n=len(raw)
while i<=n:
    l=raw[i-1]
    m=re.match(r'^    "(.*)": \{$', l)
    if m: cur=m.group(1); blocks.append(("HDR",i,i,cur,None,None))
    m2=re.match(r'^      "(name|channel|n_points)": (.*)$', l)
    if m2: blocks.append(("KV",i,i,cur,m2.group(1),m2.group(2).rstrip(',')))
    m3=re.match(r'^      "(wavelength_nm|value)": \[$', l)
    if m3:
        j=i+1; vals=[]
        while j<=n and raw[j-1].strip() not in ('],',']:'):
            vals.append(raw[j-1].strip().rstrip(',')); j+=1
        blocks.append(("ARR",i,j,cur,m3.group(1),vals)); i=j+1; continue
    i+=1
LO,HI=1001,1500
for kind,a,b,name,key,vals in blocks:
    if b<LO or a>HI: continue
    if kind=="ARR":
        full=lib[name][key]; eqsrc=(full==src[name][key])
        step=sorted(set(round(full[k+1]-full[k],6) for k in range(len(full)-1))) if key=="wavelength_nm" else None
        print(f"[{a}-{b}] {name} :: {key}  n={len(vals)}  与源逐元素全等={eqsrc}" + (f"  波长步长集合={step}" if step else ""))
        print("      值:", ", ".join(vals))
    else:
        print(f"[{a}] {name} :: {key} = {vals}")
