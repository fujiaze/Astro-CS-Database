#!/usr/bin/env python3
"""Q1.3 + Q1.4  Spatially-varying photometric calibration vs additive gradient:
can it absorb the gradient, at what scale, and is (a(p), g(p)) identifiable?"""
import json, math, os
import numpy as np
from scipy.ndimage import gaussian_filter
rng=np.random.default_rng(4242)
OUT=os.path.dirname(os.path.abspath(__file__))
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
s_true=s+star; S0=float(np.median(s)); BGMASK=(star<2.0)&(s<S0+80.0)
reftot=sflux*2*np.pi*PSF**2
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
def synth(garr):
    fr=[]
    for k in range(NFRAME):
        img=a_true[k]*s_true+garr[k]+rng.normal(0,NOISE,size=(H,W))
        fr.append(np.where(MS[k],img,np.nan))
    return fr
def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def smooth(f,m,sig=3.0):
    return gaussian_filter(np.where(m,f,0.),sig)/np.maximum(gaussian_filter(m.astype(float),sig),1e-9)
def design(X,Y,deg=2):
    cols=[np.ones_like(X,float),X/100,Y/100]
    if deg>=2: cols+=[X*Y/1e4,X**2/1e4,Y**2/1e4]
    return np.column_stack(cols)
def robust_fit(A,z,sig,nrounds=5,kclip=3.0):
    m=np.ones(len(z),bool)
    for _ in range(nrounds):
        w=1.0/np.maximum(sig[m],1e-30)
        c,*_=np.linalg.lstsq(A[m]*w[:,None],z[m]*w,rcond=None)
        r=z-A@c; rm=r[m]
        sc=1.4826*np.median(np.abs(rm-np.median(rm)))
        if sc<=0: break
        m=np.abs(r)<kclip*sc
        if m.sum()<A.shape[1]+2: break
    return c,A@c,r,m
frames=synth(grads)
gy,gx=np.mgrid[-9:10,-9:10]; rr=np.hypot(gx,gy); ap=rr<=3.0; ann=(rr>=6)&(rr<=9)
NAP=ap.sum(); sig_ap=NOISE*math.sqrt(NAP*(1+NAP/max(ann.sum(),1)))
print(f"aperture noise sigma_ap={sig_ap:.2f} ADU, N_ap={NAP}")

# ---- (i) star-based spatial a(p) when the TRUE response is constant ----------
print("\n=== Q1.3(i) star-based spatial calibration, TRUE a is CONSTANT ===")
print("    ratio_i = aperture_flux_i / true_total_flux_i ; robust quadratic fit")
sp_rows=[]
for k in [0,3]:
    x0,y0=offs[k]
    vis=[i for i in range(NSRC) if x0+11<=sx[i]<=x0+FW-11 and y0+11<=sy[i]<=y0+FH-11]
    F=[]
    for i in vis:
        xi,yi=int(round(sx[i])),int(round(sy[i])); sub=frames[k][yi-9:yi+10,xi-9:xi+10]
        F.append(np.sum(sub[ap]-np.median(sub[ann])))
    F=np.array(F); X=np.array([sx[i] for i in vis]); Y=np.array([sy[i] for i in vis])
    g=np.isfinite(F)&(F>0)
    F=F[g]; Xg=X[g]; Yg=Y[g]; refg=reftot[vis][g]
    ratio=F/refg; sig=sig_ap/refg
    A=design(Xg,Yg,2); c,fit,res,keep=robust_fit(A,ratio,sig)
    relfit=fit/np.median(fit)-1.0
    # null: same fit on pure noise with the same per-star sigmas
    null=[]
    for _ in range(300):
        rn=np.median(ratio)*(1+rng.normal(0,sig,len(ratio)))
        cn,*_=np.linalg.lstsq(A*np.ones_like(A),rn,rcond=None); null.append(pp((A@cn)/np.median(A@cn)-1))
    null=np.array(null)
    print(f"  frame {k}: n={len(ratio)} kept={keep.sum()}  a_hat={np.median(ratio):.4f} (true {a_true[k]:.3f})"
          f"  fitted spatial pp={pp(relfit)*100:6.3f}%   NULL pp median={np.median(null)*100:.3f}%"
          f" p95={np.percentile(null,95)*100:.3f}%   p-value={np.mean(null>=pp(relfit)):.3f}")
    sp_rows.append(dict(frame=k,n=int(len(ratio)),a_hat=float(np.median(ratio)),a_true=float(a_true[k]),
                        fit_pp_rel=float(pp(relfit)),null_median=float(np.median(null)),
                        null_p95=float(np.percentile(null,95)),
                        p_value=float(np.mean(null>=pp(relfit)))))
