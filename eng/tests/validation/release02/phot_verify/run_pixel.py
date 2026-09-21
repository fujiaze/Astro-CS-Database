#!/usr/bin/env python3
import os, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pixel_measure as M

PAIRS = [
 ('within','t2_m1_red',0,'t2_m1_red',1),
 ('within','t2_m2_red',0,'t2_m2_red',1),
 ('adj','t2_m1_red',0,'t2_m2_red',0),
 ('adj','t2_m2_red',0,'t2_m3_red',0),
 ('adj','t2_m4_red',0,'t2_m5_red',0),
 ('adj','t2_m5_red',0,'t2_m6_red',0),
 ('adj','t3_m2_red',0,'t3_m5_red',0),
 ('xtel','t2_m2_red',0,'t3_m2_red',0),
 ('xtel','t2_m5_red',0,'t3_m5_red',0),
 ('xtel','t2_m1_red',0,'t3_m1_red',0),
 ('xtel','t2_m4_red',0,'t3_m4_red',0),
]
res={}
for kind,ta,ia,tb,ib in PAIRS:
    tag='%s_%s_f%d_%s_f%d'%(kind,ta,ia,tb,ib)
    out = M.measure_pair(ta,ia,tb,ib,tag=tag,min_flux=8000.0,max_stars=5000)
    if out is None:
        print(tag,'NO MATCH'); continue
    r, arr = out
    np.savez_compressed('px_%s.npz'%tag, **arr)
    res[tag]=r
    m=r['meta']; b=r['binmap']
    print('%-40s n=%5d ratio=%.4f (%+.3f mag) z_mad=%.4f dex (%.3f mag)'%(tag,m['n'],m['ratio_median'],-2.5*np.log10(m['ratio_median']),m['z_mad'],2.5*m['z_mad']))
    print('     spatial: planePTP=%.4f dex (%.3f mag) quadPTP=%.4f | bin8 PTP=%.4f dex (%.3f mag) bin8MAD=%.4f | bg: sA=%.1f sB=%.1f dsky=%.1f+-%.1f | a_bgPTP=%.4f c_bgPTP=%.1f c_bgMAD=%.1f'
          %(r['poly1']['model_ptp_dex'],2.5*r['poly1']['model_ptp_dex'],r['poly2']['model_ptp_dex'],
            b['ratio_ptp_dex'],2.5*b['ratio_ptp_dex'],b['ratio_mad_dex'],
            m['sky_A_median'],m['sky_B_median'],m['dsky_median'],m['dsky_mad'],
            b['a_bg_ptp'],b['c_bg_ptp'],b['c_bg_mad']))
json.dump(res, open('pixel_results.json','w'), indent=1)
print('saved pixel_results.json n=',len(res))
