"""Independent order29 integer-range, candidate-cap and focused-error checks."""
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import tempfile
from astropy_healpix import HEALPix
from astropy.wcs import WCS
from astropy import units as u
from astropy.io.fits import Header
from mpmath import mp
import numpy as np
import round2_affine_independent_review as selected
import round2_performance as driver
import round2_sliver as truth_model
import detector_independent as detector

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_high_order_focus_review.json'


def main():
    sources=('round2_high_order_focus_review.py','round2_affine.cpp','round2_performance.py',
             'round2_affine_independent_review.py','round2_sliver.py','detector_independent.py')
    result={'command':'python 实验/science-foundation/oracles/round2_high_order_focus_review.py',
            'source_sha256':{n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in sources},
            'environment':{'python':platform.python_version(),'platform':platform.platform()},'cases':[]}
    N=1<<29
    # A genuine central point of an externally selected high-order leaf.
    hp=HEALPix(nside=N,order='nested')
    leaf=7*N*N+N*N//3+12345
    lon,lat=hp.healpix_to_lonlat(leaf)
    h=selected.fixture(0.)
    tiny=1e-5/3600
    h.update(CRVAL1=float(lon.deg),CRVAL2=float(lat.deg),CRPIX1=1.,CRPIX2=1.,
             CD1_1=tiny,CD2_2=tiny)
    mesh=np.array([(x,y) for x in np.linspace(-.5,.5,5) for y in np.linspace(-.5,.5,5)])
    sky=WCS(Header(h)).all_pix2world(mesh,0)
    members=hp.lonlat_to_healpix(sky[:,0]*u.deg,sky[:,1]*u.deg)
    assert np.all(members==leaf)
    mp.dps=100
    a=mp.mpf(tiny)*mp.pi/360
    true_area=4*mp.atan(a*a/mp.sqrt(1+2*a*a))
    base={'name':'order29_tiny_pixel','header':h,'nside':N,'pixfrac':1.,'tolerance_sr':1e-28,
          'max_nodes':101,'side':1,'cache':0,'threads':1}
    with tempfile.TemporaryDirectory(prefix='acsd-order29-') as temp:
        exe,env,build=driver.build(temp);result['build']=build
        def execute(request,args=()):
            cp=subprocess.run([str(exe),*args],input=driver.encode(request),env=env,text=True,capture_output=True,timeout=30)
            assert cp.returncode==0,(cp.returncode,cp.stderr)
            return json.loads(cp.stdout)
        got=execute(base)
        bounds=got['first']['areas'].get(str(leaf),[0.,0.])
        result['cases'].append({'request':base,'measurement':got,'external_leaf':leaf,
            'external_grid_points':len(mesh),'external_unique_members':[int(i) for i in np.unique(members)],
            'analytic_total_area_sr_decimal':mp.nstr(true_area,90),
            'contains_analytic_total_in_leaf':mp.mpf(bounds[0])<=true_area<=mp.mpf(bounds[1]),
            'independent_detector':detector.solve_overlap(Header(h),nside=N,relative_tolerance=0,
                                                          absolute_tolerance_sr=1e-28,max_nodes=101)})
        wide=dict(base);wide['name']='order29_candidate_cap';wide['header']=dict(h)
        wide['header'].update(CD1_1=1.,CD2_2=1.)
        wide.update(max_nodes=1,tolerance_sr=1e-20)
        cap=execute(wide)
        assert cap['first']['spill_sr']>0 and cap['first']['status']=='budget_exhausted'
        result['cases'].append({'request':wide,'measurement':cap,
            'judgment':'finite full-source spill retained under candidate cap; no enumeration overflow, crash, or fake empty support'})
        sh=selected.fixture(1e-7)
        sh.update(CTYPE1='RA---TAN-SIP',CTYPE2='DEC--TAN-SIP',A_ORDER=2,B_ORDER=2,A_0_2=.07)
        truth=truth_model.analytic_new(sh)
        focus=dict(base);focus.update(name='focused_sip_tiny_leaf',header=sh,nside=1,
                                     tolerance_sr=1e-20,max_nodes=1001)
        focused=execute(focus,['--leaf','0'])
        bounds=focused['first']['areas']['0'];tr=truth['area']
        result['cases'].append({'request':focus,'arguments':['--leaf','0'],'measurement':focused,
            'truth':truth,'contains_analytic_interval':bounds[0]<=tr['lower_sr']<=tr['upper_sr']<=bounds[1],
            'focus_only_success':focused['first']['status']=='bounded' and focused['first']['all_leaf_width_sr']>focus['tolerance_sr']})
    OUT.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    print([(c['request']['name'],c['measurement']['first']['status'],c.get('contains_analytic_interval',c.get('contains_analytic_total_in_leaf'))) for c in result['cases']])


if __name__=='__main__': main()
