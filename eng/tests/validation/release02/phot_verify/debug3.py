#!/usr/bin/env python3
import os, sys, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P
for t,i in [('t2_m1_red',0),('t2_m2_red',0),('t3_m1_red',0)]:
    F=P.Frame(t,i)
    q=F.qual==0
    f=F.flux[q]
    print(t,i,'n',q.sum(),'flux pct', np.percentile(f,[10,25,50,75,90,99]).round(1))
    print('   snr pct', np.percentile(F.snr[q],[10,25,50,75,90]).round(2))
