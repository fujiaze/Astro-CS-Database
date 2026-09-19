#!/usr/bin/env python3
"""Q2 stage C2: direct transparent profiles across each seam (row/col medians), plus NEW-OLD diff."""
import numpy as np, warnings
warnings.filterwarnings('ignore')
from astropy.io import fits
ROOT='/workspace/Astro CS Database'; L4=ROOT+'/run/RELEASE-02/L4-rebuild'
P={'old':L4+'/p3_r_vis/output_phase3.fits','new':L4+'/p3_r_vis_fixed/output_phase3.fits','upm':L4+'/p3_upmfix/output_phase3.fits'}
D={}
for k,p in P.items():
    with fits.open(p,memmap=True) as h: D[k]=np.asarray(h[0].data,dtype=np.float64)

def med(a): return float(np.nanmedian(a))

print('=== H1top: row-median over x[900,1900] ===')
print('   y      old          new          upm        new-old       upm-old')
for y in range(1140,1225,2):
    o=med(D['old'][y,900:1900]); n=med(D['new'][y,900:1900]); u=med(D['upm'][y,900:1900])
    print('%5d %12.5g %12.5g %12.5g %12.4g %12.4g'%(y,o,n,u,n-o,u-o))
print()
print('=== H1bot: row-median over x[900,1900] ===')
print('   y      old          new          upm        new-old       upm-old')
for y in range(1360,1430,2):
    o=med(D['old'][y,900:1900]); n=med(D['new'][y,900:1900]); u=med(D['upm'][y,900:1900])
    print('%5d %12.5g %12.5g %12.5g %12.4g %12.4g'%(y,o,n,u,n-o,u-o))
print()
print('=== V-right: col-median over y[500,3300] ===')
print('   x      old          new          upm        new-old       upm-old')
for x in range(2100,2205,2):
    o=med(D['old'][500:3300,x]); n=med(D['new'][500:3300,x]); u=med(D['upm'][500:3300,x])
    print('%5d %12.5g %12.5g %12.5g %12.4g %12.4g'%(x,o,n,u,n-o,u-o))
