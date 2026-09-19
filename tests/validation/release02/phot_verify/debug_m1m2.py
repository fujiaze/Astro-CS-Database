#!/usr/bin/env python3
import os, sys, math, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P
from scipy.spatial import cKDTree

A=P.Frame('t2_m1_red',0); B=P.Frame('t2_m2_red',0)
cd=math.cos(math.radians(A.dec.mean()))
pa=np.column_stack([A.ra*cd*3600,A.dec*3600]); pb=np.column_stack([B.ra*cd*3600,B.dec*3600])
print('A sky ra range', A.ra.min(), A.ra.max(), 'dec', A.dec.min(), A.dec.max())
print('B sky ra range', B.ra.min(), B.ra.max(), 'dec', B.dec.min(), B.dec.max())
tree=cKDTree(pb); d,j=tree.query(pa,k=1)
for tol in (0.5,1.0,2.0,5.0):
    print('tol',tol,'matches',(d<tol).sum())
m=(A.qual==0)&(A.snr>10)&(d<1.0)
print('good matches',m.sum())
print('A dec percentiles', np.percentile(A.dec,[0,5,50,95,100]))
print('B dec percentiles', np.percentile(B.dec,[0,5,50,95,100]))
# overlap in dec
lo=max(A.dec.min(),B.dec.min()); hi=min(A.dec.max(),B.dec.max())
print('dec overlap', lo, hi, 'deg =', (hi-lo)*3600,'arcsec')
mA=(A.dec>lo)&(A.dec<hi)
print('A sources in dec overlap', mA.sum())
