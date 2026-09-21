#!/usr/bin/env python3
"""Q2 stage G: patch realdata_seam.json with wide-strip + cross-axis-null results."""
import numpy as np, json, warnings
warnings.filterwarnings('ignore')
from astropy.io import fits
ROOT='/workspace/Astro CS Database'; L4=ROOT+'/run/RELEASE-02/L4-rebuild'
OUT=ROOT+'/run/RELEASE-02/q2-snr-smooth/realdata'
loci=json.load(open(OUT+'/loci.json'))
D={}
for k,p in (('p3_r_vis',L4+'/p3_r_vis/output_phase3.fits'),('p3_r_vis_fixed',L4+'/p3_r_vis_fixed/output_phase3.fits'),
            ('p3_upmfix',L4+'/p3_upmfix/output_phase3.fits')):
    with fits.open(p,memmap=True) as h: D[k]=np.asarray(h[0].data,dtype=np.float64)
def ws_y(d,yb,x,hw=50,gap=6,half=10):
    a=d[yb-gap-half:yb-gap,x-hw:x+hw]; b=d[yb+gap:yb+gap+half,x-hw:x+hw]
    return float(np.nanmedian(b)-np.nanmedian(a)) if (np.isfinite(a).any() and np.isfinite(b).any()) else np.nan
def ws_x(d,xb,y,hw=50,gap=6,half=10):
    a=d[y-hw:y+hw,xb-gap-half:xb-gap]; b=d[y-hw:y+hw,xb+gap:xb+gap+half]
    return float(np.nanmedian(b)-np.nanmedian(a)) if (np.isfinite(a).any() and np.isfinite(b).any()) else np.nan
def st(v):
    v=np.asarray([x for x in v if np.isfinite(x)],float)
    if v.size==0: return None
    return dict(n=int(v.size),median=float(np.median(v)),p16=float(np.percentile(v,16)),p84=float(np.percentile(v,84)))
K,GAP=25,1
def Jy(d,y0,xlo,xhi):
    rL=np.arange(y0-K,y0-GAP); rR=np.arange(y0+GAP+1,y0+K+1)
    L=d[rL[0]:rL[-1]+1,xlo:xhi]; R=d[rR[0]:rR[-1]+1,xlo:xhi]
    ok=np.isfinite(L).all(0)&np.isfinite(R).all(0)
    if ok.sum()<5: return np.nan
    L=L[:,ok];R=R[:,ok]; A=np.vstack([np.ones_like(rL,float),rL]).T; p=np.linalg.pinv(A).T
    eL=L.T@p; eR=R.T@p
    return float(np.median((eR[:,0]+eR[:,1]*y0)-(eL[:,0]+eL[:,1]*y0)))
def Jx(d,x0,ylo,yhi):
    cL=np.arange(x0-K,x0-GAP); cR=np.arange(x0+GAP+1,x0+K+1)
    L=d[ylo:yhi,cL[0]:cL[-1]+1]; R=d[ylo:yhi,cR[0]:cR[-1]+1]
    ok=np.isfinite(L).all(1)&np.isfinite(R).all(1)
    if ok.sum()<5: return np.nan
    L=L[ok];R=R[ok]; A=np.vstack([np.ones_like(cL,float),cL]).T; p=np.linalg.pinv(A).T
    eL=L@p; eR=R@p
    return float(np.median((eR[:,0]+eR[:,1]*x0)-(eL[:,0]+eL[:,1]*x0)))
xs=np.arange(750,1951,25); yv=np.arange(400,3501,50)
SPEC={'H1top':('y',xs,lambda x:loci['H1top']['intercept']+loci['H1top']['slope']*x),
      'H1bot':('y',xs,lambda x:loci['H1bot']['intercept']+loci['H1bot']['slope']*x),
      'V-right':('x',yv,lambda y:loci['V-right']['intercept']+loci['V-right']['slope']*y)}
BGREF={'H1top':1.03542e13,'H1bot':1.07818e13,'V-right':1.01664e13}
ws={}; null={}
for bn,(ax,smp,f) in SPEC.items():
    for shift in (0,-150,150):
        for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
            v=[(ws_y if ax=='y' else ws_x)(D[prod],int(round(f(s)+shift)),s) for s in smp]
            r=st(v)
            if r: r['pct_of_ref_old_bg']=100*r['median']/BGREF[bn]
            ws['%s|%s|shift%+d'%(bn,prod,shift)]=r
    for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
        v=[]
        for s in smp:
            if ax=='y':
                yb=int(round(f(s))); best=0.0
                for xx in range(s-8,s+9):
                    j=Jx(D[prod],xx,yb-8,yb+8)
                    if np.isfinite(j) and abs(j)>abs(best): best=j
                v.append(best)
            else:
                xb=int(round(f(s))); best=0.0
                for yy in range(s-8,s+9):
                    j=Jy(D[prod],yy,xb-8,xb+8)
                    if np.isfinite(j) and abs(j)>abs(best): best=j
                v.append(best)
        null['%s|%s'%(bn,prod)]=st(v)
J=json.load(open(OUT+'/realdata_seam.json'))
J['independent_wide_strip']=dict(note='10px strip each side, 6px gap, NO search, nanmedian difference then median over samples. BIASED where a strong gradient crosses the boundary (notably V-right); transparency only.',bg_denominator_ref=BGREF,results=ws)
J['cross_axis_null']=dict(note='other-axis step measured at the same locus (+/-8px search) - should be ~0 if only the expected seam exists',results=null)
json.dump(J,open(OUT+'/realdata_seam.json','w'),indent=1)
print('patched. wide_strip keys',len(ws),'null keys',len(null))
