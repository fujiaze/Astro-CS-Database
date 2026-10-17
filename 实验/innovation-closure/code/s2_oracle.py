#!/usr/bin/env python3
"""ACSD S2 independent cross-response / rank-aware GLS reference, NumPy+SciPy.
Run: python3 s2_oracle.py --output s2_results.json
No ACSD C++ calls, production matrix dumps, FITS or real observations.
"""
import argparse
import json
import sys
import numpy as np
from scipy.linalg import qr, solve_triangular

BASE = 'e369486e0b23dc91c71bfc0794e43c285602f72e'


def spectrum_rank(A, rtol=1e-12):
    s = np.linalg.svd(A, compute_uv=False)
    rank = int(np.count_nonzero(s > rtol * s[0])) if len(s) and s[0] else 0
    return {'rank': rank, 'singular_values': s.tolist(),
            'condition': float(s[0] / s[-1]) if len(s) and s[-1] else None}


def make_design(n_frames=3, n_positions=7, response=None, disconnected=False):
    """Model y_kj = response_k * sky_j + (C_k @ eta_0 + C_k @ eta_1*x_j) + e.
    C is a sum-zero frame contrast; it is a gauge, not an absolute sky anchor.
    """
    x = np.linspace(-1, 1, n_positions)
    C = np.concatenate([np.eye(n_frames-1), -np.ones((1, n_frames-1))])
    response = np.ones(n_frames) if response is None else np.asarray(response, float)
    records = [(k,j) for k in range(n_frames) for j in range(n_positions)
               if not(disconnected and k == n_frames-1 and j != 0)]
    H = np.zeros((len(records), n_positions))
    A = np.zeros((len(records), 2*(n_frames-1)))
    for i,(k,j) in enumerate(records):
        H[i,j] = response[k]
        A[i,:n_frames-1] = C[k]
        A[i,n_frames-1:] = x[j]*C[k]
    return H, A, x, records


def solve_case(H,A,y,variances, method='svd', rtol=1e-12):
    """Whiten using covariance Cholesky, inspect design rank, solve LS with no ridge.
    Projected QR computes the Schur residual without subtracting two big normals.
    """
    H=np.asarray(H,float); A=np.asarray(A,float); y=np.asarray(y,float)
    v=np.asarray(variances,float)
    if not np.isfinite(v).all() or np.any(v<=0):
        return {'state':'DATA_INVALID_COVARIANCE','reason':'nonpositive or nonfinite variance'}
    if not (np.isfinite(H).all() and np.isfinite(A).all() and np.isfinite(y).all()):
        return {'state':'DATA_INVALID_MATRIX'}
    W = 1/np.sqrt(v)
    F=W[:,None]*H; G=W[:,None]*A; b=W*y
    full=np.column_stack([F,G]); sf=spectrum_rank(full,rtol)
    sh=spectrum_rank(F,rtol)
    if sf['rank']<full.shape[1]:
        return {'state':'UNIDENTIFIABLE','design':sf,'sky_design':sh,
                'reason':'rank after gauge still deficient'}
    if method=='svd':
        theta=np.linalg.lstsq(full,b,rcond=rtol)[0]
    elif method=='qr_pivot':
        Q,R,piv=qr(full,pivoting=True,mode='economic')
        xp=solve_triangular(R,Q.T@b)
        theta=np.empty_like(xp);theta[piv]=xp
    elif method=='qr_projected':
        Q,_=np.linalg.qr(F,mode='reduced')
        Gp=G-Q@(Q.T@G)
        bp=b-Q@(Q.T@b)
        eta=np.linalg.lstsq(Gp,bp,rcond=rtol)[0]
        sky=np.linalg.lstsq(F,b-G@eta,rcond=rtol)[0]
        theta=np.r_[sky,eta]
    else:
        raise ValueError('unknown solver')
    residual=full@theta-b
    return {'state':'OK_MODEL_CONDITIONAL', 'method':method,'design':sf,
            'sky_design':sh,'theta':theta.tolist(),
            'weighted_objective':float(residual@residual),
            'prediction':(np.column_stack([H,A])@theta).tolist(),
            'whitened_residual_linf':float(np.max(np.abs(residual)))}


