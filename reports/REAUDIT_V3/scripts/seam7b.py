import json
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m = json.load(open(B + "fatduck_26f_upm.json"))
print("controls[0] =", m["controls"][0])
print("cell_index[0:3] =", m["cell_index"][:3])
print("C[1] len =", len(m["C"][1]))
# understand controls layout: is it [tile, gx, gy, ra, dec, M, leaf]?
# sample a few unique tiles and their gx/gy ranges
from collections import defaultdict
by_tile = defaultdict(list)
for i, c in enumerate(m["controls"]):
    by_tile[c[0]].append((c[1], c[2], i))
print("num tiles:", len(by_tile))
t0 = list(by_tile.items())[0]
print("tile sample:", t0[0], "cells:", t0[1][:5], "... total", len(t0[1]))
# check if any tile has >1 control with different gx/gy
multi = [(t, len(v)) for t,v in by_tile.items() if len(v) > 1]
print("tiles with >1 control:", len(multi), "sample:", multi[:3])
