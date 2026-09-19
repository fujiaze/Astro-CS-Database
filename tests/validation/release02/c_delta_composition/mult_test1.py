
import json, os, sys, itertools
import numpy as np

BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
S=json.load(open(os.path.join(BASE,'p2_samples.json')))
CJ=json.load(open(os.path.join(BASE,'p2_corrected.json')))
fid2name={}
for fr in CJ['frames']:
    fid2name[fr['frame_id']]=os.path.basename(fr['hips_path'])
fids=S['frame_ids']
print('n_frames',len(fids))
obs=S['observations']
# build per-frame dict control->value
from collections import defaultdict
fv=defaultdict(dict); fu=defaultdict(dict)
for o in obs:
    fv[o['frame_id']][o['control_id']]=o['value']
    fu[o['frame_id']][o['control_id']]=o['uncertainty']
cnt={f:len(fv[f]) for f in fids}
print('obs per frame min/med/max', min(cnt.values()), int(np.median(list(cnt.values()))), max(cnt.values()))
# telescope/pointing tag
def tag(name):
    # M42_M1_T2_...
    parts=name.split('_')
    return parts[1], parts[2]  # M?, T?
names={f:fid2name.get(f,'?') for f in fids}
for f in fids[:6]: print(' ',f,names[f],cnt[f])
# pairwise fits
rows=[]
for A,B in itertools.combinations(fids,2):
    ca=fv[A]; cb=fv[B]
    common=ca.keys()&cb.keys()
    n=len(common)
    if n<30: continue
    cl=list(common)
    x=np.array([cb[c] for c in cl]); y=np.array([ca[c] for c in cl])
    # least squares y = a x + c
    A_=np.vstack([x,np.ones_like(x)]).T
    coef,res,rank,sv=np.linalg.lstsq(A_,y,rcond=None)
    a,c=coef
    pred=a*x+c
    rms=float(np.sqrt(np.mean((y-pred)**2)))
    ratio=y/x
    rows.append(dict(A=A,B=B,n=n,a=float(a),c=float(c),rms=rms,
                     medratio=float(np.median(ratio)),
                     tA=tag(names[A]),tB=tag(names[B]),
                     nA=names[A],nB=names[B],
                     mA=float(np.median(y)),mB=float(np.median(x))))
print('n_pairs',len(rows))
a_arr=np.array([r['a'] for r in rows])
print('slope a: min %.4f med %.4f max %.4f'%(a_arr.min(),np.median(a_arr),a_arr.max()))
# relative deviation of a from 1
dev=np.abs(a_arr-1.0)
print('|a-1|: med %.4f p90 %.4f max %.4f'%(np.median(dev),np.percentile(dev,90),dev.max()))
# same-telescope vs cross-telescope
same=[r for r in rows if r['tA']==r['tB']]
cross=[r for r in rows if r['tA']!=r['tB']]
for lab,sub in [('same-T',same),('cross-T',cross)]:
    if sub:
        aa=np.array([r['a'] for r in sub])
        print('%s n=%d  a: min %.4f med %.4f max %.4f ; |a-1| med %.4f'%(lab,len(sub),aa.min(),np.median(aa),aa.max(),np.median(np.abs(aa-1))))
# how well does pure multiplicative (c=0) do vs full linear?
# c significance relative to level
print()
print('--- sample of extreme pairs (sorted by |a-1|) ---')
rows_s=sorted(rows,key=lambda r:-abs(r['a']-1))
for r in rows_s[:12]:
    print('a=%.4f c=%+.3e rms=%.3e medratio=%.4f n=%d | %s vs %s'%(r['a'],r['c'],r['rms'],r['medratio'],r['n'],r['nA'][:32],r['nB'][:32]))
print()
print('--- c magnitude vs level ---')
cc=np.array([abs(r['c']) for r in rows]); lv=np.array([r['mA'] for r in rows])
print('|c|/level: med %.3e p90 %.3e'%(np.median(cc/lv),np.percentile(cc/lv,90)))
json.dump(rows,open('run/RELEASE-02/c-delta/pair_fits.json','w'),indent=0)
print('saved pair_fits.json')
