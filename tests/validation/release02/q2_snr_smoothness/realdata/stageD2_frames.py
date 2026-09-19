#!/usr/bin/env python3
"""Q2 stage D2: frame table, top-level metadata, and bitref vs upmfix observation equality."""
import json, numpy as np
L4='/workspace/Astro CS Database/run/RELEASE-02/L4-rebuild'
OUT='/workspace/Astro CS Database/run/RELEASE-02/q2-snr-smooth/realdata'
A=json.load(open(L4+'/bitref_16w/p2_samples.json'))
B=json.load(open(L4+'/upmfix_out/p2_samples.json'))
for tag,J in (('bitref_16w',A),('upmfix_out',B)):
    print('---',tag)
    for k in ('schema','entry','n_controls','n_obs','input_manifest_hash','target_order','control_grid_per_tile'):
        v=J.get(k)
        print('   %-22s %s'%(k, str(v)[:120]))
    print('   frame_ids (%d): %s'%(len(J['frame_ids']), J['frame_ids'][:5]))
print()
print('observations identical between the two files (all fields, order-sensitive)?')
oa,ob=A['observations'],B['observations']
same=len(oa)==len(ob)
if same:
    keys=set()
    for o in oa[:50]: keys|=set(o.keys())
    keys=sorted(keys)
    print('   common keys:',keys)
    for k in keys:
        eq=all(oa[i].get(k)==ob[i].get(k) for i in range(len(oa)))
        if not eq:
            d=[i for i in range(len(oa)) if oa[i].get(k)!=ob[i].get(k)]
            print('   DIFF %-18s n_diff=%d  first: %s vs %s'%(k,len(d),oa[d[0]].get(k),ob[d[0]].get(k)))
            same=False
print('   -> observations identical:', same)
print('   controls identical:', A['controls']==B['controls'])
print('   frame_ids identical:', A['frame_ids']==B['frame_ids'])
print('   stats identical:', A['stats']==B['stats'])
print('   stats A:',json.dumps(A['stats'],sort_keys=True))
print('   stats B:',json.dumps(B['stats'],sort_keys=True))
print()
print('=== FRAME TABLE (bitref_16w) ===')
print('%22s %8s %14s %14s %14s %12s'%('frame_id','n_samp','value_med','value_MAD','cvar_med','civar_med'))
fr={}
for o in oa:
    fr.setdefault(int(o['frame_id']),[]).append(o)
for fid in sorted(fr):
    v=np.array([float(o['value']) for o in fr[fid]])
    cv=np.array([float(o['control_variance']) for o in fr[fid]])
    ci=np.array([float(o['control_ivar']) for o in fr[fid]])
    med=float(np.median(v)); mad=float(1.4826*np.median(np.abs(v-med)))
    print('%22d %8d %14.6g %14.4g %14.6g %12.4g'%(fid,len(v),med,mad,float(np.median(cv)),float(np.median(ci))))
print('total frames',len(fr),'total samples',sum(len(x) for x in fr.values()))
