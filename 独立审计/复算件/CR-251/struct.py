import sys, json, hashlib, os
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
p = os.path.join(root, "eng/packaging/config/filters.json")
d = json.load(open(p, encoding="utf-8"))
print("top-level keys:", list(d.keys()))
for k, v in d.items():
    if isinstance(v, dict):
        print(f"  {k}: dict keys={list(v.keys())[:12]}")
    elif isinstance(v, list):
        print(f"  {k}: list len={len(v)}")
        first = v[0]
        if isinstance(first, dict):
            print("     first item keys:", list(first.keys()))
            for kk, vv in first.items():
                if isinstance(vv, list):
                    print(f"       {kk}: list len={len(vv)} head={vv[:3]}")
                else:
                    print(f"       {kk} = {vv if not isinstance(vv,(list,dict)) else type(vv)}")
    else:
        print(f"  {k} = {v}")
pf = d["provenance"]["per_filter"]
print("per_filter count:", len(pf))
print("file size bytes:", os.path.getsize(p))
