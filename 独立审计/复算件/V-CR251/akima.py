import sys, json, subprocess, re
sys.stdout.reconfigure(encoding='utf-8')
REPO = r"F:\Astro dev\Astro CS Normalization Database"
out = subprocess.run(["git","-C",REPO,"show","HEAD:eng/packaging/config/filters.json"],
                     capture_output=True, text=True, encoding="utf-8").stdout
data = json.loads(out)
filters = data.get("filters") or data.get("items") or []
print("filters:", len(filters))

def akima_eval(xs, ys, x):
    n = len(xs)
    # Akima tangents (trigrams) - standard Akima 1970 definition
    m = [0.0]*(n+4)
    for i in range(2, n+2):
        m[i] = ((ys[i-1]-ys[i-2])*abs(ys[i]-ys[i-1])*0 +
                (ys[i-1]-ys[i-2]) + 2*(ys[i]-ys[i-1])) / (3.0) if False else 0.0
    # implement properly: d1..: slopes of secants
    s = [None] + [(ys[i]-ys[i-1])/(xs[i]-xs[i-1]) for i in range(1, n)] + [None]
    s[0] = s[1]; s[n] = s[n-1]
    # extrapolated secants for Akima's trigrams
    s_prev2 = 2*s[1]-s[2]; s_next2 = 2*s[n-1]-s[n]
    t = [0.0]*n
    def akima_slope(i):
        # i in 1..n-2 uses s[i],s[i+1],s[i-1],s[i+2]
        sm1 = s[i] if i-1>=1 else s_prev2
        sm2 = s[i-1] if i-2>=1 else (2*s_prev2 - s[1] if i-2<0 else s_prev2)
        return None
    return None

# Use the repo's own declared algorithm only at the level of "cubic Akima" - so implement
# Akima's canonical formula directly.
def akima_tangents(x, y):
    n = len(x)
    d = [0.0]*(n+4)
    for i in range(1, n):
        d[i+1] = (y[i]-y[i-1])/(x[i]-x[i-1])
    d[0] = 2*d[2]-d[3]
    d[1] = d[2]
    d[n+1] = d[n]
    d[n+2] = 2*d[n]-d[n-1]
    t = [0.0]*n
    for i in range(n):
        a1, a2 = d[i+1], d[i+2]
        b1, b2 = d[i], d[i+3]
        if abs(a2-a1) < 1e-300:
            t[i] = (a1+a2)/2.0
        else:
            t[i] = (abs(a2)*a1 + abs(a1)*a2)/(abs(a1)+abs(a2)) if (abs(a1)+abs(a2))>0 else 0.0
    return t

def hermite_eval(x0,y0,x1,y1,m0,m1,x):
    h = x1-x0
    s = (x-x0)/h
    h00 = 2*s**3-3*s**2+1
    h10 = s**3-2*s**2+s
    h01 = -2*s**3+3*s**2
    h11 = s**3-s**2
    return h00*y0 + h10*h*m0 + h01*y1 + h11*h*m1

def resample(x, y, lo, hi, step):
    t = akima_tangents(x, y)
    out = []
    v = lo
    while v <= hi + 1e-9:
        # find interval
        i = 0
        while i < len(x)-2 and x[i+1] < v:
            i += 1
        if v <= x[0]:
            out.append((v, y[0])); v += step; continue
        if v >= x[-1]:
            out.append((v, y[-1])); v += step; continue
        out.append((v, hermite_eval(x[i], y[i], x[i+1], y[i+1], t[i], t[i+1], v)))
        v += step
    return out

worst = []
for f in filters:
    wl = f.get("wavelengths") or f.get("wl")
    tr = f.get("transmission") or f.get("values") or f.get("T")
    if not wl or not tr or len(wl) != len(tr):
        continue
    if len(wl) != 7:
        continue
    sv = sum(tr); sq = sum(v*v for v in tr)
    rect = abs(sv-sq) < 1e-12
    pts = resample([float(a) for a in wl], [float(b) for b in tr], 336.0, 1020.0, 2.0)
    tmin = min(p[1] for p in pts)
    nneg = sum(1 for p in pts if p[1] < 0)
    if nneg:
        worst.append((f.get("name") or f.get("id"), rect, round(tmin,4), nneg, len(pts)))
for w in worst:
    print("NEGATIVE LOBE:", w)
print("count of 7-point rectilinear curves with negative lobes:", len(worst))
