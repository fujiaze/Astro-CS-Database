#!/usr/bin/env python3
"""第二轮 WCS 合同：独立点/导数实现对外部 WCSLIB；不 import 前轮 Oracle。

python 实验/science-foundation/oracles/round2_wcs_contract.py
数值误差不是天测物理误差；mpmath.iv 的包含性是数值证书的显式前提。
"""
from __future__ import annotations
import copy
import hashlib
import json
import math
from pathlib import Path
import platform
import re
import time
import astropy
import mpmath as mp
from astropy.io.fits import Header
from astropy.wcs import WCS, _wcs
import numpy as np
from mpmath import iv
from numpy.polynomial.legendre import leggauss

iv.dps = 50
BASE = Path(__file__).resolve().parents[1]


def pol(c, p, dx=0, dy=0):
    p = np.asarray(p)
    u, v = p[..., 0], p[..., 1]
    out = np.zeros_like(u, dtype=float)
    for (i, j), a in c.items():
        if i >= dx and j >= dy:
            out += a * math.prod(range(i-dx+1, i+1)) * math.prod(range(j-dy+1, j+1)) * u**(i-dx) * v**(j-dy)
    return out


def vectors(ll):
    a, d = np.deg2rad(np.asarray(ll)).T
    return np.column_stack((np.cos(d)*np.cos(a), np.cos(d)*np.sin(a), np.sin(d)))


def angles(a, b):
    return np.arctan2(np.linalg.norm(np.cross(a, b), axis=-1), np.sum(a*b, axis=-1))


def fixture(order=5, center=(271.113, -44.217), lonpole=107., scale=.00041):
    h = Header()
    h.update(NAXIS=2, NAXIS1=81, NAXIS2=53, RADESYS='ICRS', CUNIT1='deg', CUNIT2='deg',
             CTYPE1='RA---TAN-SIP' if order else 'RA---TAN', CTYPE2='DEC--TAN-SIP' if order else 'DEC--TAN',
             CRPIX1=17.293, CRPIX2=41.617, CRVAL1=center[0], CRVAL2=center[1], LONPOLE=lonpole,
             CD1_1=-scale, CD1_2=.31*scale, CD2_1=.19*scale, CD2_2=.83*scale)
    if order:
        h['A_ORDER'] = h['B_ORDER'] = order
        # 非对称稀疏高阶；不同于第一轮，d=7/9也有非零最高阶。
        for label, terms in [('A', {(2,0):.011,(1,1):-.007,(0,2):.004,(order,0):.003}),
                             ('B', {(2,0):-.006,(1,1):.008,(0,2):-.009,(0,order):-.002})]:
            for (i,j), value in terms.items():
                h[f'{label}_{i}_{j}'] = value * 80.**(1-i-j)
    return h


