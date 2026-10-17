#!/usr/bin/env python3
"""ACSD S1 mathematical oracle: independent NumPy/SciPy linear models, NOT FITS E2E.

Usage: python3 s1_oracle.py --output s1_results.json
The suite uses closed-form matrix references, not ACSD C++ or adapters.
Python >=3.10, NumPy required. Inferences are conditional on stated toy models.
"""
import argparse
import json
import math
import platform
import unittest
from pathlib import Path

import numpy as np


def gls_common(gamma):
    """BLUE for z = ones * B + e; SPD joint covariance only."""
    c = np.asarray(gamma, dtype=float)
    if c.ndim != 2 or c.shape[0] != c.shape[1] or not np.allclose(c, c.T):
        raise ValueError("not symmetric square covariance")
    if np.linalg.eigvalsh(c)[0] <= 0:
        raise ValueError("covariance not SPD (unmodelled exact constraints/rank)")
    one = np.ones(c.shape[0])
    x = np.linalg.solve(c, one)
    a = x / (one @ x)
    return a, float(1 / (one @ x))


def flux_information(h, c, z=None):
    """Profiled GLS information for fixed Gaussian covariance; not Poisson Fisher."""
    h = np.asarray(h, dtype=float).reshape(-1)
    c = np.asarray(c, dtype=float)
    np.linalg.cholesky(c)
    hw = np.linalg.solve(np.linalg.cholesky(c), h)
    if z is None:
        residual = hw
    else:
        zz = np.asarray(z, dtype=float)
        zw = np.linalg.solve(np.linalg.cholesky(c), zz)
        u, s, _ = np.linalg.svd(zw, full_matrices=False)
        rank = int(np.sum(s > max(zw.shape) * np.finfo(float).eps * s[0])) if len(s) else 0
        residual = hw - u[:, :rank] @ (u[:, :rank].T @ hw)
    return float(residual @ residual)


def spherical_tangent_deg(ra_dec, origin):
    """Gnomonic tangent-plane coordinates (degrees) avoiding RA wrap."""
    r0, d0 = np.deg2rad(origin)
    ra, de = np.deg2rad(np.asarray(ra_dec, dtype=float).T)
    dd = np.remainder(ra-r0+np.pi, 2*np.pi)-np.pi
    cosc = np.sin(d0)*np.sin(de)+np.cos(d0)*np.cos(de)*np.cos(dd)
    if np.any(cosc <= 1e-12):
        raise ValueError("chart outside hemisphere")
    xi = np.cos(de)*np.sin(dd)/cosc
    eta = (np.cos(d0)*np.sin(de)-np.sin(d0)*np.cos(de)*np.cos(dd))/cosc
    return np.rad2deg(np.column_stack((xi, eta)))


def support_interpolate(target, controls, log_variance, control_status):
    """Local 3-control illustrative model. NOT actual HEALPix tile lookup."""
    if any(s == "corrupt" for s in control_status):
        return {"state": "DATA_CORRUPT", "variance": None}
    if len(controls) != 3 or len(log_variance) != 3:
        return {"state": "PRECISION_UNAVAILABLE", "variance": None,
                "reason": "missing or invalid control count"}
    if any(s != "measured" for s in control_status):
        return {"state": "PRECISION_UNAVAILABLE", "variance": None,
                "reason": "control missing, not guessed"}
    coords = spherical_tangent_deg(controls, target)
    mat = np.vstack((np.ones(3), coords.T))
    if np.linalg.cond(mat) > 1e12:
        return {"state": "UNIDENTIFIABLE", "variance": None}
    alpha = np.linalg.solve(mat, [1, 0, 0])
    if np.min(alpha) < -1e-10:
        return {"state": "OUTSIDE_SUPPORT", "variance": None}
    if not np.all(np.isfinite(log_variance)):
        return {"state": "DATA_CORRUPT", "variance": None}
    return {"state": "MODEL_ESTIMATE_ONLY",
            "variance": float(np.exp(alpha @ np.asarray(log_variance))),
            "coefficients": alpha.tolist(),
            "remark": "a model prediction; no calibrated error bound supplied"}


