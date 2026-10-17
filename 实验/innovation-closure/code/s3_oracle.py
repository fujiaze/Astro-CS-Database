#!/usr/bin/env python3
"""ACSD P5/S3 independent NumPy/SciPy analytical oracle.

No production ACSD C++ calls; all synthetic inputs deterministic; NOT a FITS E2E.
Run: python3 s3_oracle.py --output s3_results.json
"""
import argparse
import hashlib
import json
import sys

import numpy as np
from scipy.linalg import null_space, qr, svd
from scipy.optimize import least_squares

BASE='6f1410e1084c87265e59d2d9a7bc109405c9d714'


def spectra(a, rtol=1e-12):
    sv=np.linalg.svd(a,compute_uv=False)
    return {'s':sv.tolist(),'rank':int(np.sum(sv>rtol*sv[0])) if len(sv) and sv[0]>0 else 0,
            'condition':float(sv[0]/sv[-1]) if len(sv) and sv[-1]>0 else None}


def bilinear(tu,tv):
    if not (0<=tu<=1 and 0<=tv<=1):
        raise ValueError('OUTSIDE_SUPPORT')
    return np.array([(1-tu)*(1-tv), tu*(1-tv), (1-tu)*tv, tu*tv])


def own_exact():
    x=np.array([.25,.75]); y=np.array([.25,.25]); target=np.array([[.5,1.5],[-.5,-1.5]])
    B=np.array([bilinear(xx,yy) for xx,yy in zip(x,y)])
    # Frame-wise f_B=-f_A constraint imposed *before* fitting; only two active nodes
    fA=np.array([0.,2.,0.,2.]);fB=-fA
    pred=np.array([B@fA,B@fB])
    # Restricted feasible x-affine bilinear space; solve using *actual evaluation* basis
    basis=B[:,[0,1]]+B[:,[2,3]] # coefficients linear interpolation along x
    fit=np.linalg.lstsq(basis,target[0],rcond=None)[0]
    pred_fit=np.array([basis@fit,-basis@fit])
    pred_bin=np.array([[1.,1.],[-1.,-1.]])
    objective=lambda pred:float(np.sum((pred-target)**2))
    # variance/weight convention for toy residuals: uniform known variance 1
    out={'input':{'positions_xy':np.column_stack([x,y]).tolist(),
                  'target_2frames':target.tolist(),'noise_covariance':np.eye(4).tolist(),
                  'actual_bilinear_rows':B.tolist(), 'gauge':'f_A+f_B=0 at all nodes'},
         'results':{'floor_bin_filled_objective':objective(pred_bin),'true_bilinear_objective':objective(pred),
                    'QR_ls_objective':objective(pred_fit),'fit_node_pair':fit.tolist(),
                    'first_order_KKT_norm':float(np.linalg.norm(basis.T@(pred_fit[0]-target[0]))),
                    'objective_from_zero':objective(np.zeros_like(target)),
                    'duplicate_observations_same_weight_solution':np.linalg.lstsq(np.repeat(basis,2,axis=0),np.repeat(target[0],2),rcond=None)[0].tolist(),
                    'out_of_support':'OUTSIDE_SUPPORT', 'missing_one_position':'MODEL_UNDERDETERMINED_WITHOUT_SUPPORT_MODEL'}}
    assert abs(out['results']['floor_bin_filled_objective']-1)<1e-12
    assert out['results']['QR_ls_objective']<1e-25
    return out


def d2_operator(nx,ny):
    n=nx*ny; rows=[]
    for y in range(ny):
        for x in range(nx-2):
            row=np.zeros(n)
            for dx,c in [(0,1),(1,-2),(2,1)]:row[y*nx+x+dx]=c
            rows.append(row)
    for y in range(ny-2):
        for x in range(nx):
            row=np.zeros(n)
            for dy,c in [(0,1),(1,-2),(2,1)]:row[(y+dy)*nx+x]=c
            rows.append(row)
    return np.stack(rows)