def physical_spectra():
    """Three discrete spectral bins: continuum, H-alpha, [N II], arbitrary units."""
    ref=np.array([1.,1.,1.]); neb=np.array([.1,10.,5.])
    r0=np.array([1.,1.,1.]); halpha=np.array([.1,1.,.05]); red=np.array([1.,.4,.7])
    responses=np.stack([halpha,red]); coeff=(r0@ref)/(responses@ref)
    calibrated=coeff[:,None]*responses
    effective=.8*calibrated[0]+.2*calibrated[1]
    star=(calibrated@ref).tolist(); nebula=(calibrated@neb).tolist()
    return {'bins':['continuum','Halpha','NII'],'reference_sed':ref.tolist(),
            'nebula_sed':neb.tolist(),'target_response':r0.tolist(),
            'responses':responses.tolist(),'coefficients':coeff.tolist(),
            'calibrated_response':calibrated.tolist(),
            'stellar_reference_flux':float(r0@ref),'calibrated_star_fluxes':star,
            'nebula_target_flux':float(r0@neb),'calibrated_nebula_fluxes':nebula,
            'effective_response_80_20':effective.tolist(),
            'nebula_mosaic_flux':float(effective@neb),
            'nebula_bias':float((effective-r0)@neb),
            'common_scalar_for_all_SEDs':False}


