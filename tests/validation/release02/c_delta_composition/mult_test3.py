
import json, os, numpy as np, itertools
from collections import defaultdict
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=CJ['frames']
tile2f=defaultdict(list)
for fi,fr in enumerate(frames):
    for t in fr['tiles']:
        tile2f[t['tile_ipix']].append((fi, fr['data_file'], int(t['offset'])))
SPAN=262144
cov=sorted(((len(v),k) for k,v in tile2f.items()), reverse=True)
tiles=[k for _,k in cov[:8]]
# load top tile fully
tip=tiles[0]
def load(tip):
    d={}
    for fi,path,off in tile2f[tip]:
        a=np.memmap(path,dtype=np.float64,mode='r',offset=off*8,shape=(SPAN,))
        d[fi]=np.array(a)
    return d
data=load(tip)
print('### LEVEL-BINNING on corrected bins (tile %d) ###'%tip)
def bin_test(A,B,nb=6):
    a=data[A]; b=data[B]; m=np.isfinite(a)&np.isfinite(b)
    x=b[m]; y=a[m]
    order=np.argsort(x); x=x[order]; y=y[order]
    print(' pair f%d vs f%d n=%d  level %.3e..%.3e (x%.1f)'%(A,B,len(x),x.min(),x.max(),x.max()/max(x.min(),1)))
    print('   level_med     med_ratio   med_diff/L   med_diff')
    for ii in np.array_split(np.arange(len(x)),nb):
        xx=x[ii]; yy=y[ii]; L=(xx+yy)/2
        print('   %.3e   %.5f     %+.3e   %+.3e'%(np.median(L),np.median(yy/xx),np.median(yy-xx)/np.median(L),np.median(yy-xx)))
# find the pair with max |slope-1|
best=None
for A,B in itertools.combinations(sorted(data),2):
    a=data[A]; b=data[B]; m=np.isfinite(a)&np.isfinite(b)
    if m.sum()<20000: continue
    x=b[m]; y=a[m]
    s=float(np.sum(x*y)/np.sum(x*x))
    if best is None or abs(s-1)>abs(best[2]-1): best=(A,B,s)
print('extreme pair:',best)
bin_test(best[0],best[1])
# also a near-1 pair
best2=None
for A,B in itertools.combinations(sorted(data),2):
    a=data[A]; b=data[B]; m=np.isfinite(a)&np.isfinite(b)
    if m.sum()<20000: continue
    x=b[m]; y=a[m]; s=float(np.sum(x*y)/np.sum(x*x))
    if best2 is None or abs(s-1)<abs(best2[2]-1): best2=(A,B,s)
print('near-unity pair:',best2)
bin_test(best2[0],best2[1])

print()
print('### global pairwise median-ratio matrix over %d tiles -> g_k ###'%len(tiles))
logr=defaultdict(list)
for tip in tiles:
    d=load(tip)
    fis=sorted(d)
    for A,B in itertools.combinations(fis,2):
        a=d[A]; b=d[B]; m=np.isfinite(a)&np.isfinite(b)
        if m.sum()<5000: continue
        x=b[m]; y=a[m]
        # background-restricted robust ratio: use lower half of level
        L=(x+y)/2
        q=np.quantile(L,0.5)
        sel=L<=q
        r=np.median(y[sel]/x[sel])
        if r>0 and np.isfinite(r):
            logr[(A,B)].append(np.log(r))
    del d
# least squares for log g
fis=set()
for (A,B) in logr: fis.add(A); fis.add(B)
fis=sorted(fis)
idx={f:i for i,f in enumerate(fis)}
rows=[];rhs=[]
for (A,B),vals in logr.items():
    v=float(np.mean(vals))
    r=np.zeros(len(fis)); r[idx[A]]=1; r[idx[B]]=-1
    rows.append(r); rhs.append(v)
R=np.array(rows); yv=np.array(rhs)
# gauge: mean log g = 0
Rg=np.vstack([R, np.ones((1,len(fis)))])
yg=np.concatenate([yv,[0.0]])
lg,_,_,_=np.linalg.lstsq(Rg,yg,rcond=None)
g=np.exp(lg)
print('n_frames',len(fis))
print('g_k: min %.4f med %.4f max %.4f'%(g.min(),np.median(g),g.max()))
order=np.argsort(g)
for i in order:
    print('   f%02d  g=%.4f'%(fis[i],g[i]))
np.save('run/RELEASE-02/c-delta/gk_est.npy', np.vstack([np.array(fis),g]))
json.dump({'frames':fis,'g':g.tolist()},open('run/RELEASE-02/c-delta/gk_est.json','w'),indent=0)
