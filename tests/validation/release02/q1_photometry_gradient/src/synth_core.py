#!/usr/bin/env python3
"""Q1 synthetic core (final): does single-scalar photometric calibration remove the additive gradient?

  version A (pure additive)     y_k = a_k*s + g_k + n
  version B (modulated by a_k)  y_k = a_k*(s + g_k) + n
Phase1 calib: one scalar a_hat_k from stars, z_k = y_k / a_hat_k.
Metrics restricted to star-rejected background control points (PMM-style).
"""
import json, math, os
import numpy as np
from scipy.ndimage import gaussian_filter

rng = np.random.default_rng(20260919)
OUT = os.path.dirname(os.path.abspath(__file__))
W=H=512; FW=FH=320; NFRAME=6; NOISE=5.0
offs=[(0,0),(120,30),(40,140),(170,170),(0,120),(120,0)]
yy,xx=np.mgrid[0:H,0:W].astype(float)
xn=(xx-W/2)/(W/2); yn=(yy-H/2)/(H/2)
s=1000.0+60*xn+35*yn+40*xn*yn+25*(xn**2-yn**2)
for (cx,cy,amp,sig) in [(180,200,900.,70.),(360,330,500.,55.),(300,120,350.,45.)]:
    s+=amp*np.exp(-((xx-cx)**2+(yy-cy)**2)/(2*sig**2))
PSF=2.0; NSRC=400
sx=rng.uniform(4,W-5,NSRC); sy=rng.uniform(4,H-5,NSRC); sflux=10**rng.uniform(2.0,4.3,NSRC)
star=np.zeros_like(s)
for i in range(NSRC): star+=sflux[i]*np.exp(-((xx-sx[i])**2+(yy-sy[i])**2)/(2*PSF**2))
s_true=s+star; S0=float(np.median(s))
BGMASK=(star<2.0)&(s<S0+80.0)
a_true=np.array([0.85,1.05,0.92,1.20,1.10,0.80])
gpp=0.04*S0; grads=[]
for k in range(NFRAME):
    c=rng.uniform(-1,1,6)
    raw=c[0]+c[1]*xn+c[2]*yn+c[3]*xn*yn+c[4]*(xn**2-1/3)+c[5]*(yn**2-1/3)
    raw-=raw.mean(); raw*=gpp/(np.percentile(raw,95)-np.percentile(raw,5)); grads.append(raw)
grads=np.array(grads)
MS=[]
for k in range(NFRAME):
    x0,y0=offs[k]; m=np.zeros((H,W),bool); m[y0:y0+FH,x0:x0+FW]=True; MS.append(m)
def pairs(): return [(i,j,MS[i]&MS[j]) for i in range(NFRAME) for j in range(i+1,NFRAME) if (MS[i]&MS[j]&BGMASK).sum()>500]
def synth(garr,mode,seed=None):
    rr=np.random.default_rng(seed) if seed is not None else rng; fr=[]
    for k in range(NFRAME):
        img=a_true[k]*s_true+garr[k] if mode=='A' else a_true[k]*(s_true+garr[k])
        img=img+rr.normal(0,NOISE,size=(H,W))
        fr.append(np.where(MS[k],img,np.nan))
    return fr
def mflux(img,x,y,ra=3.0,rn=(6.,9.)):
    xi,yi=int(round(x)),int(round(y)); R=int(math.ceil(rn[1]))
    if xi-R<0 or yi-R<0 or xi+R>=W or yi+R>=H: return np.nan
    sub=img[yi-R:yi+R+1,xi-R:xi+R+1]
    if not np.all(np.isfinite(sub)): return np.nan
    gy,gx=np.mgrid[-R:R+1,-R:R+1]; rr=np.hypot(gx,gy)
    return float(np.sum(sub[rr<=ra]-np.median(sub[(rr>=rn[0])&(rr<=rn[1])])))
def estimate_a(frames):
    est=np.full(NFRAME,np.nan); ns=np.zeros(NFRAME,int)
    for k in range(NFRAME):
        x0,y0=offs[k]
        vis=[i for i in range(NSRC) if x0+10<=sx[i]<=x0+FW-10 and y0+10<=sy[i]<=y0+FH-10]
        ns[k]=len(vis)
        F=np.array([mflux(frames[k],sx[i],sy[i]) for i in vis]); g=np.isfinite(F)&(F>0)
        est[k]=float(np.median(F[g]/sflux[vis][g]))
    est=est*np.exp(np.mean(np.log(a_true)))/np.exp(np.mean(np.log(est)))
    return est,ns
