#!/usr/bin/env python3
"""Q2 stage E2: unbiased (no-search) J at the fitted locus + J-vs-offset profile (search-bias diagnostic)."""
import numpy as np, json, warnings
warnings.filterwarnings('ignore')
from astropy.io import fits
ROOT='/workspace/Astro CS Database'; L4=ROOT+'/run/RELEASE-02/L4-rebuild'
OUT=ROOT+'/run/RELEASE-02/q2-snr-smooth/realdata'
loci=json.load(open(OUT+'/loci.json'))
P={'p3_r_vis':L4+'/p3_r_vis/output_phase3.fits','p3_r_vis_fixed':L4+'/p3_r_vis_fixed/output_phase3.fits',
   'p3_upmfix':L4+'/p3_upmfix/output_phase3.fits'}
D={}
for k,p in P.items():
    with fits.open(p,memmap=True) as h: D[k]=np.asarray(h[0].data,dtype=np.float64)
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
def st(v):
    v=np.asarray([x for x in v if np.isfinite(x)],float)
    if v.size==0: return dict(n=0,median=None,p16=None,p84=None)
    return dict(n=int(v.size),median=float(np.median(v)),p16=float(np.percentile(v,16)),p84=float(np.percentile(v,84)))

xs=np.arange(750,1951,25); yv=np.arange(400,3501,50)
SPEC={'H1top':('y',xs,lambda x:loci['H1top']['intercept']+loci['H1top']['slope']*x),
      'H1bot':('y',xs,lambda x:loci['H1bot']['intercept']+loci['H1bot']['slope']*x),
      'V-right':('x',yv,lambda y:loci['V-right']['intercept']+loci['V-right']['slope']*y)}
BGREF={'H1top':1.03542e13,'H1bot':1.07818e13,'V-right':1.01664e13}
res={'no_search':{},'profile':{},'search_bias':{}}
print('=== UNBIASED: J at the fitted locus (NO +/-8 search) ===')
for bn,(ax,smp,f) in SPEC.items():
    for shift in (0,-150,150):
        for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
            v=[(Jy if ax=='y' else Jx)(D[prod], int(round(f(s)+shift)), s-50, s+50) for s in smp]
            s_=st(v); s_['pct_of_ref_old_bg']=100*s_['median']/BGREF[bn] if s_['median'] is not None else None
            res['no_search']['%s|%s|shift%+d'%(bn,prod,shift)]=s_
            print('  %-8s %-15s %+5d  n=%3d  J=%+13.5g  p16..p84 %+.4g..%+.4g  %+7.4f%%ref'%(
                bn,prod,shift,s_['n'],s_['median'] if s_['median'] is not None else float('nan'),
                s_['p16'] if s_['p16'] is not None else float('nan'),s_['p84'] if s_['p84'] is not None else float('nan'),
                s_['pct_of_ref_old_bg'] if s_['pct_of_ref_old_bg'] is not None else float('nan')))
    print('-'*110)
print()
print('=== J vs offset from fitted locus (median over samples) -- shows where the real step sits ===')
for bn,(ax,smp,f) in SPEC.items():
    print(' ',bn)
    hdr='   off  ' + ''.join('%16s'%p for p in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'))
    print(hdr)
    prof={}
    for off in range(-8,9):
        row=[]
        for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
            v=[(Jy if ax=='y' else Jx)(D[prod], int(round(f(s)))+off, s-50, s+50) for s in smp]
            m=st(v)['median']; row.append(m if m is not None else np.nan)
        prof[off]=row
        print('  %+4d  '%off + ''.join('%+16.5g'%r for r in row))
    res['profile'][bn]={str(k):v for k,v in prof.items()}
    # search bias: median(|J| at best offset) - median(|J| at off=0), per product
    for i,prod in enumerate(('p3_r_vis_fixed','p3_upmfix','p3_r_vis')):
        best=[max((abs(prof[o][i]),prof[o][i]) for o in prof)[1] for _ in [0]]
        res['search_bias']['%s|%s'%(bn,prod)]=dict(J_off0=prof[0][i],
            J_at_maxabs=float(max((abs(prof[o][i]),prof[o][i]) for o in prof)[1]),
            best_off=int(max(prof,key=lambda o:abs(prof[o][i]))))
        print('   -> %-15s J(off=0)=%+.5g  max|J|=%+.5g at off=%+d'%(prod,res['search_bias']['%s|%s'%(bn,prod)]['J_off0'],
            res['search_bias']['%s|%s'%(bn,prod)]['J_at_maxabs'],res['search_bias']['%s|%s'%(bn,prod)]['best_off']))
json.dump(res,open(OUT+'/stageE2_nosearch.json','w'),indent=1)
print(); print('saved',OUT+'/stageE2_nosearch.json')