class Model:
    """独立的首期 reader/规范化/前向/导数；无生产对象依赖。"""
    def __init__(self, header, domain=None):
        self.original = header.copy()
        h = header
        if h.get('NAXIS', 2) != 2 or h.get('WCSAXES', 2) != 2:
            raise ValueError('unsupported_dimension')
        if h.get('RADESYS') != 'ICRS':
            raise ValueError('unsupported_frame')
        if [h.get('CUNIT1'),h.get('CUNIT2')] != ['deg','deg']:
            raise ValueError('unsupported_units')
        ct = [h.get('CTYPE1'),h.get('CTYPE2')]
        if ct not in [['RA---TAN','DEC--TAN'],['RA---TAN-SIP','DEC--TAN-SIP']]:
            raise ValueError('unsupported_projection_or_axis_order')
        if any(re.match(r'^(PV\d+_|PS\d+_|CPDIS|DP\d|D2IM|DET2IM|CROTA)', k) for k in h):
            raise ValueError('unsupported_distortion')
        cdkeys = any(re.match(r'^CD\d_\d$', k) for k in h)
        pckeys = any(re.match(r'^PC\d_\d$', k) for k in h)
        if cdkeys and pckeys:
            raise ValueError('ambiguous_linear_transform')
        if cdkeys:
            cd = np.array([[h.get(f'CD{i}_{j}',0.) for j in (1,2)] for i in (1,2)])
        else:
            if not all(f'CDELT{i}' in h for i in (1,2)):
                raise ValueError('missing_linear_transform')
            cd = np.diag([h['CDELT1'],h['CDELT2']]) @ np.array([[h.get(f'PC{i}_{j}',float(i==j)) for j in (1,2)] for i in (1,2)])
        self.r = np.array([h['CRPIX1'],h['CRPIX2']])
        center = np.array([h['CRVAL1'],h['CRVAL2']])
        if not np.all(np.isfinite(np.r_[cd.ravel(),self.r,center,h.get('LONPOLE',180.)])) or abs(center[1])>90:
            raise ValueError('invalid_numeric_header')
        if np.linalg.cond(cd)>1.e8:
            raise ValueError('ill_conditioned_linear_transform')
        # FITS Paper II: TAN theta0=90; phi_p default is 0 when delta0>=theta0.
        # 因而精确北极默认0，而其余合法Dec默认180；不能一概假定180。
        default_lonpole=0. if center[1]==90. else 180.
        rho = np.deg2rad((h.get('LONPOLE',default_lonpole)-180.)%360.)
        roll = np.array([[np.cos(rho),np.sin(rho)],[-np.sin(rho),np.cos(rho)]])
        self.cd = np.deg2rad(roll @ cd)
        a,d = np.deg2rad(center)
        self.c = np.array([np.cos(d)*np.cos(a),np.cos(d)*np.sin(a),np.sin(d)])
        self.e = np.array([-np.sin(a),np.cos(a),0.])
        self.n = np.array([-np.sin(d)*np.cos(a),-np.sin(d)*np.sin(a),np.cos(d)])
        self.E = np.column_stack((self.e,self.n))
        self.coeff = []
        sip = ct[0].endswith('-SIP')
        for name in ['A','B']:
            order = h.get(name+'_ORDER',0)
            if (sip and (not isinstance(order,int) or not 2<=order<=9)) or (not sip and order):
                raise ValueError('unsupported_sip_order_or_suffix')
            terms = {}
            for key in h:
                mt = re.fullmatch(name+r'_(\d+)_(\d+)',key)
                if mt:
                    i,j = map(int,mt.groups())
                    if not sip or i+j<2 or i+j>order or not math.isfinite(h[key]):
                        raise ValueError('invalid_sip_coefficient')
                    terms[i,j] = float(h[key])
            self.coeff.append(terms)
        self.domain = np.array(domain if domain is not None else [[-.5,-.5],[h['NAXIS1']-.5,h['NAXIS2']-.5]],float)
        radius=[]
        for axis in range(2):
            urange=iv.mpf([float(self.domain[0,axis]),float(self.domain[1,axis])])+1-iv.mpf(float(self.r[axis]))
            radius.append(np.nextafter(max(abs(float(urange.a)),abs(float(urange.b))),np.inf))
        radius=np.array(radius)
        # 每一导数单项绝对上界，向外区间累加；不是采样 Jacobian。
        bound = []
        for coeff in self.coeff:
            row=[]
            for axis in [0,1]:
                acc=iv.mpf(0)
                for (i,j),value in coeff.items():
                    powers=[i,j]
                    k=powers[axis]
                    if k:
                        powers[axis]-=1
                        acc += abs(iv.mpf(value))*k*iv.mpf(radius[0])**powers[0]*iv.mpf(radius[1])**powers[1]
                row.append(float(acc.b))
            bound.append(row)
        self.derivative_bounds=np.nextafter(np.array(bound),np.inf)
        self.L=float(np.nextafter(float(max(sum(iv.mpf(v) for v in row).b for row in self.derivative_bounds)),np.inf))
        if self.L>=1:
            raise ValueError('injectivity_unresolved')
        maxdist=np.array([sum(abs(iv.mpf(v))*iv.mpf(radius[0])**i*iv.mpf(radius[1])**j for (i,j),v in c.items()) for c in self.coeff],object)
        qbound=[]
        for row in self.cd:
            qbound.append(sum(abs(iv.mpf(a))*(iv.mpf(b)+c) for a,b,c in zip(row,radius,maxdist)))
        qnorm=iv.sqrt(sum(v*v for v in qbound))
        self.qnorm_upper=float(np.nextafter(float(qnorm.b),np.inf))
        if self.qnorm_upper>math.sqrt(3):
            raise ValueError('unsupported_field_domain')

    def uv(self,p): return np.asarray(p)+1-self.r
    def plane(self,p):
        u=self.uv(p)
        return (u+np.stack([pol(c,u) for c in self.coeff],axis=-1))@self.cd.T
    def forward(self,p):
        q=self.plane(p)
        v=self.c+q@self.E.T
        return v/np.linalg.norm(v,axis=-1,keepdims=True)
    def derivative(self,p):
        u=self.uv(p)
        jac=np.eye(2)+np.array([[pol(c,u,1,0),pol(c,u,0,1)] for c in self.coeff])
        q=self.plane(p)
        n=self.forward(p)
        dq=self.cd@jac
        return (np.eye(3)-np.outer(n,n))@self.E@dq/math.sqrt(1+q@q)
    def density(self,p):
        u=self.uv(p)
        j=np.eye(2)+np.array([[pol(c,u,1,0),pol(c,u,0,1)] for c in self.coeff])
        q=self.plane(p)
        return abs(np.linalg.det(self.cd)*np.linalg.det(j))/(1+q@q)**1.5
    def footprint(self,rect):
        """四条连续TAN平面多项式，而不是四个球面角点。"""
        lo,hi=np.asarray(rect,float)
        corners=[lo,[hi[0],lo[1]],hi,[lo[0],hi[1]],lo]
        degree=max([1]+[i+j for c in self.coeff for i,j in c])
        edges=[]
        for start,end in zip(corners[:-1],corners[1:]):
            a=self.uv(start);v=np.asarray(end)-start
            poly=np.zeros((2,degree+1));poly[:,0]=a;poly[:,1]=v
            for axis,c in enumerate(self.coeff):
                for (i,j),value in c.items():
                    for k in range(i+1):
                        for l in range(j+1):
                            poly[axis,k+l]+=value*math.comb(i,k)*math.comb(j,l)*a[0]**(i-k)*a[1]**(j-l)*v[0]**k*v[1]**l
            edges.append(self.cd@poly)
        return edges
    def density_interval(self,rect):
        lo,hi=np.asarray(rect,float)
        u=[iv.mpf([float(lo[k]),float(hi[k])])+1-iv.mpf(float(self.r[k])) for k in [0,1]]
        def p(c,axis=None):
            out=iv.mpf(0)
            for (i,j),value in c.items():
                powers=[i,j];factor=1
                if axis is not None:
                    factor=powers[axis]
                    if not factor:continue
                    powers[axis]-=1
                out+=iv.mpf(value)*factor*u[0]**powers[0]*u[1]**powers[1]
            return out
        f=[u[k]+p(self.coeff[k]) for k in [0,1]]
        q=[sum(iv.mpf(self.cd[i,j])*f[j] for j in [0,1]) for i in [0,1]]
        j=[[iv.mpf(int(i==k))+p(c,k) for k in [0,1]] for i,c in enumerate(self.coeff)]
        cd=[[iv.mpf(v) for v in row] for row in self.cd]
        g=abs((cd[0][0]*cd[1][1]-cd[0][1]*cd[1][0])*(j[0][0]*j[1][1]-j[0][1]*j[1][0]))/(1+q[0]**2+q[1]**2)**iv.mpf('1.5')
        return [float(np.nextafter(float(g.a),-np.inf)),float(np.nextafter(float(g.b),np.inf))]
    def inverse(self, direction, tolerance=1.e-10):
        dc=direction@self.c
        if dc<=0:return {'status':'outside_tangent_hemisphere'}
        z=np.linalg.solve(self.cd,np.array([direction@self.e,direction@self.n])/dc)
        u=z.copy()
        for step in range(40):
            f=u+np.array([pol(c,u) for c in self.coeff])-z
            j=np.eye(2)+np.array([[pol(c,u,1,0),pol(c,u,0,1)] for c in self.coeff])
            trial=np.linalg.solve(j,f)
            old=np.linalg.norm(f,np.inf)
            alpha=1.
            while alpha>2**-20:
                un=u-alpha*trial
                fn=un+np.array([pol(c,un) for c in self.coeff])-z
                if np.linalg.norm(fn,np.inf)<=old:break
                alpha*=.5
            u=un
            if np.linalg.norm(fn,np.inf)<tolerance*(1-self.L):break
        # iv 残差包含的是浮点已确定 z 的多项式根；天空→z的舍入另记。
        resid=[]
        for k,c in enumerate(self.coeff):
            f=iv.mpf(float(u[k]))-iv.mpf(float(z[k]))
            for (i,j),v in c.items():f+=iv.mpf(v)*iv.mpf(float(u[0]))**i*iv.mpf(float(u[1]))**j
            resid.append(max(abs(float(f.a)),abs(float(f.b))))
        err=float(np.nextafter(max(resid)/(1-self.L),np.inf))
        radius=2*err+1.e-25
        # 反三角不等式/压缩映射；局部球包含于全域时保证存在且唯一。
        p=u+self.r-1
        interior=np.all(p-radius>=self.domain[0]) and np.all(p+radius<=self.domain[1])
        return {'status':'root_for_rounded_tangent_target' if interior and err<=tolerance else 'inverse_domain_or_budget_unresolved',
                'pixel':p.tolist(),'polynomial_root_error_inf_pixel':err,'iterations':step+1,
                'forward_angle_residual_rad':float(angles(self.forward(p),direction))}