def deflation_oracle():
    nx,ny=7,6;n=nx*ny
    rng=np.random.default_rng(83176)
    D=d2_operator(nx,ny);P=D.T@D;Q=null_space(D, rcond=1e-12)
    assert Q.shape==(n,4)  # 1,x,y,xy, not merely 1,x,y
    A=rng.normal(size=(n+9,n));H=A.T@A / n + .3*np.eye(n)
    lam=.5;alpha=3.0
    xtruth=rng.normal(size=n); b=H@xtruth
    Alin=H+lam*P; Aa=Alin+alpha*Q@Q.T
    direct=np.linalg.solve(Alin,b)
    anchored=np.linalg.solve(Aa,b)
    G=Q.T@H@Q
    t=np.linalg.solve(G,Q.T@b - Q.T@H@anchored)
    wrong=anchored+Q@t
    # Exact anchor reversal solves in span(Aa^-1 Q), not span(Q): Woodbury.
    K=np.linalg.solve(Aa,Q)
    t2=np.linalg.solve(np.eye(Q.shape[1])-alpha*(Q.T@K),alpha*(Q.T@anchored))
    corrected=anchored+K@t2
    # QR on the *same* penalized data model, using a source matrix whose Gram = H.
    gram_sqrt=np.linalg.cholesky(H).T
    y=np.linalg.solve(gram_sqrt.T,b)
    augmented=np.vstack([gram_sqrt, np.sqrt(lam)*D])
    qr_target=np.concatenate([y,np.zeros(D.shape[0])])
    qrsol=np.linalg.lstsq(augmented,qr_target,rcond=None)[0]
    # Unpenalized problem separate from anchor correction: both correctly fit different objectives.
    unpen=np.linalg.solve(H,b)
    res=lambda sol:float(np.linalg.norm(Alin@sol-b) / np.linalg.norm(b))
    data_res=lambda sol:float(np.linalg.norm(gram_sqrt@sol-y))
    pen=lambda sol:float(np.linalg.norm(D@sol)**2)
    reswrong=res(wrong)
    assert reswrong>1e-5, reswrong
    assert res(corrected)<1e-11 and res(qrsol)<1e-11
    assert np.linalg.norm(direct-unpen)>1e-4
    output={'input':{'grid':[nx,ny],'n':n,'D_shape':list(D.shape),'D_triplets_per_row':'1,-2,1 in x and y',
                     'P':'D.T@D','seed':83176,'H':'X.T@X/n + .3*I, X normal(size=(n+9,n))',
                     'b':'H @ normal_vector_from_same_rng','lambda':lam,'alpha':alpha,
                     'sha256_H':hashlib.sha256(np.ascontiguousarray(H).tobytes()).hexdigest(),
                     'sha256_b':hashlib.sha256(np.ascontiguousarray(b).tobytes()).hexdigest(),
                     'noise_covariance':'identity in gram_sqrt-lifted data equation',
                     'fallback_injection':'FORCED_TEST_ONLY: solve anchored equation even though direct Cholesky succeeds'},
            'results':{'rank_D':int(np.linalg.matrix_rank(D)),'nullity_P':int(Q.shape[1]),
                       'P_Q_norm':float(np.linalg.norm(P@Q)),
                       'H_Q_off_subspace_norm':float(np.linalg.norm((np.eye(n)-Q@Q.T)@H@Q)),
                       'unpenalized_solution_change_norm':float(np.linalg.norm(direct-unpen)),
                       'relative_residual_old_Q_only_correction':reswrong,
                       'relative_residual_anchored':res(anchored),
                       'relative_residual_correct_Woodbury':res(corrected),
                       'relative_residual_direct':res(direct),
                       'relative_residual_augmented_QR':res(qrsol),
                       'prediction_difference_wrong_vs_direct':float(np.linalg.norm(wrong-direct)),
                       'prediction_difference_correct_vs_direct':float(np.linalg.norm(corrected-direct)),
                       'data_residual_direct':data_res(direct),
                       'data_residual_unpenalized':data_res(unpen),
                       'penalty_direct':pen(direct),'penalty_unpenalized':pen(unpen),
                       'H_singular':spectra(H),'penalized_singular':spectra(Alin),
                       'fallback_injected':True,
                       'production_fallback_executed':False,
                       'branch_is_forced_for_formula_test':True}}
    return output


def construct_design(g,x,valid_mask=None,psf=None):
    """Same response toy: each frame y_kj=g_k * S_j + a_k0+a_k1*x_j.
       Orthogonal contrast constrains sum_k a_kq=0; no sky absolute anchor.
    """
    g=np.asarray(g);nframe=len(g);npos=len(x)
    C=null_space(np.ones((1,nframe)))
    rows=[]
    for k in range(nframe):
        for j in range(npos):
            h=np.zeros(npos)
            h[j]=g[k]
            if psf is not None:
                h=g[k]*psf[k][j]
            add=np.r_[C[k,:], C[k,:]*x[j]]
            rows.append(np.r_[h,add])
    M=np.stack(rows)
    if valid_mask is not None:M=M[np.asarray(valid_mask).ravel()]
    return M,C


