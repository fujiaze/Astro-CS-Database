"""Independent finite-dimensional scientific oracle for ACSD issue 111.

No production algorithm is imported. Run from any directory with Python >=3.10,
NumPy and SciPy. All numbers are generated, never copied from production results.
Units: integrated native input L; surface output L/sr; spectral f W/m^2/nm.
This is an inspectable small-matrix baseline, not a production-scale implementation.
"""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
import scipy
from scipy.integrate import quad
from scipy.linalg import null_space
from scipy.optimize import least_squares
from scipy.special import ndtr
from scipy.stats import beta, norm

SEED = 11120261020
TRIALS = 100000
HPLANCK = 6.62607015e-34
CLIGHT = 299792458.0


def binomial_interval(k, n, level=.95):
    """Exact two-sided Clopper-Pearson interval; MC trials must be independent."""
    alpha = 1-level
    return [float(beta.ppf(alpha/2, k, n-k+1)) if k else 0.0,
            float(beta.ppf(1-alpha/2, k+1, n-k)) if k < n else 1.0]


def fraction_report(events):
    n = np.size(events)
    k = int(np.count_nonzero(events))
    return {"count": k, "denominator": n, "fraction": k/n,
            "binomial_95_interval": binomial_interval(k, n)}


def coverage_report(error, stderr):
    return {str(level): fraction_report(np.abs(error) <= norm.ppf((1+level)/2)*stderr)
            for level in (.68, .95)}


def drizzle_operator(overlap_jp, drop_area, source_area):
    """Return R[p,j] [sr^-1] and N[p] [sr]; no covered leaf may have N=0.

    a[j,p] and A[j], Omega[j] are in sr. Geometry is supplied independently.
    The oracle concerns statistical propagation, not a geometry implementation.
    """
    a = np.asarray(overlap_jp, float)
    A, omega = np.asarray(drop_area, float), np.asarray(source_area, float)
    if np.any(a < 0) or np.any(A <= 0) or np.any(omega <= 0):
        raise ValueError("INVALID_GEOMETRY")
    fraction = a/A[:, None]
    N = fraction.T @ omega
    if np.any(N <= 0):
        raise ValueError("NO_SUPPORT")
    return fraction.T/N[:, None], N


def linear_gls_operator(design, covariance, requested=0, rtol=1e-12):
    """Estimate theta from y=A theta+n, including exact zero-noise constraints.

    Returns K such that theta_hat=K y, K C K^T, and diagnostics. Redundant
    nuisance columns are permitted; requested parameter MUST be estimable.
    Eigen/SVD truncation is explicit and reported. Tiny positive eigenvalues
    below rtol*lambda_max are model truncations, not a precision certificate.
    """
    A, C = np.asarray(design, float), np.asarray(covariance, float)
    if not np.all(np.isfinite(C)):
        raise ValueError('NOISE_UNAVAILABLE')
    if not np.allclose(C,C.T,rtol=rtol,atol=0):
        raise ValueError('NONSYMMETRIC_COVARIANCE')
    e, U = np.linalg.eigh(C)
    tol = rtol * max(float(np.max(np.abs(e))), np.finfo(float).tiny)
    if np.min(e) < -tol:
        raise ValueError("NOT_A_COVARIANCE")
    positive = e > tol
    W = (U[:, positive]/np.sqrt(e[positive])).T
    E = U[:, ~positive].T
    B = E @ A
    K0 = np.linalg.pinv(B, rcond=rtol) @ E
    N = null_space(B, rcond=rtol)
    D = W @ A @ N
    K = K0 + N @ np.linalg.pinv(D, rcond=rtol) @ W @ (np.eye(len(C))-A@K0)
    unidentifiable = null_space(np.vstack((B, W@A)), rcond=rtol)
    if unidentifiable.size and np.linalg.norm(unidentifiable[requested]) > 1e-9:
        raise ValueError("REQUESTED_PARAMETER_NOT_IDENTIFIABLE")
    return K, K@C@K.T, {"noise_rank": int(sum(positive)),
        "noise_eigenvalues": e.tolist(), "truncation_relative_tolerance": rtol,
        "unidentifiable_parameter_dimension": unidentifiable.shape[1]}


