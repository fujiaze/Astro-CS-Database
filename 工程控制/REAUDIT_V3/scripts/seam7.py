import json, math, itertools
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m = json.load(open(B + "fatduck_26f_upm.json"))
# frames[1] is the non-reference frame (per earlier pick: 1414626549821219884)
print("frames[1] =", m["frames"][1])
C1 = {k: v for k, v in m["C"][1]}
# controls[i] = [tile_ipix, gx, gy, ra_deg, dec_deg, M?, leaf_ipix]
# We want C differences between control nodes that are ADJACENT in the tile grid (same tile,
# |dgx|<=1 and |dgy|<=1) and physically near.
# Build: for each tile, map (gx,gy) -> (leaf, C, ra, dec)
tiles = {}
for i, ctr in enumerate(m["controls"]):
    tile, gx, gy = ctr[0], ctr[1], ctr[2]
    if i in C1:
        tiles.setdefault(tile, {})[(gx, gy)] = (ctr[6], C1[i], ctr[3], ctr[4])
diffs = []
for tile, cells in tiles.items():
    keys = list(cells.keys())
    for a, b in itertools.combinations(keys, 2):
        dx, dy = abs(a[0]-b[0]), abs(a[1]-b[1])
        if (dx <= 1 and dy <= 1) and (dx+dy >= 1):
            # angular separation
            ra1, de1 = math.radians(cells[a][2]), math.radians(cells[a][3])
            ra2, de2 = math.radians(cells[b][2]), math.radians(cells[b][3])
            ang = math.acos(max(-1, min(1, math.sin(de1)*math.sin(de2) + math.cos(de1)*math.cos(de2)*math.cos(ra1-ra2))))
            d = abs(cells[a][1] - cells[b][1])
            diffs.append((d, ang, tile))
print("adjacent control-node pairs:", len(diffs))
if diffs:
    dvals = sorted(d for d,_,_ in diffs)
    angs = [a for _,a,_ in diffs]
    def pct(v, p): return v[min(len(v)-1, int((len(v)-1)*p))]
    print("C-diff p50 = %.6f  p95 = %.6f  max = %.6f" % (pct(dvals,0.5), pct(dvals,0.95), dvals[-1]))
    print("mean ang sep = %.3f deg (%.1f arcmin)" % (sum(angs)/len(angs), sum(angs)/len(angs)*60))
    # how many have >2x median (jump candidates)
    med = pct(dvals,0.5)
    jumps = sum(1 for d in dvals if d > 2*med if med>0)
    print("pairs with dC > 2*median:", jumps, "/", len(dvals))
    # per-tile max (seam candidates = tiles where adjacent diff spikes)
    from collections import defaultdict
    per = defaultdict(list)
    for d,_,t in diffs: per[t].append(d)
    bad = sorted(((max(v), t, len(v)) for t,v in per.items()), reverse=True)[:5]
    print("top-5 tiles by max adjacent dC:", [(round(mx,6), t, n) for mx,t,n in bad])