def run():
    H,A,x,rec=make_design()
    n=len(rec); t=np.r_[np.linspace(2.,3.,len(x)), [.2,-.15,.1,.05]]
    D=np.column_stack([H,A]); y=D@t
    v=np.ones(n)
    plus={m:solve_case(H,A,y,v,m) for m in ('svd','qr_pivot','qr_projected')}
    assert all(r['state']=='OK_MODEL_CONDITIONAL' for r in plus.values())
    assert max(np.max(abs(np.array(r['theta'])-t)) for r in plus.values())<1e-11
    # Legitimate full-row units re-expression: y'=U y, D'=U D, Sigma'=U Sigma U^T.
    units=np.array([1e-16,1.,1e16])
    u=np.array([units[k] for k,j in rec])
    scaled={m:solve_case(u[:,None]*H,u[:,None]*A,u*y,v*u*u,m)
            for m in ('svd','qr_pivot','qr_projected')}
    assert all(r['state']=='OK_MODEL_CONDITIONAL' for r in scaled.values())
    assert max(np.max(abs(np.array(scaled[m]['prediction'])/u -
                          np.array(plus[m]['prediction']))) for m in plus)<1e-10
    # Valid column reparameterization: theta=P z, D'=D P, same y and weights.
    p=np.array([1e-8]*H.shape[1]+[1e8]*A.shape[1]); Dp=D*p
    from_bad=solve_case(Dp[:,:H.shape[1]],Dp[:,H.shape[1]:],y,v,'svd')
    colnorm=np.linalg.norm(Dp,axis=0)
    from_precond=solve_case((Dp/colnorm)[:,:H.shape[1]],
                            (Dp/colnorm)[:,H.shape[1]:],y,v,'svd')
    assert from_bad['state']=='UNIDENTIFIABLE'
    assert from_precond['state']=='OK_MODEL_CONDITIONAL'
    assert np.max(abs(np.array(from_precond['prediction'])-y))<1e-10
    # No calibration by adding ridge; rank deficiency must be reported.
    Hd,Ad,_,_=make_design(disconnected=True)
    rank_negative=solve_case(Hd,Ad,np.ones(Hd.shape[0]),np.ones(Hd.shape[0]))
    assert rank_negative['state']=='UNIDENTIFIABLE'
    badv=solve_case(H,A,y,np.r_[-1,np.ones(n-1)])
    assert badv['state']=='DATA_INVALID_COVARIANCE'
    # Anchor/gauge transform via invertible contrast parameterization: A' = A diag(...)
    q=np.array([2.,3.,.5,4.]); revised=solve_case(H,A*q,y,v,'svd')
    assert revised['state']=='OK_MODEL_CONDITIONAL'
    assert np.max(abs(np.array(revised['prediction'])-y))<1e-12
    # Frame-order and nominal-label invariance, when actual numbers/response fixed.
    order=np.array([i for k in (2,0,1) for i,(fk,j) in enumerate(rec) if fk==k])
    perm=solve_case(H[order],A[order],y[order],v[order],'svd')
    assert np.max(abs(np.array(perm['theta'])-t))<1e-11
    # Deliberately non-equivalent "overlay": change y & variance but NOT D.
    overlay_u=np.array([5.70245780684985e-17 if k in (0,1) else 1 for k,j in rec])
    overlay=solve_case(H,A,overlay_u*y,v*overlay_u**2,'svd')
    # In this toy model it can become ill-conditioned, and is NOT a legitimate unit conversion.
    # Schur PSD theorem and subtraction cancellation, in units of raw weighted design.
    F,G=H,A; N11=F.T@F; N12=F.T@G; N22=G.T@G
    sub=N22-N12.T@np.linalg.solve(N11,N12)
    Q,_=np.linalg.qr(F,mode='reduced'); residualG=G-Q@(Q.T@G)
    stable=residualG.T@residualG
    assert np.min(np.linalg.eigvalsh(stable))>0
    # Positive semidefinite normal with exact gauge-free rank deficiency (singular negative).
    singular=np.r_[np.linalg.svd(np.column_stack([Hd,Ad]),compute_uv=False)]
    # Linear-algebraically negative direction impossible if W>=0 and no exact Hessian term.
    response=physical_spectra()
    assert np.allclose(response['calibrated_star_fluxes'],3.)
    assert abs(response['nebula_bias'])>.1
    return {'schema':'acsd.s2.oracle/v1','base_main':BASE,
       'environment':{'python':sys.version.split()[0], 'numpy':np.__version__},
       'status':'PASS_TOY_ORACLE','not_run':['production_C++','frozen_E0_real_matrix_replay',
          'real_FITS','HST_scene','real_Gaia_XP_response'],
       'response':response,
       'numerics':{
        'input':{'n_frames':3,'n_positions':len(x),'x_positions':x.tolist(),
                 'n_observations':n,'design_shape':list(D.shape),'model':'y=H S+A eta',
                 'true_theta':t.tolist(), 'variance_native':v.tolist()},
        'same_response_positive':{k:{'state':r['state'],'objective':r['weighted_objective'],
                                  'condition':r['design']['condition'],'spectrum':r['design']['singular_values'],
                                  'max_theta_error':float(np.max(abs(np.array(r['theta'])-t)))}
                                  for k,r in plus.items()},
        'equivalent_row_units':{'multipliers_per_frame':units.tolist(),
                'singular_spectrum':scaled['svd']['design']['singular_values'],
                'prediction_max_abs_difference_original_units':float(np.max(abs(
                   np.array(scaled['svd']['prediction'])/u-y)))},
        'column_transform':{'param_factors':p.tolist(),
          'unscaled_state':from_bad['state'],'unscaled_cond':from_bad['design']['condition'],
          'column_norms':colnorm.tolist(),'scaled_state':from_precond['state'],
          'scaled_singular_spectrum':from_precond['design']['singular_values'],
          'prediction_max_abs_error':float(np.max(abs(np.array(from_precond['prediction'])-y)))},
        'negative_rank':{'state':rank_negative['state'], 'spectrum':singular.tolist(),
            'rank':rank_negative['design']['rank'], 'columns':int(Hd.shape[1]+Ad.shape[1])},
        'negative_variance':{'state':badv['state']},
        'gauge_transform':{'state':revised['state'],
                'max_prediction_error':float(np.max(abs(np.array(revised['prediction'])-y)))},
        'frame_permutation':{'state':perm['state'],
                'max_param_error':float(np.max(abs(np.array(perm['theta'])-t)))},
        'overlay_not_equivalent':{'state':overlay['state'],
             'note':'row values and variances changed without design H/A; not a unit basis change'},
        'schur':{'min_eig_direct_subtraction':float(np.linalg.eigvalsh(sub)[0]),
            'min_eig_projected_gram':float(np.linalg.eigvalsh(stable)[0]),
            'max_matrix_difference':float(np.max(abs(sub-stable)))},
        'E0_external_evidence_only':{'observations':17237,'overlap_controls':5807,
            'weight_median_span':1.4045816335826419e32,
            'lambda_min_original':-0.0022655921742014084,
            'lambda_max_original':13118516092622.555,
            'normalized_rank':31,'normalized_columns':31,
            'method':'read from repo issue80 E0, NOT rerun here'}
       }}


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',default='s2_results.json')
    args=ap.parse_args()
    data=run()
    with open(args.output,'w',encoding='utf-8') as f:
        json.dump(data,f,ensure_ascii=False,indent=2,allow_nan=False)
        f.write('\n')
    print(json.dumps({'status':data['status'],
          'cases':list(data['numerics']),
          'nebula_bias':data['response']['nebula_bias']},ensure_ascii=False))
