import sys, json, os
from collections import defaultdict
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
d = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))
pf = d["provenance"]["per_filter"]; fl = d["filters"]
g = defaultdict(list)
for n,v in pf.items(): g[json.dumps(v["curve_stats"],sort_keys=True)].append(n)
print("== identical curve_stats groups ==")
for k,v in g.items():
    if len(v)>1: print(" ", v)
g2 = defaultdict(list)
for n,v in fl.items():
    key = json.dumps({"wl":v.get("wavelength_nm"),"val":v.get("value"),"channel":v.get("channel")})
    g2[key].append(n)
print("== identical (channel,wavelength,value) groups ==")
for k,v in g2.items():
    if len(v)>1: print(" ", v, "| npts=", len(json.loads(k)["wl"]))
# recompute curve_stats from filters and compare with declared
print("== declared curve_stats vs recomputed from filters[] ==")
bad=0
for n,v in pf.items():
    cs=v["curve_stats"]; f=fl[n]
    wl=f.get("wavelength_nm"); val=f.get("value")
    calc={"n_points":len(wl),"wl_min":min(wl),"wl_max":max(wl),"val_min":min(val),"val_max":max(val),
          "wl_sum":round(sum(wl),6),"val_sum":round(sum(val),6),"val_sumsq":round(sum(x*x for x in val),9)}
    diff={k:(cs.get(k),calc[k]) for k in calc if abs((cs.get(k) or 0)-calc[k])>1e-9}
    if diff:
        bad+=1; print("  MISMATCH",n,diff)
print("  mismatched filters:",bad,"/",len(pf))
print("== filters entry key sets ==")
ks=set()
for n,v in fl.items(): ks.add(tuple(sorted(v.keys())))
for k in ks: print("  ",k, sum(1 for n,v in fl.items() if tuple(sorted(v.keys()))==k))
print("== n_points check (declared n_points vs len(value)) ==")
for n,v in fl.items():
    if v.get("n_points")!=len(v.get("wavelength_nm",[])) or len(v.get("wavelength_nm",[]))!=len(v.get("value",[])):
        print("   ",n,v.get("n_points"),len(v.get("wavelength_nm",[])),len(v.get("value",[])))