def apply_linear_gls(design, covariance, data, requested=0):
    """Apply the operator and verify zero-noise equations are consistent."""
    K,V,diagnostics=linear_gls_operator(design,covariance,requested)
    C=np.asarray(covariance);A=np.asarray(design);y=np.asarray(data)
    e,U=np.linalg.eigh(C)
    tol=1e-12*max(float(np.max(np.abs(e))),np.finfo(float).tiny)
    E=U[:,e<=tol].T
    theta=K@y
    residual=E@(y-A@theta)
    if np.linalg.norm(residual)>1e-10*max(np.linalg.norm(y),1.):
        raise ValueError('INCONSISTENT_EXACT_CONSTRAINTS')
    return theta,V,diagnostics


def sky_profile_operator(H, A, C, gauge, rtol=1e-12):
    """Additive-only GLS after protecting the common H S sky.

    gauge @ beta=0 fixes coordinates. Data covariance must be SPD here.
    Returns K: beta_hat=K y. Applying to signal uses P=I-E K T;
    T and E must be the actual control sampling and evaluation operators.
    """
    chol = np.linalg.cholesky(C)
    W = np.linalg.solve(chol, np.eye(len(C)))
    Hw = W@H
    UH, sh, _ = np.linalg.svd(Hw, full_matrices=False)
    rank_h = int(sum(sh > rtol*sh[0])) if len(sh) and sh[0] else 0
    Q = np.eye(len(C))-UH[:, :rank_h]@UH[:, :rank_h].T
    N = null_space(gauge, rcond=rtol)
    D = Q@W@A@N
    _, singular, _ = np.linalg.svd(D, full_matrices=False)
    rank = int(sum(singular > rtol*singular[0])) if len(singular) and singular[0] else 0
    if rank != N.shape[1]:
        raise ValueError("RELATIVE_SKY_NOT_IDENTIFIABLE")
    K = N@np.linalg.pinv(D, rcond=rtol)@Q@W
    return K, {"common_sky_rank": rank_h, "relative_rank": rank,
               "singular_values": singular.tolist(), "protected_residual": float(np.linalg.norm(K@H))}


def covariance_factor_action(L, D, U, latent_cov, v):
    """(L D L^T + U Lambda U^T) v, never forms a pixel covariance.

    D can itself be an SPD sparse/operator covariance. L includes calibration,
    normalized Drizzle and same-data sky correction. U holds shared Gaia/flat
    latent identities; concatenate frame factors before combining frames.
    """
    return L @ (D @ (L.T @ v)) + U @ (latent_cov @ (U.T @ v))


def woodbury_solve(diagonal, U, latent_cov, b):
    """Exact SPD diagonal-plus-low-rank inverse action; no dense N x N array."""
    d = np.asarray(diagonal)
    if np.any(~np.isfinite(d)) or np.any(d <= 0):
        raise ValueError("UNKNOWN_OR_NONPOSITIVE_VARIANCE")
    V = U@np.linalg.cholesky(latent_cov)
    DiV = V/d[:, None]
    Db = b/d if np.ndim(b) == 1 else b/d[:, None]
    return Db-DiV@np.linalg.solve(np.eye(V.shape[1])+V.T@DiV, V.T@Db)


def gaia_photon_integral(basis, coefficients, spectral_cov, band_lo_nm, band_hi_nm,
                         qe=lambda x: 1.0, breakpoints=()):
    """q=integral f_lambda[nm^-1] T QE lambda_nm*1e-9/(hc) d lambda_nm.

    basis(lambda_nm) is the actual spectral reconstruction basis, not assumed
    independent XP samples. Spectral coefficient covariance may be unknown.
    Exact passband jumps must be provided as interval boundaries; additional
    interpolation discontinuities belong in breakpoints. Return photons/s/m^2.
    """
    points = sorted(set([band_lo_nm, band_hi_nm]+[
        x for x in breakpoints if band_lo_nm < x < band_hi_nm]))
    weights = np.zeros(len(coefficients))
    errors = np.zeros(len(coefficients))
    for j in range(len(coefficients)):
        for a, b in zip(points[:-1], points[1:]):
            scale = max(abs(basis(l)[j]*qe(l)*l*1e-9/(HPLANCK*CLIGHT))
                        for l in (a, (a+b)/2, b))*(b-a)
            v, err = quad(lambda l: basis(l)[j]*qe(l)*l*1e-9/(HPLANCK*CLIGHT),
                          a, b, epsabs=1e-12*scale, epsrel=3e-13)
            weights[j] += v
            errors[j] += err
    q = float(weights@coefficients)
    var = None if spectral_cov is None else float(weights@spectral_cov@weights)
    return {"q_photons_s_m2": q, "variance": var, "weights": weights.tolist(),
            "quadrature_error_estimate": float(errors@np.abs(coefficients)),
            "spectral_covariance_status": "unknown" if spectral_cov is None else "provided"}


