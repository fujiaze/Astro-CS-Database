import sys, json, os
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
d=json.load(open(os.path.join(root,"eng/packaging/config/filters.json"),encoding="utf-8"))
F=d["filters"]; P=d["provenance"]["per_filter"]
for n in ["Astronomik UV-IR Block L-1","Astronomik UV-IR Block L-2","Astronomik UV-IR Block L-3","Baader UV/IR Cut / L CMOS Optimized"]:
    f=F[n]; print(n, "ch=",repr(f["channel"]))
    print("   wl :",f["wavelength_nm"])
    print("   val:",f["value"])
    s=P[n]["curve_stats"]
    print("   stats:",s)
    v=f["value"]; print("   0/1-only?", all(x in (0,1) for x in v), "| #ones=",sum(1 for x in v if x==1))
print("=== max val across all 45 (top vs rest) ===")
mx=sorted(((max(f["value"]),n) for n,f in F.items()),reverse=True)[:6]
print(mx)
print("=== how many curves have any element exactly 1.0 ===",[n for n,f in F.items() if any(x==1.0 for x in f["value"])])
