"""Independent analytic true-HEALPix slivers, not an A/B interval comparison.

Run from repository: python 实验/science-foundation/oracles/round2_sliver.py
Only the final A/B comparison imports those solvers. Analytic reference uses
dOmega=dphi dz and an independently derived TAN rectangle intersection.
mpmath interval primitives are assumptions, not formally certified machine code.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import time
from mpmath import mp, iv
from astropy.io.fits import Header

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_sliver.json'
mp.dps=110
iv.dps=100


def istr(x):
    return mp.nstr(x,105)


def interval(x):
    return iv.mpf(istr(x))


def iasin(x):
    return iv.atan2(x,iv.sqrt(1-x*x))


def outward(x):
    return {'lower_sr':max(0.,math.nextafter(float(x.a),-math.inf)),
            'upper_sr':math.nextafter(float(x.b),math.inf)}


def bracket_root(fun,lo,hi,iterations=310):
    """Bisection signs evaluated with interval arithmetic; monotonicity analytic.

    fun(lo)<0<fun(hi), fun strictly increasing. Endpoints are dyadic MP values.
    Every accepted bracket endpoint is checked with iv, not an mp sign.
    """
    assert float(fun(interval(lo)).b)<0 and float(fun(interval(hi)).a)>0
    for _ in range(iterations):
        mid=(lo+hi)/2
        f=fun(interval(mid))
        if float(f.b)<0: lo=mid
        elif float(f.a)>0: hi=mid
        else: break
    return iv.mpf([istr(lo),istr(hi)]),{'lower':istr(lo),'upper':istr(hi)}


def make_header(top_rad):
    # x=[-h,h], y=[top-2h,top]; h set by exact binary64 header CD.
    scale=2.0
    cdrad=scale*math.pi/180
    return {'NAXIS':2,'NAXIS1':1,'NAXIS2':1,
            'CTYPE1':'RA---TAN','CTYPE2':'DEC--TAN',
            'CUNIT1':'deg','CUNIT2':'deg','RADESYS':'ICRS',
            'CRVAL1':45.0,'CRVAL2':0.0,'CRPIX1':1.0,
            'CRPIX2':1.5-top_rad/cdrad,
            'CD1_1':scale,'CD1_2':0.0,'CD2_1':0.0,'CD2_2':scale}


def analytic_new(h):
    """Nside=1 face0: z >= k |theta|, k=8/(3pi), near bottom vertex.

    TAN top y=d gives ztop=d*cos(theta)/sqrt(1+d² cos²(theta)).
    theta* unique monotone root k theta-ztop(theta)=0. Its positivity
    and d>0 prove positive area; source side/bottom inactive checked below.
    """
    iv.dps=100
    d=iv.mpf(h['CD2_2'])*iv.pi/180*(iv.mpf('1.5')-iv.mpf(h['CRPIX2']))
    half=iv.mpf(h['CD1_1'])*iv.pi/360
    if h['CRPIX2']>=1.5:
        return {'classification':'strict_nonoverlap' if h['CRPIX2']>1.5 else 'exact_vertex_touch',
                'area':{'lower_sr':0.,'upper_sr':0.},'top_y_rad_interval':str(d)}
    k=8/(3*iv.pi)
    def f(t):
        return k*t-d*iv.cos(t)/iv.sqrt(1+d*d*iv.cos(t)**2)
    theta,root=bracket_root(f,mp.mpf(0),mp.mpf(str(float(half.a))))
    assert float(iv.tan(theta).b)<float(half.a)
    area=2*iasin(d*iv.sin(theta)/iv.sqrt(1+d*d))-k*theta**2
    # Integrand nonnegative, and a positive interior rectangle is an
    # independent lower certificate, rather than relying on cancellation.
    # theta in [-thetaL/2,thetaL/2], z in [k thetaL/2, ztop(thetaL/2)].
    tl=iv.mpf(root['lower'])
    ztop=d*iv.cos(tl/2)/iv.sqrt(1+d*d*iv.cos(tl/2)**2)
    inner=tl*(ztop-k*tl/2)
    assert float(inner.a)>0
    return {'classification':'proved_positive','area':outward(area),
            'area_decimal_interval':str(area),'positive_rectangle_lower_sr':math.nextafter(float(inner.a),-math.inf),
            'root_theta_rad':root,'top_y_rad_interval':str(d),
            'side_inactive':True,'method':'analytic integral in (phi,z), interval-bracketed unique root'}


def old_sliver():
    """Original N=8 leaves282/293: lower corner triangles, exact header model.

    Leaf304 is s,t in [.5,.625]; adjacent282/293 are below z=k|phi|.
    Source bottom and east side are great circles. Integrate their analytic
    z(phi) lower envelopes, split at their exact source corner.
    """
    iv.dps=100
    dec=iv.mpf(float(.02772239523293825))*iv.pi/180
    half=iv.mpf(float(.03))*iv.pi/360
    k=8/(3*iv.pi)
    cd,sd=iv.cos(dec),iv.sin(dec)
    tlo=(sd-half*cd)/(cd+half*sd)
    corner=iv.atan2(half,cd+half*sd)
    def zlo(t): return tlo*iv.cos(t)/iv.sqrt(1+tlo*tlo*iv.cos(t)**2)
    def zright(t):
        a=iv.sin(t)-half*cd*iv.cos(t)
        return a/iv.sqrt(a*a+(half*sd)**2)
    cm=mp.mpf(str(float(corner.a)))
    # Use wide brackets containing exact corner, not float-rounded corner.
    lowroot,lr=bracket_root(lambda t:k*t-zlo(t),mp.mpf(0),cm*mp.mpf('1.001'))
    highroot,hr=bracket_root(lambda t:zright(t)-k*t,cm*mp.mpf('.999'),cm*mp.mpf('1.001'))
    assert float((corner-lowroot).a)>0 and float((highroot-corner).a)>0
    def il(t): return iasin(tlo*iv.sin(t)/iv.sqrt(1+tlo*tlo))
    def ir(t): return -iasin((iv.cos(t)+half*cd*iv.sin(t))/iv.sqrt(1+half*half))
    area=k*(highroot**2-lowroot**2)/2-(il(corner)-il(lowroot))-(ir(highroot)-ir(corner))
    assert float(area.a)>0
    return {'leaf_ids':[282,293],'area_each':outward(area),'area_decimal_interval':str(area),
            'phi_lower_root':lr,'phi_upper_root':hr,'phi_source_corner':str(corner),
            'method':'piecewise analytic great-circle boundary integrals; iv root signs; not A/B or external HEALPix membership',
            'classification':'both_proved_positive'}


def compare_one(h,truth,area_tol,max_nodes):
    import chart_overlap as chart
    import detector_independent as detector
    iv.dps=30
    aa=chart.solve_overlap(h,nside=1,absolute_tolerance_sr=area_tol,max_nodes=max_nodes)
    bb=detector.solve_overlap(Header(h),nside=1,relative_tolerance=0,
                              absolute_tolerance_sr=area_tol,max_nodes=max_nodes)
    rec={'area_tolerance_sr':area_tol,'max_nodes':max_nodes,'chart':aa,'detector':bb}
    for name,sol in [('chart',aa),('detector',bb)]:
        got=sol['areas'].get('0',{'lower_sr':0.,'upper_sr':0.})
        t=truth['area']; mid=(got['lower_sr']+got['upper_sr'])/2
        rec[name+'_analytic_comparison']={'leaf0_interval':got,
            'contains_analytic_interval':got['lower_sr']<=t['lower_sr']<=t['upper_sr']<=got['upper_sr'],
            'midpoint_error_upper_sr':max(abs(mid-t['lower_sr']),abs(mid-t['upper_sr']))}
    return rec


def science_decision(area_interval,truth,x,source_area,tol):
    # Here f=1: A=Omega, independent integrated signal x; output F=w*x.
    # For uncertain support, do not manufacture B; F bounds remain meaningful.
    lo,hi=area_interval['lower_sr'],area_interval['upper_sr']
    flo=x*lo/source_area; fhi=x*hi/source_area
    if flo>fhi: flo,fhi=fhi,flo
    true=x*sum(truth.values())/(2*source_area)
    radius=(fhi-flo)/2
    return {'x_integrated_L':x,'source_area_sr':source_area,'flux_interval_L':[flo,fhi],
            'true_flux_L':true,'midpoint_true_error_L':abs((flo+fhi)/2-true),
            'predicted_radius_L':radius,'science_tolerance_L':tol,
            'decision':'bounded_science_tolerance_met' if radius<=tol else 'science_unconverged_refine_or_fail',
            'support':'positive' if lo>0 else ('absent' if hi==0 else 'unresolved'),
            'surface':'do_not_publish_normalized_B_with_Nlower_zero' if lo==0<hi else 'defined_if_supported'}


def run(output,max_nodes):
    sources={n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in
             ('round2_sliver.py','chart_overlap.py','detector_independent.py')}
    result={'command':f'python 实验/science-foundation/oracles/round2_sliver.py --max-nodes {max_nodes}',
            'environment':{'python':platform.python_version(),'platform':platform.platform()},
            'source_sha256':sources,'seed':None,
            'qualification':'DERIVED geometry and conditional interval enclosures; mpmath.iv primitives not formally machine-certified',
            'original_tiny_boundary_sliver':old_sliver(),'cases':[]}
    for name,top,tol in [('strict_nonoverlap',-1e-5,1e-6),('exact_touch',0.,1e-6),
                         ('small_positive',2e-4,1e-6),('tiny_positive',1e-7,1e-6)]:
        h=make_header(top);truth=analytic_new(h)
        print(name,truth['area'],flush=True)
        t0=time.perf_counter(); sol=compare_one(h,truth,tol,max_nodes)
        rec={'name':name,'header':h,'nside':1,'pixfrac':1.,'center_xy0':[0.,0.],
             'truth':truth,'comparison':sol,'wall_seconds':time.perf_counter()-t0}
        # Actual source area exact rectangle integral independently evaluated.
        iv.dps=100
        a=iv.mpf(h['CD1_1'])*iv.pi/360
        yt=iv.mpf(h['CD2_2'])*iv.pi/180*(iv.mpf('1.5')-iv.mpf(h['CRPIX2']))
        yb=yt-2*a
        primitive=lambda y:2*iv.atan2(a*y,iv.sqrt(1+a*a+y*y))
        source_area=float((primitive(yt)-primitive(yb)).a)
        got=sol['chart_analytic_comparison']['leaf0_interval']
        rec['science']=[science_decision(got,truth['area'],x,source_area,.01) for x in (1.,1e12,-1e12)]
        result['cases'].append(rec)
        output.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=OUT)
    p.add_argument('--max-nodes',type=int,default=12001);args=p.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    run(args.output,args.max_nodes)
