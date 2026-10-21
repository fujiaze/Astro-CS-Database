#!/usr/bin/env python3
"""不读取C++实现，仅消费标准request/result，独立外库成员/面积置信核查。

python 实验/science-foundation/oracles/round2_cpp_external_membership.py
MC只给置信证据，不能提升为严格几何证明；边界点另列以免零测集混淆。
"""
from pathlib import Path
import hashlib
import json
import math
import platform
import time
import astropy
from astropy import units as u
from astropy.io.fits import Header
from astropy.wcs import WCS, _wcs
from astropy_healpix import HEALPix
import astropy_healpix
import mpmath as mp
from mpmath import iv
import numpy as np
from scipy.stats import beta

BASE=Path(__file__).resolve().parents[1]
iv.dps=45
mp.mp.dps=70


def density_bounds(h,center,pixfrac):
    # 独立幂基微分/自然区间。不调用坐标/几何研究模块或C++代码。
    half=iv.mpf(float(pixfrac))/2
    uv=[iv.mpf(float(center))+iv.mpf([-1,1])*half+1-iv.mpf(float(h[f'CRPIX{i}'])) for i in [1,2]]
    coefficients=[]
    for label in ['A','B']:
        c={}
        for i in range(10):
            for j in range(10-i):
                if f'{label}_{i}_{j}' in h:c[i,j]=iv.mpf(float(h[f'{label}_{i}_{j}']))
        coefficients.append(c)
    def p(c,axis=None):
        total=iv.mpf(0)
        for (i,j),value in c.items():
            powers=[i,j];factor=1
            if axis is not None:
                factor=powers[axis]
                if not factor:continue
                powers[axis]-=1
            total+=value*factor*uv[0]**powers[0]*uv[1]**powers[1]
        return total
    cd=[[iv.mpf(float(h[f'CD{i}_{j}']))*iv.pi/180 for j in [1,2]] for i in [1,2]]
    f=[uv[i]+p(coefficients[i]) for i in [0,1]]
    q=[sum(cd[i][j]*f[j] for j in [0,1]) for i in [0,1]]
    jac=[[iv.mpf(int(i==j))+p(coefficients[i],j) for j in [0,1]] for i in [0,1]]
    area_factor=iv.mpf(float(pixfrac))**2
    v=area_factor*abs((cd[0][0]*cd[1][1]-cd[0][1]*cd[1][0])*(jac[0][0]*jac[1][1]-jac[0][1]*jac[1][0]))/(1+q[0]**2+q[1]**2)**iv.mpf('1.5')
    return [float(np.nextafter(float(v.a),-np.inf)),float(np.nextafter(float(v.b),np.inf))]


def tan_exact_total(h,center,f):
    with mp.workdps(70):
        center=mp.mpf(center);half=mp.mpf(float(f))/2
        cd=[[mp.mpf(float(h[f'CD{i}_{j}']))*mp.pi/180 for j in [1,2]] for i in [1,2]]
        points=[]
        for x,y in [(-1,-1),(1,-1),(1,1),(-1,1)]:
            uv=[center+x*half+1-mp.mpf(float(h['CRPIX1'])),center+y*half+1-mp.mpf(float(h['CRPIX2']))]
            v=[sum(cd[k][j]*uv[j] for j in [0,1]) for k in [0,1]]+[mp.mpf(1)]
            length=mp.sqrt(sum(z*z for z in v));points.append([z/length for z in v])
        dot=lambda a,b:sum(x*y for x,y in zip(a,b))
        def excess(a,b,c):
            det=mp.det(mp.matrix([a,b,c]))
            return 2*mp.atan2(abs(det),1+dot(a,b)+dot(b,c)+dot(c,a))
        return excess(*points[:3])+excess(points[0],points[2],points[3])


