"""第三路审核高阶极区交叠反例；只共享构型数据，积分与成员均独立外库。"""
import json
from pathlib import Path
import numpy as np
import astropy.units as units
from astropy.wcs import WCS
from astropy.io.fits import Header
from astropy_healpix import HEALPix
import chart_overlap as target


def main():
    base=Path(__file__).resolve().parents[1]
    case=next(c for c in json.loads((base/'evidence/cross_geometry.json').read_text(encoding='utf8'))['cases'] if c['name']=='high_order_small_drop')
    h=case['header'];par=case['parameters'];N=par['nside'];f=par['pixfrac']
    w=WCS(Header(h));hp=HEALPix(nside=N,order='nested');rng=np.random.default_rng(1117002)
    p=rng.uniform(-f/2,f/2,(2000000,2));u,v=p.T
    # 手动解析该fixture，并直接求导；不调用A/B的Jacobian或积分。
    aa=.12*u*u-.07*u*v+.035*v**3
    bb=-.1*v*v+.04*u*v-.025*u**3
    ax=.24*u-.07*v;ay=-.07*u+.105*v*v
    bx=.04*v-.075*u*u;by=-.2*v+.04*u
    cd=np.deg2rad(np.array([[h['CD1_1'],h['CD1_2']],[h['CD2_1'],h['CD2_2']]]))
    plane=np.column_stack([u+aa,v+bb])@cd.T
    jac=abs(np.linalg.det(cd))*abs((1+ax)*(1+by)-ay*bx)/(1+np.sum(plane*plane,axis=1))**1.5
    sky=w.all_pix2world(p,0);idx=hp.lonlat_to_healpix(sky[:,0]*units.deg,sky[:,1]*units.deg)
    rows={}
    for leaf in np.unique(idx):
        x=jac*(idx==leaf)*f*f
        rows[str(leaf)]={'area_sr':float(np.mean(x)),'standard_error_sr':float(np.std(x,ddof=1)/np.sqrt(len(x))), 'samples_inside':int(np.sum(idx==leaf))}
    # 外库face精确偏移点对自写interval映射，集中于实际四叶，避开只有低阶随机点。
    face_rows=[]
    for leaf in rows:
        leaf=int(leaf);face=leaf//(N*N);local=leaf%(N*N)
        ix=sum(((local>>(2*k))&1)<<k for k in range(18));iy=sum(((local>>(2*k+1))&1)<<k for k in range(18))
        dx,dy=rng.uniform(.001,.999,(2,100))
        lon,lat=hp.healpix_to_lonlat(np.repeat(leaf,len(dx)),dx=dx,dy=dy)
        truth=np.column_stack([np.cos(lat.rad)*np.cos(lon.rad),np.cos(lat.rad)*np.sin(lon.rad),np.sin(lat.rad)])
        vv=[]
        for x,y in zip(dx,dy):
            vec=target.face_vector(face,target.iv.mpf((ix+x)/N),target.iv.mpf((iy+y)/N))
            vv.append([(target.low(z)+target.high(z))/2 for z in vec])
        err=np.linalg.norm(np.cross(vv,truth),axis=1)
        face_rows.append({'leaf':leaf,'face':face,'ix':ix,'iy':iy,'max_angle_rad':float(err.max())})
    out={'seed':1117002,'samples':len(p),'membership':'Astropy all_pix2world(origin=0) + astropy_healpix lonlat_to_healpix', 'jacobian':'manually differentiated explicit six fixture SIP coefficients + exact TAN area formula','areas':rows,'total_area_sr':float(jac.mean()*f*f),'face_mapping':face_rows}
    # 仅观察A声称可完全内含的块，再用外库球面offset+WCS逆独立检验成员。
    records=[];original=target.Source.classify
    def observed(self,face,depth,ix,iy):
        kind=original(self,face,depth,ix,iy)
        if kind is True:records.append((face,depth,ix,iy))
        return kind
    try:
        target.Source.classify=observed
        solved=target.solve_overlap(h,par['center_xy0'],f,N,absolute_tolerance_sr=2.6e-13,max_nodes=50000)
    finally:
        target.Source.classify=original
    checks=0;violations=0;worst_margin=float('inf')
    for depth in sorted(set(r[1] for r in records)):
        subset=[r for r in records if r[1]==depth]
        hi_hp=HEALPix(nside=1<<depth,order='nested')
        ip=[]
        for face,d,ix,iy in subset:
            # 整数索引组装只定义外库像元，不使用A的morton实现。
            local=0
            for k in range(depth):local|=((ix>>k)&1)<<(2*k);local|=((iy>>k)&1)<<(2*k+1)
            ip.extend([face*(1<<(2*depth))+local]*8)
        dx,dy=rng.uniform(.001,.999,(2,len(ip)))
        lon,lat=hi_hp.healpix_to_lonlat(np.asarray(ip,dtype=np.int64),dx=dx,dy=dy)
        xy=w.all_world2pix(np.column_stack([lon.deg,lat.deg]),0,tolerance=1e-11,maxiter=100)
        margin=f/2-np.max(np.abs(xy),axis=1)
        checks+=len(ip);violations+=int(np.sum(margin<-1e-7));worst_margin=min(worst_margin,float(margin.min()))
    out['classified_inside_audit']={'proved_inside_nodes_observed':len(records),'independent_points_checked':checks,'outside_violations_beyond_1e-7_pixel':violations,'minimum_inside_margin_pixel':worst_margin,'solve_status':solved['status'],'audit':'A classify records observed only; point offsets and inverse membership independently from Astropy-HEALPix/Astropy.'}
    path=base/'evidence/high_order_polar_independent.json';path.write_text(json.dumps(out,indent=2)+'\n',encoding='utf8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
