
import json, sys, re
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import hpx
d = json.load(open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_signal_list.json'))
# fix parents in place
d2 = []
for (K,parent,rel) in d:
    m = re.match(r'Norder(\d+)/Dir(\d+)/Npix(\d+)\.fits$', rel)
    dirv = int(m.group(2)); npv = int(m.group(3))
    parent = dirv*10000 + npv
    d2.append((K, parent, rel))
json.dump(d2, open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_signal_list.json','w'))
def rng(K,parent):
    lo=90.; hi=-90.; ralo=360.; rahi=-360.
    for yi in [0,511,128,384,256]:
        for xi in [0,511,128,384,256]:
            ra,dec = hpx.tile_pix2ang(K,parent,xi,yi)
            lo=min(lo,dec); hi=max(hi,dec); ralo=min(ralo,ra); rahi=max(rahi,ra)
    return lo,hi,ralo,rahi
# sanity: order7 field tiles
o7=[t for t in d2 if t[0]==7]
field=0
for (K,p,l) in o7:
    lo,hi,ralo,rahi=rng(K,p)
    if lo>=-27 and hi<=-9 and 268<=ralo and rahi<=278: field+=1
print('o7 field tiles:', field, '/', len(o7))
BANDS={'B12':-15.70,'B23':-20.85}; W=0.35
sel={b:[] for b in BANDS}
for (K,parent,rel) in d2:
    lo,hi,ralo,rahi=rng(K,parent)
    for b,B in BANDS.items():
        if lo<=B+W and hi>=B-W:
            sel[b].append((K,parent,rel,round(lo,3),round(hi,3)))
for b in BANDS:
    from collections import Counter
    print(b,'band tiles:',len(sel[b]),'by order:',dict(Counter(K for K,_,_,_,_ in sel[b])))
json.dump(sel, open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_band_tiles.json','w'))