def loo_rejection(y, C, family_alpha=.001, min_n=4, max_rejections=1):
    """Conservative ONE-removal external-noise Gaussian LOO, Bonferroni FWER.

    y: trials x frames, C: externally known joint covariance (fixed).
    N<4 is explicitly disabled. All decisions use original data; no iterative
    threshold reuse. Only the largest absolute standardized residual can be
    rejected. Simultaneously rejecting all extreme LOO scores causes swamping
    under one contaminated frame (retained failed-experiment evidence).
    Poisson use is an approximation and is tested separately.
    If zero/unknown residual variance occurs, fail instead of inventing noise.
    """
    y = np.atleast_2d(y)
    n = y.shape[1]
    accepted = np.ones(y.shape, bool)
    residual = np.zeros(y.shape)
    if n < min_n:
        return accepted, residual, None
    threshold = float(norm.isf(family_alpha/(2*n)))
    for k in range(n):
        other = np.delete(np.arange(n), k)
        Coo = C[np.ix_(other, other)]
        inverse_one = np.linalg.solve(Coo, np.ones(n-1))
        a = inverse_one/inverse_one.sum()
        variance = C[k, k]+a@Coo@a-2*C[k, other]@a
        if not np.isfinite(variance) or variance <= 0:
            raise ValueError("LOO_VARIANCE_UNAVAILABLE")
        residual[:, k] = (y[:, k]-y[:, other]@a)/np.sqrt(variance)
    if max_rejections is None:
        # Retained failed comparator: parallel LOO plus empty-stack fallback.
        accepted = np.abs(residual) <= threshold
        accepted[~accepted.any(axis=1)] = True
    elif max_rejections == 1:
        maximum = np.argmax(np.abs(residual), axis=1)
        rows = np.arange(len(y))
        reject = np.abs(residual[rows, maximum]) > threshold
        accepted[rows[reject], maximum[reject]] = False
    else:
        raise ValueError('UNVALIDATED_REJECTION_POLICY')
    return accepted, residual, threshold


def accepted_gls(y, C, accepted):
    """GLS after data selection; returned sigma is fixed-mask conditional only.

    Grouping by bit mask avoids a per-trial inverse and supports correlated C.
    Coverage must be evaluated end to end; this sigma is not a theorem about
    selection. Empty stacks are undefined, caller must resolve their identity.
    """
    n = y.shape[1]
    code = accepted.astype(np.int64) @ (1 << np.arange(n))
    value, sigma = np.full(len(y), np.nan), np.full(len(y), np.nan)
    for mask in np.unique(code):
        use = np.flatnonzero(mask & (1 << np.arange(n)))
        if not len(use):
            continue
        ci = C[np.ix_(use, use)]
        w = np.linalg.solve(ci, np.ones(len(use)))
        w /= w.sum()
        which = code == mask
        value[which] = y[which][:, use]@w
        sigma[which] = np.sqrt(w@ci@w)
    return value, sigma


def fit_shared_gaia(counts, gaia_q, design, star_index, count_cov, gaia_cov):
    """Joint Gaussian count/XP likelihood, retaining latent star fluxes.

    C_i = exp(X_i theta) q_{star(i)}+noise, q_Gaia=q+eta.
    Count and Gaia observations are independent here, but Gaia may have a
    nondiagonal covariance and each source identity is shared across frames.
    General correlated observations require a joint whitening matrix. This
    baseline handles positive high-SNR calibrators; it does not clip saturated
    or variable stars, model Poisson likelihood, or turn unknown C into zero.
    Fit q as nuisance rather than treating noisy Gaia q as exact regressors.
    """
    nc, ns = len(counts), len(gaia_q)
    nt = design.shape[1]
    lc, lg = np.linalg.cholesky(count_cov), np.linalg.cholesky(gaia_cov)
    wc, wg = np.linalg.solve(lc,np.eye(nc)), np.linalg.solve(lg,np.eye(ns))
    qscale = np.median(gaia_q)
    incidence=np.eye(ns)[star_index]
    def residual(p):
        theta,u = p[:nt],p[nt:]
        mean = np.exp(design@theta)*(incidence@u)*qscale
        return np.r_[wc@(counts-mean), wg@(gaia_q-u*qscale)]
    def jacobian(p):
        a=np.exp(design@p[:nt])
        mean=a*(incidence@p[nt:])*qscale
        top=np.column_stack([-wc@(mean[:,None]*design),-wc@(a[:,None]*incidence*qscale)])
        return np.vstack([top,np.column_stack([np.zeros((ns,nt)),-wg*qscale])])
    initial=np.r_[np.zeros(nt),gaia_q/qscale]
    lower=np.r_[np.full(nt,-np.inf),np.zeros(ns)]
    result=least_squares(residual,initial,jac=jacobian,bounds=(lower,np.inf),
                         ftol=1e-11,xtol=1e-11,gtol=1e-9,max_nfev=200)
    if not result.success:
        raise ValueError('GAIA_JOINT_FIT_NOT_CONVERGED')
    _,s,vt=np.linalg.svd(result.jac,full_matrices=False)
    if s[-1] <= 1e-12*s[0]:
        raise ValueError('GAIA_JOINT_FIT_NOT_IDENTIFIABLE')
    # Gaussian fixed covariance information; nonlinear curvature and finite
    # sample behavior are separately assessed by independent simulations.
    covariance=(vt.T/s**2)@vt
    # Influence of [count errors, Gaia errors] on fitted theta.
    whitening=np.zeros((nc+ns,nc+ns)); whitening[:nc,:nc]=wc;whitening[nc:,nc:]=wg
    influence=-np.linalg.pinv(result.jac)@whitening
    return result.x[:nt],covariance[:nt,:nt],influence[:nt],{
        'singular_values':s.tolist(),'nfev':result.nfev,
        'joint_chi_square':float(result.fun@result.fun)}


