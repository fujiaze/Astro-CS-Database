import sys, json, os
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
lib = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))["filters"]
rows=[]
for n,f in sorted(lib.items()):
    wl=f["wavelength_nm"]; v=f["value"]
    mono = all(wl[i]<wl[i+1] for i in range(len(wl)-1))
    dups = len(set(wl))!=len(wl)
    step = sorted(set(round(wl[i+1]-wl[i],6) for i in range(len(wl)-1)))
    out01 = [x for x in v if x<0 or x>1]
    nan = [x for x in v if isinstance(x,float) and x!=x]
    rows.append((n,len(wl),len(v),mono,dups,step[:3],min(v),max(v),out01,nan))
print(f"{'filter':34s} n  mono dup step  vmin vmax out[0,1] nan")
for r in rows:
    print(f"{r[0]:34s} {r[1]:3d} {str(r[2]):3s} {r[3]} dup={r[4]} step={r[5]} v=[{r[6]},{r[7]}] out={r[8]} nan={r[9]}")
print("len mismatch wl vs value:",[r[0] for r in rows if r[1]!=r[2]])
print("non-monotonic:",[r[0] for r in rows if not r[3]])
print("values outside [0,1]:",[(r[0],r[8]) for r in rows if r[8]])
print("n_points<10:",[(r[0],r[1]) for r in rows if r[1]<10])