def area(model,rect,n=12):
    nodes,weights=leggauss(n)
    lo,hi=np.array(rect)
    total=0.
    for i,x in enumerate(nodes):
        for j,y in enumerate(nodes):
            p=lo+(np.array([x,y])+1)*(hi-lo)/2
            total+=weights[i]*weights[j]*model.density(p)
    return total*np.prod(hi-lo)/4


def independent_tan_polygon_area(h,rect):
    """70位球面三角 excess；不调用Model/Jacobian/WCSLIB。只适用无SIP。"""
    with mp.workdps(70):
        lo,hi=rect
        cd=[[mp.mpf(float(h[f'CD{i}_{j}']))*mp.pi/180 for j in [1,2]] for i in [1,2]]
        vertices=[]
        for p in [lo,[hi[0],lo[1]],hi,[lo[0],hi[1]]]:
            u=[mp.mpf(float(p[k]))+1-mp.mpf(float(h[f'CRPIX{k+1}'])) for k in [0,1]]
            a=[sum(cd[k][j]*u[j] for j in [0,1]) for k in [0,1]]+[mp.mpf(1)]
            norm=mp.sqrt(sum(v*v for v in a));vertices.append([v/norm for v in a])
        dot=lambda a,b:sum(x*y for x,y in zip(a,b))
        def tri(a,b,c):
            det=a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
            return 2*mp.atan2(abs(det),1+dot(a,b)+dot(a,c)+dot(b,c))
        return str(tri(vertices[0],vertices[1],vertices[2])+tri(vertices[0],vertices[2],vertices[3]))


