"""曲边参考解的解析/外库对照、全分面和病态构型；输出实际误差。"""
import argparse
import json
import math
import platform
from pathlib import Path

import astropy
import astropy.units as u
import astropy_healpix
from astropy.wcs import WCS
from astropy.io.fits import Header
from astropy_healpix import HEALPix
from scipy.integrate import quad
import mpmath as mp
import numpy as np

from chart_overlap import face_vector, iv, low, high, fixture, solve_overlap, morton


def vector(lon,lat):
    return np.array([np.cos(lat)*np.cos(lon),np.cos(lat)*np.sin(lon),np.sin(lat)]).T


def jacobian(header,x,y):
    uu,vv=x+1-header['CRPIX1'],y+1-header['CRPIX2']
    a,b=np.zeros_like(x),np.zeros_like(x)
    ax,ay,bx,by=[np.zeros_like(x) for _ in range(4)]
    for axis,name in enumerate(('A','B')):
        for i in range(header.get(name+'_ORDER',0)+1):
            for j in range(header.get(name+'_ORDER',0)+1-i):
                c=header.get(f'{name}_{i}_{j}',0)
                if axis==0:
                    a+=c*uu**i*vv**j
                    if i: ax+=c*i*uu**(i-1)*vv**j
                    if j: ay+=c*j*uu**i*vv**(j-1)
                else:
                    b+=c*uu**i*vv**j
                    if i: bx+=c*i*uu**(i-1)*vv**j
                    if j: by+=c*j*uu**i*vv**(j-1)
    cd=np.radians([[header['CD1_1'],header['CD1_2']],[header['CD2_1'],header['CD2_2']]])
    xi=cd[0,0]*(uu+a)+cd[0,1]*(vv+b)
    eta=cd[1,0]*(uu+a)+cd[1,1]*(vv+b)
    return abs(np.linalg.det(cd))*abs((1+ax)*(1+by)-ay*bx)/(1+xi*xi+eta*eta)**1.5


def detector_mc(h,nside,pixfrac,n=200000,seed=20261020):
    """独立外库成员+iid积分。Hoeffding 同时界无需假定未命中=零。

    该统计比较不代替 detector_independent.py 的第二确定性求解器。
    """
    rng=np.random.default_rng(seed)
    p=rng.uniform(-pixfrac/2,pixfrac/2,(n,2))
    sky=WCS(Header(h)).all_pix2world(p,0)
    hp=HEALPix(nside=nside,order='nested')
    idx=hp.lonlat_to_healpix(sky[:,0]*u.deg,sky[:,1]*u.deg)
    g=jacobian(h,p[:,0],p[:,1])*pixfrac**2
    unique,inv=np.unique(idx,return_inverse=True)
    val=np.bincount(inv,weights=g)/n
    # 对本组 fixture 的 detSIP 上界：由独立稠密网格不能证明最大，
    # 所以使用全局粗解析系数界，在 |u|,|v|<=.5 的受控定义下成立。
    ma=[]
    for name in ('A','B'):
        row=[]
        for axis in (0,1):
            d=0.
            for i in range(h.get(name+'_ORDER',0)+1):
                for j in range(h.get(name+'_ORDER',0)+1-i):
                    power=i if axis==0 else j
                    if power:
                        d+=abs(h.get(f'{name}_{i}_{j}',0))*power*.5**(i+j-1)
            row.append(d)
        ma.append(row)
    cd=np.radians([[h['CD1_1'],h['CD1_2']],[h['CD2_1'],h['CD2_2']]])
    bound=abs(np.linalg.det(cd))*((1+ma[0][0])*(1+ma[1][1])+ma[0][1]*ma[1][0])*pixfrac**2
    delta=.001
    err=bound*math.sqrt(math.log(2*(12*nside*nside)/delta)/(2*n))
    return {'areas':{str(i):float(v) for i,v in zip(unique,val)},'simultaneous_hoeffding_error_sr':err,
            'confidence':1-delta,'samples':n,'seed':seed,'mean_total_sr':float(g.mean())}


def coordinate_comparison():
    rng=np.random.default_rng(721)
    results=[]
    for nside in (1,4,256,262144):
        hp=HEALPix(nside=nside,order='nested')
        err=[]
        for f in range(12):
            for _ in range(5):
                ix,iy=rng.integers(0,nside,2)
                dx,dy=rng.uniform(.001,.999,2)
                s,t=(ix+dx)/nside,(iy+dy)/nside
                vv=face_vector(f,iv.mpf(s),iv.mpf(t))
                point=np.array([(low(x)+high(x))/2 for x in vv])
                ip=f*nside*nside+morton(int(ix),int(iy),int(math.log2(nside)))
                lon,lat=hp.healpix_to_lonlat(ip,dx=dx,dy=dy)
                truth=vector(lon.rad,lat.rad)
                err.append(np.linalg.norm(np.cross(point,truth)))
        results.append({'nside':nside,'samples':len(err),'max_vector_angle_rad':float(max(err))})
    return results


