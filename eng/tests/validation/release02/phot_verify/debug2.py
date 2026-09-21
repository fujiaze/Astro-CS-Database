#!/usr/bin/env python3
import os, sys, math, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P
A=P.Frame('t2_m1_red',0); B=P.Frame('t2_m2_red',0)
for tol in (0.5,0.7,1.0):
    i,j,d = P.match(A,B,tol_arcsec=tol,min_snr=10.0)
    print('tol',tol,'n',len(i))
i,j,d = P.match(A,B,tol_arcsec=0.7,min_snr=0.0)
print('min_snr=0 n',len(i))
