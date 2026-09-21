
import json, numpy as np, sys
sys.path.insert(0,'run/RELEASE-02/trail')
from layout import leaf_to_fits, fits_to_local, SPAN
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
ctrl=M['controls']; Marr=np.array([c[5] for c in ctrl])
C=np.zeros((len(frames),len(ctrl)))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
ftile={}
for i,fr in enumerate(CJ['frames']): ftile[i]={t['tile_ipix']:int(t['offset']) for t in fr['tiles']}
cpath=[fr['data_file'] for fr in CJ['frames']]
def read_pix(fi, tip, idx):
    if tip not in ftile[fi]: return np.nan
    off=ftile[fi][tip]
    mm=np.memmap(cpath[fi],dtype=np.float64,mode='r',offset=(off+int(idx))*8,shape=(1,))
    return float(mm[0])
obs=S['observations']
print(' ctrl      frame  raw_obs      M         raw-C       corr[A=fits]  corr[B=nested]  corr[C=local?]')
for o in obs[:4000:137]:
    fi=FI[o['frame_id']]; ck=o['control_id']
    leaf=ctrl[ck][6]; tip=leaf//SPAN; loc=int(leaf%SPAN)
    a=read_pix(fi,tip,leaf_to_fits(loc))
    b=read_pix(fi,tip,loc)
    c=read_pix(fi,tip,fits_to_local(loc))
    print('c%05d  f%02d  %.5g  %.5g  %.5g  %.5g  %.5g  %.5g'%(ck,fi,o['value'],Marr[ck],o['value']-C[fi,ck],a,b,c))
