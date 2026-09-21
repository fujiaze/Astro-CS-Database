#!/usr/bin/env python3
import json, math, numpy as np
d=json.load(open('pair_results.json'))
def line(k,r):
    mag=-2.5*math.log10(r['ratio_median'])
    return ('%-34s n=%6d ratio=%.4f (%+.3f mag) scat=%.4f dex(%.3f mag) planePTP=%.4f dex(%.3f mag) plane_res=%.4f radial_res=%.4f rad_span=%.3f'
        %(k,r['n_match'],r['ratio_median'],mag,r['z_std_dex'],r['z_std_mag'],
          r['poly1']['model_ptp_dex'],r['poly1']['model_ptp_mag'],r['poly1']['resid_mad_dex'],
          r['radial']['resid_mad_dex'],r['radial']['rA2_minus_rB2_span_1e6px2']))
print('### ADJACENT-TILE pairs')
for k in sorted(d):
    if k.startswith('adj_'): print(line(k,d[k]))
print()
print('### CROSS-TELESCOPE pairs')
for k in sorted(d):
    if k.startswith('xtel_'): print(line(k,d[k]))
print()
print('### WITHIN-TILE summary (scatter of scatter)')
w=[v for k,v in d.items() if k.startswith('within_')]
import statistics
print('n within pairs', len(w))
print('z_std_dex median', np.median([v['z_std_dex'] for v in w]))
print('plane PTP dex median', np.median([v['poly1']['model_ptp_dex'] for v in w]))
print('plane PTP dex p90', np.percentile([v['poly1']['model_ptp_dex'] for v in w],90))
print('worst within pairs by |ratio|:')
for k,v in sorted(d.items(), key=lambda kv:-abs(math.log10(kv[1]['ratio_median'])))[:8]:
    if k.startswith('within_'): print('  ',line(k,v))
