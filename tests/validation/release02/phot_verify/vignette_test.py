#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: vignetting-type (detector-fixed radial) vs gradient-type (sky-plane)
decomposition of the residual spatial multiplicative pattern."""
import os, sys, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pixel_measure as M
import pair_ratio as P

PAIRS = [
 ('within_t2_m1_red_f0_t2_m1_red_f1','t2_m1_red',0,'t2_m1_red',1),
 ('adj_t2_m2_red_f0_t2_m3_red_f0','t2_m2_red',0,'t2_m3_red',0),
 ('adj_t2_m5_red_f0_t2_m6_red_f0','t2_m5_red',0,'t2_m6_red',0),
 ('xtel_t2_m1_red_f0_t3_m1_red_f0','t2_m1_red',0,'t3_m1_red',0),
 ('xtel_t2_m4_red_f0_t3_m4_red_f0','t2_m4_red',0,'t3_m4_red',0),
 ('xtel_t2_m5_red_f0_t3_m5_red_f0','t2_m5_red',0,'t3_m5_red',0),
]
out={}
for name,ta,ia,tb,ib in PAIRS:
    f='px_%s.npz'%name
    if not os.path.exists(f):
        print(name,'missing npz'); continue
    d=np.load(f)
    z=d['z']; px=d['px']; py=d['py']
    rA2=(d['Ax']-2047.5)**2+(d['Ay']-2047.5)**2
    rB2=(d['Bx']-2047.5)**2+(d['By']-2047.5)**2
    n=len(z)
    # design matrices
    ones=np.ones(n)
    G=np.column_stack([ones, px, py])            # sky gradient (plane)
    V=np.column_stack([ones, (rA2-rB2)/1e6])     # detector radial (vignetting)
    GV=np.column_stack([ones, px, py, (rA2-rB2)/1e6])
    def fit(Mx):
        c,*_=np.linalg.lstsq(Mx,z,rcond=None); r=z-Mx@c
        return c, float(1.482602218505602*np.median(np.abs(r-np.median(r))))
    cG,rG=fit(G); cV,rV=fit(V); cGV,rGV=fit(GV)
    c0,r0=fit(ones.reshape(-1,1))
    # correlation of z with sky-plane vs radial
    def corr(a,b):
        a=a-a.mean(); b=b-b.mean()
        return float((a*b).sum()/math.sqrt((a*a).sum()*(b*b).sum()+1e-300))
    out[name]=dict(n=n, z_mad=r0, sky_plane=dict(coef=cG.tolist(), resid_mad=rG),
                   vignette=dict(coef=cV.tolist(), resid_mad=rV),
                   both=dict(coef=cGV.tolist(), resid_mad=rGV),
                   corr_skyx=corr(z,px), corr_skyy=corr(z,py),
                   corr_radial=corr(z,(rA2-rB2)),
                   rA2_span_1e6=float((rA2.max()-rA2.min())/1e6),
                   rB2_span_1e6=float((rB2.max()-rB2.min())/1e6))
    print('%-34s n=%5d  z_mad=%.4f | skyGrad_res=%.4f (corr x=%+.3f y=%+.3f) | vign_res=%.4f (corr=%+.3f) | both_res=%.4f'
          %(name,n,r0,rG,out[name]['corr_skyx'],out[name]['corr_skyy'],rV,out[name]['corr_radial'],rGV))
json.dump(out, open('vignette_test.json','w'), indent=1)
print('saved vignette_test.json')
