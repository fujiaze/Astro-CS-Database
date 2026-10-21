"""Actual science-driven leaf refinement of a tiny positive HPX sliver.

Independent analytic truth and TAN source/drop area come from separate research
oracles. A fixed x=+/-1e12 native integral requests absolute allocated flux .01.
This is an integral request, not an invented normalized surface measurement.
"""
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile
import time
import round2_performance as driver
from round2_affine_independent_review import fixture
from round2_sliver import analytic_new
from round2_cpp_science_bridge import tan_area
from round2_science_budget import Interval, Q


def main():
    here=Path(__file__).resolve().parent
    output=here.parent/'evidence/round2_focused_budget.json'
    h=fixture(1e-7);A=tan_area(h)
    truth=analytic_new(h)['area']
    atruth=Interval(Q.from_float(truth['lower_sr']),Q.from_float(truth['upper_sr']))
    # The sufficient width makes every midpoint's error <= .01, with margin.
    requested_flux=Q(1,100);x=Q(10**12)
    requested_area_width=requested_flux*A.lo/x
    data={'command':'python 实验/science-foundation/oracles/round2_focused_budget.py',
          'source_sha256':{n:hashlib.sha256((here/n).read_bytes()).hexdigest() for n in
            ('round2_focused_budget.py','round2_affine.cpp','round2_performance.py','round2_sliver.py','round2_science_budget.py','round2_cpp_science_bridge.py')},
          'environment':{'python':platform.python_version(),'platform':platform.platform()},
          'known_native_drop_area_sr':A.dump(),'analytic_overlap_sr':truth,
          'requested_flux_abs_L':float(requested_flux),'native_integral_abs_L':float(x),
          'requested_area_width_sr':float(requested_area_width),
          'interpretation':'Deterministic conditional bounds. Focused success certifies requested leaf only; other returned leaf widths need not meet the same tolerance.',
          'attempts':[]}
    with tempfile.TemporaryDirectory(prefix='acsd-focused-') as temp:
        exe,env,build=driver.build(temp);data['build']=build
        # Include the old arbitrary global width, the derived request, and a
        # tighter impossible-at-this-budget request without deleting failures.
        for name,goal,focus in [('coarse_global',1e-18,False),
                                ('science_focused',float(requested_area_width),True),
                                ('overstrict_focused',1e-27,True)]:
            case=dict(name=name,header=h,nside=1,pixfrac=1.,tolerance_sr=goal,max_nodes=6401,
                      side=1,cache=0,threads=1)
            cmd=[str(exe)]+(['--leaf','0'] if focus else [])
            result=json.loads(subprocess.check_output(cmd,input=driver.encode(case),env=env,text=True,timeout=120))
            aa=result['first']['areas'].get('0',[0.,0.]);a=Interval(aa[0],aa[1]+result['first']['spill_sr'])
            assert a.lo<=atruth.lo<=atruth.hi<=a.hi
            rows=[]
            for sign in (1,-1):
                F=(a/A)*Q(sign)*x;trueF=(atruth/A)*Q(sign)*x
                actual=abs(F.mid-trueF.mid)+trueF.radius
                bound=max(F.mid-F.lo,F.hi-F.mid)
                rows.append({'native_x_L':float(sign*x),'allocated_flux_L':F.dump(),
                             'true_flux_L':trueF.dump(),'actual_error_upper_L':float(actual),
                             'predicted_error_L':float(bound),'science_status':
                                 'science_bounded' if bound<=requested_flux else 'science_unconverged'})
            data['attempts'].append({'name':name,'request':case,'focused_leaf':0 if focus else None,
                                     'measurement':result,'science':rows})
            output.write_text(json.dumps(data,indent=2,allow_nan=False)+'\n',encoding='utf-8')
            print(name,result['first']['status'],result['nodes'],result['seconds'],[(v['predicted_error_L'],v['science_status']) for v in rows],flush=True)


if __name__=='__main__': main()