def joint_world(name,eta_true,additive=True,noise=True):
    x=np.linspace(-1,1,23)
    n=3; contrast=null_space(np.ones((1,n)))
    # True dark-cloud trough and broad diffuse nebular signal: low-frequency modes retained
    S=6+1.1*x+2*np.exp(-((x-.25)/.28)**2)-1.5*np.exp(-((x+.35)/.21)**2)
    eta=np.array(eta_true,float);eta-=eta.mean(); g=np.exp(eta)
    if additive:
        base=np.array([.22,-.15,-.07]); slope=np.array([-.08,.06,.02])
    else:base=np.zeros(n);slope=np.zeros(n)
    a=base[:,None]+slope[:,None]*x[None,:]
    clean=g[:,None]*S[None,:]+a
    rng=np.random.default_rng(20261017+int(sum(100*np.abs(eta)))+int(additive))
    sigma=.025; obs=clean+rng.normal(0,sigma,size=clean.shape) if noise else clean
    # Independent matched-star aperture corrected log measurements; star flux is nuisance
    nstar=36; fs=np.linspace(5,20,nstar)
    star_sigma=.012
    logs=np.log(fs)[None,:]+eta[:,None]+rng.normal(0,star_sigma,(n,nstar))
    # centroid across frames removes each stellar nuisance flux and log-gain gauge
    centered=logs-logs.mean(axis=0,keepdims=True)
    star_eta=centered.mean(axis=1)
    sigma_eta=star_sigma*np.sqrt((n-1)/n/nstar)
    max_z=float(np.max(np.abs(star_eta)/sigma_eta))
    activated=bool(max_z>3.5) # freeze before image-data use, reduces false gains
    def conditional_fit(coeff):
        et=contrast@np.asarray(coeff)
        M,_=construct_design(np.exp(et),x)
        sol=np.linalg.lstsq(M,obs.flatten(),rcond=None)[0]
        return sol,M
    if activated:
        # Variable projection over sky+additive and star independent anchor.
        def stacked(coef):
            sol,M=conditional_fit(coef)
            res=(obs.ravel()-M@sol)/sigma
            eta_est=contrast@coef
            # Star likelihood centered per-star; covariance on contrast space is scalar.
            star_res=((contrast.T@eta_est)-(contrast.T@star_eta))/sigma_eta
            return np.r_[res,star_res]
        opt=least_squares(stacked,contrast.T@star_eta,method='trf',max_nfev=80,
                          ftol=1e-11,xtol=1e-11,gtol=1e-11)
        coef=opt.x;nfev=opt.nfev
    else:
        coef=np.zeros(n-1);nfev=0
    sol,M=conditional_fit(coef)
    s_est=sol[:len(x)]; ad_est=(M@sol).reshape(n,len(x))-np.exp(contrast@coef)[:,None]*s_est[None,:]
    pred=(M@sol).reshape(n,len(x))
    assert np.linalg.norm(pred-obs)<4.0  # conditionally fitted
    loss=float(np.sum(((obs-pred)/sigma)**2))
    signal_err=s_est-S
    return {'input':{'world':name,'seed_rule':'20261017+int(sum(100*abs(eta)))+int(additive)',
                     'n_frames':n,'n_positions':len(x),'n_independent_stars':nstar,
                     'signal_sample_truth':S.tolist(),'true_eta':eta.tolist(),
                     'true_additive_const':base.tolist(),'true_additive_slope':slope.tolist(),
                     'noise_sigma_image':sigma,'star_log_sigma':star_sigma,
                     'response_condition':'same known response, identity PSF, no rejection'},
            'results':{'star_max_abs_z':max_z,'multiplicative_enabled':activated,'eta_est':(contrast@coef).tolist(),
                       'eta_max_abs_error':float(np.max(np.abs(contrast@coef-eta))),
                       'a_rmse':float(np.sqrt(np.mean((ad_est-a)**2))),
                       'sky_rmse':float(np.sqrt(np.mean(signal_err**2))),
                       'lowfreq_error_mean':float(np.mean(signal_err)),
                       'sky_truth_range':float(np.max(S)-np.min(S)),
                       'train_chisquare':loss,'train_n':int(obs.size),
                       'rank_fixed_eta':spectra(M),'optimizer_nfev':int(nfev),
                       'reference_signal_truth_status':'TOY_KNOWN_ONLY'}}


