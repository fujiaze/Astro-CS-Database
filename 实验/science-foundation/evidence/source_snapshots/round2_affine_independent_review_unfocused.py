"""Independently selected true-HEALPix corner fixture for the C++ microkernel.

Known truth is the analytic dphi dz intersection, not agreement of solvers.
Also re-runs A/B with a different (small astronomical image scale) header.
"""
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile
import time
from astropy.io.fits import Header
from mpmath import iv
import round2_sliver as truth_model
import round2_performance as driver
import chart_overlap as a_solver
import detector_independent as b_solver

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_affine_independent_review.json'


def fixture(top):
    h=truth_model.make_header(0.)
    h['CD1_1']=h['CD2_2']=.002
    import math
    h['CRPIX2']=1.5-top/(.002*math.pi/180)
    return h


def run():
    names=('round2_affine_independent_review.py','round2_affine.cpp','round2_performance.py',
           'round2_sliver.py','chart_overlap.py','detector_independent.py')
    result={'command':'python 实验/science-foundation/oracles/round2_affine_independent_review.py',
            'source_sha256':{n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in names},
            'environment':{'python':platform.python_version(),'platform':platform.platform()},
            'qualification':'Analytic mathematics; MPFR/GMP/IEEE basic arithmetic and mpmath.iv enclosures are library/compiler assumptions, not formal verification',
            'cases':[]}
    with tempfile.TemporaryDirectory(prefix='acsd-independent-review-') as temp:
        exe,env,build=driver.build(temp);result['build']=build
        for label,top in [('strict_outside',-1e-7),('touch',0.),('tiny_positive',1e-7)]:
            h=fixture(top);truth=truth_model.analytic_new(h)
            request={'name':label,'header':h,'nside':1,'pixfrac':1.,'tolerance_sr':1e-18,
                     'max_nodes':5001,'side':1,'cache':0,'threads':1}
            t0=time.perf_counter()
            cp=subprocess.run([str(exe)],input=driver.encode(request),env=env,text=True,capture_output=True,check=True)
            cpp=json.loads(cp.stdout)
            ar=a_solver.solve_overlap(h,nside=1,absolute_tolerance_sr=1e-12,max_nodes=12001)
            br=b_solver.solve_overlap(Header(h),nside=1,relative_tolerance=0,absolute_tolerance_sr=1e-12,max_nodes=12001)
            records=[]
            for name,sol in [('cpp',cpp['first']),('A',ar),('B',br)]:
                if name=='cpp':
                    pair=sol['areas'].get('0',[0.,0.]);pair=[pair[0],pair[1]+sol['spill_sr']]
                else:
                    z=sol['areas'].get('0',{'lower_sr':0.,'upper_sr':0.});pair=[z['lower_sr'],z['upper_sr']]
                tr=truth['area']
                records.append({'method':name,'leaf0_interval_sr':pair,
                    'contains_analytic_interval':pair[0]<=tr['lower_sr']<=tr['upper_sr']<=pair[1],
                    'proved_positive':pair[0]>0})
            result['cases'].append({'name':label,'request':request,'truth':truth,
                'cpp':cpp,'A':ar,'B':br,'comparisons':records,'wall_seconds':time.perf_counter()-t0})
            OUT.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
            print(label,records,flush=True)


if __name__=='__main__': run()
