
import json, sys
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import hpx
d = json.load(open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/C_signal_list.json'))
def rng(K,parent):
    lo=90.; hi=-90.
    for yi in range(0,512,32):
        for xi in range(0,512,32):
            ra,dec = hpx.tile_pix2ang(K,parent,xi,yi)
            if dec<lo: lo=dec
            if dec>hi: hi=dec
    return lo,hi
# order6 and 7 tiles in the field
o6 = [(K,p,l) for (K,p,l) in d if K==6]
o7 = [(K,p,l) for (K,p,l) in d if K==7]
print('order6 n=',len(o6),' order7 n=',len(o7))
for (K,p,l) in o6:
    lo,hi=rng(K,p)
    if hi>=-17 and lo<=-14:
        print('  o6 p=%d dec[%.3f,%.3f] %s' % (p,lo,hi,l))
print('--- order7 near -15.7/-20.85 ---')
cnt=0
for (K,p,l) in o7:
    lo,hi=rng(K,p)
    if hi>=-16.5 and lo<=-14.9:
        print('  o7 p=%d dec[%.3f,%.3f] %s' % (p,lo,hi,l)); cnt+=1
print('  o7 near B12:',cnt)
# how many o7 tiles in whole field [-27,-9]
field=0
for (K,p,l) in o7:
    lo,hi=rng(K,p)
    if lo>=-27 and hi<=-9: field+=1
print('  o7 field tiles:', field, '/', len(o7))
