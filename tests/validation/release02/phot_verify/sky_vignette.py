#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: single-frame residual-vignetting probe on the SKY background.

The night sky is (to first order) a smooth plane over a 1.1 deg field. After
flat-fielding, any remaining radial bowl in the sky level = residual vignetting /
flat error. Stars are masked with the detection catalogue.
"""
import os, sys, math, json
import numpy as np
from astropy.io import fits
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pixel_measure as M
import pair_ratio as P

def probe(tile, idx, rbin=200.0):
    F = P.Frame(tile, idx)
    img = fits.getdata(M.cal_path(tile, F.file)).astype(np.float32)
    H, W = img.shape
    # mask stars (r=8 px) using the detection catalogue (quality any)
    mask = np.ones((H, W), bool)
    xs = np.round(F.x).astype(int); ys = np.round(F.y).astype(int)
    for dx in range(-8, 9):
        for dy in range(-8, 9):
            if dx*dx+dy*dy > 64: continue
            xx = np.clip(xs+dx, 0, W-1); yy = np.clip(ys+dy, 0, H-1)
            mask[yy, xx] = False
    yy, xx = np.mgrid[0:H, 0:W]
    r = np.sqrt((xx-2047.5)**2 + (yy-2047.5)**2)
    # plane fit on sky pixels (coarse grid to bound memory)
    sy, sx = np.mgrid[0:H:16, 0:W:16]
    v = img[0:H:16, 0:W:16].astype(np.float64)
    m = mask[0:H:16, 0:W:16]
    A = np.column_stack([sx[m].ravel(), sy[m].ravel(), np.ones(m.sum())])
    coef, *_ = np.linalg.lstsq(A, v[m].ravel(), rcond=None)
    plane = coef[0]*sx + coef[1]*sy + coef[2]
    res = v - plane
    rr = np.sqrt((sx-2047.5)**2 + (sy-2047.5)**2)
    out=[]
    for lo in range(0, 3000, int(rbin)):
        hi = lo+rbin
        sel = m & (rr >= lo) & (rr < hi)
        if sel.sum() < 30: continue
        out.append(dict(r_mid=0.5*(lo+hi), n=int(sel.sum()),
                        sky_med=float(np.median(v[sel])),
                        sky_resid_med=float(np.median(res[sel])),
                        sky_resid_mad=float(1.482602218505602*np.median(np.abs(res[sel]-np.median(res[sel]))))))
    del img
    return dict(tile=tile, idx=idx, file=F.file,
                plane_coef=[float(c) for c in coef],
                plane_grad_adu_per_px=[float(coef[0]), float(coef[1])],
                sky_level_med=float(np.median(v[m])),
                annuli=out)

res={}
for tile,idx in [('t2_m1_red',0),('t2_m1_red',1),('t2_m4_red',0),('t2_m6_red',0),
                 ('t3_m1_red',0),('t3_m6_red',0)]:
    p=probe(tile,idx)
    res['%s_f%d'%(tile,idx)]=p
    print('==',tile,'f%d'%idx,'sky=%.1f ADU/px  plane grad=(%.3f,%.3f) ADU/px'%(p['sky_level_med'],p['plane_grad_adu_per_px'][0],p['plane_grad_adu_per_px'][1]))
    for a in p['annuli']:
        print('    r=%5.0f px  sky=%.1f  resid=%+.2f +- %.2f ADU/px  (%.3f%% of sky)'
              %(a['r_mid'],a['sky_med'],a['sky_resid_med'],a['sky_resid_mad'],100*a['sky_resid_med']/a['sky_med']))
json.dump(res, open('sky_vignette.json','w'), indent=1)
print('saved sky_vignette.json')
