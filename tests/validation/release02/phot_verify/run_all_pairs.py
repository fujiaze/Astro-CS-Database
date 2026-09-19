#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: run cross-frame multiplicative-residual analysis over L4 pairs."""
import os, sys, json, math, glob
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pair_ratio as P

TILES = ['t2_m1_red','t2_m2_red','t2_m3_red','t2_m4_red','t2_m5_red','t2_m6_red',
         't3_m1_red','t3_m2_red','t3_m3_red','t3_m4_red','t3_m5_red','t3_m6_red']
T2 = ['t2_m1_red','t2_m2_red','t2_m3_red','t2_m4_red','t2_m5_red','t2_m6_red']
T3 = ['t3_m1_red','t3_m2_red','t3_m3_red','t3_m4_red','t3_m5_red','t3_m6_red']
# adjacency in the 3x2 grid (M1 M2 M3 / M4 M5 M6)
ADJ = [(0,1),(1,2),(3,4),(4,5),(0,3),(1,4),(2,5)]

cache = {}
def F(tile, idx):
    k=(tile,idx)
    if k not in cache: cache[k]=P.Frame(tile,idx)
    return cache[k]

def run(A,B,tag,min_flux=5000.0):
    r = P.analyse(A,B,min_flux=min_flux)
    if r is None:
        print('%-38s NO MATCH'%tag); return None
    arr = r.pop('_arrays')
    np.savez_compressed(os.path.join(OUT, 'arr_%s.npz'%tag.replace('/','_')),
                        **{k:v for k,v in arr.items()})
    mag = -2.5*math.log10(r['ratio_median'])
    print('%-38s n=%6d ratio=%.4f (%+.3f mag) scat=%.4f dex (%.3f mag) | plane_ptp=%.4f dex (%.3f mag) plane_res=%.4f dex | radial_ptp_span=%.3f k=%.4f res=%.4f'
          %(tag, r['n_match'], r['ratio_median'], mag, r['z_std_dex'], r['z_std_mag'],
            r['poly1']['model_ptp_dex'], r['poly1']['model_ptp_mag'], r['poly1']['resid_mad_dex'],
            r['radial']['rA2_minus_rB2_span_1e6px2'], r['radial']['coef'][1], r['radial']['resid_mad_dex']))
    r['tag']=tag
    return r

OUT = os.path.dirname(os.path.abspath(__file__))
results = {}

print('=========== 1) WITHIN-TILE consecutive frame pairs (same pointing, small dither) ===========')
for tile in TILES:
    sj = json.load(open(os.path.join(P.NORM, tile, 'p1_sources.json')))
    n = len(sj['frames'])
    for a in range(n):
        for b in range(a+1, n):
            r = run(F(tile,a), F(tile,b), 'within_%s_f%d_f%d'%(tile,a,b))
            if r: results[r['tag']]=r

print()
print('=========== 2) ADJACENT-TILE pairs (same telescope) ===========')
for tel,ts in (('t2',T2),('t3',T3)):
    for (u,v) in ADJ:
        r = run(F(ts[u],0), F(ts[v],0), 'adj_%s_%s_%s'%(tel,ts[u][3:],ts[v][3:]))
        if r: results[r['tag']]=r

print()
print('=========== 3) CROSS-TELESCOPE same tile (t2 vs t3) ===========')
for i in range(6):
    r = run(F(T2[i],0), F(T3[i],0), 'xtel_m%d'%(i+1))
    if r: results[r['tag']]=r

json.dump(results, open(os.path.join(OUT,'pair_results.json'),'w'), indent=1)
print()
print('saved', os.path.join(OUT,'pair_results.json'), 'n=',len(results))