def experiment_gaia_calibration(rng):
    ns,nf=12,2
    stars=np.tile(np.arange(ns),nf)
    x=np.tile(np.linspace(-1,1,ns),nf)
    X=np.zeros((ns*nf,4))
    X[:ns,:2]=np.column_stack([np.ones(ns),x[:ns]])
    X[ns:,2:]=np.column_stack([np.ones(ns),x[ns:]])
    theta=np.array([.20,.03,-.10,-.02])
    q=np.linspace(800,1600,ns)
    cm=np.exp(X@theta)*q[stars]
    CC=np.diag(np.repeat([9.,16.],ns))
    # Shared 0.5% Gaia absolute mode plus 1% star-specific error.
    CG=np.diag((.01*q)**2)+np.outer(.005*q,.005*q)
    got,Ctheta,influence,diagnostics=fit_shared_gaia(cm,q,X,stars,CC,CG)
    assert np.max(np.abs(got-theta))<1e-10
    joint=np.zeros((len(cm)+ns,len(cm)+ns));joint[:len(cm),:len(cm)]=CC;joint[len(cm):,len(cm):]=CG
    assert np.max(np.abs(influence@joint@influence.T-Ctheta))<1e-12
    nmc=1000
    samples=[]; sigmas=[]
    lc,lg=np.linalg.cholesky(CC),np.linalg.cholesky(CG)
    for j in range(nmc):
        ci=cm+lc@rng.normal(size=len(cm)); qi=q+lg@rng.normal(size=ns)
        th,cov,_,_=fit_shared_gaia(ci,qi,X,stars,CC,CG)
        samples.append(th);sigmas.append(np.sqrt(np.diag(cov)))
    samples=np.array(samples);sigmas=np.array(sigmas)
    empirical=np.cov(samples,rowvar=False)
    # Independent known-truth negative case: stellar colour equals position,
    # so spatial and colour coefficients have identical count Jacobian
    # columns. Gaia's noisy latent q cannot create the missing distinction.
    xpos=np.linspace(-1,1,ns)
    Xbad=np.column_stack([np.ones(ns),xpos,xpos])
    theta_bad=np.array([.2,.03,0.])
    a_bad=np.exp(Xbad@theta_bad)
    cbad=a_bad*q
    CCbad=np.eye(ns)*9.
    Wc=np.eye(ns)/3.
    Wg=np.linalg.solve(np.linalg.cholesky(CG),np.eye(ns))
    qscale=np.median(q)
    Jbad=np.vstack([
        np.column_stack([-Wc@(cbad[:,None]*Xbad),-Wc@np.diag(a_bad*qscale)]),
        np.column_stack([np.zeros((ns,3)),-Wg*qscale])])
    singular_bad=np.linalg.svd(Jbad,compute_uv=False)
    rank_bad=int(np.count_nonzero(singular_bad>1e-12*singular_bad[0]))
    try:
        fit_shared_gaia(cbad,q,Xbad,np.arange(ns),CCbad,CG)
        rank_status='UNEXPECTED_SUCCESS'
    except ValueError as exc:
        rank_status=str(exc)
    assert rank_status=='GAIA_JOINT_FIT_NOT_IDENTIFIABLE'
    assert rank_bad==ns+2 and Jbad.shape[1]==ns+3
    rank_negative={'construction':'X=[1,x,colour], colour=x exactly',
        'parameter_columns':3,'joint_parameter_dimension':ns+3,
        'joint_jacobian_rank':rank_bad,'relative_rank_tolerance':1e-12,
        'singular_values':singular_bad.tolist(),'fit_status':rank_status,
        'unidentifiable_direction':[0.,1.,-1.],
        'null_direction_count_jacobian_norm':float(np.linalg.norm(Xbad@np.array([0.,1.,-1.])))}
    return {'trials':nmc,'stars':ns,'frames':nf,'parameters_true':theta.tolist(),
        'noise_free_parameter_max_abs_error':float(np.max(np.abs(got-theta))),
        'linearized_covariance':Ctheta.tolist(),'empirical_covariance':empirical.tolist(),
        'parameter_bias':np.mean(samples-theta,axis=0).tolist(),
        'parameter_bias_mc_se':(np.std(samples-theta,axis=0,ddof=1)/np.sqrt(nmc)).tolist(),
        'parameter_coverage':{str(i):coverage_report(samples[:,i]-theta[i],sigmas[:,i]) for i in range(4)},
        'shared_Gaia_logzero_covariance':float(Ctheta[0,2]),'solver':diagnostics,
        'rank_negative_colour_equals_position':rank_negative,
        'scope':'Known Gaussian count and Gaia covariances, high SNR, no saturation/variability/SED mismatch.'}