def science_counterexamples():
    # Identical data from distinct true (S,a) under additive gauge shift u(x): nonidentifiability
    x=np.linspace(-1,1,7); s=5+2*x; a=np.array([[.2]*7,[-.1]*7]);g=np.array([1.,1.])
    y=g[:,None]*s[None,:]+a
    u=1.3+.4*x
    yalt=g[:,None]*(s+u)[None,:]+(a-g[:,None]*u[None,:])
    assert np.max(np.abs(yalt-y))<1e-14
    # multiplicative global gauge S -> exp(d)S; eta->eta-d
    eta=np.array([.1,-.1]);scale=np.exp(.2)
    ymult=np.exp(eta)[:,None]*s[None,:]
    ymult_alt=np.exp(eta-.2)[:,None]*(scale*s)[None,:]
    assert np.max(abs(ymult-ymult_alt))<1e-14
    # Shot noise sky subtraction : Var(Pois(S+sky)-sky) = S+sky (known sky).
    shot={'source_expectation_e':13.,'sky_expectation_e':40.,'shot_variance_after_background_mean_subtraction_e2':53.,
          'counterfactual_wrong_variance_if_sky_shot_removed_e2':13.}
    # Cross-response: no common S unless response maps constrained by SED model.
    r1=np.array([1.,0.]);r2=np.array([0.,1.]);scene=np.array([10.,2.]);
    return {'additive_gauge':{'max_prediction_difference':float(np.max(abs(y-yalt))),
                              'u':u.tolist(),'status':'UNIDENTIFIABLE_ABSOLUTE_BACKGROUND'},
            'gain_gauge':{'max_prediction_difference':float(np.max(abs(ymult-ymult_alt))),
                          'status':'UNIDENTIFIABLE_GLOBAL_SCALE_WITHOUT_ANCHOR'},
            'sky_shot_noise':shot,
            'cross_response':{'R1':r1.tolist(),'R2':r2.tolist(),'scene_spectral_coeff':scene.tolist(),
                              'observations':[float(r1@scene),float(r2@scene)],
                              'status':'NO_SINGLE_SCALAR_COMMON_ESTIMAND'}}




def adversarial_controls():
    """Physics/geometry/model-selection negatives; not actual P5 empirical E2."""
    # Known encircled-energy difference is confounded with spatially constant gain.
    ee=np.array([.8,1.,.9]); eta_wrong=np.log(ee)-np.mean(np.log(ee))
    eta_after=np.log(ee/ee)-np.mean(np.log(ee/ee))
    # Missing matched stars at nonzero spatial x: gain slope has no support.
    C=null_space(np.ones((1,3)))
    x0=np.zeros(8); xx=np.tile(np.array([-.8,.8]),4)
    def star_design(xx):
        return np.vstack([np.r_[C[k,:], x*C[k,:]] for k in range(3) for x in xx])
    spatial_missing=star_design(x0)
    spatial_full=star_design(xx)
    # Two disjoint spatial regions with independent frame coverage cannot determine
    # a shared relative sky zero point for separately observed positions without an anchor.
    # Exact rank demonstration with sky at 2 locations and one additive per frame.
    full=np.array([[1,0,1,0],[1,0,0,1],[0,1,1,0],[0,1,0,1]],dtype=float)
    disconnected=full[[0,3]]
    # Noise reality: correlated calibration fitting and shared sky model.
    cov=np.array([[1.4,.4],[.4,1.4]]); avg=np.array([.5,.5]);
    conditional_diag=float(avg@np.diag(np.diag(cov))@avg)
    marginal=float(avg@cov@avg)
    # Negative cov = invalid sample input; physically different R = incomparable.
    return {'psf_aperture_coupling':{'encircled_energy':ee.tolist(),
                   'spurious_log_gain_without_PSF_correction':eta_wrong.tolist(),
                   'max_false_gain':float(np.max(abs(eta_wrong))),
                   'gain_after_correct_encircled_energy':eta_after.tolist(),
                   'state':'APPARENT_GAIN_IS_PSF_APERTURE_MISMATCH'},
            'spatial_gain_support':{'stars_all_at_x_zero_rank':spectra(spatial_missing),
                   'stars_with_plus_minus_support_rank':spectra(spatial_full),
                   'model_unknown_count':4,'state':'SLOPE_UNIDENTIFIABLE_WITHOUT_SPATIAL_STARS'},
            'disconnected_support':{'full_rank':spectra(full),
                   'no_cross_observations_rank':spectra(disconnected),
                   'state':'GAUGE_PER_OVERLAP_COMPONENT_REQUIRED'},
            'shared_covariance':{'Gamma':cov.tolist(),
                   'diagonal_only_reported_variance':conditional_diag,
                   'correct_marginal_variance':marginal,
                   'state':'PARAMETER_COVARIANCE_NOT_INDEPENDENT'},
            'negative_control':{'negative_variance':-1.,'state':'DATA_INVALID_COVARIANCE'},
            'unknown_cross_response':{'state':'PHYSICALLY_INCOMPARABLE_NO_COMMON_SED'}}


