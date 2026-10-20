"""Independent sparse ACTUAL-SCENE variance interpolation on one sphere chart.

This is not reference-SNR interpolation. Bounds require externally established
log-variance control error bounds and a chart-Hessian norm bound. Without that
prior a smooth bump proves nonidentifiability, even for noiseless controls.
"""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
import scipy
from scipy.spatial import Delaunay

SEED = 11120261021


class GnomonicChart:
    """Chart coordinates are dimensionless TAN-plane offsets, not exact angles.

    M must bound Hessian in these actual coordinates. There is no implicit
    conversion of a sky angular curvature bound into this chart bound.
    """
    def __init__(self, ra_rad, dec_rad, maximum_chart_radius=.1):
        self.center=np.array([np.cos(dec_rad)*np.cos(ra_rad),
                              np.cos(dec_rad)*np.sin(ra_rad),np.sin(dec_rad)])
        self.east=np.array([-np.sin(ra_rad),np.cos(ra_rad),0.])
        self.north=np.cross(self.center,self.east)
        self.maximum_chart_radius=maximum_chart_radius

    def from_chart(self,xy):
        p=np.asarray(xy)
        n=self.center+p[...,0,None]*self.east+p[...,1,None]*self.north
        return n/np.linalg.norm(n,axis=-1,keepdims=True)

    def to_chart(self,directions):
        n=np.asarray(directions)
        dot=n@self.center
        if np.any(dot<=0):
            raise ValueError('OUTSIDE_CHART_HEMISPHERE')
        p=np.stack((n@self.east/dot,n@self.north/dot),axis=-1)
        if np.any(np.linalg.norm(p,axis=-1)>self.maximum_chart_radius):
            raise ValueError('OUTSIDE_DECLARED_LOCAL_CHART')
        return p


class SparseSceneVariance:
    """Delaunay local barycentric interpolation of log(V/V0).

    All controls must refer to one actual-scene estimator, conditional noise
    model and variance unit. Condition identity includes response/PSF, sampling,
    masks and included/omitted errors. No Fisher/reference-source V is accepted.
    delta >=0 bounds absolute log-control errors. M >=0 bounds the spectral
    norm of the log(V/V0) Hessian over each full supported triangle.
    """
    def __init__(self,directions,variances,log_error_bounds,chart,
                 estimator_id,condition_id,variance_unit,variance_unit_scale=1.,
                 curvature_bound=None,condition='actual_scene'):
        if condition!='actual_scene':
            raise ValueError('NOT_ACTUAL_SCENE_VARIANCE')
        if not estimator_id or not condition_id or not variance_unit:
            raise ValueError('ESTIMATOR_CONDITION_OR_UNIT_UNAVAILABLE')
        if variance_unit_scale<=0 or not np.isfinite(variance_unit_scale):
            raise ValueError('INVALID_VARIANCE_UNIT_SCALE')
        v=np.asarray(variances,float);delta=np.asarray(log_error_bounds,float)
        if np.any(~np.isfinite(v)) or np.any(v<=0):
            raise ValueError('UNKNOWN_OR_NONPOSITIVE_SCENE_VARIANCE')
        if np.any(~np.isfinite(delta)) or np.any(delta<0):
            raise ValueError('CONTROL_ERROR_BOUND_UNAVAILABLE')
        if curvature_bound is not None and (not np.isfinite(curvature_bound) or curvature_bound<0):
            raise ValueError('INVALID_CHART_CURVATURE_BOUND')
        self.chart=chart;self.xy=chart.to_chart(directions)
        self.logv=np.log(v/variance_unit_scale);self.delta=delta
        if len(self.xy)!=len(v) or delta.shape!=v.shape:
            raise ValueError('CONTROL_SHAPE_MISMATCH')
        self.tri=Delaunay(self.xy)
        self.M=curvature_bound;self.scale=variance_unit_scale
        self.estimator_id=estimator_id;self.condition_id=condition_id
        self.variance_unit=variance_unit

    def query(self,direction):
        p=self.chart.to_chart(direction)
        # Exact declared support policy: no nearest-neighbour extrapolation.
        k=int(self.tri.find_simplex(p,tol=0.))
        if k<0:
            return {'status':'unavailable','reason':'outside_convex_control_support'}
        transform=self.tri.transform[k]
        first=transform[:2]@(p-transform[2])
        lam=np.r_[first,1-first.sum()]
        # Do not clip negative weights; clipping would change affine exactness.
        if np.any(lam<0):
            return {'status':'unavailable','reason':'numerical_boundary_ambiguity'}
        vertices=self.tri.simplices[k]
        dx=self.xy[vertices]-p
        reproduction=float(np.linalg.norm(lam@self.xy[vertices]-p))
        uhat=float(lam@self.logv[vertices])
        result={'status':'conditional_interpolation','variance':float(self.scale*np.exp(uhat)),
                'log_variance_over_unit_scale':uhat,'vertices':vertices.tolist(),
                'barycentric_weights':lam.tolist(),'affine_reproduction_residual':reproduction,
                'estimator_id':self.estimator_id,'condition_id':self.condition_id,
                'variance_unit':self.variance_unit}
        if self.M is None:
            result.update(interval_status='unavailable',variance_interval=None,
                          interval_reason='chart_curvature_bound_unavailable')
            return result
        control_term=float(lam@self.delta[vertices])
        curvature_term=float(self.M/2*(lam@np.sum(dx*dx,axis=1)))
        bound=control_term+curvature_term
        result.update(interval_status='conditional_deterministic',
            log_error_bound=bound,control_error_term=control_term,
            curvature_error_term=curvature_term,
            variance_interval=(self.scale*np.exp([uhat-bound,uhat+bound])).tolist(),
            numerical_status='FP64_reference_not_interval_arithmetic')
        return result