def experiment_covariance(rng):
    # Three native integrated pixels, four spherical leaves; intentionally
    # nonunit native areas and a split source produce correlations.
    a = np.array([[.3, .2, 0, 0], [0, .3, .2, 0], [0, 0, .1, .4]])
    R, N = drizzle_operator(a, np.full(3, .5), np.array([1., 2., 3.]))
    D = np.diag([4., 9., 16.])
    U = np.array([[2., .1], [3., -.2], [5., .4]])
    Lambda = np.array([[.04, .003], [.003, .01]])
    Cx = D+U@Lambda@U.T
    Cy = R@Cx@R.T
    simulations = (rng.normal(size=(TRIALS, 3))@np.linalg.cholesky(D).T+
                   rng.normal(size=(TRIALS, 2))@np.linalg.cholesky(Lambda).T@U.T)@R.T
    empirical = np.cov(simulations, rowvar=False)
    action = covariance_factor_action(R, D, R@U, Lambda, np.ones(4))
    assert np.max(np.abs(action-Cy@np.ones(4))) < 1e-12
    assert np.max(np.abs(R@np.array([1.,2.,3.])-1)) < 1e-15
    # A point-source and a surface pixel can have equal flux precision but
    # unequal surface precision. Values are algebraic, not empirical fits.
    pa, pb = np.array([.5,.5]), np.array([.9,.1])
    va, vb = .5, .82
    fa = va/(pa@pa)
    fb = vb/(pb@pb)
    # Singular covariance needs exact constraints, not naive C+.
    K, V, diag = linear_gls_operator(np.array([[0.],[1.]]), np.diag([1.,0.]))
    assert np.max(np.abs(K@np.array([17.,8.])-8)) < 1e-12 and V[0,0] == 0
    redundant=np.array([[1.,0.,0.],[0.,1.,2.],[0.,1.,2.]])
    _,vr,dr=linear_gls_operator(redundant,np.eye(3))
    assert abs(vr[0,0]-1)<1e-12 and dr['unidentifiable_parameter_dimension']==1
    try:
        linear_gls_operator(np.ones((3,2)),np.eye(3))
        raise AssertionError('Did not detect nonidentifiable flux')
    except ValueError as exc:
        assert str(exc)=='REQUESTED_PARAMETER_NOT_IDENTIFIABLE'
    try:
        apply_linear_gls(np.array([[1.],[1.]]),np.zeros((2,2)),np.array([1.,2.]))
        raise AssertionError('Did not detect inconsistent exact data')
    except ValueError as exc:
        assert str(exc)=='INCONSISTENT_EXACT_CONSTRAINTS'
    # Oversampling preserves information only if R is full column rank and
    # both templates and covariance are transported by the same R.
    h = np.array([.6,.3,.1])
    I_native = float(h@np.linalg.solve(Cx,h))
    I_sphere = float((R@h)@np.linalg.pinv(Cy)@(R@h))
    assert abs(I_sphere/I_native-1) < 1e-12
    # Shared noise floor cannot be reduced by pretending frames independent.
    floor = {str(n): {"true_variance": 1/n+.04,
                      "incorrect_independent_variance": 1.04/n}
             for n in (1,2,10,100)}
    d = np.array([1.,2.,3.])
    wood = woodbury_solve(d,U,Lambda,np.ones(3))
    wood_error = np.max(np.abs(wood-np.linalg.solve(np.diag(d)+U@Lambda@U.T,np.ones(3))))
    return {"R": R.tolist(), "normalizer_sr": N.tolist(), "C_surface": Cy.tolist(),
        "empirical_C_surface": empirical.tolist(),
        "relative_covariance_frobenius_error": float(np.linalg.norm(empirical-Cy)/np.linalg.norm(Cy)),
        "factor_action_max_error": float(np.max(np.abs(action-Cy@np.ones(4)))),
        "woodbury_max_error": float(wood_error),
        "shared_floor": floor, "Fisher_vs_surface": {"V_F_A": fa,"V_F_B":fb,
          "surface_weight_A":1/va,"surface_weight_B":1/vb},
        "PSD_exact_constraint": {"K":K.tolist(),"V_F":float(V[0,0]),**diag},
        "information_invariance": {"native":I_native,"resampled":I_sphere,
                                   "relative_error":abs(I_sphere/I_native-1)},
        "unknown_variance": {"full_variance":None,"reason":"nonzero source coefficient has unknown noise"}}


