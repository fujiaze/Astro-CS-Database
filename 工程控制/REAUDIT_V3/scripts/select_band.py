
import json, sys
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import hpx
d = json.load(open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_signal_list.json'))
def cell_dec_range(K, parent):
    lo=90.; hi=-90.
    # sample edges + center coarsely (8x8)
    for yi in range(0,512,64):
        for xi in range(0,512,64):
            ra,dec = hpx.tile_pix2ang(K,parent,xi,yi)
            lo=min(lo,dec); hi=max(hi,dec)
    # sample corners densely
    for x in [0,511]:
        for y in [0,511]:
            ra,dec = hpx.tile_pix2ang(K,parent,x,y)
            lo=min(lo,dec); hi=max(hi,dec)
    return lo,hi
BANDS = {'B12': -15.70, 'B23': -20.85}
W = 0.35  # dec window half-width (deg) to include tiles
sel = {b: [] for b in BANDS}
for (K,parent,rel) in d:
    lo,hi = cell_dec_range(K,parent)
    for b,B in BANDS.items():
        if lo <= B+W and hi >= B-W:
            sel[b].append((K,parent,rel,round(lo,3),round(hi,3)))
for b in BANDS:
    print(b, 'band tiles:', len(sel[b]))
    from collections import Counter
    print('   by order:', dict(Counter(K for K,_,_,_,_ in sel[b])))
json.dump(sel, open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_band_tiles.json','w'))
print('sample B12:', sel['B12'][:5])
print('sample B23:', sel['B23'][:5])