def run_case(rng,chart,xy,queries,kind):
    offset=.4; gradient=np.array([8.,-3.])
    Q=np.array([[240.,70.],[70.,-140.]]) if kind=='quadratic' else np.zeros((2,2))
    M=float(np.linalg.norm(Q,2))
    def true_u(p):
        return offset+p@gradient+.5*np.einsum('...i,ij,...j->...',p,Q,p)
    error_bound=.002+.001*rng.random(len(xy))
    errors=error_bound*rng.uniform(-1,1,len(xy))
    observed=np.exp(true_u(xy)+errors)
    model=SparseSceneVariance(chart.from_chart(xy),observed,error_bound,chart,
        'surface_leaf_B','fixed_actual_scene_C','L^2/sr^2',curvature_bound=M)
    truth=true_u(queries)
    result=[model.query(n) for n in chart.from_chart(queries)]
    valid=[i for i,r in enumerate(result) if r['status']!='unavailable']
    r=[result[i] for i in valid]
    e=np.array([ri['log_variance_over_unit_scale'] for ri in r])-truth[valid]
    b=np.array([ri['log_error_bound'] for ri in r])
    # Floating reference slack; not a rigorous rounding-error enclosure.
    violations=np.abs(e)>b+1e-13
    assert not np.any(violations)
    assert all(min(ri['barycentric_weights'])>=0 for ri in r)
    noiseless=SparseSceneVariance(chart.from_chart(xy),np.exp(true_u(xy)),np.zeros(len(xy)),
        chart,'surface_leaf_B','fixed_actual_scene_C','L^2/sr^2',curvature_bound=M)
    affine_error=max(abs(noiseless.query(chart.from_chart(queries[i]))['log_variance_over_unit_scale']-truth[i])
                     for i in valid)
    return {'truth':kind,'queries_requested':len(queries),'queries_supported':len(valid),
        'hessian_matrix':Q.tolist(),'chart_hessian_spectral_bound':M,
        'interval_violations':int(np.count_nonzero(violations)),
        'max_absolute_log_error':float(np.max(np.abs(e))),
        'max_log_error_bound':float(np.max(b)),
        'max_error_to_bound_ratio':float(np.max(np.abs(e)/b)),
        'max_noiseless_interpolation_error':float(affine_error),
        'maximum_affine_reproduction_residual':max(ri['affine_reproduction_residual'] for ri in r),
        'maximum_weight_sum_error':max(abs(sum(ri['barycentric_weights'])-1) for ri in r),
        'example':r[0]}


