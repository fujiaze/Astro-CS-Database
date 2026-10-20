"""只共享输入头，逐叶比较两套独立曲边解算器的区间。"""
import argparse
import hashlib
import json
import math
from pathlib import Path

from astropy.io.fits import Header
from mpmath import iv
import chart_overlap as chart
import detector_independent as detector

SOURCES_AT_START={x:hashlib.sha256(Path(__file__).with_name(x).read_bytes()).hexdigest()
                  for x in ('chart_overlap.py','detector_independent.py','cross_geometry.py')}

def compare():
    wanted={'base_face_00','base_face_04','north_pole_sip','south_pole_sip',
            'equatorial_transition_sip','ra_wrap_sip','tiny_boundary_sliver','high_order_small_drop'}
    records=[]
    for data in detector.fixtures():
        if data['name'] not in wanted:
            continue
        h=Header(data['header']);h['RADESYS']='ICRS'
        params={k:data[k] for k in ('center_xy0','pixfrac','nside')}
        scale=math.radians(max(abs(h['CD1_1']),abs(h['CD2_2'])))
        goal=scale**2*data['pixfrac']**2*.025
        iv.dps=30
        a=chart.solve_overlap(dict(h),**params,absolute_tolerance_sr=goal,max_nodes=50000)
        b=detector.solve_overlap(h,**params,relative_tolerance=.01,max_nodes=80001)
        comparisons=[]
        for p in sorted(set(a['areas'])|set(b['areas']),key=int):
            aa=a['areas'].get(p,{'lower_sr':0.,'upper_sr':0.})
            bb=b['areas'].get(p,{'lower_sr':0.,'upper_sr':0.})
            lo=max(aa['lower_sr'],bb['lower_sr']);hi=min(aa['upper_sr'],bb['upper_sr'])
            comparisons.append({'ipix':int(p),'intersection_lower_sr':lo,'intersection_upper_sr':hi,
                                'intervals_intersect':lo<=hi})
        al,au=a['total_area']['lower_sr'],a['total_area']['upper_sr']
        bl,bu=b['total_area']['lower_sr'],b['total_area']['upper_sr']
        rec={'name':data['name'],'header':dict(h),'parameters':params,'chart':a,'detector':b,
             'per_leaf_comparison':comparisons,'all_leaf_intervals_intersect':all(x['intervals_intersect'] for x in comparisons),
             'total_intervals_intersect':max(al,bl)<=min(au,bu),
             'interpretation':'Shared header data only; A inverse-face/curved-boundary solver and B independent detector/forward-floor solver both assume interval arithmetic primitives. External library occurs only in separate validation. Midpoints are not forced to partition.'}
        records.append(rec)
        print(data['name'],a['status'],b['status'],rec['all_leaf_intervals_intersect'],rec['total_intervals_intersect'],flush=True)
    return {'source_sha256':SOURCES_AT_START,
            'cases':records}


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    result=compare();args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
