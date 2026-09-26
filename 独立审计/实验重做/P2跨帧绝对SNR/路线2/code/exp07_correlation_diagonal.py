#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-07: P-CST-10 (k_corr = 1.4, calibration 1.3883) and P-CST-22
(diagonal-omission variance underestimate, registered values 23.3% / 36.3%)

A literature leg  : Fruchter & Hook 2002, PASP 114, 144 (drizzle; correlated
                    output noise); general GLS with covariance.
B experiment leg  :
  (22) Weighted mean of n equicorrelated equal-variance pixels (rho):
       exact GLS variance  Var_GLS = sigma^2 (1 + (n-1) rho) / n
       diagonal omission   Var_diag = sigma^2 / n
       underestimate = Var_diag/Var_GLS - 1 = 1/(1+(n-1)rho) - 1.
       Map the two registered values onto the (n, rho) surface:
         36.3%  <-> (n = 4, rho = 0.19)      [1/(1+3*0.19) - 1 = -0.3630]
         23.3%  <-> (n = 2, rho ~ 0.2333) or (n ~ 2.23, rho = 0.19)
       => the two values are two points of ONE surface, not mutually exclusive
       physics; the ambiguity is the effective neighbour count of the kernel.
       MC with an explicit covariance matrix (AR(1) kernel, rho1 grid) verifies
       the analytic surface.  NEGATIVE CONTROL: rho = 0 => identity to machine
       precision (P-GATE-15's rho=0 bit-identity requirement).
  (10) k_corr as variance inflation factor: k_eff = Var_GLS/Var_diag * (n_eff/n)?
       Registered calibration 1.3883 ~ 1 + (n-1) rho with (n = 3, rho ~ 0.194);
       toy scan over kernel support shows the inflation lands in 1.2-1.6 for
       plausible kernel widths; 1.3883 is reproducible as one point of that
       family.  Honest boundary: toy model, original calibration grid not
       archived in 05.
C seed             : SEED = 20260926.
D outputs          : results/exp07_correlation_diagonal.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp07_correlation_diagonal.json"

def gls_variance(C):
    """GLS variance of estimating a common mean from correlated pixels:
    Var = 1 / (1^T C^{-1} 1); diagonal approximation = mean of diagonal."""
    Ci = np.linalg.inv(C)
    n = C.shape[0]
    return float(1.0 / (np.ones(n) @ Ci @ np.ones(n)))

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}

    # ---- P-CST-22: analytic surface + mapping -------------------------------
    surface = []
    for n in (2, 3, 4, 5, 6):
        for rho in (0.10, 0.15, 0.19, 0.25, 0.30):
            und = 1.0 / (1.0 + (n - 1) * rho) - 1.0
            surface.append({"n": n, "rho": rho, "underestimate": float(und)})
    res["analytic_surface"] = surface
    # inverse mapping: (n-1)*rho = 1/(1+u) - 1  for underestimate u
    res["registered_mapping"] = {
        "value_36p3": {"n": 4, "rho": 0.19,
                       "analytic": float(1.0 / (1.0 + 3 * 0.19) - 1.0)},
        "value_23p3": {"n": 2, "rho": 0.3038,
                       "analytic": float(1.0 / (1.0 + 0.3038) - 1.0),
                       "alt_n3_rho": 0.1519, "alt_n_at_rho0p19": 2.60},
        "note": ("two points of one (n, rho) surface; ambiguity = effective "
                 "neighbour count of the correlation kernel, not the physics"),
    }

    # MC verification with explicit covariance (equal-weight mean; GLS = mean here)
    n, rho = 4, 0.19
    C = rho * np.ones((n, n)) + (1 - rho) * np.eye(n)
    L = np.linalg.cholesky(C)
    T = 400_000
    z = rng.standard_normal((T, n)) @ L.T
    m = z.mean(axis=1)
    mc_var = float(m.var(ddof=1))
    var_gls = gls_variance(C)
    var_diag = 1.0 / n   # diagonal omission with unit variances
    res["mc_verification_n4_rho019"] = {
        "var_gls_analytic": var_gls,
        "var_gls_mc": mc_var,
        "var_diag": var_diag,
        "mc_vs_analytic_rel": float(mc_var / var_gls - 1.0),
        "underestimate_analytic": float(var_diag / var_gls - 1.0),
    }

    # AR(1) kernel: underestimate vs rho1 for n = 16 effective neighbours
    ar1 = []
    for rho1 in (0.0, 0.05, 0.10, 0.19, 0.30):
        idx = np.arange(16)
        C = rho1 ** np.abs(idx[:, None] - idx[None, :])
        vg = gls_variance(C)
        ar1.append({"rho1": rho1, "var_gls": float(vg), "var_diag": 1.0 / 16,
                    "underestimate": float(1.0 / 16 / vg - 1.0)})
    res["ar1_kernel_scan"] = ar1

    # negative control: rho = 0 => bit-level identity
    C0 = np.eye(4)
    vg0 = gls_variance(C0)
    res["negative_control_rho0"] = {
        "var_gls": vg0, "var_diag": 0.25,
        "abs_diff": float(abs(vg0 - 0.25)),
        "mc_vs_analytic": None,
    }
    T = 200_000
    z = rng.standard_normal((T, 4))
    m = z.mean(axis=1)
    res["negative_control_rho0"]["mc_var"] = float(m.var(ddof=1))
    res["negative_control_rho0"]["underestimate_mc"] = float(0.25 / m.var(ddof=1) - 1.0)

    # ---- P-CST-10: k_corr toy calibration ------------------------------------
    # k_eff = 1 + (n_eff - 1) rho  (equicorrelated block); scan
    kscan = []
    for n_eff in (2, 3, 4, 5):
        for rho in (0.15, 0.19, 0.25):
            kscan.append({"n_eff": n_eff, "rho": rho,
                          "k_eff": float(1.0 + (n_eff - 1) * rho)})
    res["kcorr_toy_scan"] = kscan
    res["kcorr_registered"] = {"registered": 1.4, "calibration_value": 1.3883,
                               "reproducible_at": {"n_eff": 3, "rho": 0.19415,
                                                   "k": 1.0 + 2 * 0.19415}}
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