def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def smooth(field,mask,sig=4.0):
    w=mask.astype(float)
    return gaussian_filter(np.where(mask,field,0.),sig)/np.maximum(gaussian_filter(w,sig),1e-9)
def quadpp(field,mask):
    ys,xs=np.nonzero(mask); z=field[mask]
    A=np.column_stack([np.ones_like(xs,float),xs/100,ys/100,xs*ys/1e4,xs**2/1e4,ys**2/1e4])
    c,*_=np.linalg.lstsq(A,z,rcond=None); return pp(A@c)
def ratio_stats(za,zb,m):
    r=za[m]/zb[m]; r0=np.median(r); rel=r/r0-1.0
    ys,xs=np.nonzero(m)
    A=np.column_stack([np.ones_like(xs,float),xs/100,ys/100])
    c,*_=np.linalg.lstsq(A,rel,rcond=None)
    return pp(rel), pp(A@c), float(np.median((za[m]+zb[m])/2.0))

def run(mode,garr,tag):
    frames=synth(garr,mode); a_hat,ns=estimate_a(frames); eps=a_hat/a_true-1
    z=[frames[k]/a_hat[k] for k in range(NFRAME)]
    zs=[smooth(z[k],MS[k]&BGMASK) for k in range(NFRAME)]
    rows=[]
    for (i,j,m0) in pairs():
        m=m0&BGMASK
        ideal=(garr[i]/a_true[i]-garr[j]/a_true[j]) if mode=='A' else (garr[i]-garr[j])
        ds=(zs[i]-zs[j])[m]
        rpp,rpoly,B=ratio_stats(z[i],z[j],m)
        rows.append(dict(pair=f"{i}-{j}",n=int(m.sum()),B=B,
            ideal_pp=pp(ideal[m]), smooth_pp=pp(ds), quad_pp=quadpp(np.where(m0,z[i]-z[j],np.nan),m0),
            surv=(pp(ds)/pp(ideal[m])) if pp(ideal[m])>0 else float('nan'),
            ratio_rel_pp=rpp, ratio_poly_pp=rpoly, mult_seam_ADU=rpp*B, add_smooth_ADU=quadpp(np.where(m0,z[i]-z[j],np.nan),m0)))
    return dict(a_hat=a_hat.tolist(),eps=eps.tolist(),nstar=ns.tolist(),rows=rows)

out={}
for MODE in ('A','B'):
    r=run(MODE,grads,MODE); out[MODE]=r
    print(f"=== version {MODE} === a_true={np.round(a_true,3)} a_hat={np.round(r['a_hat'],4)} eps={np.round(r['eps'],5)}")
    print(f"  {'pair':>5} {'ideal_pp':>9} {'smooth_pp':>10} {'surv':>6} {'quad_pp':>8} {'ratio_pp%':>10} {'mult_seam':>10} {'add_sm':>8}")
    for x in r['rows']:
        print(f"  {x['pair']:>5} {x['ideal_pp']:>9.2f} {x['smooth_pp']:>10.2f} {x['surv']:>6.3f} {x['quad_pp']:>8.2f} "
              f"{100*x['ratio_rel_pp']:>10.3f} {x['mult_seam_ADU']:>10.2f} {x['add_smooth_ADU']:>8.2f}")
    print()
# control: zero gradient
r0=run('A',np.zeros_like(grads),'ctrl'); out['control_nograd']=r0
print("=== NEGATIVE CONTROL: true gradient = 0 (gradient must vanish) ===")
print(f"  {'pair':>5} {'smooth_pp':>10} {'quad_pp':>8} {'ratio_pp%':>10} {'mult_seam':>10} {'add_sm':>8}")
for x in r0['rows']:
    print(f"  {x['pair']:>5} {x['smooth_pp']:>10.2f} {x['quad_pp']:>8.2f} {100*x['ratio_rel_pp']:>10.3f} {x['mult_seam_ADU']:>10.2f} {x['add_smooth_ADU']:>8.2f}")

