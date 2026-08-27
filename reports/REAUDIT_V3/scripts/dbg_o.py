
import json, sys
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import hpx
d = json.load(open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_signal_list.json'))
o7 = [(K,p,l) for (K,p,l) in d if K==7][:12]
for (K,p,l) in o7:
    c = hpx.tile_pix2ang(K,p,256,256)
    lo=90.; hi=-90.; ralo=360.; rahi=-360.
    for yi in [0,511,256]:
        for xi in [0,511,256]:
            ra,dec = hpx.tile_pix2ang(K,p,xi,yi)
            if dec<lo: lo=dec
            if dec>hi: hi=dec
            ralo=min(ralo,ra); rahi=max(rahi,ra)
    print('o7 p=%d  %s  center=(%.2f,%.2f) dec[%.2f,%.2f] ra[%.2f,%.2f]' % (p,l,c[0],c[1],lo,hi,ralo,rahi))
print('--- o6 sample ---')
o6 = [(K,p,l) for (K,p,l) in d if K==6][:6]
for (K,p,l) in o6:
    c = hpx.tile_pix2ang(K,p,256,256)
    print('o6 p=%d %s center=(%.2f,%.2f)' % (p,l,c[0],c[1]))
print('--- o5 sample ---')
o5 = [(K,p,l) for (K,p,l) in d if K==5][:6]
for (K,p,l) in o5:
    c = hpx.tile_pix2ang(K,p,256,256)
    print('o5 p=%d %s center=(%.2f,%.2f)' % (p,l,c[0],c[1]))
