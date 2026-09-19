
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
fi=np.array([FI[o['frame_id']] for o in obs]); ck=np.array([o['control_id'] for o in obs])
val=np.array([o['value'] for o in obs]); civ=np.array([o['control_ivar'] for o in obs])
unc=np.array([abs(o['uncertainty']) for o in obs])
def qf_of(fl):
    if fl&16: return 0.0
    if fl&2: return 0.1
    if fl&1: return 1.0
    if fl==0: return 0.5
    return 0.5
qf=np.array([qf_of(o['quality_flags']) for o in obs])
raw=qf*civ; sums=np.bincount(ck,weights=raw,minlength=K)
raw_w=np.where(sums[ck]>0, raw/np.where(sums[ck]>0,sums[ck],1),0.0)
Cobs=C[fi,ck]; r=val-Marr[ck]-Cobs; sig=np.maximum(unc,0.001); z=r/sig
hw=np.where(np.abs(z)<=1.345,1.0,1.345/np.maximum(np.abs(z),1e-300)); w=raw_w*hw
rho=np.where(np.abs(z)<=1.345,0.5*z*z,1.345*(np.abs(z)-0.5*1.345))
obj=float(np.sum(raw_w*rho))
print('objective replicated = %.4f   model objective = %.4f  (ratio %.6f)'%(obj,M['objective'],obj/M['objective']))
# relative dM
ref=0; mask=(fi==ref)
den=np.bincount(ck[mask],weights=w[mask],minlength=K)
num=np.bincount(ck[mask],weights=w[mask]*(val[mask]-C[ref,ck[mask]]),minlength=K)
den2=np.bincount(ck,weights=w,minlength=K); num2=np.bincount(ck,weights=w*(val-Cobs),minlength=K)
fb=den<=1e-12; den=np.where(fb,den2,den); num=np.where(fb,num2,num)
Mnew=np.where(den>1e-12,num/np.maximum(den,1e-300),Marr)
dM=np.abs(Mnew-Marr); rel=dM/np.maximum(np.abs(Marr),1e-300)
print('dM: max %.4g  rel(max|M|) %.4g ; rel dM med %.4g p90 %.4g max %.4g'%(dM.max(),dM.max()/np.abs(Marr).max(),np.median(rel),np.percentile(rel,90),rel.max()))
print()
# ===== delta_k comparison =====
SP=json.load(open(BASE+'/p2_sky_plane.bin'))
deltas=np.array(SP['deltas']); sfids=SP['frame_ids']
assert sfids==frames, 'frame order mismatch'
ra=np.array([c[3] for c in M['controls']]); dec=np.array([c[4] for c in M['controls']])
ra0=SP['ra0_deg']; dec0=SP['dec0_deg']; uc=SP['uc']; vc=SP['vc']; us=SP['us']; vs=SP['vs']
d2r=np.pi/180
r0=ra0*d2r; d0=dec0*d2r
rr=ra*d2r; dd=dec*d2r
cd=np.cos(dd); sd=np.sin(dd); cd0=np.cos(d0); sd0=np.sin(d0)
dl=rr-r0; cdl=np.cos(dl); sdl=np.sin(dl)
cosc=sd0*sd+cd0*cd*cdl
u=cd*sdl/cosc; v=(cd0*sd-sd0*cd*cdl)/cosc
xi=(u-uc)/us; eta=(v-vc)/vs
print('=== delta_k vs C_k at %d controls ==='%K)
print('delta_k = d0 + d1*xi + d2*eta (delta_order=%d)'%SP['delta_order'])
print('frame  rms(delta)  rms(C_k)  corr(C_k,delta)  slope(C~delta)  rms(C_k - slope*delta)')
rows=[]
for f in range(nF):
    if f==ref: continue
    dk=deltas[f,0]+deltas[f,1]*xi+deltas[f,2]*eta
    c=C[f]
    sel=(c!=0)
    if sel.sum()<500: continue
    x=dk[sel]; y=c[sel]
    a=np.polyfit(x,y,1)[0]; rho_=np.corrcoef(x,y)[0,1]
    resid=np.std(y-a*x)
    rows.append((f,np.std(x),np.std(y),rho_,a,resid))
rows.sort(key=lambda z:-abs(z[3]))
for f,sd_,sc,rho_,a,resid in rows[:10]:
    print('f%02d  %.4g  %.4g  %+.3f  %+.3f  %.4g'%(f,sd_,sc,rho_,a,resid))
print('...')
for f,sd_,sc,rho_,a,resid in rows[-4:]:
    print('f%02d  %.4g  %.4g  %+.3f  %+.3f  %.4g'%(f,sd_,sc,rho_,a,resid))
rr_=np.array([z[3] for z in rows]); aa=np.array([z[4] for z in rows])
print('corr: min %.3f med %.3f max %.3f ; slope: min %+.3f med %+.3f max %+.3f'%(rr_.min(),np.median(rr_),rr_.max(),aa.min(),np.median(aa),aa.max()))
print('frac |corr|>0.5: %.2f ; frac slope in [0.5,2]: %.2f'%(np.mean(np.abs(rr_)>0.5),np.mean((aa>0.5)&(aa<2))))
