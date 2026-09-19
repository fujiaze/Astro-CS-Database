#!/usr/bin/env python3
"""Q1.5  Real-data: multiplicative vs additive frame-to-frame residual on the 49
Phase1-calibrated L4 frames (run/RELEASE-02/L4-rebuild, norm/* + upmfix_out).

Input: p2_samples.json = star-rejected background control points (HEALPix), the
same sample set the Phase2 additive solver consumes.
For every overlapping frame pair we measure
  * ratio residual   rel = (v_A/v_B)/median(v_A/v_B) - 1   (MULTIPLICATIVE, cannot be
    removed by any per-frame scalar; a spatial scalar would need its own structure)
  * difference field d = v_A - v_B                          (ADDITIVE gradient)
and split each into a coherent (2-D quadratic, i.e. what a smooth model can absorb)
and an incoherent part.  A variogram intercept gives the per-point noise floor.
"""
import json, os, math, itertools
import numpy as np
BASE='/workspace/Astro CS Database/run/RELEASE-02/L4-rebuild/upmfix_out'
OUT=os.path.dirname(os.path.abspath(__file__))
d=json.load(open(os.path.join(BASE,'p2_samples.json')))
obs=d['observations']
frames={}
for o in obs:
    frames.setdefault(o['frame_id'],[]).append((o['control_id'],o['value'],o['ra_deg'],o['dec_deg']))
fids=sorted(frames)
print(f"frames={len(fids)}  observations={len(obs)}  controls={d['n_controls']}")
F={}
for f in fids:
    a=np.array(frames[f],dtype=float)
    order=np.argsort(a[:,0]); F[f]=a[order]
    print(f"  frame {f}: n={len(a)}  median value={np.median(a[:,1]):.4g}")
allv=np.concatenate([F[f][:,1] for f in fids])
B0=float(np.median(allv))
print(f"global background level B0={B0:.4g}")

def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def fit_poly(X,Y,z,deg=2):
    cols=[np.ones_like(X),X,X*Y,Y,X*X,Y*Y] if deg==2 else [np.ones_like(X),X,Y]
    A=np.column_stack(cols)
    # scale columns for conditioning
    sc=np.sqrt((A*A).mean(axis=0)); A=A/sc
    c,*_=np.linalg.lstsq(A,z,rcond=None)
    fit=A@c
    return fit, z-fit, c/sc
def variogram(x,y,val,edges):
    n=len(val)
    if n>1200:
        idx=np.linspace(0,n-1,1200).astype(int); x,y,val=x[idx],y[idx],val[idx]; n=len(val)
    dx=x[:,None]-x[None,:]; dy=y[:,None]-y[None,:]
    sep=np.hypot(dx,dy); dv=(val[:,None]-val[None,:])**2
    iu=np.triu_indices(n,1); s=sep[iu]; v=dv[iu]
    out=[]
    for lo,hi in zip(edges[:-1],edges[1:]):
        m=(s>=lo)&(s<hi)
        if m.sum()>20: out.append((0.5*(lo+hi), 0.5*v[m].mean(), int(m.sum())))
    return out

pairs=[]
for a,b in itertools.combinations(fids,2):
    ca,cb=F[a][:,0],F[b][:,0]
    common,ia,ib=np.intersect1d(ca,cb,return_indices=True)
    if len(common)<150: continue
    va,vb=F[a][ia,1],F[b][ib,1]
    ra,dec=F[a][ia,2],F[a][ia,3]
    x=(ra-np.median(ra))*math.cos(math.radians(np.median(dec))); y=dec-np.median(dec)
    B=float(np.median((va+vb)/2))
    r=va/vb; r0=float(np.median(r)); rel=r/r0-1.0
    dfit,dres,_=fit_poly(x,y,va-vb)
    rfit,rres,_=fit_poly(x,y,rel)
    # noise floor from variogram intercept (on the raw fields)
    edges=np.array([0,0.002,0.005,0.01,0.02,0.05,0.1,0.2,0.5,1.0])
    vg=variogram(x,y,rel,edges)
    nfloor_rel = vg[0][1] if vg else float('nan')
    pairs.append(dict(a=int(a),b=int(b),n=int(len(common)),B=B,
        rel_pp=pp(rel), rel_coh_pp=pp(rfit), rel_res_pp=pp(rres),
        d_pp=pp(va-vb), d_coh_pp=pp(dfit), d_res_pp=pp(dres),
        d_rms=float(np.std(va-vb)), rel_rms=float(np.std(rel)),
        vario0=float(nfloor_rel),
        mult_coh_flux=pp(rfit)*B, mult_res_flux=pp(rres)*B,
        add_coh_flux=pp(dfit), add_res_flux=pp(dres)))