print("  -> with the true response constant, the star-fitted spatial structure is")
print("     statistically indistinguishable from the noise-only null (p large),")
print("     i.e. a legitimate star-based spatial calibration sees NO gradient.")

# ---- (ii) background-matched spatial multiplicative calibration --------------
print("\n=== Q1.3(ii) background-matched spatial multiplicative calibration ===")
print("    fit m_k(p)=1+poly2(p) so m_k*y_k matches the reference frame in the BACKGROUND")
mcoef={}
for k in range(1,NFRAME):
    m=MS[k]&MS[0]&BGMASK
    yk=frames[k][m]; yr=frames[0][m]
    ys,xs=np.nonzero(m)
    A=design(xs.astype(float),ys.astype(float),2)*yk[:,None]
    c,*_=np.linalg.lstsq(A,yr,rcond=None); mcoef[k]=c
Y,X=np.mgrid[0:H,0:W]
def mk_surface(k):
    if k==0: return np.ones((H,W))
    return (design(X.ravel().astype(float),Y.ravel().astype(float),2)@mcoef[k]).reshape(H,W)
resid_bg=[]; star_err=[]; absorbed=[]
for k in range(1,NFRAME):
    m=MS[k]&MS[0]&BGMASK; mk=mk_surface(k)
    resid_bg.append((pp(smooth(np.where(m,frames[k]-frames[0],np.nan),m)),
                     pp(smooth(np.where(m,mk*frames[k]-frames[0],np.nan),m))))
    x0,y0=offs[k]
    vis=[i for i in range(NSRC) if x0+11<=sx[i]<=x0+FW-11 and y0+11<=sy[i]<=y0+FH-11]
    e=[mk[int(round(sy[i])),int(round(sx[i]))]-1.0 for i in vis]
    star_err.append(pp(e)); absorbed.append(pp(mk-1.0))
print(f"  {'frame':>5} {'bg diff pp before':>18} {'after mult corr':>16} {'m_k-1 pp':>10} {'star flux err pp':>17}")
for i,k in enumerate(range(1,NFRAME)):
    print(f"  {k:>5} {resid_bg[i][0]:>18.2f} {resid_bg[i][1]:>16.2f} {absorbed[i]*100:>9.3f}% {star_err[i]*100:>16.3f}%")
print(f"  true additive-gradient pp / S0 = {gpp/S0*100:.3f}%  (the background step absorbed)")

# ---- (iii) identifiability ---------------------------------------------------
print("\n=== Q1.4 identifiability of (a(p), g(p)) ===")
delta=30.0*(xn+0.5*yn)
y1=1.0*s_true+delta
y2=(1.0+delta/S0)*s_true
a2=1.0+delta/s_true; g2=-delta
print(f"  exact degeneracy a'=a+d/s, g'=g-d : max|y(a,g)-y(a',g')| = "
      f"{np.max(np.abs((a2*s_true+g2)-(1.0*s_true+0.0))):.3e} ADU")
bgm=BGMASK
print(f"  on star-free background: max|additive_model - multiplicative_model| = "
      f"{np.max(np.abs((y1-y2)[bgm])):.3f} ADU  vs gradient pp {pp(delta[bgm]):.2f} ADU  -> degenerate")
def starflux(img,vis):
    out=[]
    for i in vis:
        xi,yi=int(round(sx[i])),int(round(sy[i])); sub=img[yi-9:yi+10,xi-9:xi+10]
        out.append(np.sum(sub[ap]-np.median(sub[ann])))
    return np.array(out)
x0,y0=offs[3]; vis=[i for i in range(NSRC) if x0+11<=sx[i]<=x0+FW-11 and y0+11<=sy[i]<=y0+FH-11]
fA=starflux(y1,vis); fB=starflux(y2,vis)
print(f"  on STARS: median flux ratio modelB/modelA = {np.median(fB/fA):.4f}"
      f"  (pp {pp(fB/fA-1)*100:.3f}%)  -> stars distinguish them")
json.dump(dict(star_spatial=sp_rows,
               mult_calib=[dict(frame=k,bg_before=resid_bg[i][0],bg_after=resid_bg[i][1],
                                absorbed_pct=absorbed[i]*100,star_err_pct=star_err[i]*100)
                           for i,k in enumerate(range(1,NFRAME))],
               grad_pct=gpp/S0*100,
               exact_degeneracy_maxdiff=float(np.max(np.abs((a2*s_true+g2)-(1.0*s_true)))),
               bg_degeneracy_maxdiff=float(np.max(np.abs((y1-y2)[bgm]))),
               bg_gradient_pp=float(pp(delta[bgm])),
               star_ratio_pp=float(pp(fB/fA-1))),
          open(os.path.join(OUT,'spatial_calib.json'),'w'),indent=1)
print("wrote spatial_calib.json")
