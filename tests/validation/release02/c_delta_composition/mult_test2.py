
import json, os, itertools
import numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
S=json.load(open(os.path.join(BASE,'p2_samples.json')))
CJ=json.load(open(os.path.join(BASE,'p2_corrected.json')))
fid2name={fr['frame_id']:os.path.basename(fr['hips_path']) for fr in CJ['frames']}
from collections import defaultdict
fv=defaultdict(dict)
for o in S['observations']:
    fv[o['frame_id']][o['control_id']]=o['value']
byname={fid2name[f]:f for f in S['frame_ids']}
def find(sub):
    return [n for n in byname if sub in n]
print('names sample:')
for n in sorted(byname)[:4]: print('  ',n)
def bin_test(nameA,nameB,nb=8):
    A=byname[nameA]; B=byname[nameB]
    ca,cb=fv[A],fv[B]
    common=list(ca.keys()&cb.keys())
    x=np.array([cb[c] for c in common]); y=np.array([ca[c] for c in common])
    order=np.argsort(x)
    x=x[order]; y=y[order]
    print('== %s  vs  %s   n=%d'%(nameA[:38],nameB[:38],len(common)))
    print('   level range %.3e .. %.3e (ratio %.1f)'%(x.min(),x.max(),x.max()/max(x.min(),1)))
    idx=np.array_split(np.arange(len(x)),nb)
    print('   bin   level_med    med_ratio   med_diff/level')
    for ii in idx:
        if len(ii)<5: continue
        xx=x[ii]; yy=y[ii]
        r=yy/xx; d=yy-xx; L=(xx+yy)/2
        print('   %4d  %.3e   %.5f     %+.3e'%(len(ii),np.median(L),np.median(r),np.median(d)/np.median(L)))
    # global fits
    A_=np.vstack([x,np.ones_like(x)]).T
    coef,_,_,_=np.linalg.lstsq(A_,y,rcond=None)
    a0=float(np.sum(x*y)/np.sum(x*x))
    print('   full linear a=%.5f c=%.3e ; through-origin a=%.5f ; medratio=%.5f'%(coef[0],coef[1],a0,np.median(y/x)))

# same pointing different telescope M4
n1=find('M42_M4_T2_flying_dutchman-20251216'); n2=find('M42_M4_T3_flying_dutchman-20251216')
if n1 and n2: bin_test(n1[0],n2[0])
# same telescope same pointing different dates
n3=find('M42_M1_T2_flying_dutchman-20251212'); n4=find('M42_M1_T2_flying_dutchman-20251224')
if n3 and n4: bin_test(n3[0],n4[0])
# same telescope different pointing
n5=find('M42_M2_T2_flying_dutchman-20251212'); n6=find('M42_M4_T2_flying_dutchman-20251216')
if n5 and n6: bin_test(n5[0],n6[0])
