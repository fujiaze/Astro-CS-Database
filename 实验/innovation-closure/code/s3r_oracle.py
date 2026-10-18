#!/usr/bin/env python3
"""ACSD S3-R independent scientific corrections, NumPy/SciPy reference only.

Run next to the historical, unmodified s3_oracle.py and s3_results.json:
    python3 s3r_oracle.py --output s3r_results.json
No production C++, HST FITS, or actual P5 fallback path is executed.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
import scipy
from scipy.linalg import null_space, qr, solve_triangular, lstsq
from scipy.optimize import least_squares
import s3_oracle as historical

BASE = "476c023d395b224518aa1831f730d64822250289"
PRIOR = "d3a056be23c6dc2f1c5b74b356ac08c7fb398494"
N = 36
K = 3
SIGMA = 0.012
FRAME_SD = SIGMA * np.sqrt((K-1)/(K*N))
CONTRAST_SD = SIGMA / np.sqrt(N)


def pack(a):
    if isinstance(a, np.ndarray): return a.tolist()
    if isinstance(a, (np.integer,)): return int(a)
    if isinstance(a, (np.floating,)): return float(a)
    return a


def fingerprint(a):
    arr = np.ascontiguousarray(np.asarray(a,dtype=np.float64))
    return hashlib.sha256(arr.tobytes()).hexdigest()


def eig_svd_report(A, rtol=1e-12):
    s = np.linalg.svd(A,compute_uv=False)
    rank = int(np.count_nonzero(s>rtol*s[0])) if s.size and s[0]>0 else 0
    return {"shape": list(A.shape), "singular_values":s.tolist(), "rank":rank,
            "relative_rank_threshold":rtol,
            "threshold_absolute":float(rtol*s[0]) if s.size else None,
            "condition":float(s[0]/s[-1]) if len(s) and s[-1]>0 else None}


def explicit_qr(A,b,rtol=1e-12):
    """Actual SciPy CPQR; refuse a falsely unique answer when rank deficient."""
    Q,R,piv=qr(A,mode="economic",pivoting=True,check_finite=True)
    d=np.abs(np.diag(R))
    tol=float(rtol*d[0]) if len(d) else 0
    rank=int(np.count_nonzero(d>tol))
    if rank<A.shape[1]:
        return {"driver":"scipy.linalg.qr(pivoting=True) + scipy.linalg.solve_triangular",
                "status":"RANK_DEFICIENT_NO_UNIQUE_QR", "rank":rank,
                "rank_threshold":tol,"pivot_indices":piv.tolist(),
                "R_diagonal_abs":d.tolist(), "solution":None}
    y=solve_triangular(R[:rank,:rank],Q[:,:rank].T@b,lower=False)
    beta=np.empty_like(y)
    beta[piv]=y
    return {"driver":"scipy.linalg.qr(mode=economic,pivoting=True) + scipy.linalg.solve_triangular",
            "status":"OK", "rank":rank,"rank_threshold":tol,
            "pivot_indices":piv.tolist(),"R_diagonal_abs":d.tolist(),"solution":beta}


def explicit_svd(A,b,rtol=1e-12):
    beta,resid,rank,s=lstsq(A,b,cond=rtol,lapack_driver="gelsd")
    return {"driver":"scipy.linalg.lstsq(lapack_driver=gelsd; SVD rank-revealing)",
            "status":"OK" if rank==A.shape[1] else "RANK_DEFICIENT",
            "rank":int(rank), "singular_values":s.tolist(), "solution":beta}


def star_sd_study(nsim=24000):
    """MC covariance of orthonormal contrasts, including correlated negatives."""
    C=null_space(np.ones((1,K)))
    assert np.allclose(C.T@C,np.eye(K-1)) and np.allclose(C.T@np.ones(K),0)
    M=np.eye(K)-np.ones((K,K))/K
    theoretical_frame=(SIGMA**2/N)*M
    theoretical_contrast=C.T@theoretical_frame@C
    rng=np.random.default_rng(2026101801)
    e=rng.normal(0,SIGMA,(nsim,K,N))
    et=np.mean(e-e.mean(axis=1,keepdims=True),axis=2)
    ct=et@C
    mc_ct=np.cov(ct,rowvar=False,ddof=1)
    mc_frame=np.cov(et,rowvar=False,ddof=1)
    assert np.allclose(np.diag(mc_ct),CONTRAST_SD**2,rtol=.045)
    assert np.allclose(np.diag(mc_frame),FRAME_SD**2,rtol=.045)

    # Per-frame heteroscedastic noises, identical through stars, off-diagonal contrast cov.
    sf=np.array([.008,.012,.018])
    eh=rng.normal(size=(nsim,K,N))*sf[None,:,None]
    hc=eh.mean(axis=2)@C
    h_expect=C.T@np.diag(sf**2/N)@C
    h_mc=np.cov(hc,rowvar=False,ddof=1)
    assert np.allclose(h_mc,h_expect,rtol=.055,atol=5e-8)

    # Error correlated among frames within a given star, shared star scalar cancels.
    rho=.4
    cf=(SIGMA**2)*((1-rho)*np.eye(K)+rho*np.ones((K,K)))
    chol=np.linalg.cholesky(cf)
    es=rng.normal(size=(nsim,K,N))
    ec=np.einsum('ab,tbn->tan',chol,es).mean(axis=2)@C
    corr_expect=C.T@cf@C/N
    corr_mc=np.cov(ec,rowvar=False,ddof=1)
    assert np.allclose(corr_mc,corr_expect,rtol=.055,atol=5e-8)

    # Correlated frame zero-point systematics shared by *all stars* do NOT average as 1/N.
    sig_zero=.005
    zm=rng.normal(size=(nsim,K))*sig_zero
    zz=(rng.normal(0,SIGMA,(nsim,K,N)).mean(axis=2)+zm)@C
    z_expect=(SIGMA**2/N+sig_zero**2)*np.eye(K-1)
    z_mc=np.cov(zz,rowvar=False,ddof=1)
    assert np.allclose(z_mc,z_expect,rtol=.055,atol=5e-7)

    # A scalar per-star common-mode calibration zero point cancels in *contrast*.
    shared_per_star=rng.normal(0,.05,(nsim,1,N))
    zc=(rng.normal(0,SIGMA,(nsim,K,N))+shared_per_star).mean(axis=2)@C
    shared_mc=np.cov(zc,rowvar=False,ddof=1)
    assert np.allclose(shared_mc,np.eye(K-1)*CONTRAST_SD**2,rtol=.055,atol=5e-8)

    return {"seed":2026101801,"n_draws":nsim,"K":K,"N_star":N,"sigma_star_log":SIGMA,
            "contrast_basis_C":C.tolist(),"covariance_centered_frame":theoretical_frame.tolist(),
            "covariance_orthonormal_contrast":theoretical_contrast.tolist(),
            "sd_frame_centered":FRAME_SD,"sd_orthonormal_contrast":CONTRAST_SD,
            "old_precision_over_true":(CONTRAST_SD/FRAME_SD)**2,
            "MC_covariance_frame":mc_frame.tolist(),"MC_covariance_contrast":mc_ct.tolist(),
            "heteroscedastic_frame_SD":sf.tolist(),"heteroscedastic_analytic_contrast_cov":h_expect.tolist(),
            "heteroscedastic_MC_contrast_cov":h_mc.tolist(),
            "same_star_cross_frame_rho":rho,"same_star_analytic_contrast_cov":corr_expect.tolist(),
            "same_star_MC_contrast_cov":corr_mc.tolist(),
            "shared_frame_zero_point_SD":sig_zero,"shared_zero_analytic_contrast_cov":z_expect.tolist(),
            "shared_zero_MC_contrast_cov":z_mc.tolist(),
            "shared_per_star_common_zero_MC_contrast_cov":shared_mc.tolist(),
            "status":"TOY_PASS; scalar sd insufficient for heteroscedastic / correlated scenarios"}


def refit_world(name, etatrue, additive, old_result=None):
    """Same random stream and model as historical S3; change *only* contrast SD."""
    x=np.linspace(-1,1,23)
    contrast=null_space(np.ones((1,K)))
    S=6+1.1*x+2*np.exp(-((x-.25)/.28)**2)-1.5*np.exp(-((x+.35)/.21)**2)
    eta=np.array(etatrue,float);eta-=eta.mean();g=np.exp(eta)
    if additive:base=np.array([.22,-.15,-.07]);slope=np.array([-.08,.06,.02])
    else:base=np.zeros(K);slope=np.zeros(K)
    a=base[:,None]+slope[:,None]*x[None,:]
    clean=g[:,None]*S[None,:]+a
    seed=20261017+int(sum(100*np.abs(eta)))+int(additive)
    rng=np.random.default_rng(seed)
    sigma=.025
    obs=clean+rng.normal(0,sigma,size=clean.shape)
    star_sigma=.012
    fs=np.linspace(5,20,N)
    logs=np.log(fs)[None,:]+eta[:,None]+rng.normal(0,star_sigma,(K,N))
    centered=logs-logs.mean(axis=0,keepdims=True)
    star_eta=centered.mean(axis=1)
    zmax=float(np.max(np.abs(star_eta)/FRAME_SD))
    activated=bool(zmax>3.5)
    y=obs.ravel()
    def conditional_fit(coeff):
        M,_=historical.construct_design(np.exp(contrast@np.asarray(coeff)),x)
        sol=np.linalg.lstsq(M,y,rcond=None)[0]
        return sol,M
    def components(coeff):
        sol,M=conditional_fit(coeff)
        err_image=(y-M@sol)/sigma
        err_star=contrast.T@(contrast@coeff-star_eta)
        return sol,M,err_image,err_star
    def dofit(use_sd):
        if activated:
            opt=least_squares(lambda beta: np.r_[components(beta)[2],
                                                 components(beta)[3]/use_sd],
                contrast.T@star_eta,method='trf',max_nfev=80,
                ftol=1e-11,xtol=1e-11,gtol=1e-11)
            beta=opt.x;nfev=opt.nfev
        else:
            beta=np.zeros(K-1);nfev=0
        sol,M,errimg,errstar=components(beta)
        ehat=contrast@beta
        sest=sol[:len(x)]
        aest=(M@sol).reshape(K,len(x))-np.exp(ehat)[:,None]*sest[None,:]
        return {"eta_est":ehat.tolist(),"eta_max_abs_error":float(np.max(np.abs(ehat-eta))),
                "sky_rmse":float(np.sqrt(np.mean((sest-S)**2))),
                "a_rmse":float(np.sqrt(np.mean((aest-a)**2))),
                "image_chisquare":float(errimg@errimg),
                "star_chisquare_correct":float(errstar@errstar / CONTRAST_SD**2),
                "star_chisquare_used":float(errstar@errstar / use_sd**2),
                "full_loss_correct_weights":float(errimg@errimg+errstar@errstar / CONTRAST_SD**2),
                "optimizer_nfev":nfev,"multiplicative_enabled":activated,
                "prediction_hash":fingerprint(M@sol),
                "contrast_params":beta.tolist(),"contrast_residual":errstar.tolist()}
    old=dofit(FRAME_SD);new=dofit(CONTRAST_SD)
    if old_result:
        for key,prior_key in [("eta_est","eta_est"),("sky_rmse","sky_rmse"),
                              ("a_rmse","a_rmse"),("image_chisquare","train_chisquare")]:
            assert np.allclose(old[key],old_result[prior_key],rtol=1e-7,atol=5e-8), (name,key,old[key],old_result[prior_key])
    changes={k:float(new[k]-old[k]) for k in
             ("eta_max_abs_error","sky_rmse","a_rmse","image_chisquare",
              "full_loss_correct_weights")}
    assert new["full_loss_correct_weights"]<=old["full_loss_correct_weights"]+1e-8
    assert old["multiplicative_enabled"]==new["multiplicative_enabled"]
    return {"seed":seed,"world":name,"star_max_abs_z":zmax,"activation_unchanged":True,
            "model":"identical old 3-frame x23 scene, seeds, observations, noise; only whiten contrast differently",
            "old_likelihood_wrong_contrast_sd":old,"new_likelihood_correct_contrast_sd":new,
            "delta_new_minus_old":changes}


def mask_d2(nx,ny,active):
    """Match sky_plane.cpp add_stencil(): omit row if any of 3 full indices is inactive."""
    active=np.asarray(active,bool).reshape(ny,nx)
    coords=np.argwhere(active)
    free={int(y*nx+x):i for i,(y,x) in enumerate(coords)}
    rows=[]
    for y in range(ny):
        for x in range(nx-2):
            idx=[y*nx+x+j for j in range(3)]
            if all(t in free for t in idx):
                row=np.zeros(len(free))
                for k,c in zip(idx,(1.,-2.,1.)):row[free[k]]=c
                rows.append(row)
    for x in range(nx):
        for y in range(ny-2):
            idx=[(y+j)*nx+x for j in range(3)]
            if all(t in free for t in idx):
                row=np.zeros(len(free))
                for k,c in zip(idx,(1.,-2.,1.)):row[free[k]]=c
                rows.append(row)
    D=np.stack(rows) if rows else np.zeros((0,len(free)))
    poly=np.array([[1.,x,y,x*y] for y,x in coords],float)
    return D,poly,coords


def support_zero_spaces():
    cases={}
    configs={
        "full_7x6":np.ones((6,7),dtype=bool),
        "two_3x3_islands_7x3":np.tile(np.array([1,1,1,0,1,1,1],bool),(3,1)),
        "single_hole_7x6":np.ones((6,7),dtype=bool),
        "missing_full_row_7x6":np.ones((6,7),dtype=bool)}
    configs["single_hole_7x6"][3,3]=False
    configs["missing_full_row_7x6"][3,:]=False
    for key,active in configs.items():
        ny,nx=active.shape
        D,poly,coords=mask_d2(nx,ny,active)
        s=np.linalg.svd(D,compute_uv=False)
        t=1e-11*s[0] if s.size else 0
        rank=int(np.sum(s>t))
        q=null_space(D,rcond=1e-11)
        qpoly=qr(poly,mode="economic",pivoting=False)[0]
        prk=np.linalg.matrix_rank(poly,tol=1e-11)
        dist=float(np.linalg.norm((np.eye(len(coords))-qpoly@qpoly.T)@q))
        theoretical=len(coords)-rank
        assert q.shape[1]==theoretical
        assert np.linalg.norm(D@poly)<1e-10
        if key=="full_7x6": assert (len(coords),rank,theoretical)==(42,38,4)
        if key=="two_3x3_islands_7x3": assert (len(coords),rank,theoretical)==(18,10,8)
        cases[key]={"grid":[nx,ny],"n_free":len(coords),
            "mask_active_rows":active.astype(int).tolist(),"D_shape":list(D.shape),
            "D_hash_sha256":fingerprint(D),"D_rank":rank,"D_singular_values":s.tolist(),
            "rank_tolerance_absolute":float(t),"nullity":theoretical,
            "Q_true_DQ_frobenius":float(np.linalg.norm(D@q)),
            "production_four_polynomial_rank":int(prk),
            "D_times_four_polynomials_norm":float(np.linalg.norm(D@poly)),
            "nullspace_beyond_four_poly_frobenius":dist,
            "status":"TRUE_NULLSPACE_GT_4" if theoretical>4 else "NULLITY_4"}
    return cases


def numeric_solver_recheck():
    nx,ny=7,6;n=nx*ny
    rng=np.random.default_rng(83176)
    D=historical.d2_operator(nx,ny)
    P=D.T@D
    Q=null_space(D,rcond=1e-12)
    X=rng.normal(size=(n+9,n))
    H=X.T@X/n+.3*np.eye(n)
    b=H@rng.normal(size=n)
    lam=.5;alpha=3.0;A=H+lam*P;Aa=A+alpha*Q@Q.T
    xref=np.linalg.solve(A,b)
    Ba=np.linalg.solve(Aa,b)
    tq=np.linalg.solve(Q.T@H@Q, Q.T@b-Q.T@H@Ba)
    bad=Ba+Q@tq
    Aq=np.linalg.solve(Aa,Q)
    tw=np.linalg.solve(np.eye(Q.shape[1])-alpha*Q.T@Aq,alpha*Q.T@Ba)
    wood=Ba+Aq@tw
    sqrtH=np.linalg.cholesky(H).T
    f=np.linalg.solve(sqrtH.T,b)
    augmented=np.vstack([sqrtH,np.sqrt(lam)*D])
    target=np.r_[f,np.zeros(D.shape[0])]
    qrs=explicit_qr(augmented,target)
    svds=explicit_svd(augmented,target)
    np_lstsq=np.linalg.lstsq(augmented,target,rcond=1e-12)[0]
    assert qrs["status"]=="OK" and svds["status"]=="OK"
    def assessment(sol):
        return {"relative_original_A_residual":float(np.linalg.norm(A@sol-b)/np.linalg.norm(b)),
                "augmented_LS_residual":float(np.linalg.norm(augmented@sol-target)),
                "penalty_norm_squared":float(np.linalg.norm(D@sol)**2),
                "relative_parameter_error_vs_direct":float(np.linalg.norm(sol-xref)/np.linalg.norm(xref)),
                "prediction_difference_l2_vs_direct":float(np.linalg.norm(sqrtH@(sol-xref))),
                "full_penalized_objective":float(np.linalg.norm(augmented@sol-target)**2)}
    checks={"direct_solve":assessment(xref),"old_Q_only":assessment(bad),
            "Woodbury":assessment(wood),"actual_SciPy_pivoted_QR":assessment(qrs["solution"]),
            "actual_SciPy_gelsd_SVD":assessment(svds["solution"]),
            "historical_numpy_lstsq":assessment(np_lstsq)}
    assert checks["old_Q_only"]["relative_original_A_residual"]>.01
    assert all(checks[k]["relative_original_A_residual"]<1e-11 for k in
               ("Woodbury","actual_SciPy_pivoted_QR","actual_SciPy_gelsd_SVD"))
    # Restricted x-affine bilinear own model, now explicit actual QR/SVD.
    x=np.array([.25,.75]); b2=np.array([.5,1.5])
    F=np.column_stack([1-x,x])
    q2=explicit_qr(F,b2);s2=explicit_svd(F,b2)
    assert np.linalg.norm(F@q2["solution"]-b2)<1e-12
    return {"grid":[nx,ny],"matrix_hashes":{"H":fingerprint(H),"b":fingerprint(b),
              "D":fingerprint(D),"A":fingerprint(A),"augmented":fingerprint(augmented)},
            "augmented_design":eig_svd_report(augmented),
            "penalized_normal":eig_svd_report(A),
            "QR_driver":qrs["driver"],"QR_rank":qrs["rank"],
            "QR_pivot_indices":qrs["pivot_indices"],
            "SVD_driver":svds["driver"],"SVD_rank":svds["rank"],
            "SVD_singular_values":svds["singular_values"],
            "numpy_driver_label":"numpy.linalg.lstsq (no explicit QR decomposition)",
            "comparison":checks,
            "own_two_parameter_case":{"operator":F.tolist(),
                 "pivoted_QR":{k:v for k,v in q2.items() if k!="solution"},
                 "SVD":{k:v for k,v in s2.items() if k!="solution"},
                 "QR_solution":q2["solution"].tolist(),"SVD_solution":s2["solution"].tolist(),
                 "full_two_frame_sse_QR":float(2*np.linalg.norm(F@q2["solution"]-b2)**2),
                 "historical_floor_bin_sse":1.0},
            "forced_anchor_test_only":True,"production_fallback_executed":False}


def chromatic_psf_case():
    """Exact 2-position, 2-wavelength forward; detector wavelength response is known."""
    L=np.array([[1.,0.],[0.,1.]])  # spectral bins by spatial position
    R=np.array([1.,1.])
    H1=np.eye(2)
    H2=np.array([[.75,.25],[.25,.75]])
    y=H1@(L[0]*R[0])+H2@(L[1]*R[1])
    reduced_sky=np.sum(R[:,None]*L,axis=0)
    averaged_H=(H1+H2)/2
    invalid=averaged_H@reduced_sky
    d=y-invalid
    assert np.allclose(y,[1.25,.75]) and np.allclose(invalid,[1.,1.])
    phi=np.array([.6,.4])
    separable_L=phi[:,None]*np.array([1.,2.])[None,:]
    exact_sep=H1@(phi[0]*np.array([1.,2.]))+H2@(phi[1]*np.array([1.,2.]))
    effective_H=phi[0]*H1+phi[1]*H2
    assert np.allclose(exact_sep,effective_H@np.array([1.,2.]))
    return {"two_wavelength_PSF":{"lambda_bin_by_position_L":L.tolist(),
        "PSF_lambda_1":H1.tolist(),"PSF_lambda_2":H2.tolist(),
        "responses":R.tolist(),"correct_integrate_operators_after_response":y.tolist(),
        "incorrect_integrate_L_then_mean_PSF":invalid.tolist(),
        "pixel_bias_naive_minus_true":(-d).tolist(),
        "maximum_absolute_bias":float(np.max(np.abs(d)))},
        "separable_SED_positive":{"common_SED_phi":phi.tolist(),
         "source_space_amplitudes":[1.,2.],"effective_chromatic_H":effective_H.tolist(),
         "maximum_prediction_error":float(np.max(abs(exact_sep-effective_H@[1,2])))},
        "model":"mu_ki = g_ki int_lambda [H_(k,lambda)(R_k(lambda,omega) L_lambda(omega))]_i d_lambda + a_ki",
        "status":"CHROMATIC_PSF_NONCOMMUTES_UNLESS_VERIFIED_SEPARABLE"}


def main():
    a=argparse.ArgumentParser()
    a.add_argument("--output",default="s3r_results.json")
    ar=a.parse_args()
    old_file=Path(__file__).with_name("s3_results.json")
    old_json=json.loads(old_file.read_text(encoding="utf-8")) if old_file.exists() else None
    scenarios={"common_only":([0.,0.,0.],False),
               "additive_only":([0.,0.,0.],True),
               "gain_only":([-.06,.08,-.02],False),
               "gain_and_additive":([-.06,.08,-.02],True)}
    worlds={}
    for name,(eta,additive) in scenarios.items():
        hist=old_json["cases"]["joint_synthetic_worlds"][name]["results"] if old_json else None
        worlds[name]=refit_world(name,eta,additive,hist)
    # S3's center-coordinate z test is deliberately not reweighted.
    oldmc=historical.gain_null_monte_carlo()
    newmc=historical.gain_null_monte_carlo()
    assert oldmc["result"]==newmc["result"]
    output={"schema":"acsd.s3r.oracle/v1","base_main_sha":BASE,
            "prior_s3_commit":PRIOR,
            "prior_s3_json_sha256":hashlib.sha256(old_file.read_bytes()).hexdigest() if old_file.exists() else None,
            "python":sys.version.split()[0],"numpy":np.__version__,"scipy":scipy.__version__,
            "status":"TOY_PASS_ONLY",
            "corrections":{"contrast_covariance":star_sd_study(),
                           "world_old_vs_corrected":worlds,
                           "500_activation_unchanged":newmc,
                           "masked_D2_zero_spaces":support_zero_spaces(),
                           "genuine_QR_SVD_and_deflation":numeric_solver_recheck(),
                           "chromatic_PSF_noncommutation":chromatic_psf_case()},
            "not_run":["PRODUCTION_CXX","ACTUAL_PRODUCTION_FALLBACK",
                       "REAL_M42_FITS","HST_E1","REAL_STAGE2_STAGE3_E2E",
                       "REAL_NOISE_COVARIANCE","END_TO_END_COVERAGE","USER_VISUAL_REVIEW"]}
    with open(ar.output,"w",encoding="utf-8") as f:
        json.dump(output,f,indent=2,ensure_ascii=False,allow_nan=False,
                  default=pack)
        f.write("\n")
    print(json.dumps({
        "status":output["status"],
        "main":BASE,
        "contrast_sds":[FRAME_SD,CONTRAST_SD],
        "gain_activation_count":newmc["result"]["n_false_activation"],
        "world_delta":{k:v["delta_new_minus_old"] for k,v in worlds.items()},
        "D2_nullity":{k:v["nullity"] for k,v in output["corrections"]["masked_D2_zero_spaces"].items()},
        "solver_QR_residual":output["corrections"]["genuine_QR_SVD_and_deflation"]["comparison"]["actual_SciPy_pivoted_QR"]["relative_original_A_residual"],
        "chromatic_pixel_bias":output["corrections"]["chromatic_PSF_noncommutation"]["two_wavelength_PSF"]["maximum_absolute_bias"]
    },ensure_ascii=False,indent=2))


if __name__=="__main__":
    main()
