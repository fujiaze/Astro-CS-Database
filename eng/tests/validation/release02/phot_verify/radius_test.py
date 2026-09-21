#!/usr/bin/env python3
"""Aperture-radius sensitivity of the frame-level ratio (bounds the seeing-induced
systematic in the multiplicative-residual measurement)."""
import os, sys, math
import numpy as np
from astropy.io import fits
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pixel_measure as M
import pair_ratio as P

CASES = [('t2_m1_red',0,'t2_m1_red',1,'within_m1_f0f1'),
         ('t2_m5_red',0,'t2_m6_red',0,'adj_m5_m6'),
         ('t2_m1_red',0,'t3_m1_red',0,'xtel_m1')]
for ta,ia,tb,ib,name in CASES:
    A=P.Frame(ta,ia); B=P.Frame(tb,ib)
    i,j,d = P.match(A,B,tol_arcsec=0.7,min_flux=8000.0)
    imgA = fits.getdata(M.cal_path(ta,A.file)).astype(np.float32)
    imgB = fits.getdata(M.cal_path(tb,B.file)).astype(np.float32)
    print('---',name,'n_matched',len(i))
    for r in (3.0,4.0,5.0,6.0,8.0,10.0):
        fA,sA,_=M.aperture_sum(imgA,A.x[i],A.y[i],r,14.0,20.0)
        fB,sB,_=M.aperture_sum(imgB,B.x[j],B.y[j],r,14.0,20.0)
        ok=np.isfinite(fA)&np.isfinite(fB)&(fA>0)&(fB>0)
        z=np.log10(fA[ok]/fB[ok])
        for _ in range(3):
            m=np.median(z); s=1.482602218505602*np.median(np.abs(z-m))
            if s<=0: break
            k=np.abs(z-m)<4*s
            if k.all(): break
            z=z[k]
        print('   r=%4.1f n=%5d ratio=%.4f (%+.4f mag)  z_mad=%.4f dex (%.4f mag)'%(r,ok.sum(),10**np.median(z),-2.5*np.median(z),1.482602218505602*np.median(np.abs(z-np.median(z))),2.5*1.482602218505602*np.median(np.abs(z-np.median(z)))))
    del imgA,imgB