def experiment_sky(rng):
    m, n = 17, 3
    x = np.linspace(-1,1,m)
    H = np.tile(np.eye(m),(n,1))
    phi = np.column_stack([np.ones(m),x])
    A = np.zeros((n*m,2*n))
    for k in range(n):
        A[k*m:(k+1)*m,2*k:2*k+2] = phi
    G = np.tile(np.eye(2),(1,n))
    C = np.diag(np.repeat([1.,2.,4.],m))
    K, diagnostics = sky_profile_operator(H,A,C,G)
    beta_true = np.array([2.,.6,-1.,-.2,-1.,-.4])
    true_sky = 12.+3*x+2*np.exp(-x*x/.03)
    ymean = H@true_sky+A@beta_true
    recovered = K@ymean
    P = np.eye(n*m)-A@K
    Cz = P@C@P.T
    wrong = C+A@K@C@K.T@A.T
    cross = -C@K.T@A.T-A@K@C
    assert np.linalg.norm(wrong+cross-Cz) < 1e-11
    assert np.max(np.abs(P@ymean-H@true_sky)) < 1e-11
    # One output coordinate provides direct MC of same-data correction.
    j = m//2
    sample = rng.normal(size=(TRIALS,n*m))*np.sqrt(np.diag(C))
    z = sample@P[j]
    var = float(Cz[j,j])
    # Disconnected components: same gauge cannot resolve second component.
    Hdis = np.zeros((2*m,2*m)); Hdis[:m,:m]=np.eye(m); Hdis[m:,m:]=np.eye(m)
    Adis = np.kron(np.eye(2),np.ones((m,1)))
    try:
        sky_profile_operator(Hdis,Adis,np.eye(2*m),np.ones((1,2)))
        disconnected = "UNEXPECTED_SUCCESS"
    except ValueError as exc:
        disconnected = str(exc)
    assert disconnected == "RELATIVE_SKY_NOT_IDENTIFIABLE"
    # Incorrect common response: a 10% colour/response slope is exactly
    # absorbed as a sky plane, even though true additive sky is zero.
    H2 = np.tile(np.eye(m),(2,1))
    A2 = np.zeros((2*m,4)); A2[:m,:2]=phi; A2[m:,2:]=phi
    K2,_ = sky_profile_operator(H2,A2,np.eye(2*m),np.tile(np.eye(2),(1,2)))
    gradient = 12+3*x
    response_y = np.r_[gradient,1.1*gradient]
    fake = K2@response_y
    # PSF mismatch oracle with asymmetric finite support; no additive offset.
    hp1 = np.exp(-((x+.35)/.09)**2); hp2 = np.exp(-((x+.35)/.28)**2)
    hp1/=hp1.sum(); hp2/=hp2.sum()
    psf_y = np.r_[100*hp1,100*hp2]
    psf_fake = K2@psf_y
    return {"profile_diagnostics":diagnostics,"true_coefficients":beta_true.tolist(),
        "coefficient_max_abs_error":float(np.max(np.abs(recovered-beta_true))),
        "common_gradient_max_error":float(np.max(np.abs(P@(H@gradient)-H@gradient))),
        "same_data_covariance": {"coordinate":j,"correct_variance":var,
          "incorrect_add_only_variance":float(wrong[j,j]),
          "empirical_variance":float(np.var(z,ddof=1)),
          "cross_term_max_abs":float(np.max(np.abs(cross))),
          "coverage_correct":coverage_report(z,np.sqrt(var)),
          "coverage_add_only":coverage_report(z,np.sqrt(wrong[j,j]))},
        "disconnected_without_component_gauges":disconnected,
        "response_mismatch_absorbed_as_fake_sky":fake.tolist(),
        "PSF_mismatch_absorbed_as_fake_sky":psf_fake.tolist(),
        "scope":"Common gradient protection proved only for the declared H; mismatch is a counterexample."}


