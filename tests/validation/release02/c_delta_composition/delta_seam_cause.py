
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
SP=json.load(open(BASE+'/p2_sky_plane.bin'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
deltas=np.array(SP['deltas'])
# evaluate delta_k at each control
ra=np.array([c[3] for c in M['controls']]); dec=np.array([c[4] for c in M['controls']])
d2r=np.pi/180; r0=SP['ra0_deg']*d2r; d0=SP['dec0_deg']*d2r
rr=ra*d2r; dd=dec*d2r
cd=np.cos(dd); sd=np.sin(dd); cd0=np.cos(d0); sd0=np.sin(d0); dl=rr-r0
cosc=sd0*sd+cd0*cd*np.cos(dl)
u=cd*np.sin(dl)/cosc; v=(cd0*sd-sd0*cd*np.cos(dl))/cosc
xi=(u-SP['uc'])/SP['us']; eta=(v-SP['vc'])/SP['vs']
D=np.array([deltas[f,0]+deltas[f,1]*xi+deltas[f,2]*eta for f in range(nF)])  # (nF,K)
# per-control frames covering (C!=0)
print('=== per-control: production corrected = M - delta_k (C already aligned frames) ===')
spreads=[]; loosteps=[]; bgs=[]; dspread=[]
for k in range(K):
    sel=np.nonzero(C[:,k]!=0)[0]
    if sel.size<2: continue
    dk=D[sel,k]
    m=Marr[k]
    if m<=1e11: continue
    spreads.append(float(np.std(dk)))
    dspread.append(float(np.std(dk)/m))
    # leave-one-out step magnitude
    tot=np.sum(dk)
    n=sel.size
    l=[]
    for x in dk:
        others=(tot-x)/(n-1)
        l.append(abs(x-others))
    loosteps.append(float(np.median(l)))
    bgs.append(float(m))
spreads=np.array(spreads); dspread=np.array(dspread); loosteps=np.array(loosteps); bgs=np.array(bgs)
print('n controls',len(spreads))
print('per-control std(delta_k): med %.4g  = %.3f%% of M (med)'%(np.median(spreads),100*np.median(dspread)))
print('leave-one-out step |delta_k - mean(others)|: med %.4g  = %.3f%% of M'%(np.median(loosteps),100*np.median(loosteps/bgs)))
print('p90 leave-one-out step %%: %.3f%%'%(100*np.percentile(loosteps/bgs,90)))
print()
print('=== contrast: if only C is applied, corrected = M for every frame (spread=0 by construction) ===')
# quantify: how much does delta add vs C?
print('rms(C)=%.4g  rms(delta)=%.4g  => delta is %.0f%% of C magnitude'%(np.sqrt(np.mean(C**2)),np.sqrt(np.mean(D**2)),100*np.sqrt(np.mean(D**2))/np.sqrt(np.mean(C**2))))
# correlation of delta with M (is delta a multiplicative residual too?)
sel=Marr>1e11
cs=[]; ss=[]
for f in range(nF):
    if f==0: continue
    d=D[f][sel]; m=Marr[sel]
    cs.append(np.corrcoef(d,m)[0,1]); ss.append(np.polyfit(m,d,1)[0])
print('corr(delta_k, M): med %+.3f ; slope: med %+.4f'%(np.median(cs),np.median(ss)))
