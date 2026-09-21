
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin')); S=json.load(open(BASE+'/p2_samples.json'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
obs=S['observations']
fi=np.array([FI[o['frame_id']] for o in obs]); ck=np.array([o['control_id'] for o in obs])
val=np.array([o['value'] for o in obs]); civ=np.array([o['control_ivar'] for o in obs])
unc=np.array([abs(o['uncertainty']) for o in obs])
def qf_of(fl):
    if fl&16: return 0.0
    if fl&2: return 0.1
    if fl&1: return 1.0
    return 0.5
qf=np.array([qf_of(o['quality_flags']) for o in obs])
raw=qf*civ; sums=np.bincount(ck,weights=raw,minlength=K)
raw_w=np.where(sums[ck]>0, raw/np.where(sums[ck]>0,sums[ck],1),0.0)
sig=np.maximum(unc,0.001); anchor=0.001; ref=0
print('iter  max_dM      rel_dM     max_dC      rel_dC     objective')
for it in range(15):
    Cobs=C[fi,ck]; r=val-Marr[ck]-Cobs; z=r/sig
    hw=np.where(np.abs(z)<=1.345,1.0,1.345/np.maximum(np.abs(z),1e-300)); w=raw_w*hw
    rho=np.where(np.abs(z)<=1.345,0.5*z*z,1.345*(np.abs(z)-0.5*1.345))
    obj=float(np.sum(raw_w*rho))
    mask=(fi==ref)
    den=np.bincount(ck[mask],weights=w[mask],minlength=K)
    num=np.bincount(ck[mask],weights=w[mask]*(val[mask]-C[ref,ck[mask]]),minlength=K)
    den2=np.bincount(ck,weights=w,minlength=K); num2=np.bincount(ck,weights=w*(val-Cobs),minlength=K)
    fb=den<=1e-12; den=np.where(fb,den2,den); num=np.where(fb,num2,num)
    Mnew=np.where(den>1e-12,num/np.maximum(den,1e-300),Marr)
    dM=np.abs(Mnew-Marr); Marr=Mnew
    dC=0.0
    for f in range(nF):
        if f==ref: continue
        m=(fi==f)
        if not m.any(): continue
        denf=np.bincount(ck[m],weights=w[m],minlength=K)
        rhsf=np.bincount(ck[m],weights=w[m]*(val[m]-Marr[ck[m]]),minlength=K)
        Cn=np.where(denf+anchor>0, rhsf/(denf+anchor), 0.0)
        dC=max(dC,float(np.abs(Cn-C[f]).max())); C[f]=Cn
    print('%3d  %.4e  %.3e  %.4e  %.3e  %.4f'%(it,dM.max(),dM.max()/max(np.abs(Marr).max(),1e-300),dC,dC/max(np.abs(C).max(),1e-300),obj))