def experiment_spectral(rng):
    coeff = np.array([1e-16, .13e-16, .03e-16])
    basis = lambda l: np.array([1.,(l-656.28)/10,((l-656.28)/10)**2])
    sigma = np.array([.02e-16,.01e-16,.005e-16])
    Corr = np.array([[1.,.5,-.2],[.5,1.,.1],[-.2,.1,1.]])
    Cf = sigma[:,None]*Corr*sigma
    qe = lambda l: .7+.001*(l-656.28)
    bands = {}
    for width in (3.,7.):
        lo, hi = 656.28-width/2,656.28+width/2
        result = gaia_photon_integral(basis,coeff,Cf,lo,hi,qe)
        weights = np.array(result['weights'])
        noise = rng.normal(size=(TRIALS,3))@np.linalg.cholesky(Cf).T
        errors = noise@weights
        result['covariance_mc_ratio'] = float(np.var(errors,ddof=1)/result['variance'])
        result['coverage'] = coverage_report(errors,np.sqrt(result['variance']))
        # Independent polynomial integration through numpy.polynomial,
        # including wavelength SI conversion and QE. Quadrature remains
        # only a numerical estimate for general user basis.
        from numpy.polynomial import Polynomial as Poly
        u = Poly([-656.28/10, .1])
        f = coeff[0]+coeff[1]*u+coeff[2]*u*u
        p = f*Poly([.7-.001*656.28,.001])*Poly([0,1e-9/(HPLANCK*CLIGHT)])
        # Integrate shifted polynomial to avoid subtracting large powers.
        v = Poly([0,1.])
        fshift=coeff[0]+coeff[1]*v/10+coeff[2]*(v/10)**2
        pshift=fshift*Poly([.7,.001])*Poly([656.28e-9/(HPLANCK*CLIGHT),1e-9/(HPLANCK*CLIGHT)])
        pi=pshift.integ()
        truth=float(pi(width/2)-pi(-width/2))
        result['independent_analytic_q'] = truth
        result['relative_quadrature_error'] = abs(result['q_photons_s_m2']/truth-1)
        # Coarse 2nm centre sampling misses exact top-hat endpoints.
        grid=np.arange(640.,674.,2.)
        coarse=sum(float(basis(l)@coeff)*qe(l)*l*1e-9/(HPLANCK*CLIGHT)*2
                   for l in grid if lo<=l<=hi)
        result['coarse_2nm_relative_bias']=coarse/truth-1
        # Unresolved line: true narrow Gaussian sigma=.05nm vs convolved
        # low-resolution Gaussian sigma=4nm, conserving total line flux.
        line_nm=657.0
        exact_fraction=ndtr((hi-line_nm)/.05)-ndtr((lo-line_nm)/.05)
        lowres_fraction=ndtr((hi-line_nm)/4)-ndtr((lo-line_nm)/4)
        result['unresolved_line']={'line_center_nm':line_nm,'true_sigma_nm':.05,
            'lowresolution_sigma_nm':4.,'true_inband_fraction':float(exact_fraction),
            'lowresolution_inband_fraction':float(lowres_fraction),
            'relative_line_flux_bias':float(lowres_fraction/exact_fraction-1)}
        bands[str(int(width))]=result
    q3,q7=[bands[w]['q_photons_s_m2'] for w in ('3','7')]
    # A continuum calibrated 3nm->7nm scalar overpredicts a narrow central
    # line even with perfect numerical quadrature and noiseless data.
    bands['continuum_scalar_3_to_7']=q7/q3
    bands['central_line_3_to_7_scalar_bias']=q7/q3-1
    bands['unknown_covariance_output']=gaia_photon_integral(basis,coeff,None,655,658,qe)['variance']
    return bands


