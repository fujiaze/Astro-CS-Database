
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
obs=S['observations']
fi=np.array([FI[o['frame_id']] for o in obs])
ck=np.array([o['control_id'] for o in obs])
val=np.array([o['value'] for o in obs])
civ=np.array([o['control_ivar'] for o in obs])
unc=np.array([abs(o['uncertainty']) for o in obs])
qf=np.array([1.0 if (o['quality_flags']&16)==0 else 0.0 for o in obs])
# quality_factor per source
def qf_of(fl):
    if fl&16: return 0.0
    if fl&2: return 0.1
    if fl&1: return 1.0
    if fl==0: return 0.5
    return 0.5
qf=np.array([qf_of(o['quality_flags']) for o in obs])
raw=qf*civ
sums=np.bincount(ck,weights=raw,minlength=K)
raw_w=np.where(sums[ck]>0, raw/np.where(sums[ck]>0,sums[ck],1),0.0)
print('raw_w nonzero',int((raw_w>0).sum()),'of',len(raw_w))
# residual at saved state
Cobs=C[fi,ck]
r=val-Marr[ck]-Cobs
sig=np.maximum(unc,0.001)
z=r/sig
hw=np.where(np.abs(z)<=1.345,1.0,1.345/np.maximum(np.abs(z),1e-300))
w=raw_w*hw
print('z=r/sigma: med %.3f p90 %.3f max %.1f'%(np.median(np.abs(z)),np.percentile(np.abs(z),90),np.abs(z).max()))
print('huber_w: med %.4f p90 %.4f min %.3g'%(np.median(hw),np.percentile(hw,90),hw.min()))
print('|r|/value: med %.5f'%np.median(np.abs(r)/np.abs(val)))
# --- M update ---
ref=0
mask_ref=(fi==ref)
den=np.bincount(ck[mask_ref],weights=w[mask_ref],minlength=K)
num=np.bincount(ck[mask_ref],weights=w[mask_ref]*(val[mask_ref]-C[ref,ck[mask_ref]]),minlength=K)
fallback=den<=1e-12
den2=np.bincount(ck,weights=w,minlength=K)
num2=np.bincount(ck,weights=w*(val-Cobs),minlength=K)
den=np.where(fallback,den2,den); num=np.where(fallback,num2,num)
Mnew=np.where(den>1e-12,num/np.maximum(den,1e-300),Marr)
dM=np.abs(Mnew-Marr)
print()
print('=== ONE ITERATION from saved state ===')
print('max_dM = %.6g   (tolerance=1e-6)  ratio=%.3g'%(dM.max(),dM.max()/1e-6))
print('  dM percentiles p50 %.4g p90 %.4g max %.4g ; |M| max %.4g  ULP(max|M|)=%.4g'%(np.percentile(dM,50),np.percentile(dM,90),dM.max(),np.abs(Marr).max(),np.spacing(np.abs(Marr).max())))
# --- C update ---
anchor=0.001
dC=0.0
for f in range(nF):
    if f==ref: continue
    m=(fi==f)
    if not m.any(): continue
    denf=np.bincount(ck[m],weights=w[m],minlength=K)
    rhsf=np.bincount(ck[m],weights=w[m]*(val[m]-Mnew[ck[m]]),minlength=K)
    Cnew=np.where(denf+anchor>0, rhsf/(denf+anchor), 0.0)
    d=np.abs(Cnew-C[f]); dC=max(dC,d.max())
print('max_dC = %.6g   (tolerance=1e-6)  ratio=%.3g'%(dC,dC/1e-6))
print('  |C| max %.4g  ULP(max|C|)=%.4g'%(np.abs(C).max(),np.spacing(np.abs(C).max())))
