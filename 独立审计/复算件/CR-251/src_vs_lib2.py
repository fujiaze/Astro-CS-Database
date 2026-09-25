import sys, json, os
from collections import Counter, defaultdict
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
lib = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))["filters"]
src = json.load(open(os.path.join(root,"lib/algorithms/photometry/data/response_curves/filters.json"), encoding="utf-8"))
tf = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))["transcription"]["transcribed_fields"]
print("src n =",len(src)," lib n =",len(lib))
print("only src:",sorted(set(src)-set(lib)),"only lib:",sorted(set(lib)-set(src)))
allkeys=set()
for v in list(src.values())+list(lib.values()): allkeys|=set(v)
print("union of field names:",sorted(allkeys)," transcribed_fields:",tf)
diff=[]
for n in sorted(set(src)&set(lib)):
    a,b=src[n],lib[n]
    ka,kb=set(a),set(b)
    d={}
    if ka-kb: d["keys_missing_in_lib"]=sorted(ka-kb)
    if kb-ka: d["keys_extra_in_lib"]=sorted(kb-ka)
    for k in sorted(ka&kb):
        if a[k]!=b[k]: d[k]=(a[k] if not isinstance(a[k],list) else "list-diff", b[k] if not isinstance(b[k],list) else "list-diff")
    if d: diff.append((n,d))
print("entries differing:",len(diff),"/",len(set(src)&set(lib)))
for n,d in diff: print("  ",n,":",{k:(str(v)[:80]) for k,v in d.items()})
# duplicates inside source
g=defaultdict(list)
for n,v in src.items(): g[json.dumps({k:v.get(k) for k in ("channel","wavelength_nm","value")},sort_keys=True)].append(n)
print("== identical curve groups in SOURCE ==")
for k,v in g.items():
    if len(v)>1: print("  ",v)
# channel domain
ch=Counter(v.get("channel") for v in lib.values())
print("== channel counts (lib, whole file) ==", dict(ch))
chs=Counter(v.get("channel") for v in src.values())
print("== channel counts (source) ==", dict(chs))