def transform_header(h,m,b):
    """单项轴置换/缩放；直接重新代入多项式，没有调用前轮 transform。"""
    m,b=np.array(m,float),np.array(b,float)
    inv=np.linalg.inv(m)
    out=h.copy()
    old=Model(h)
    r=1+inv@(old.r-1-b)
    # 保持原始 LONPOLE，避免规范化后再次旋转。
    cd=np.array([[h[f'CD{i}_{j}'] for j in [1,2]] for i in [1,2]])@m
    for i in range(2):
        out[f'CRPIX{i+1}']=r[i]
        for j in range(2):out[f'CD{i+1}_{j+1}']=cd[i,j]
    for name in ['A','B']:
        for key in list(out):
            if re.fullmatch(name+r'_\d+_\d+',key):del out[key]
    axis=np.argmax(abs(m),axis=1)
    assert len(set(axis))==2 and np.count_nonzero(m)==2
    transformed=[{},{}]
    for k,c in enumerate(old.coeff):
        for (i,j),v in c.items():
            powers=[0,0]
            powers[axis[0]]+=i
            powers[axis[1]]+=j
            for row in range(2):
                ij=tuple(powers)
                transformed[row][ij]=transformed[row].get(ij,0)+inv[row,k]*v*m[0,axis[0]]**i*m[1,axis[1]]**j
    for name,c in zip(['A','B'],transformed):
        for (i,j),v in c.items():out[f'{name}_{i}_{j}']=v
    return out


