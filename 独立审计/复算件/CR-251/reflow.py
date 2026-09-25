import sys, os, json
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
p=os.path.join(root,"eng/packaging/config/filters.json")
raw=open(p,encoding="utf-8").read().splitlines()
lib=json.load(open(p,encoding="utf-8"))["filters"]
src=json.load(open(os.path.join(root,"lib/algorithms/photometry/data/response_curves/filters.json"),encoding="utf-8"))
lo,hi=1001,1500
i=lo
# identify which filter objects overlap [lo,hi]
import re
cur=None
while i<=hi:
    line=raw[i-1]
    m=re.match(r'^    "([^"]+)": \{$', line)
    if m: cur=m.group(1)
    if re.match(r'^      "(name|channel|n_points)"', line) or re.match(r'^    "([^"]+)": \{$', line):
        print(i, line.rstrip()); i+=1; continue
    if line.strip() in ('"wavelength_nm": [','"value": ['):
        key=line.strip().rstrip('[').strip().strip('"').replace('_nm','_nm')
        j=i+1; vals=[]
        while raw[j-1].strip() not in ('],',']:'):
            vals.append(raw[j-1].strip().rstrip(',')); j+=1
        arr=[float(v) for v in vals]
        # full array of this key for the current filter (may extend past hi)
        full = lib[cur]["wavelength_nm"] if 'wavelength' in key else lib[cur]["value"]
        same = (arr==full[len(full)-len(arr):]) or (arr==full[:len(arr)])
        step=sorted(set(round(full[k+1]-full[k],6) for k in range(len(full)-1))) if 'wavelength' in key else None
        eq_src = lib[cur][ 'wavelength_nm' if 'wavelength' in key else 'value']==src[cur]['wavelength_nm' if 'wavelength' in key else 'value']
        print(f"{i}-{j} {key} ({cur}) n_in_window={len(arr)} 全部值: {', '.join(vals)}")
        print(f"      该键整数组 n={len(full)} min={min(full)} max={max(full)} 等差/步长集合={step} 与转录源逐元素全等={eq_src}")
        i=j+1; continue
    i+=1