def gain_null_monte_carlo():
    # Independent star photometry noise, no gain; 500 independent repetitions.
    from scipy.stats import beta
    nsim=500; nframe=3;nstar=36; sigma=.012
    rng=np.random.default_rng(80376)
    noisy=rng.normal(0,sigma,size=(nsim,nframe,nstar))
    centered=noisy-noisy.mean(axis=1,keepdims=True)
    estimates=centered.mean(axis=2)
    sigma_eta=sigma*np.sqrt((nframe-1)/nframe/nstar)
    zmax=np.max(np.abs(estimates)/sigma_eta,axis=1)
    n_false=int(np.sum(zmax>3.5))
    # exact two-sided Clopper Pearson interval (not Wald); 95% for positive count
    ci_lo=float(beta.ppf(.025,n_false,nsim-n_false+1)) if n_false else 0.0
    ci_hi=float(beta.ppf(.975,n_false+1,nsim-n_false)) if n_false<nsim else 1.
    return {'input':{'n_simulations':nsim,'seed':80376,'n_stars':nstar,
                     'n_frames':nframe,'sigma_star_log':sigma,
                     'threshold_max_z':3.5,
                     'selection_rule':'independent-star-only, multiple-frame max |z| threshold'},
            'result':{'n_false_activation':n_false,'false_activation_fraction':n_false/nsim,
                      'confidence_95_clopper_pearson':[ci_lo,ci_hi],
                      'mean_eta_of_null_trials':estimates.mean(axis=0).tolist(),
                      'std_eta_of_null_trials':estimates.std(axis=0,ddof=1).tolist()},
            'scope':'test of gain activation rule only; does not prove diffuse flux/SNR coverage'}


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',default='s3_results.json');a=p.parse_args()
    worlds={
        'common_only':joint_world('common_only',[0,0,0],False),
        'additive_only':joint_world('additive_only',[0,0,0],True),
        'gain_only':joint_world('gain_only',[-.06,.08,-.02],False),
        'gain_and_additive':joint_world('gain_and_additive',[-.06,.08,-.02],True)}
    out={'schema':'acsd.s3.oracle/v1','base_main_sha':BASE,'python':sys.version.split()[0],
         'numpy':np.__version__,'scipy':'scipy.linalg/scipy.optimize',
         'status':'TOY_PASS_ONLY','seed':83176,
         'cases':{'own_fit_eval_mismatch':own_exact(),'highdim_penalty_deflation':deflation_oracle(),
                  'joint_synthetic_worlds':worlds,'identifiability':science_counterexamples(),
                  'adversarial_controls':adversarial_controls(), 'gain_null_mc':gain_null_monte_carlo()},
         'not_run':['PRODUCTION_CXX','PRODUCTION_DEFLATION_BRANCH','REAL_M42_FITS',
                    'HST_TRUTH_MASK_EXPERIMENT','COMPLETE_PASSBAND_SED','REAL_STAGE2_STAGE3_E2E','USER_VISUAL_REVIEW',
                    'FULL_SCENE_MONTE_CARLO_COVERAGE','REAL_NOISE_COVARIANCE']}
    with open(a.output,'w',encoding='utf-8') as f:json.dump(out,f,ensure_ascii=False,indent=2,allow_nan=False)
    print(json.dumps({'status':out['status'],'own_bin_loss':out['cases']['own_fit_eval_mismatch']['results']['floor_bin_filled_objective'],
                      'deflation_bad_residual':out['cases']['highdim_penalty_deflation']['results']['relative_residual_old_Q_only_correction'],
                      'deflation_fixed_residual':out['cases']['highdim_penalty_deflation']['results']['relative_residual_correct_Woodbury'],
                      'worlds':{k:v['results']['multiplicative_enabled'] for k,v in worlds.items()}},indent=2))

if __name__=='__main__':main()