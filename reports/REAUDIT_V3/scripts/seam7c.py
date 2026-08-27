import json, math, itertools
from collections import defaultdict
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m = json.load(open(B + "fatduck_26f_upm.json"))
C1 = {k: v for k, v in m["C"][1]}
# per-tile grid of ALL 16 controls: (gx,gy) -> C value (0 if absent from sparse C)
tiles = defaultdict(dict)
for i, ctr in enumerate(m["controls"]):
    tile, gx, gy = ctr[0], ctr[1], ctr[2]
    tiles[tile][(gx, gy)] = C1.get(i, 0.0)
diffs = []
for tile, cells in tiles.items():
    for a, b in itertools.combinations(cells.keys(), 2):
        dx, dy = abs(a[0]-b[0]), abs(a[1]-b[1])
        if (dx <= 1 and dy <= 1) and (dx+dy >= 1):
            diffs.append(abs(cells[a] - cells[b]))
dvals = sorted(diffs)
def pct(v, p): return v[min(len(v)-1, int((len(v)-1)*p))]
print("adjacent control-node pairs:", len(dvals))
print("C-diff p50 = %.6f  p95 = %.6f  p99 = %.6f  max = %.6f" % (pct(dvals,0.5), pct(dvals,0.95), pct(dvals,0.99), dvals[-1]))
# only nonzero-C pairs matter for continuity; also report pairs where both sides have C in the sparse set
sparse_diffs = []
for tile, cells in tiles.items():
    for a, b in itertools.combinations(cells.keys(), 2):
        dx, dy = abs(a[0]-b[0]), abs(a[1]-b[1])
        if (dx <= 1 and dy <= 1) and (dx+dy >= 1):
            ca, cb = C1.get(a[0]*0+tile, None), None  # placeholder
# proper sparse-pair computation: need original indices
tiles_idx = defaultdict(dict)
for i, ctr in enumerate(m["controls"]):
    tiles_idx[ctr[0]][(ctr[1], ctr[2])] = i
sd = []
for tile, cells in tiles_idx.items():
    for a, b in itertools.combinations(cells.keys(), 2):
        dx, dy = abs(a[0]-b[0]), abs(a[1]-b[1])
        if (dx <= 1 and dy <= 1) and (dx+dy >= 1):
            ia, ib = cells[a], cells[b]
            if ia in C1 and ib in C1:
                sd.append(abs(C1[ia] - C1[ib]))
sd.sort()
print("pairs where BOTH sides have nonzero C:", len(sd))
if sd:
    print("  (continuity of the actual fitted field) p50=%.6f p95=%.6f max=%.6f" % (pct(sd,0.5), pct(sd,0.95), sd[-1]))
# cross-tile (boundary between tiles) adjacency: check gap
print("NOTE: metric-7 (C_f continuity across PANEL seams) needs the boundary-strip extraction from Stage1 per-frame HiPS + the stage2 mosaic; this run computes the INTRA-model control-grid continuity of C_f on the real 26-frame production model (a partial real-data component).")
