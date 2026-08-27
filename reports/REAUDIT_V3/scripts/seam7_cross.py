import json, itertools, os
from collections import defaultdict
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
mods = {"2f": "fatduck_2f_upm.json", "5f": "fatduck_5f_upm.json", "10f": "fatduck_10f_upm.json", "26f": "fatduck_26f_upm.json"}
def pct(v, p): return v[min(len(v)-1, int((len(v)-1)*p))]
for name, fn in mods.items():
    p = os.path.join(B, fn)
    if not os.path.exists(p):
        print(name, "MISSING", fn); continue
    m = json.load(open(p))
    nframes = len(m.get("frames", []))
    cand = None
    for fi, cmap in enumerate(m["C"]):
        if len(cmap) > 0 and fi != 0:
            cand = fi; break
    if cand is None:
        cand = 0
    Cmap = {k: v for k, v in m["C"][cand]}
    tiles = defaultdict(dict)
    for i, ctr in enumerate(m["controls"]):
        tile, gx, gy = ctr[0], ctr[1], ctr[2]
        tiles[tile][(gx, gy)] = Cmap.get(i, 0.0)
    diffs = []
    for tile, cells in tiles.items():
        for a, b in itertools.combinations(cells.keys(), 2):
            dx, dy = abs(a[0]-b[0]), abs(a[1]-b[1])
            if (dx <= 1 and dy <= 1) and (dx+dy >= 1):
                diffs.append(abs(cells[a]-cells[b]))
    dv = sorted(diffs)
    nz = [d for d in dv if d > 0]
    nzm = max(nz) if nz else 0.0
    cf = m["frames"][cand] if nframes > cand else "?"
    print(f"{name}: frames={nframes} cand_frame={cf} tiles={len(tiles)} pairs={len(dv)} p50={pct(dv,0.5):.6f} p95={pct(dv,0.95):.6f} p99={pct(dv,0.99):.6f} max={dv[-1]:.6f} nonzero={len(nz)} nonzero_max={nzm:.6f}")