def run():
    start=time.perf_counter()
    rng=np.random.default_rng(11120261021)
    result={'identity':{'seed':11120261021,'python':platform.python_version(),'astropy':astropy.__version__,'wcslib':_wcs.__version__,
                        'numpy':np.__version__,'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
            'new_fixture_policy':'no first-round coordinate fixture or implementation imported',
            'forward_derivative_inverse':[],'affine':{},'negative':{},'rounding_scope':'polynomial interval root bounds condition on rounded TAN target z; external WCSLIB errors and forward sky residual reported separately'}
    cases=[('south_mid_d5',fixture()),('north_exact_d7',fixture(7,(14.713,90.),191.,.00017)),
           ('south_exact_d9',fixture(9,(223.759,-90.),0.,.000037)),
           ('wrap_d2_pc',fixture(2,(359.99987,32.441),270.,.000081)),
           ('wide_no_sip',fixture(0,(98.713,-5.11),11.,.30))]
    north_default=fixture(3,(332.187,90.),37.,.00025);del north_default['LONPOLE']
    south_default=fixture(4,(42.691,-90.),37.,.00025);del south_default['LONPOLE']
    cases.extend([('north_default_lonpole_d3',north_default),('south_default_lonpole_d4',south_default)])
    # PC/CDELT 表示使用非单位对角，验证矩阵乘序。
    hp=cases[3][1]
    pc=np.array([[hp[f'CD{i}_{j}'] for j in [1,2]] for i in [1,2]])
    for key in list(hp):
        if re.fullmatch(r'CD\d_\d',key):del hp[key]
    hp['CDELT1']=-.000081;hp['CDELT2']=.000081
    pc=np.diag([1/hp['CDELT1'],1/hp['CDELT2']])@pc
    for i in range(2):
        for j in range(2):hp[f'PC{i+1}_{j+1}']=pc[i,j]
    for name,h in cases:
        m=Model(h);w=WCS(h)
        points=rng.uniform([-.25,-.25],[80.25,52.25],(300,2))
        # 明确反例坐标、非整数参考点、足迹边缘：仅前向包含真实边界。
        forward_points=np.vstack((points,[[-.5,-.5],[80.5,52.5]],m.r-1))
        independent=m.forward(forward_points)
        oracle=vectors(w.all_pix2world(forward_points,0))
        err=angles(independent,oracle)
        inverse=[m.inverse(v) for v in oracle[:len(points)]]
        pix=np.array([v['pixel'] for v in inverse])
        p_err=np.max(abs(pix-points),axis=1)
        # 外部向量的四阶差分，步长.02避免微小角/极点的球坐标消去。
        derivative_err=[];density_err=[]
        for p in points[:30]:
            axes=[]
            for axis in range(2):
                shift=np.eye(2)[axis]*.02
                val=vectors(w.all_pix2world([p-2*shift,p-shift,p+shift,p+2*shift],0))
                axes.append((val[0]-8*val[1]+8*val[2]-val[3])/(12*.02))
            external=np.column_stack(axes)
            derivative_err.append(np.linalg.norm(m.derivative(p)-external)/np.linalg.norm(m.derivative(p)))
            measured=np.linalg.norm(np.cross(external[:,0],external[:,1]))
            density_err.append(abs(m.density(p)-measured)/m.density(p))
        canonical=h.copy()
        for key in list(canonical):
            if re.fullmatch(r'PC\d_\d',key) or re.fullmatch(r'CDELT\d',key):del canonical[key]
        canonical['LONPOLE']=180.
        for i in range(2):
            for j in range(2):canonical[f'CD{i+1}_{j+1}']=np.rad2deg(m.cd[i,j])
        ce=angles(oracle,vectors(WCS(canonical).all_pix2world(forward_points,0)))
        directfit=w.all_pix2world(forward_points+1,1)
        # 故意丢失原点变换；记录实际绝对世界误差。
        wrong=angles(oracle,vectors(w.all_pix2world(forward_points,1)))
        row={'name':name,'header':dict(h),'points':len(forward_points),'max_forward_angle_rad':float(max(err)),
             'max_inverse_absolute_error_pixel':float(max(p_err)), 'all_inverse_status_root':all(x['status']=='root_for_rounded_tangent_target' for x in inverse),
             'max_inverse_polynomial_root_bound_pixel':max(x['polynomial_root_error_inf_pixel'] for x in inverse),
             'max_inverse_forward_angle_residual_rad':max(x['forward_angle_residual_rad'] for x in inverse),
             'max_derivative_external_relative_difference':float(max(derivative_err)),
             'max_solid_angle_jacobian_external_relative_difference':float(max(density_err)),
             'max_lonpole_canonicalization_angle_rad':float(max(ce)),
             'fits0_1_world_difference_deg':float(np.max(abs(w.all_pix2world(forward_points,0)-directfit))),
             'omit_plus1_min_max_angle_rad':[float(min(wrong)),float(max(wrong))],
             'sip_L_infinity_upper':m.L,'qnorm_upper':m.qnorm_upper,
             'footprint_area_sr_GL12':area(m,[[6.5,12.5],[7.5,13.5]]),
             'footprint_area_GL12_GL20_difference_sr':abs(area(m,[[6.5,12.5],[7.5,13.5]])-area(m,[[6.5,12.5],[7.5,13.5]],20))}
        rect=[[6.5,12.5],[7.5,13.5]]
        density_bounds=m.density_interval(rect)
        row['footprint_area_natural_interval_sr']=density_bounds # 1 pixel squared domain
        row['GL_comparison_is_not_certified_error_bound']=True
        edgeerr=[]
        edgecorners=[rect[0],[rect[1][0],rect[0][1]],rect[1],[rect[0][0],rect[1][1]],rect[0]]
        for coefficients,a,b in zip(m.footprint(rect),edgecorners[:-1],edgecorners[1:]):
            t=np.linspace(0,1,21)
            qedge=np.stack([sum(coefficients[k,l]*t**l for l in range(coefficients.shape[1])) for k in [0,1]],axis=-1)
            v=m.c+qedge@m.E.T;v/=np.linalg.norm(v,axis=-1,keepdims=True)
            p=np.array(a)+(np.array(b)-a)*t[:,None]
            edgeerr.append(float(max(angles(v,vectors(w.all_pix2world(p,0))))))
        row['continuous_four_edge_polynomials_points']=84
        row['max_edge_polynomial_external_angle_rad']=max(edgeerr)
        if not h['CTYPE1'].endswith('-SIP'):
            truth=independent_tan_polygon_area(h,rect)
            row['footprint_independent_70digit_area_sr']=truth
            row['footprint_GL12_absolute_true_error_sr']=abs(row['footprint_area_sr_GL12']-float(truth))
            assert density_bounds[0]<=float(truth)<=density_bounds[1]
        result['forward_derivative_inverse'].append(row)
        if name=='north_default_lonpole_d3':
            wrong_default=h.copy();wrong_default['LONPOLE']=180.
            wrong_angles=angles(oracle,vectors(WCS(wrong_default).all_pix2world(forward_points,0)))
            result['negative']['north_pole_missing_lonpole_incorrect_180_default_max_angle_rad']=float(max(wrong_angles))
        assert max(err)<3e-14 and max(ce)<3e-14
        assert max(p_err)<3e-7 and row['all_inverse_status_root']
        assert max(derivative_err)<3e-6 and max(density_err)<3e-6
    h=fixture();old=Model(h)
    # 不同分箱因子/裁剪/组合：第一轮为2x3，此轮3x4及各Bayer相。
    operations={'crop':(np.eye(2),[11,5]),'sum_bin3x4':(np.diag([3,4]),[1,1.5]),
                'flip_x':(np.diag([-1,1]),[80,0]),'flip_y':(np.diag([1,-1]),[0,52]),
                'transpose':([[0,1],[1,0]],[0,0]),'transpose_flip_bin':([[0,-4],[3,0]],[79,1]),
                **{f'bayer_{a}{b}':(np.diag([2,2]),[a,b]) for a in [0,1] for b in [0,1]}}
    q=rng.uniform([1,1],[9,9],(200,2))
    for name,(m,b) in operations.items():
        m,b=np.array(m,float),np.array(b,float)
        newh=transform_header(h,m,b)
        # 给变换图形显式的小有效域；不错误继承81x53。
        new=Model(newh,[[.5,.5],[9.5,9.5]])
        e=angles(vectors(WCS(h).all_pix2world(q@m.T+b,0)),vectors(WCS(newh).all_pix2world(q,0)))
        de=max(np.linalg.norm(new.derivative(p)-old.derivative(m@p+b)@m) for p in q[:10])
        # Bayer不是2x2收光域，故此矩形面积映射仅验证坐标变换，不声称Bayer收光。
        densityerr=max(abs(new.density(p)-abs(np.linalg.det(m))*old.density(m@p+b))/new.density(p) for p in q[:10])
        result['affine'][name]={'old_from_new_matrix':m.tolist(),'offset_pixel':b.tolist(),
                              'max_angle_rad':float(max(e)),'max_derivative_chain_abs':float(de),'max_density_chain_relative':float(densityerr)}
        assert max(e)<3e-14 and densityerr<1e-12
    bad=h.copy();bad['CRPIX1']-=11;bad['CRPIX2']-=5
    # crop中SIP变量不变，只有CRPIX更新就是正确；反例是改CD后不改SIP的bin。
    bad=transform_header(h,np.diag([3,4]),[1,1.5])
    for key in list(bad):
        if re.fullmatch(r'[AB]_\d+_\d+',key):del bad[key]
    for key in h:
        if re.fullmatch(r'[AB]_\d+_\d+',key):bad[key]=h[key]
    true=vectors(WCS(h).all_pix2world(q@np.diag([3,4])+[1,1.5],0))
    result['negative']['bin_cd_only_sip_not_transformed_max_angle_rad']=float(max(angles(true,vectors(WCS(bad).all_pix2world(q,0)))))
    badcrop=h.copy() # crop without CRPIX update
    result['negative']['crop_header_unchanged_min_angle_rad']=float(min(angles(vectors(WCS(h).all_pix2world(q+[11,5],0)),vectors(WCS(badcrop).all_pix2world(q,0)))))
    for label,changes in [('fk5',{'RADESYS':'FK5'}),('pv',{'PV2_1':.02}),('lookup',{'CPDIS1':'LOOKUP'}),
                          ('zpn',{'CTYPE1':'RA---ZPN','CTYPE2':'DEC--ZPN'}),('order10',{'A_ORDER':10}),
                          ('overwide',{'CD1_1':10.}),('fold',{'A_2_0':-.1}),('nan',{'B_1_1':float('nan')})]:
        bad=dict(h);bad.update(changes)
        try:Model(bad);status='INCORRECT_ACCEPTANCE'
        except ValueError as exc:status=str(exc)
        result['negative'][label]=status
        assert status!='INCORRECT_ACCEPTANCE'
    result['negative']['back_hemisphere']=old.inverse(-old.c)['status']
    # 一个det=1却全局非单射的非SIP反例：指数极坐标条带。
    p1=np.array([0.,0.]);p2=np.array([0.,2*np.pi])
    polar=lambda p:np.array([np.exp(p[0])*np.cos(p[1]),np.exp(p[0])*np.sin(p[1])])
    result['negative']['local_det_nonzero_not_global_injective']={'map':'exp(u)*(cos(v),sin(v)), [u,v] in [-1,1]x[0,2pi]',
          'jacobian_determinant':'exp(2u)>0 everywhere','distinct_preimage': [p1.tolist(),p2.tolist()], 'image_distance':float(np.linalg.norm(polar(p1)-polar(p2)))}
    # 信号/PRF/噪声用与几何分开的线性观测算子验证。
    x=np.arange(12,dtype=float)-3;D=np.diag(np.arange(12)+1.)
    G=np.ones((1,12));M=G/12
    sumvar=float((G@D@G.T)[0,0]);meanvar=float((M@D@M.T)[0,0])
    # 简单线性去马赛克：共用同一原始像元，明确产生非零cov。
    demosaic=np.array([[.5,.5,0],[0,.5,.5]])
    democov=demosaic@np.diag([2.,3.,5.])@demosaic.T
    result['prf_and_noise']={'sum_bin_integrated_signal':float((G@x)[0]),'sum_bin_variance':sumvar,
          'mean_bin_signal':float((M@x)[0]),'mean_bin_variance':meanvar,
          'expected_mean_variance_sum_div_144':sumvar/144,
          'software_12_pixel_read_variance_sigma2_4':48.,'ideal_hardware_single_read_variance_sigma2_4':4.,
          'bayer_native_photosite_area_pixel2':1.,'wrong_bayer_lattice_cell_area_pixel2':4.,
          'linear_demosaic_operator':demosaic.tolist(),'linear_demosaic_covariance':democov.tolist(),
          'interpretation':'channel extraction selects disjoint original 1x1 sites; 2x2 spacing is not collecting footprint; PRF sum and physical read sequence required'}
    # 前轮生产证据不冒充重跑；校验TU字节身份、位置及外审确认范围。
    root=BASE.parents[1]
    prior=json.loads((BASE/'evidence/coordinates_independent.json').read_text(encoding='utf8'))
    hashes=prior.get('production_probe',{}).get('source_sha256',{})
    result['prior_production_probe_integrity']={path:hashlib.sha256((root/path).read_bytes()).hexdigest()==value for path,value in hashes.items()}
    centroid_hashes=prior.get('centroid_probe',{}).get('source_sha256',{})
    result['prior_centroid_probe_integrity']={path:hashlib.sha256((root/path).read_bytes()).hexdigest()==value for path,value in centroid_hashes.items()}
    result['seconds']=time.perf_counter()-start
    return result


if __name__=='__main__':
    output=BASE/'evidence/round2_wcs_contract.json'
    try:
        data=run()
    except Exception as error:
        # 保留本轮实际失败；重跑成功不覆盖失败文件。
        failure=BASE/'evidence'/f'round2_wcs_failed_{time.time_ns()}.json'
        failure.write_text(json.dumps({'exception':repr(error),'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
        raise
    output.write_text(json.dumps(data,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(json.dumps({'output':str(output),'cases':len(data['forward_derivative_inverse']),'seconds':data['seconds']},ensure_ascii=False))
