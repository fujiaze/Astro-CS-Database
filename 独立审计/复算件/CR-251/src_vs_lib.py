import sys, json, os
from collections import Counter, defaultdict
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
lib = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))["filters"]
src = json.load(open(os.path.join(root,"lib/algorithms/photometry/data/response_curves/filters.json"), encoding="utf-8"))
print("source top type:", type(src).__name__, (list(src.keys())[:8] if isinstance(src,dict) else len(src)))
if isinstance(src, dict):
    for k,v in src.items():
        if isinstance(v,(list,dict)) and len(v)>5:
            print("  list-ish key:",k,len(v), type(v[0]).__name__ if v else "", (list(v[0].keys()) if isinstance(v[0],dict) else v[:3]))