def bump_counterexample(chart):
    # A genuine C-infinity compactly supported bump has all derivative orders
    # zero at its support boundary. No control lies inside the bump support.
    controls=np.array([[-.01,-.01],[-.01,.01],[.01,-.01],[.01,.01]])
    target=np.array([0.,0.]);radius=.005
    def smooth_bump(p,amplitude):
        t=np.linalg.norm(p-target,axis=-1)/radius
        out=np.zeros_like(t)
        inside=t<1
        out[inside]=amplitude*np.exp(1-1/(1-t[inside]**2))
        return out
    model=SparseSceneVariance(chart.from_chart(controls),np.ones(4),np.zeros(4),chart,
        'surface_leaf_B','fixed_actual_scene_C','L^2/sr^2',curvature_bound=None)
    examples=[]
    for amplitude in (1.,10.,100.):
        u_controls=smooth_bump(controls,amplitude)
        u_target=float(smooth_bump(target[None,:],amplitude)[0])
        assert np.max(np.abs(u_controls))==0
        examples.append({'amplitude':amplitude,'control_log_values':u_controls.tolist(),
            'target_log_value':u_target,'target_variance':float(np.exp(u_target))})
    return {'construction':'u_A(x)=A exp(1-1/(1-r^2)) for r<1; zero otherwise; r=||x||/.005',
        'control_variances':[1.,1.,1.,1.],
        'control_log_values_are_zero_not_variances_zero':True,
        'query_without_curvature_bound':model.query(chart.from_chart(target)),
        'outside_support':model.query(chart.from_chart([.02,0.])),
        'examples':examples,
        'proof':'For every finite proposed upper variance bound choose A>log(bound/unit_scale). '
                'All control values remain exactly unchanged, and the true target variance is exp(A). '
                'Smoothness alone places no uniform bound on this class.'}


def main():
    rng=np.random.default_rng(SEED)
    chart=GnomonicChart(np.deg2rad(30.),np.deg2rad(45.),.1)
    axis=np.linspace(-.02,.02,5)
    xy=np.array([(x,y) for x in axis for y in axis])
    # Interior jitter avoids artificial regular-grid triangulation ties.
    inner=np.all(np.abs(xy)<.02,axis=1)
    xy[inner]+=rng.uniform(-.001,.001,size=(sum(inner),2))
    queries=rng.uniform(-.019,.019,size=(5000,2))
    report={'identity':{'seed':SEED,'python':platform.python_version(),
        'numpy':np.__version__,'scipy':scipy.__version__,
        'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'estimator':'actual-scene leaf surface variance, never reference SNR',
        'bound_type':'deterministic conditional on external delta and chart Hessian M',
        'numerical_bound_status':'FP64 with measured residuals; no rigorous interval-rounding certificate'},
        'log_affine':run_case(rng,chart,xy,queries,'affine'),
        'log_quadratic':run_case(rng,chart,xy,queries,'quadratic'),
        'nonidentifiable_without_curvature':bump_counterexample(chart)}
    output=Path(__file__).resolve().parents[1]/'evidence'/'sparse_variance_reference.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(output)
    print(json.dumps({k:{x:report[k][x] for x in ('queries_supported','interval_violations',
          'max_noiseless_interpolation_error','max_error_to_bound_ratio')}
          for k in ('log_affine','log_quadratic')},indent=2))


if __name__=='__main__':
    main()
