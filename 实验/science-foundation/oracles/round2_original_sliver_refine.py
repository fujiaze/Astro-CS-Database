"""Re-run original tiny sliver against *independently analytic* true area.

No generic A/B solver is modified. Preserve resource failure of global A;
B's local membership subdivision eventually obtains positive lower bounds.
"""
import hashlib
import json
from pathlib import Path
import platform
import time
from astropy.io.fits import Header
import chart_overlap as chart
import detector_independent as detector
import round2_sliver as analytic

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_original_sliver_refined.json'


def main():
    hashes={n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in
            ('round2_original_sliver_refine.py','round2_sliver.py','chart_overlap.py','detector_independent.py')}
    original=next(x for x in detector.fixtures() if x['name']=='tiny_boundary_sliver')
    header=Header(original['header']);header['RADESYS']='ICRS'
    truth=analytic.old_sliver()
    result={'command':'python 实验/science-foundation/oracles/round2_original_sliver_refine.py',
            'source_sha256':hashes,'environment':{'python':platform.python_version(),'platform':platform.platform()},
            'header':dict(header),'parameters':{'center_xy0':[0.,0.],'pixfrac':1.,'nside':8},
            'analytic_truth':truth,'runs':[]}
    # A controls whole-drop boundary uncertainty, so it can spend heavily on
    # the distant leaf304 boundary. Retain failure rather than loosening goal.
    for name,absolute,nodes in [('chart',1e-11,15001),('detector',1e-16,40001)]:
        t0=time.perf_counter()
        if name=='chart':
            sol=chart.solve_overlap(dict(header),nside=8,absolute_tolerance_sr=absolute,max_nodes=nodes)
        else:
            sol=detector.solve_overlap(header,nside=8,relative_tolerance=0,
                                       absolute_tolerance_sr=absolute,max_nodes=nodes)
        comparisons=[]
        for p in (282,293):
            got=sol['areas'][str(p)];true=truth['area_each']
            comparisons.append({'ipix':p,'contains_analytic_interval':
                got['lower_sr']<=true['lower_sr']<=true['upper_sr']<=got['upper_sr'],
                'proved_positive':got['lower_sr']>0,
                'midpoint_true_error_upper_sr':max(abs(got['mid_sr']-true['lower_sr']),abs(got['mid_sr']-true['upper_sr']))})
        result['runs'].append({'method':name,'requested_absolute_sr':absolute,'max_nodes':nodes,
                               'result':sol,'analytic_comparisons':comparisons,'wall_seconds':time.perf_counter()-t0})
        OUT.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
        print(name,sol['status'],[(x['ipix'],x['proved_positive'],x['contains_analytic_interval']) for x in comparisons],flush=True)


if __name__=='__main__': main()
