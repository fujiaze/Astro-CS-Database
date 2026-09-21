#!/usr/bin/env python3
import os, sys, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P

pairs = [
    ('t2_m1_red',0,'t2_m1_red',1),
    ('t2_m1_red',0,'t2_m2_red',0),
    ('t2_m1_red',0,'t2_m4_red',0),
    ('t2_m1_red',0,'t3_m1_red',0),
]
out=[]
for (ta,ia,tb,ib) in pairs:
    A=P.Frame(ta,ia); B=P.Frame(tb,ib)
    r=P.analyse(A,B,min_snr=10.0)
    if r is None:
        print('--- %s[%d] vs %s[%d]  NO MATCH'%(ta,ia,tb,ib)); continue
    mag = -2.5*math.log10(r['ratio_median'])
    print('--- %s[%d] vs %s[%d]'%(ta,ia,tb,ib))
    print('  n=%d dist_med=%.3f arcsec  ratio_med=%.4f (%.3f mag)  scatter=%.4f dex (%.3f mag)'
          %(r['n_match'],r['dist_med_arcsec'],r['ratio_median'],mag,r['z_std_dex'],r['z_std_mag']))
    print('  overlap extent arcsec', [round(v,1) for v in r['overlap_px_arcsec']])
    for o in (0,1,2):
        p=r['poly%d'%o]
        print('  poly%d resid_mad=%.4f dex (%.3f mag) model_ptp=%.4f dex (%.3f mag) coef=%s'
              %(o,p['resid_mad_dex'],p['resid_mad_mag'],p['model_ptp_dex'],p['model_ptp_mag'],[round(c,6) for c in p['coef']]))
    out.append(r)
json.dump(out, open('pair_probe.json','w'), indent=1)