def main():
    started=time.perf_counter()
    evidence=BASE/'evidence/round2_performance_certified_cells.json'
    raw=evidence.read_bytes();data=json.loads(raw)
    rng=np.random.default_rng(1112712026)
    nsample=65536;alpha=1.e-4
    rows=[]
    for case in data['cases']:
        request=case['request'];measurement=case['measurement']
        name=request['name'];center=-(request['side']-1)/2
        half=request['pixfrac']/2
        header=Header(request['header']);w=WCS(header)
        hp=HEALPix(nside=request['nside'],order='nested')
        first=measurement['first'];intervals={int(k):v for k,v in first['areas'].items()};spill=first['spill_sr']
        corner=np.array([[a,b] for a in [-half,half] for b in [-half,half]])+center
        t=np.linspace(-half,half,33)
        edge=np.vstack((np.column_stack((t,t*0-half)),np.column_stack((t,t*0+half)),
                        np.column_stack((t*0-half,t)),np.column_stack((t*0+half,t))))+center
        # 闭区间grid的边界另列；严格内部点才用于positive-measure成员漏选反例。
        g=np.linspace(-half,half,35)[1:-1]
        inside=np.array([[x,y] for x in g for y in g])+center
        iid=rng.uniform(-half,half,(nsample,2))+center
        def members(points):
            ll=w.all_pix2world(points,0)
            if not np.all(np.isfinite(ll)):raise AssertionError(f'{name}: external WCS undefined')
            return np.asarray(hp.lonlat_to_healpix(ll[:,0]*u.deg,ll[:,1]*u.deg),np.int64)
        corner_ids=members(corner);edge_ids=members(edge);inside_ids=members(inside);iid_ids=members(iid)
        unique,counts=np.unique(iid_ids,return_counts=True)
        countdict=dict(zip(map(int,unique),map(int,counts)))
        known=set(intervals)
        interior_missing=sorted((set(map(int,inside_ids))|set(countdict))-known)
        boundary_missing=sorted((set(map(int,corner_ids))|set(map(int,edge_ids)))-known)
        bounds=density_bounds(request['header'],center,request['pixfrac'])
        # simultaneous union over ALL leaves, not a data-selected set of observed leaves.
        tail=alpha/(len(data['cases'])*12*request['nside']**2*2)
        perleaf=[]
        for leaf in sorted(known|set(countdict)):
            k=countdict.get(leaf,0)
            pl=0. if k==0 else float(beta.ppf(tail,k,nsample-k+1))
            pu=1. if k==nsample else float(beta.isf(tail,k+1,nsample-k))
            ci=[pl*bounds[0],pu*bounds[1]]
            numerical=intervals.get(leaf,[0.,0.])
            numerical=[numerical[0],numerical[1]+spill]
            compatible=max(ci[0],numerical[0])<=min(ci[1],numerical[1])
            perleaf.append({'ipix':leaf,'iid_hits':k,'iid_area_confidence_sr':ci,
                            'reported_interval_plus_global_spill_sr':numerical,'compatible':compatible})
        row={'name':name,'center0':[center,center],'pixfrac':request['pixfrac'],'nside':request['nside'],
             'external_points':{'corners':len(corner),'edges':len(edge),'strict_interior_grid':len(inside),'iid':nsample},
             'hit_unique_leaves':len(set(map(int,np.concatenate([corner_ids,edge_ids,inside_ids,iid_ids])))),
             'interior_missing_leaves_without_spill':interior_missing if spill==0 else [],
             'boundary_missing_leaves_without_spill':boundary_missing if spill==0 else [],
             'missing_members_absorbed_by_global_spill':sorted(set(interior_missing+boundary_missing)) if spill else [],
             'spill_sr':spill,'perleaf':perleaf,'whole_drop_density_mass_bounds_sr':bounds}
        if not header.get('A_ORDER') and not header.get('B_ORDER'):
            truth=tan_exact_total(request['header'],center,request['pixfrac'])
            lower=sum(mp.mpf(float(x[0])) for x in intervals.values())
            upper=sum(mp.mpf(float(x[1])) for x in intervals.values())+mp.mpf(float(spill))
            row['independent_70digit_tan_total_area_sr']=str(truth)
            row['total_interval_contains_exact_tan']=bool(lower<=truth<=upper)
        rows.append(row)
    output={'identity':{'python':platform.python_version(),'numpy':np.__version__,'astropy':astropy.__version__,
                       'wcslib':_wcs.__version__,'astropy_healpix':astropy_healpix.__version__,'seed':1112712026,
                       'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                       'performance_evidence_sha256':hashlib.sha256(raw).hexdigest(),
                       'reviewed_cpp_sha256_from_input_only':data['build']['source_sha256']},
            'independence':'never reads/imports C++ kernel or prior geometry/coordinate oracle implementations; only standard header,request,result',
            'confidence_scope':{'family_failure_probability':alpha,'all_case_all_leaf_union':True,
                                'iid_samples_each':nsample,'not_a_deterministic_bound':True,
                                'density_interval_primitives':'conditional on mpmath.iv enclosure and outward conversion'},
            'cases':rows,'seconds':time.perf_counter()-started}
    output['summary']={'cases':len(rows),'all_members_represented_or_spill':all(not r['interior_missing_leaves_without_spill'] and not r['boundary_missing_leaves_without_spill'] for r in rows),
                       'all_iid_area_intervals_compatible':all(all(p['compatible'] for p in r['perleaf']) for r in rows),
                       'all_tan_total_areas_enclosed':all(r.get('total_interval_contains_exact_tan',True) for r in rows)}
    out=BASE/'evidence/round2_cpp_external_membership_final.json'
    out.write_text(json.dumps(output,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(json.dumps(output['summary'],ensure_ascii=False))
    assert all(output['summary'][k] for k in ['all_members_represented_or_spill','all_iid_area_intervals_compatible','all_tan_total_areas_enclosed'])


if __name__=='__main__':main()
