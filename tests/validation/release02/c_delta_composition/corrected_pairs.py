
import json, os, numpy as np
from collections import defaultdict
NEW='run/RELEASE-02/L4-rebuild/upmfix_out'
CJ=json.load(open(NEW+'/p2_corrected.json'))
frames=CJ['frames']
tile2f=defaultdict(list)
for fi,fr in enumerate(frames):
    for t in fr['tiles']:
        tile2f[t['tile_ipix']].append((fi, fr['data_file'], int(t['offset'])))
cov=sorted(((len(v),k) for k,v in tile2f.items()), reverse=True)
print('n_tiles',len(tile2f),'top coverage',cov[:5])
tip=cov[0][1]; fl=tile2f[tip]
SPAN=262144
data={}
for fi,path,off in fl:
    a=np.memmap(path,dtype=np.float64,mode='r',offset=off*8,shape=(SPAN,))
    data[fi]=np.array(a)
print('tile',tip,'frames',len(data))
for fi in list(data)[:5]:
    d=data[fi]; fin=np.isfinite(d)
    print('  f%02d fin=%d median=%.4g'%(fi,fin.sum(),np.nanmedian(d)))
# pairwise fits on this tile
fis=sorted(data)
rows=[]
import itertools
for A,B in itertools.combinations(fis,2):
    a=data[A]; b=data[B]
    m=np.isfinite(a)&np.isfinite(b)
    n=int(m.sum())
    if n<5000: continue
    x=b[m]; y=a[m]
    # robust: bin by x
    L=(x+y)/2
    r=y/x; d=y-x
    # linear fit
    A_=np.vstack([x,np.ones_like(x)]).T
    coef,_,_,_=np.linalg.lstsq(A_,y,rcond=None)
    rows.append((A,B,n,float(coef[0]),float(coef[1]),float(np.median(r)),float(np.median(d/L))))
print('pairs with n>=5000:',len(rows))
arr=np.array([[r[3] for r in rows]]).ravel()
print('slope a: min %.4f med %.4f max %.4f'%(arr.min(),np.median(arr),arr.max()))
cc=np.array([abs(r[4]) for r in rows]); ll=np.array([1.0 for r in rows])
print('|c| absolute: med %.3e max %.3e'%(np.median(cc),cc.max()))
for r in sorted(rows,key=lambda z:-abs(z[3]-1))[:10]:
    print('  a=%.4f c=%+.3e medratio=%.4f medd/L=%+.3e n=%d  f%d vs f%d'%(r[3],r[4],r[5],r[6],r[2],r[0],r[1]))