def experiment_rejection(rng):
    reports = {}
    for model in ('gaussian','poisson_read'):
        for n in range(1,8):
            # Means after background correction; Gaussian pedestal shift
            # invariance is tested explicitly. The photon noise mean includes
            # sky+object even though the sky mean is subsequently subtracted.
            variance=np.linspace(2.,5.,n) if model=='gaussian' else np.linspace(80.,140.,n)+9.
            C=np.diag(variance)
            truth=0. if model=='gaussian' else 20.
            if model=='gaussian':
                data=rng.normal(size=(TRIALS,n))*np.sqrt(variance)
            else:
                total=np.linspace(80.,140.,n)
                data=rng.poisson(total,size=(TRIALS,n))-total+truth+rng.normal(size=(TRIALS,n))*3
            accepted,t,threshold=loo_rejection(data,C)
            estimate,se=accepted_gls(data,C,accepted)
            plain,plain_se=accepted_gls(data,C,np.ones_like(accepted))
            shifted,_,_=loo_rejection(data+1234.5,C)
            assert np.array_equal(accepted,shifted)
            contamination=data.copy(); contamination[:,0]+=12*np.sqrt(variance[0])
            acc_bad,_,_=loo_rejection(contamination,C)
            bad_est,bad_se=accepted_gls(contamination,C,acc_bad)
            report={'model':model,'n':n,'trials':TRIALS,'family_alpha':.001,
                'automatic_enabled':n>=4,'threshold':threshold,
                'clean_stack_false_reject':fraction_report(~accepted.all(axis=1)),
                'clean_bias':float(np.mean(estimate-truth)),
                'clean_mean_error_mc_se':float(np.std(estimate,ddof=1)/np.sqrt(TRIALS)),
                'clean_coverage':coverage_report(estimate-truth,se),
                'no_selection_coverage':coverage_report(plain-truth,plain_se),
                'injected_12sigma_sample0_rejected':fraction_report(~acc_bad[:,0]),
                'injected_clean_sample_false_reject':fraction_report(~acc_bad[:,1:].all(axis=1)),
                'contaminated_bias':float(np.mean(bad_est-truth)),
                'contaminated_coverage':coverage_report(bad_est-truth,bad_se),
                'pedestal_invariant':True,
                'all_rejected_before_fallback':int(np.count_nonzero((np.abs(t)>threshold).all(axis=1))) if threshold else 0}
            reports[f'{model}_N{n}']=report
    # The documented percentile counterexample is recomputed independently.
    examples=[]
    for delta in (0.,10.):
        v=np.arange(-2.,3.)+delta; median=np.median(v)
        accepted=(v>=median-.2*abs(median))&(v<=median+.1*abs(median))
        examples.append({'pedestal':delta,'accepted':v[accepted].tolist(),
                         'mean':float(np.mean(v[accepted]))})
    reports['percentile_counterexample']=examples
    # Correlation makes externally known joint C, including the covariance
    # between each observation and its LOO predictor, essential.
    Ccorr=np.diag([1.,2.,3.,4.,5.])+np.full((5,5),.7)
    data=rng.normal(size=(TRIALS,5))@np.linalg.cholesky(Ccorr).T
    accepted,_,_=loo_rejection(data,Ccorr)
    value,se=accepted_gls(data,Ccorr,accepted)
    reports['correlated_gaussian_N5']={'clean_stack_false_reject':fraction_report(~accepted.all(axis=1)),
        'clean_coverage':coverage_report(value,se),'shared_variance':.7}
    return reports


def main():
    rng=np.random.default_rng(SEED)
    report={'identity':{'seed':SEED,'trials_per_mc':TRIALS,
              'python':platform.python_version(),'numpy':np.__version__,'scipy':scipy.__version__,
              'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'uses_production_algorithms':False,'real_data':False,
              'scope':'independent synthetic evidence; not formal scientific acceptance'},
            'covariance':experiment_covariance(rng),'sky':experiment_sky(rng),
            'Gaia_integral':experiment_spectral(rng),'rejection':experiment_rejection(rng),
            'Gaia_joint_calibration':experiment_gaia_calibration(rng)}
    output=Path(__file__).resolve().parents[1]/'evidence'/'statistics_independent.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(output)
    print(json.dumps({'covariance_MC_relative_error':report['covariance']['relative_covariance_frobenius_error'],
        'sky_max_bias':report['sky']['coefficient_max_abs_error'],
        'gaia_3nm_quad_error':report['Gaia_integral']['3']['relative_quadrature_error'],
        'gaussian_N5_FWER':report['rejection']['gaussian_N5']['clean_stack_false_reject']['fraction']},indent=2))


if __name__=='__main__':
    main()
