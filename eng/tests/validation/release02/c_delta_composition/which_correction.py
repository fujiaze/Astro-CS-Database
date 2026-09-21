
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin')); S=json.load(open(BASE+'/p2_samples.json')); SP=json.load(open(BASE+'/p2_sky_plane.bin'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
deltas=np.array(SP['deltas'])
ra=np.array([c[3] for c in M['controls']]); dec=np.array([c[4] for c in M['controls']])
d2r=np.pi/180; r0=SP['ra0_deg']*d2r; d0=SP['dec0_deg']*d2r
rr=ra*d2r; dd=dec*d2r; cd=np.cos(dd); sd=np.sin(dd); cd0=np.cos(d0); sd0=np.sin(d0); dl=rr-r0
cosc=sd0*sd+cd0*cd*np.cos(dl); u=cd*np.sin(dl)/cosc; v=(cd0*sd-sd0*cd*np.cos(dl))/cosc
xi=(u-SP['uc'])/SP['us']; eta=(v-SP['vc'])/SP['vs']
D=np.array([deltas[f,0]+deltas[f,1]*xi+deltas[f,2]*eta for f in range(nF)])
# per (frame,control) observed value = weighted mean of obs
obs=S['observations']
val=np.array([o['value'] for o in obs]); ck=np.array([o['control_id'] for o in obs]); fi=np.array([FI[o['frame_id']] for o in obs])
V=np.full((nF,K),np.nan)
for f in range(nF):
    m=(fi==f)
    if not m.any(): continue
    s=np.bincount(ck[m],weights=val[m],minlength=K); n=np.bincount(ck[m],minlength=K)
    V[f]=(np.where(n>0,s/np.maximum(n,1),np.nan))
ok=(~np.isnan(V)).sum(0)
print('control coverage: >=2 frames: %d ; >=3: %d'%((ok>=2).sum(),(ok>=3).sum()))
def spread(X,lo=False):
    out=[]
    for k in range(K):
        col=X[:,k]; s=col[~np.isnan(col)]
        if s.size<2 or Marr[k]<=1e11: continue
        if lo:
            t=s.sum(); n=s.size
            out.append(np.median([abs(x-(t-x)/(n-1)) for x in s])/Marr[k])
        else:
            out.append(np.std(s)/Marr[k])
    return np.array(out)
cases={
 'raw (no correction)':V,
 'raw - delta_k':V-D,
 'raw - C_k':V-C,
 'raw - C_k - delta_k (PRODUCTION)':V-C-D,
}
print()
print('%-34s %10s %10s'%('case','std%%med','LOO-step%%med'))
for name,X in cases.items():
    a=spread(X,False); b=spread(X,True)
    print('%-34s %9.3f%% %9.3f%%'%(name,100*np.median(a),100*np.median(b)))