# ---------- mosaic seam, 4 correction variants -------------------------------
def build_mosaic(zlist):
    num=np.zeros((H,W)); den=np.zeros((H,W))
    for k in range(NFRAME):
        num=np.where(MS[k]&np.isfinite(zlist[k]),num+zlist[k],num)
        den=np.where(MS[k]&np.isfinite(zlist[k]),den+1.0,den)
    return np.where(den>0,num/np.maximum(den,1e-9),np.nan), den
def seam_metric(mos):
    """Seam step = |median(A on side with cov=c) - median(A on side with cov=c')|
    for pixels straddling a coverage boundary.  A = mosaic - true signal.
    Uses local box medians so white noise does not dominate."""
    cov=np.zeros((H,W),int)
    for k in range(NFRAME): cov+=MS[k].astype(int)
    A=mos-s_true
    R=6
    bp=[]
    for y in range(R,H-R):
        for x in range(R,W-R):
            c=cov[y,x]
            if c==0: continue
            nb=[cov[y-1,x],cov[y+1,x],cov[y,x-1],cov[y,x+1]]
            if any(n!=c for n in nb) and any(n<c for n in nb):
                bp.append((y,x,c))
    if len(bp)>4000:
        sel=rng.choice(len(bp),4000,replace=False); bp=[bp[i] for i in sel]
    steps=[]
    for (y,x,c) in bp:
        box=A[y-R:y+R+1,x-R:x+R+1]; cb=cov[y-R:y+R+1,x-R:x+R+1]
        si=np.median(box[(cb==c)&np.isfinite(box)])
        co=np.median(box[(cb<c)&np.isfinite(box)])
        if np.isfinite(si) and np.isfinite(co): steps.append(si-co)
    steps=np.array(steps)
    As=smooth(A,np.isfinite(A))
    return pp(steps), float(np.median(np.abs(steps))), pp(As[np.isfinite(As)])
frames=synth(grads,'A'); a_hat,ns=estimate_a(frames)
z_raw=[frames[k] for k in range(NFRAME)]
z_scalar=[frames[k]/a_hat[k] for k in range(NFRAME)]
z_oracle=[frames[k]/a_hat[k]-grads[k]/a_hat[k] for k in range(NFRAME)]
# PMM-style additive estimate: reference frame 0, per-frame poly2 fit to smoothed difference
z_est=[np.array(v) for v in z_scalar]
for k in range(1,NFRAME):
    m=MS[k]&MS[0]&BGMASK
    d=smooth(np.where(m,z_scalar[k]-z_scalar[0],np.nan),m)
    ys,xs=np.nonzero(m); zz=d[m]
    A=np.column_stack([np.ones_like(xs,float),xs/100,ys/100,xs*ys/1e4,xs**2/1e4,ys**2/1e4])
    c,*_=np.linalg.lstsq(A,zz,rcond=None)
    Y,X=np.mgrid[0:H,0:W]
    corr=c[0]+c[1]*X/100+c[2]*Y/100+c[3]*X*Y/1e4+c[4]*X**2/1e4+c[5]*Y**2/1e4
    z_est[k]=np.where(MS[k],z_scalar[k]-corr,z_scalar[k])
seam={}
for name,zl in [('raw_no_correction',z_raw),('scalar_calib_only',z_scalar),
                ('scalar_+_true_additive(oracle)',z_oracle),('scalar_+_PMM_style_additive',z_est)]:
    mos,_=build_mosaic(zl); p95,med,tot=seam_metric(mos)
    seam[name]=dict(jump_pp=p95,jump_median=med,artifact_pp=tot)
    print(f"  SEAM {name:>34}: jump_pp={p95:8.2f}  jump_median={med:7.2f}  artifact_pp={tot:8.2f}")
out['mosaic_seam']=seam
out['meta']=dict(S0=S0,NOISE=NOISE,a_true=a_true.tolist(),grad_pp_true=[pp(g) for g in grads],BGMASK_pix=int(BGMASK.sum()))
json.dump(out,open(os.path.join(OUT,'synth_core.json'),'w'),indent=1)
print("\nwrote synth_core.json")
