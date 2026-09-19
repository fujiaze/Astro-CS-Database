
import json, os, sys, numpy as np
sys.path.insert(0,'run/RELEASE-02/trail')
from layout import leaf_to_fits, SPAN
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
S=json.load(open(BASE+'/p2_samples.json'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
ctr={c['control_id']:c for c in S['controls']}
fid2name={fr['frame_id']:os.path.basename(fr['hips_path']) for fr in CJ['frames']}
fid2idx={fr['frame_id']:i for i,fr in enumerate(CJ['frames'])}
# frame -> tile offset map
ftile={}
for fr in CJ['frames']:
    ftile[fr['frame_id']]={t['tile_ipix']:int(t['offset']) for t in fr['tiles']}
def corrected(fid, leaf):
    tip=leaf//SPAN; loc=leaf%SPAN; fi=leaf_to_fits(loc)
    fr=CJ['frames'][fid2idx[fid]]
    if tip not in ftile[fid]: return np.nan
    off=ftile[fid][tip]
    mm=np.memmap(fr['data_file'],dtype=np.float64,mode='r',offset=(off+int(fi))*8,shape=(1,))
    return float(mm[0])
# build corrected matrix (sparse) at controls
obs=S['observations']
rows=[];cols=[];vals=[];raws=[]
cframes=sorted(set(o['frame_id'] for o in obs))
for o in obs:
    c=o['control_id']; fid=o['frame_id']
    leaf=ctr[c]['leaf_ipix']
    v=corrected(fid,leaf)
    if np.isfinite(v):
        rows.append(fid);cols.append(c);vals.append(v);raws.append(o['value'])
rows=np.array(rows); cols=np.array(cols); vals=np.array(vals); raws=np.array(raws)
print('n corrected obs',len(vals))
uF=sorted(set(rows)); uC=sorted(set(cols))
Fi={f:i for i,f in enumerate(uF)}; Ci={c:i for i,c in enumerate(uC)}
print('frames',len(uF),'controls',len(uC))
# additive-only residual: for each control, value across frames; model = per-control mean
from collections import defaultdict
byc=defaultdict(list)
for f,c,v in zip(rows,cols,vals): byc[c].append((f,v))
# per-control relative spread
rel=[]
for c,lst in byc.items():
    if len(lst)<2: continue
    vv=np.array([x[1] for x in lst])
    m=np.median(vv)
    if m>0: rel.append(np.std(vv)/m)
rel=np.array(rel)
print('per-control frame-to-frame rel spread (corrected): median %.4f p90 %.4f'%(np.median(rel),np.percentile(rel,90)))
# rank-1 multiplicative fit corrected_k(c) = g_k * s_c  (log-space alternating)
# build dense-ish via lists
lC=defaultdict(list)
for f,c,v in zip(rows,cols,vals): lC[c].append((Fi[f],v))
s=np.ones(len(uF))
lg=np.zeros(len(uF))
for it in range(200):
    # s_c = sum_k g_k v / sum g_k^2
    num=np.zeros(len(uC)); den=np.zeros(len(uC))
    for c,lst in lC.items():
        ci=Ci[c]
        for fi,v in lst:
            num[ci]+=np.exp(lg[fi])*v; den[ci]+=np.exp(lg[fi])**2
    snew=np.where(den>0,num/np.where(den>0,den,1),1.0)
    # g_k = sum_c s_c v / sum s_c^2
    num2=np.zeros(len(uF)); den2=np.zeros(len(uF))
    for c,lst in lC.items():
        ci=Ci[c]; sc=snew[ci]
        for fi,v in lst:
            num2[fi]+=sc*v; den2[fi]+=sc*sc
    lgn=np.log(np.where(den2>0,num2/np.where(den2>0,den2,1),1.0))
    lgn-=lgn.mean()
    if np.max(np.abs(lgn-lg))<1e-10: lg=lgn; s=snew; break
    lg=lgn; s=snew
g=np.exp(lg)
print('rank-1 multiplicative g_k on CORRECTED controls: min %.4f med %.4f max %.4f'%(g.min(),np.median(g),g.max()))
# residual after rank-1
res=[]; resadd=[]
for c,lst in lC.items():
    ci=Ci[c]; sc=s[ci]
    for fi,v in lst:
        pred=np.exp(lg[fi])*sc
        if pred>0: res.append((v-pred)/pred)
print('rank-1 multiplicative residual (corrected): rms %.4f  median|.| %.4f'%(np.sqrt(np.mean(np.array(res)**2)),np.median(np.abs(res))))
# additive-only: residual = v - s_c (per-control median)
res2=[]
for c,lst in lC.items():
    vv=np.array([v for fi,v in lst]); m=np.median(vv)
    if m>0: res2.extend(((vv-m)/m).tolist())
print('additive-only (per-control const) residual: rms %.4f  median|.| %.4f'%(np.sqrt(np.mean(np.array(res2)**2)),np.median(np.abs(res2))))
json.dump({'frames':uF,'g':g.tolist(),'names':[fid2name[f] for f in uF]},open('run/RELEASE-02/c-delta/gk_controls.json','w'),indent=0)
