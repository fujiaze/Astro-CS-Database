
import json, os, numpy as np, itertools
from collections import defaultdict
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=CJ['frames']
fid2name={fr['frame_id']:os.path.basename(fr['hips_path']) for fr in CJ['frames']}
tile2f=defaultdict(list)
for fi,fr in enumerate(frames):
    for t in fr['tiles']:
        tile2f[t['tile_ipix']].append((fi, fr['data_file'], int(t['offset'])))
SPAN=262144
gk=json.load(open('run/RELEASE-02/c-delta/gk_est.json'))
G={int(f):float(g) for f,g in zip(gk['frames'],gk['g'])}
print('g_k global: min %.4f med %.4f max %.4f'%(min(G.values()),np.median(list(G.values())),max(G.values())))
# map global idx to name
for i in sorted(G,key=lambda k:G[k])[:3]+sorted(G,key=lambda k:G[k])[-3:]:
    print('   f%02d g=%.4f  %s'%(i,G[i],fid2name[frames[i]['frame_id']]))
cov=sorted(((len(v),k) for k,v in tile2f.items()), reverse=True)
tiles=[k for _,k in cov[:8]]
def load(tip):
    d={}
    for fi,path,off in tile2f[tip]:
        a=np.memmap(path,dtype=np.float64,mode='r',offset=off*8,shape=(SPAN,))
        d[fi]=np.array(a)
    return d
before=[]; after=[]
for tip in tiles:
    d=load(tip)
    fis=sorted(d)
    for A,B in itertools.combinations(fis,2):
        a=d[A]; b=d[B]; m=np.isfinite(a)&np.isfinite(b)
        if m.sum()<5000: continue
        x=a[m]; y=b[m]
        lev=np.median((x+y)/2)
        if lev<=0: continue
        db=np.median(np.abs(x-y))/lev
        xa=x/G[A]; ya=y/G[B]
        da=np.median(np.abs(xa-ya))/lev
        before.append(db); after.append(da)
    del d
before=np.array(before); after=np.array(after)
print('pairs',len(before))
print('rel |A-B|/level : BEFORE median %.4f  mean %.4f  p90 %.4f'%(np.median(before),before.mean(),np.percentile(before,90)))
print('rel |A/gA-B/gB| : AFTER  median %.4f  mean %.4f  p90 %.4f'%(np.median(after),after.mean(),np.percentile(after,90)))
print('reduction factor (median before/after): %.2fx ; (mean): %.2fx'%(np.median(before)/max(np.median(after),1e-12), before.mean()/max(after.mean(),1e-12)))
json.dump({'before_median':float(np.median(before)),'after_median':float(np.median(after)),
           'before_mean':float(before.mean()),'after_mean':float(after.mean())},
          open('run/RELEASE-02/c-delta/counterfactual_gk.json','w'),indent=1)