def run_cases():
    cases = {}
    # Units Uout / Uin, conditional covariance + shared calibration covariance.
    alpha = np.array([2e-17, 3e-17])
    x = np.array([100., 200.])
    c = np.array([[4., 0.25], [0.25, 9.]])
    a = np.diag(alpha)
    c_ap = a @ c @ a.T
    assert np.allclose(c_ap / 1e-34, np.array([[16,1.5],[1.5,81]]), atol=1e-12)
    cases["units_deterministic"] = {
        "input": {"x_native":[100,200], "native_unit":"ADU", "alpha":[2e-17,3e-17],
                  "alpha_unit":"Uref/ADU", "C_native_ADU2": c.tolist(), "pixel_area_sr":0.5},
        "result": {"x_applied_Uref": (a @ x).tolist(),
                   "C_applied_Uref2": c_ap.tolist(),
                   "B_applied_Uref_sr": (a @ x / .5).tolist(),
                   "variance_B_Uref2_sr2":(np.diag(c_ap)/.5**2).tolist()}}
    # Shared stochastic calibration factor θ plus correlated estimating noise.
    y = np.array([2.,3.]); cs = np.diag([1.,4.])
    sigma_th2 = .04; cross = np.array([.01,-.02])
    jac = y.copy()  # dy'/dθ = y, evaluated at θ=4
    conditional = 16*cs
    nuisance = sigma_th2*np.outer(jac,jac)
    cross_term = 4*(np.outer(cross,jac)+np.outer(jac,cross))
    total = conditional+nuisance+cross_term
    assert np.linalg.eigvalsh(total)[0] > 0 and abs(total[0,1]) > 0
    cases["stochastic_scale"] = {
        "input":{"x":[2,3],"alpha_mean":4,"var_theta":sigma_th2,
                 "cov_x_theta":cross.tolist(),"Cov_x":cs.tolist()},
        "result":{"conditional":conditional.tolist(),"parameter":nuisance.tolist(),
                  "cross":cross_term.tolist(),"marginal_linearized":total.tolist()},
        "assumptions":"small parameter error; joint covariance specified; 1st-order delta method"}
    # Two differently shaped source PSFs, but the SAME pre-defined B estimator l.
    psf_a=np.array([1.,0.]);psf_b=np.array([.5,.5]);eye=np.eye(2)
    l=np.array([1.,0.])  # unbiased B for spatially constant 2-pixel field
    vb=float(l @ eye @ l)
    ia=flux_information(psf_a,eye); ib=flux_information(psf_b,eye)
    weights=np.array([ia,ib]);weights/=weights.sum()
    wrong=float(np.sum(weights**2 * vb))
    cases["psf_vs_brightness"]={
        "input":{"Cov_pixels":eye.tolist(),"B_estimator_l":l.tolist(),
                 "PSF_A":psf_a.tolist(),"PSF_B":psf_b.tolist(),
                 "assumption":"constant B over the two raw pixels; fixed l=[1,0]"},
        "result":{"V_B_A":vb,"V_B_B":vb,"I_F_A":ia,"I_F_B":ib,
                  "T_A":vb*ia,"T_B":vb*ib, "coadd_optimal_variance":vb/2,
                  "coadd_using_IF":wrong,"relative_excess":wrong/(vb/2)-1}}
    # 2D heteroscedastic profiled GLS nuisance: flux template vs DC.
    h=np.array([.8,.2]);cv=np.diag([1.,4.]);z=np.ones((2,1))
    known=flux_information(h,cv); unknown=flux_information(h,cv,z)
    cases["heteroscedastic_background"]={
        "input":{"h":h.tolist(),"C":cv.tolist(),"Z":z.tolist()},
        "result":{"I_F_background_known":known,"V_F_background_known":1/known,
                  "I_F_background_free":unknown,"V_F_background_free":1/unknown,
                  "nuisance_penalty_ratio":known/unknown},
        "expected":{"I_known":.65,"I_unknown":.072}}
    assert abs(known-.65)<1e-14 and abs(unknown-.072)<1e-13
    # Same observation marginal variances, but correlated frames.
    gamma=np.array([[2.,1.],[1.,2.]])
    a_true,vtrue=gls_common(gamma)
    vnaive=1/np.sum(1/np.diag(gamma))
    cases["shared_calibration"]={
        "input":{"Gamma":gamma.tolist(),"e_k":"delta + independent eta_k",
                 "Var_delta":1,"Var_eta":1},
        "result":{"GLS_coeff":a_true.tolist(),"true_V_B":vtrue,
                  "reported_if_diagonal_only":vnaive,
                  "fraction_underreported":1-vnaive/vtrue}}
    assert abs(vtrue-1.5)<1e-14
    mixed=np.array([[1.,.3],[.3,4.]])
    am, vm=gls_common(mixed)
    adiag=(1/np.diag(mixed));adiag/=sum(adiag)
    cases["general_GLS"]={
        "input":{"Gamma":mixed.tolist(),"signal_model":"z=[B,B]+noise"},
        "result":{"BLUE_coeff":am.tolist(),"BLUE_variance":vm,
                  "diagonal_only_coeff":adiag.tolist(),
                  "actual_variance_diagonal_only":float(adiag@mixed@adiag)}}
    # Scene-dependent source Poisson: frozen Gaussian variance not portable.
    cases["reference_vs_scene"]={
        "input":{"background_plus_read_variance_ADU2":1,
                 "reference_flux_ADU":100,"gain_e_per_ADU":2,
                 "actual_scene_source_flux_ADU":0},
        "result":{"V_reference_ADU2":51,"V_actual_scene_ADU2":1,
                  "ratio_ref_to_scene":51},
        "assumptions":"scalar unweighted readout, P=[1], known sky; Poisson variance=F/g"}
    # Identical marginal variances, unprovided correlation: two worlds.
    cases["unknown_correlation"]={
        "input":{"known_marginals":[1,1],
                 "Gamma_world1":[[1,0],[0,1]],
                 "Gamma_world2":[[1,.99],[.99,1]]},
        "result":{"variance_of_mean_world1":.5,
                  "variance_of_mean_world2":.995,"complete_precision_identifiable":False}}
    # Spatial alpha(u) defines an amplitude parameter, not a total applied flux.
    pstar=np.array([.25,.75]); aspatial=np.array([1.,3.]); a_at_u=2.; fnative=100.
    fref=a_at_u*fnative; h_ref=(aspatial/a_at_u)*pstar
    ftotal=float(fnative*(aspatial@pstar)); ratio=ftotal/fref
    assert abs(fref-200)<1e-12 and abs(ftotal-250)<1e-12
    assert np.allclose(fref*h_ref, fnative*aspatial*pstar)
    cases["reference_parameter_vs_total"]={
        "input":{"P_integrated":pstar.tolist(),"alpha_per_pixel":aspatial.tolist(),
                 "alpha_at_source":a_at_u,"F_native":fnative,"V_F_ref_example":4},
        "result":{"F_ref":fref,"h_ref":h_ref.tolist(),
                  "F_applied_total":ftotal,"P_applied_normalized":(h_ref/ratio).tolist(),
                  "scale_c":ratio,"V_F_applied_total_example":4*ratio**2}}
    # A shared source pixel through Drizzle yields distinct leaf covariance.
    r=np.array([[.75,.25],[.25,.75]])/.2
    cin=np.diag([1.,4.]); cout=r@cin@r.T
    assert np.allclose(cout,[[20.3125,23.4375],[23.4375,57.8125]])
    cases["drizzle_correlated_leaf"]={
        "input":{"raw_integral_unit":"L","source_area_sr":.2,
                 "R_per_sr":r.tolist(),"C_source_L2":cin.tolist()},
        "result":{"C_leaf_L2_sr2":cout.tolist(),"variance_leaf":np.diag(cout).tolist(),
                  "offdiagonal_nonzero":bool(cout[0,1]>0)},
        "assumptions":"both leaf estimators are normalized average B on same source pixels"}
    # Missing a calibration control cannot imply finite local precision.
    cases["missing_control_two_worlds"]={
        "input":{"measured_SNR":[100,100,100],"fourth_SNR":None,
                 "F_ref":1, "candidate_impute_SNR":100},
        "result":{"worldA_missing_SNR":100,"worldB_missing_SNR":1,
                  "worldA_I_F":10000,"worldB_I_F":1,
                  "identified_upper_information":None,
                  "support_state":"PRECISION_UNAVAILABLE"}}
    seam_controls=[[359.8,0],[.2,0],[0,.3]]
    center=support_interpolate([0,.1],seam_controls,np.log([1.,2.,4.]),["measured"]*3)
    missing=support_interpolate([0,.1],seam_controls,np.log([1.,2.,4.]),
                                ["measured","missing","measured"])
    outside=support_interpolate([20,20],seam_controls,np.log([1.,2.,4.]),
                                ["measured"]*3)
    corrupt=support_interpolate([0,.1],seam_controls,np.log([1.,2.,4.]),
                                ["measured","corrupt","measured"])
    assert center["state"]=="MODEL_ESTIMATE_ONLY"
    assert missing["state"]=="PRECISION_UNAVAILABLE"
    assert outside["state"]=="OUTSIDE_SUPPORT"
    assert corrupt["state"]=="DATA_CORRUPT"
    polar = spherical_tangent_deg([[10,89.8],[130,89.8]], [70,89.9])
    cases["spherical_support"]={
        "input":{"ra_seam_controls_deg":seam_controls, "target":[0,.1],
                 "polar_points_deg":[[10,89.8],[130,89.8]],"polar_center":[70,89.9]},
        "result":{"valid":center,"missing":missing,"outside":outside,
                  "corrupt":corrupt,"polar_tangent_deg":polar.tolist()},
        "scope":"independent ICRS tangent geometry; NOT NESTED HEALPix cross-face index test"}
    assert len(cases)==12
    assert abs(cases["psf_vs_brightness"]["result"]["relative_excess"]-1/9)<1e-13
    assert abs(cases["shared_calibration"]["result"]["fraction_underreported"]-1/3)<1e-13
    assert cases["general_GLS"]["result"]["BLUE_variance"] < cases["general_GLS"]["result"]["actual_variance_diagonal_only"]
    assert cases["unknown_correlation"]["result"]["variance_of_mean_world2"] > cases["unknown_correlation"]["result"]["variance_of_mean_world1"]
    return cases


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",default="s1_results.json")
    args=parser.parse_args()
    cases=run_cases()
    payload={"schema":"acsd.s1.oracle/1","base_main_sha":"51e06540eb021c7272e87cf67b34e53c0648cf7a",
             "runtime":{"python":platform.python_version(),"numpy":np.__version__},
             "method":"closed-form + NumPy LAPACK (Cholesky/solve/SVD)",
             "status":"PASS_ANALYTIC_ONLY",
             "tests_count":len(cases),
             "not_run":["production_C++", "real_FITS", "Monte_Carlo",
                        "independent_HEALPix_NESTED_face_crossing", "Gaia_XP_real_response"],
             "cases":cases}
    Path(args.output).write_text(json.dumps(payload,ensure_ascii=False,indent=2,allow_nan=False)+"\n",
                                 encoding="utf-8")
    print(json.dumps({"status":payload["status"],"tests_count":len(cases),
                     "psf_excess":cases["psf_vs_brightness"]["result"]["relative_excess"],
                     "shared_underreport":cases["shared_calibration"]["result"]["fraction_underreported"],
                     "output":args.output},ensure_ascii=False))
if __name__=="__main__":
    main()