def polar_counterexample():
    mp.mp.dps=65
    output=[]
    for nside in (4,16,256,8192,262144):
        # face 0 的东北 apex 叶，直接从 z,phi 解析式计算。
        def point(s,t):
            rr=2-s-t
            z=1-rr*rr/3
            phi=mp.pi/4*(1+(s-t)/rr) if rr else mp.mpf(0)
            rho=mp.sqrt(1-z*z)
            return mp.matrix([rho*mp.cos(phi),rho*mp.sin(phi),z])
        hi=mp.mpf(1);lo=1-mp.mpf(1)/nside
        a,b,c,d=[point(s,t) for s,t in ((lo,lo),(hi,lo),(hi,hi),(lo,hi))]
        def area(a,b,c):
            cross=mp.matrix([b[1]*c[2]-b[2]*c[1],b[2]*c[0]-b[0]*c[2],b[0]*c[1]-b[1]*c[0]])
            return 2*mp.atan2(abs((a.T*cross)[0]),1+(a.T*b)[0]+(a.T*c)[0]+(b.T*c)[0])
        exact=mp.pi/(3*nside*nside)
        chord=area(a,b,c)+area(a,c,d)
        wedge=quad(lambda delta:4/(3*nside*nside*(1+4*abs(delta)/math.pi)**2),-math.pi/8,math.pi/8,epsabs=1e-25)[0]
        output.append({'nside':nside,'exact_sr':float(exact),'geodesic_relative_error':float(chord/exact-1),
                       'central_wedge_fraction':wedge/float(exact),'central_wedge_exact':2/3})
    return output


def run_suite():
    hp=HEALPix(nside=1,order='nested')
    cases=[]
    for face in range(12):
        lon,lat=hp.healpix_to_lonlat(face)
        cases.append((f'face_{face}',fixture(lon.deg,lat.deg,6.),4,1.))
    cases.extend([
        ('north_apex',fixture(45.,89.8,2.),32,1.),
        ('south_apex',fixture(315.,-89.8,2.),32,.7),
        ('transition',fixture(45.,math.degrees(math.asin(2/3)),2.),16,.5),
        ('ra_wrap_crossface',fixture(359.999,0.,2.),16,1.),
        ('fine_order',fixture(83.5,22.,1/3600),262144,.7),
    ])
    output=[]
    for name,h,nside,pixfrac in cases:
        scale=abs(h['CD1_1'])*math.pi/180
        tolerance=scale*scale*pixfrac*pixfrac*.035
        result=solve_overlap(h,pixfrac=pixfrac,nside=nside,absolute_tolerance_sr=tolerance,max_nodes=25000)
        mc=detector_mc(h,nside,pixfrac)
        err=mc['simultaneous_hoeffding_error_sr']
        missing=set(mc['areas'])-set(result['areas'])
        violations=[]
        for ip,area in mc['areas'].items():
            bound=result['areas'].get(ip,{'lower_sr':0,'upper_sr':0})
            if area+err<bound['lower_sr'] or area-err>bound['upper_sr']:
                violations.append(ip)
        item={'name':name,'header':h,'nside':nside,'pixfrac':pixfrac,'absolute_tolerance_sr':tolerance,
              'chart':result,'independent_mc':mc,'mc_members_missing_from_candidates':sorted(missing),
              'mc_simultaneous_bound_disjoint_leaves':violations}
        output.append(item)
        print(name,result['status'],result['processed_nodes'],len(result['areas']),missing,violations,flush=True)
    # 无SIP矩形TAN总面积有独立解析值，图上多个真实叶的和应包围它。
    h=fixture(0.,0.,2.,False);h['CD1_2']=h['CD2_1']=0.
    result=solve_overlap(h,nside=16,absolute_tolerance_sr=2e-5,max_nodes=20000)
    a=math.radians(1.)
    truth=4*math.atan(a*a/math.sqrt(1+2*a*a))
    output.append({'name':'analytic_tan_rectangle','header':h,'nside':16,'pixfrac':1.,'chart':result,
                   'analytic_total_sr':truth,'truth_in_bounds':result['total_area']['lower_sr']<=truth<=result['total_area']['upper_sr']})
    return {'environment':{'python':platform.python_version(),'astropy':astropy.__version__,
                           'astropy_healpix':astropy_healpix.__version__,'mpmath':mp.__version__},
            'face_coordinates':coordinate_comparison(),'polar_counterexample':polar_counterexample(),
            'sip_halfplane_counterexample':{'kappa':.12,'true_area':.49,'four_corner_area':.47},'cases':output}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    result=run_suite()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
