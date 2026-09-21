
import json, numpy as np, sys
sys.path.insert(0,'run/RELEASE-02/trail')
from layout import leaf_to_fits, SPAN
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
ctrl=M['controls']
Marr=np.array([c[5] for c in ctrl])
print('M stats: min %.4g max %.4g med %.4g  nzero=%d'%(Marr.min(),Marr.max(),np.median(Marr),(Marr==0).sum()))
# C dense
C=np.zeros((len(frames),len(ctrl)))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
print('C stats: min %.4g max %.4g med %.4g  nnz=%d'%(C.min(),C.max(),np.median(C),(C!=0).sum()))
# build tile offset map per frame idx
ftile={}
for i,fr in enumerate(CJ['frames']):
    ftile[i]={t['tile_ipix']:int(t['offset']) for t in fr['tiles']}
cpath=[fr['data_file'] for fr in CJ['frames']]
def corr_pix(fi, leaf):
    tip=leaf//SPAN; loc=leaf%SPAN; fidx=leaf_to_fits(loc)
    if tip not in ftile[fi]: return np.nan
    off=ftile[fi][tip]
    mm=np.memmap(cpath[fi],dtype=np.float64,mode='r',offset=(off+int(fidx))*8,shape=(1,))
    return float(mm[0])
# residual check on a sample of observations
obs=S['observations']
rng=np.random.default_rng(0)
idx=rng.choice(len(obs),4000,replace=False)
rows=[]
for i in idx:
    o=obs[i]; fi=FI[o['frame_id']]; ck=o['control_id']
    r=o['value']-Marr[ck]-C[fi,ck]
    sig=max(abs(o['uncertainty']),1e-3)
    rows.append((abs(r)/abs(o['value']), abs(r)/sig, o['value'], C[fi,ck], Marr[ck]))
rows=np.array(rows)
print('residual |r|/|value|: med %.4f p90 %.4f'%(np.median(rows[:,0]),np.percentile(rows[:,0],90)))
print('residual |r|/sigma : med %.3f p90 %.3f'%(np.median(rows[:,1]),np.percentile(rows[:,1],90)))
# compare corrected pixel vs raw obs for same (frame,control)
print()
print('--- corrected-pixel vs raw-obs at control leaves ---')
cnt=0
for o in obs[:200000]:
    fi=FI[o['frame_id']]; ck=o['control_id']
    cp=corr_pix(fi, ctrl[ck][6])
    if not np.isfinite(cp): continue
    print('f%02d c%05d raw_obs=%.5g C=%+.4g M=%.5g  corr_pix=%.5g  raw-C=%.5g'%(fi,ck,o['value'],C[fi,ck],Marr[ck],cp,o['value']-C[fi,ck]))
    cnt+=1
    if cnt>=8: break