print(f"\npairs with >=150 common controls: {len(pairs)}")
def agg(key):
    v=np.array([p[key] for p in pairs]); v=v[np.isfinite(v)]
    return dict(median=float(np.median(v)),p16=float(np.percentile(v,16)),p84=float(np.percentile(v,84)),
                p05=float(np.percentile(v,5)),p95=float(np.percentile(v,95)))
keys=['n','rel_pp','rel_coh_pp','rel_res_pp','d_pp','d_coh_pp','d_res_pp','d_rms','rel_rms',
      'mult_coh_flux','mult_res_flux','add_coh_flux','add_res_flux']
summary={k:agg(k) for k in keys}
print(f"\n{'metric':>16} {'median':>12} {'p05':>12} {'p95':>12}")
for k in keys:
    s=summary[k]; print(f"{k:>16} {s['median']:>12.4g} {s['p05']:>12.4g} {s['p95']:>12.4g}")
# relative-to-background numbers (the seam step as a fraction of sky background)
med_B=float(np.median([p['B'] for p in pairs]))
print(f"\nmedian overlap background B={med_B:.4g}")
for k in ['mult_coh_flux','mult_res_flux','add_coh_flux','add_res_flux']:
    s=summary[k]; print(f"  {k:>16}: median {100*s['median']/med_B:6.3f}% of background")
# which dominates the *irreducible* (post-smooth-additive) seam?
irr_mult=np.array([p['mult_coh_flux']+p['mult_res_flux'] for p in pairs])
irr_add =np.array([p['add_res_flux'] for p in pairs])
coh_add =np.array([p['add_coh_flux'] for p in pairs])
print(f"\nIRREDUCIBLE after an ideal smooth per-frame additive correction:")
print(f"  multiplicative (coherent+incoherent)  median pp = {np.median(irr_mult):.4g}  ({100*np.median(irr_mult)/med_B:.3f}% of bg)")
print(f"  additive incoherent (not fit by quad) median pp = {np.median(irr_add):.4g}  ({100*np.median(irr_add)/med_B:.3f}% of bg)")
print(f"  additive coherent (removable)         median pp = {np.median(coh_add):.4g}  ({100*np.median(coh_add)/med_B:.3f}% of bg)")
print(f"  ratio mult_irr/add_irr = {np.median(irr_mult)/np.median(irr_add):.3f}")
# per-frame global scale from pairwise median ratios (empirical k_photo ratios)
print("\nper-frame relative scale (log least squares over pair medians):")
n=len(fids); idx={f:i for i,f in enumerate(fids)}
A=[];bvec=[]
for p in pairs:
    A.append(np.zeros(n)); A[-1][idx[p['a']]]=1; A[-1][idx[p['b']]]=-1
    bvec.append(math.log(np.median([1.0])) if False else 0.0)
# recompute log ratios
A=[];bvec=[]
for p in pairs:
    row=np.zeros(n); row[idx[p['a']]]=1; row[idx[p['b']]]=-1; A.append(row)
    ca,cb=F[p['a']][:,0],F[p['b']][:,0]
    common,ia,ib=np.intersect1d(ca,cb,return_indices=True)
    bvec.append(math.log(float(np.median(F[p['a']][ia,1]/F[p['b']][ib,1]))))
A=np.array(A); bvec=np.array(bvec)
# gauge: mean log scale = 0
Ag=np.vstack([A,np.ones((1,n))/n]); bg=np.append(bvec,0.0)
sol,*_=np.linalg.lstsq(Ag,bg,rcond=None)
scales={int(fids[i]):float(math.exp(sol[i])) for i in range(n)}
sv=np.array(list(scales.values()))
print(f"  scale spread: min={sv.min():.3f} max={sv.max():.3f} ratio={sv.max()/sv.min():.3f}  pp={pp(sv):.3f}")
json.dump(dict(B0=B0,med_B=med_B,n_pairs=len(pairs),summary=summary,
               per_frame_scale=scales,
               mult_irr_median=float(np.median(irr_mult)),add_irr_median=float(np.median(irr_add)),
               add_coh_median=float(np.median(coh_add)),
               pairs=pairs),open(os.path.join(OUT,'real_data.json'),'w'),indent=1)
print("\nwrote real_data.json")
