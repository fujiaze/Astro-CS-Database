#!/usr/bin/env python3
import json, math, os
d=json.load(open('pixel_results.json'))
p=json.load(open('pair_results.json'))
rows=[]
for k,r in d.items():
    m=r['meta']; b=r['binmap']
    rows.append((k,m['n'],-2.5*math.log10(m['ratio_median']),2.5*m['z_mad'],
                 2.5*r['poly1']['model_ptp_dex'],2.5*b['ratio_ptp_dex']))
out=dict(pixel_table=[dict(tag=k,n=n,frame_level_mag=fl,scatter_mag=sc,plane_ptp_mag=pp,bin8_ptp_mag=bp) for k,n,fl,sc,pp,bp in rows])
# catalog-flux table for all pairs
cat=[]
for k,r in p.items():
    cat.append(dict(tag=k,n=r['n_match'],ratio_median=r['ratio_median'],
                    frame_level_mag=-2.5*math.log10(r['ratio_median']),
                    scatter_mag=r['z_std_mag'],
                    plane_ptp_mag=r['poly1']['model_ptp_mag'],
                    plane_resid_mad_mag=r['poly1']['resid_mad_mag']))
out['catalog_table']=cat
json.dump(out, open('evidence_summary.json','w'), indent=1)
print(json.dumps(out['pixel_table'],indent=1))
