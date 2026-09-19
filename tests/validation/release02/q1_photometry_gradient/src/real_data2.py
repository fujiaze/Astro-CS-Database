#!/usr/bin/env python3
"""Q1.5b Real data, model-completeness test.

For every overlapping frame pair (star-rejected background control points):
    d(p) = v_A(p) - v_B(p)          (frame-to-frame additive difference)
    sigma_d from the per-observation 'uncertainty' field
Fit d with additive smooth models of increasing complexity (constant ... deg-4
polynomial, and a local 6x6 bilinear grid) and with a multiplicative+additive
model  d = beta(p)*vbar + additive.  Report residual RMS / peak-to-peak and the
weighted chi^2 so the *noise floor* is explicit.  Whichever model leaves a
residual above the noise floor defines what additive correction cannot remove.
"""
import json, os, math, itertools
import numpy as np
BASE='/workspace/Astro CS Database/run/RELEASE-02/L4-rebuild/upmfix_out'
OUT=os.path.dirname(os.path.abspath(__file__))
d=json.load(open(os.path.join(BASE,'p2_samples.json')))
frames={}
for o in d['observations']:
    frames.setdefault(o['frame_id'],[]).append((o['control_id'],o['value'],o['uncertainty'],o['ra_deg'],o['dec_deg']))
fids=sorted(frames); F={}
for f in fids:
    a=np.array(frames[f],dtype=float); a=a[np.argsort(a[:,0])]; F[f]=a
print(f"frames={len(fids)}")

def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def poly_basis(x,y,deg):
    cols=[]
    for i in range(deg+1):
        for j in range(deg+1-i):
            cols.append((x**i)*(y**j))
    return np.column_stack(cols)
def grid_basis(x,y,nx=6,ny=6):
    cols=[]
    for i in range(nx):
        for j in range(ny):
            cols.append(np.clip(1-np.abs(x-(2*i/(nx-1)-1))/(2/(nx-1)),0,None)*
                        np.clip(1-np.abs(y-(2*j/(ny-1)-1))/(2/(ny-1)),0,None))
    return np.column_stack(cols)
def wls(A,z,sig):
    w=1.0/np.maximum(sig,1e-30)
    coef,*_=np.linalg.lstsq(A*w[:,None],z*w,rcond=None)
    res=z-A@coef
    dof=max(len(z)-A.shape[1],1)
    chi2=float(np.mean((res/sig)**2)*len(z)/dof)
    return res,chi2,coef

rows=[]
for a,b in itertools.combinations(fids,2):
    ca,cb=F[a][:,0],F[b][:,0]
    common,ia,ib=np.intersect1d(ca,cb,return_indices=True)
    if len(common)<300: continue
    va,vb=F[a][ia,1],F[b][ib,1]; ua,ub=F[a][ia,2],F[b][ib,2]
    ra,dec=F[a][ia,3],F[a][ia,4]
    x=(ra-np.median(ra))/max((ra.max()-ra.min())/2,1e-9)
    y=(dec-np.median(dec))/max((dec.max()-dec.min())/2,1e-9)
    d_=va-vb; sd=np.hypot(ua,ub); B=float(np.median((va+vb)/2)); vbar=(va+vb)/2
    rec=dict(a=int(a),b=int(b),n=int(len(common)),B=B,
             d_pp=pp(d_), d_rms=float(np.std(d_)), sig_rel=float(np.median(sd/B)))
    for deg in (0,1,2,3,4):
        A=poly_basis(x,y,deg); res,chi2,coef=wls(A,d_,sd)
        rec[f'deg{deg}_rms']=float(np.std(res)); rec[f'deg{deg}_pp']=pp(res); rec[f'deg{deg}_chi2']=chi2
    A=grid_basis(x,y); res,chi2,coef=wls(A,d_,sd)
    rec['grid_rms']=float(np.std(res)); rec['grid_pp']=pp(res); rec['grid_chi2']=chi2
    # multiplicative + additive:  d = beta(p)*vbar + additive(poly2)
    Aq=poly_basis(x,y,2)
    Am=np.column_stack([vbar[:,None]*Aq, Aq])
    res,chi2,coef=wls(Am,d_,sd)
    rec['mult_rms']=float(np.std(res)); rec['mult_pp']=pp(res); rec['mult_chi2']=chi2
    rows.append(rec)
print(f"pairs with >=300 common controls: {len(rows)}")
def agg(k):
    v=np.array([r[k] for r in rows]); v=v[np.isfinite(v)]
    return float(np.median(v)), float(np.percentile(v,16)), float(np.percentile(v,84))
keys=['n','sig_rel','d_rms','d_pp','deg0_rms','deg0_pp','deg0_chi2','deg1_pp','deg1_chi2',
      'deg2_rms','deg2_pp','deg2_chi2','deg3_pp','deg3_chi2','deg4_pp','deg4_chi2',
      'grid_rms','grid_pp','grid_chi2','mult_rms','mult_pp','mult_chi2']
print(f"{'metric':>12} {'median':>12} {'p16':>12} {'p84':>12}")
S={}
for k in keys:
    m,a,b=agg(k); S[k]=dict(median=m,p16=a,p84=b); print(f"{k:>12} {m:>12.5g} {a:>12.5g} {b:>12.5g}")
medB=float(np.median([r['B'] for r in rows]))
print(f"\nmedian background B={medB:.4g}")
for k in ['d_pp','deg2_pp','deg4_pp','grid_pp','mult_pp','deg2_rms','grid_rms','mult_rms']:
    print(f"  {k:>10}: {100*S[k]['median']/medB:7.3f}% of background")
print(f"  median per-observation relative uncertainty = {S['sig_rel']['median']*100:.3f}%")
print(f"  => pairwise noise floor on d (pp of sqrt2*sigma over ~700 pts) ~ {100*3.3*math.sqrt(2)*S['sig_rel']['median']:.2f}% of background")
json.dump(dict(medB=medB,summary=S,pairs=rows),open(os.path.join(OUT,'real_data2.json'),'w'),indent=1)
print("wrote real_data2.json")
