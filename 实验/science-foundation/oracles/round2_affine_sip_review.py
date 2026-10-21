"""Fresh asymmetric quadratic SIP shear with analytically unchanged sliver.

X=c(u+q v²), Y=c v, q=.07. determinant=1 everywhere: no fold.
Only side edges curve. The known tiny corner intersection never reaches those
edges, certified below, hence its top-boundary analytic area remains exact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile
import time
from astropy.io.fits import Header
from mpmath import iv
import round2_affine_independent_review as selected
import round2_sliver as truth_model
import round2_performance as driver
import chart_overlap as a_solver
import detector_independent as b_solver

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_affine_sip_review.json'


def main(output=OUT):
    source_names=('round2_affine_sip_review.py','round2_affine_independent_review.py',
                  'round2_affine.cpp','round2_performance.py','round2_sliver.py',
                  'chart_overlap.py','detector_independent.py')
    h=selected.fixture(1e-7)
    h.update(CTYPE1='RA---TAN-SIP',CTYPE2='DEC--TAN-SIP',A_ORDER=2,B_ORDER=2,A_0_2=.07)
    truth=truth_model.analytic_new(h)
    iv.dps=100
    cd=iv.mpf(h['CD1_1'])*iv.pi/180
    d=cd*(iv.mpf('1.5')-iv.mpf(h['CRPIX2']))
    theta=iv.mpf([truth['root_theta_rad']['lower'],truth['root_theta_rad']['upper']])
    # On the whole sliver 0<=Y<=d, so shear shift<=q*d²/c.
    margin=cd/2-iv.tan(theta)-iv.mpf(h['A_0_2'])*d*d/cd
    assert float(margin.a)>0
    request={'name':'independent_quadratic_shear_sliver','header':h,'nside':1,'pixfrac':1.,
             'tolerance_sr':1e-18,'max_nodes':8001,'side':1,'cache':0,'threads':1}
    result={'command':'python 实验/science-foundation/oracles/round2_affine_sip_review.py --output '+str(output),
        'source_sha256':{n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in source_names},
        'environment':{'python':platform.python_version(),'platform':platform.platform()},
        'request':request,'truth':truth,'side_inactive_margin_rad_interval':str(margin),
        'global_injectivity_proof':'Shear has polynomial inverse u=X/c-q(Y/c)^2,v=Y/c, determinant exactly1',
        'qualification':'Analytic true area and independent finite run; conditional on arithmetic library/compiler assumptions; not formal machine certification'}
    with tempfile.TemporaryDirectory(prefix='acsd-sip-review-') as temp:
        exe,env,build=driver.build(temp);result['build']=build
        cp=subprocess.run([str(exe)],input=driver.encode(request),env=env,text=True,capture_output=True,check=True)
        cpp=json.loads(cp.stdout);result['cpp']=cpp
    result['A']=a_solver.solve_overlap(h,nside=1,absolute_tolerance_sr=1e-12,max_nodes=12001)
    result['B']=b_solver.solve_overlap(Header(h),nside=1,relative_tolerance=0,absolute_tolerance_sr=1e-12,max_nodes=12001)
    result['comparisons']=[]
    for name,sol in [('cpp',cpp['first']),('A',result['A']),('B',result['B'])]:
        if name=='cpp':
            pair=sol['areas'].get('0',[0.,0.]);pair=[pair[0],pair[1]+sol['spill_sr']]
        else:
            z=sol['areas'].get('0',{'lower_sr':0.,'upper_sr':0.});pair=[z['lower_sr'],z['upper_sr']]
        tr=truth['area']
        result['comparisons'].append({'method':name,'leaf0_interval_sr':pair,
            'contains_analytic_interval':pair[0]<=tr['lower_sr']<=tr['upper_sr']<=pair[1],
            'proved_positive':pair[0]>0})
    output.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    print(result['comparisons'],flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=OUT)
    main(p.parse_args().output)
