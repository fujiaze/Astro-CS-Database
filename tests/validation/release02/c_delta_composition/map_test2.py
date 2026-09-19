
import json, numpy as np, sys
sys.path.insert(0,'run/RELEASE-02/trail')
from layout import leaf_to_fits, SPAN
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
mf=M['frames']; cf=[fr['frame_id'] for fr in CJ['frames']]
print('model frames == corrected frames order?', mf==cf)
print('model[0:4]',mf[:4]); print('corr [0:4]',cf[:4])
same=sum(1 for a,b in zip(mf,cf) if a==b); print('same positions',same,'/',len(mf))
FI={f:i for i,f in enumerate(mf)}; CI={f:i for i,f in enumerate(cf)}
ctrl=M['controls']; Marr=np.array([c[5] for c in ctrl])
C=np.zeros((len(mf),len(ctrl)))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
ftile={}
for i,fr in enumerate(CJ['frames']): ftile[i]={t['tile_ipix']:int(t['offset']) for t in fr['tiles']}
cpath=[fr['data_file'] for fr in CJ['frames']]
def read_pix(ci, tip, idx):
    if tip not in ftile[ci]: return np.nan
    off=ftile[ci][tip]
    mm=np.memmap(cpath[ci],dtype=np.float64,mode='r',offset=(off+int(idx))*8,shape=(1,))
    return float(mm[0])
obs=S['observations']
print()
print(' ctrl      frame  raw_obs      M         raw-C       corr_pix    ratio')
cnt=0
for o in obs:
    mi=FI[o['frame_id']]; ci=CI[o['frame_id']]; ck=o['control_id']
    leaf=ctrl[ck][6]; tip=leaf//SPAN; loc=int(leaf%SPAN)
    cp=read_pix(ci,tip,leaf_to_fits(loc))
    if not np.isfinite(cp): continue
    rc=o['value']-C[mi,ck]
    print('c%05d  f%02d  %.5g  %.5g  %.5g  %.5g  %.4f'%(ck,mi,o['value'],Marr[ck],rc,cp,cp/rc))
    cnt+=1
    if cnt>=12: break
print('found',cnt)
