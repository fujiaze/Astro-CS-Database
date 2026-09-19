#!/usr/bin/env python3
"""Q1.5c Real-data MULTIPLICATIVE spatial residual, measured from STARS.
Stars are point sources: a multiplicative response shows up as a spatially
varying flux ratio between frames; an additive sky gradient does NOT (local
background subtraction removes it).  This breaks the background degeneracy.

usage: star_residual.py <norm_group_dir> [...]
"""
import json, os, sys, math, glob
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
OUT=os.path.dirname(os.path.abspath(__file__))

def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def pb(x,y,deg=2):
    return np.column_stack([(x**i)*(y**j) for i in range(deg+1) for j in range(deg+1-i)])

def load_group(g):
    F=json.load(open(os.path.join(g,'p1_flux.json')))['frames']
    out=[]
    for fr in F:
        cal=os.path.join(g,'calibrated_'+fr['file'].replace('cleaned_',''))
        h=fits.getheader(cal); w=WCS(h)
        res=[r for r in fr['results'] if r.get('valid') and r.get('flux',0)>0 and np.isfinite(r.get('flux',np.nan))]
        x=np.array([r['x'] for r in res]); y=np.array([r['y'] for r in res])
        f=np.array([r['flux'] for r in res]); fe=np.array([r['flux_error'] for r in res])
        snr=np.array([r['snr'] for r in res])
        ra,dec=w.all_pix2world(x,y,0)
        out.append(dict(name=fr['file'],ra=ra,dec=dec,f=f,fe=fe,snr=snr,n=len(res)))
    return out

def match(A,B,tol=1.5/3600.0):
    # nearest neighbour in sky (small field: plain O(N*M) too big -> use sorting on ra)
    oa=np.argsort(A['ra']); ob=np.argsort(B['ra'])
    ra=A['ra'][oa]; rb=B['ra'][ob]
    ia=[];ib=[]
    j0=0
    for i in range(len(ra)):
        while j0<len(rb) and rb[j0]<ra[i]-tol: j0+=1
        j=j0
        best=-1; bd=1e9
        while j<len(rb) and rb[j]<ra[i]+tol:
            dd=(rb[j]-ra[i])**2+(B['dec'][ob[j]]-A['dec'][oa[i]])**2
            if dd<bd: bd=dd; best=j
            j+=1
        if best>=0 and bd<tol*tol: ia.append(oa[i]); ib.append(ob[best])
    return np.array(ia),np.array(ib)

rows=[]
for g in sys.argv[1:]:
    if not os.path.isdir(g): continue
    frames=load_group(g)
    print(f"=== {g.split('/')[-1]} : {len(frames)} frames, nsrc={[f['n'] for f in frames]}")
    for i in range(len(frames)):
        for j in range(i+1,len(frames)):
            A,B=frames[i],frames[j]
            ia,ib=match(A,B)
            if len(ia)<200: continue
            # only bright, well-measured stars
            m=(A['snr'][ia]>100)&(B['snr'][ib]>100)&(A['f'][ia]>0)&(B['f'][ib]>0)
            ia,ib=ia[m],ib[m]
            if len(ia)<100: continue
            ratio=A['f'][ia]/B['f'][ib]
            rel_err=np.hypot(A['fe'][ia]/A['f'][ia],B['fe'][ib]/B['f'][ib])
            r0=float(np.median(ratio))
            rel=ratio/r0-1.0
            x=(A['ra'][ia]-np.median(A['ra'][ia]))/max(np.ptp(A['ra'][ia])/2,1e-9)
            y=(A['dec'][ia]-np.median(A['dec'][ia]))/max(np.ptp(A['dec'][ia])/2,1e-9)
            X=pb(x,y,2); c,*_=np.linalg.lstsq(X,rel,rcond=None); fit=X@c; res=rel-fit
            # noise floor: weighted mean of per-star relative errors
            nf=float(np.median(rel_err))
            # chi2 of the global-ratio model
            chi2_global=float(np.mean((rel/rel_err)**2))
            chi2_flat=float(np.mean((res/rel_err)**2))
            rows.append(dict(group=os.path.basename(g),i=i,j=j,n=int(len(ia)),r0=r0,
                             rel_pp=pp(rel), fit_pp=pp(fit), res_pp=pp(res),
                             noise_pp=3.3*math.sqrt(2)*nf, nf_rel=nf,
                             chi2_global=chi2_global, chi2_spatial=chi2_flat,
                             fit_coef=c.tolist()))
            print(f"  pair {i}-{j}: n={len(ia)} global_ratio={r0:.4f} rel_pp={pp(rel)*100:7.3f}% "
                  f"coherent_pp={pp(fit)*100:7.3f}% resid_pp={pp(res)*100:7.3f}% "
                  f"noise_floor_pp={3.3*math.sqrt(2)*nf*100:6.3f}% chi2_glob={chi2_global:8.1f} chi2_spat={chi2_flat:8.1f}")
if rows:
    import statistics
    print("\n--- aggregate over pairs ---")
    for k in ['n','r0','rel_pp','fit_pp','res_pp','noise_pp','chi2_global','chi2_spatial']:
        v=np.array([r[k] for r in rows]); print(f"  {k:>12}: median={np.median(v):.5g}")
json.dump(rows,open(os.path.join(OUT,'star_residual.json'),'w'),indent=1)
print("wrote star_residual.json")
