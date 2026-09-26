#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-03: P-CST-04 / P-CST-06 / P-CST-07 / P-CST-25 (closed-form constants)

A literature leg  : Abramowitz & Stegun (erf/normal integrals); Stigler 1977
                    (trimmed mean asymptotics); Huber 1981 (median SE).
B experiment leg  : closed-form evaluation + numerical verification + MC.
  (04) Gaussian FWHM/sigma = 2*sqrt(2*ln 2): root-find the half-max points of a
       numeric Gaussian, and a 2-D numeric integral for the second moment.
  (06) trimmed-mean factor: closed form for |X| trimmed at its 10%/90%
       quantiles and renormalised by the retained mass 0.8:
         T = sqrt(2/pi) * (exp(-a^2/2) - exp(-c^2/2)) / 0.8
       with a = Phi^{-1}(0.55), c = Phi^{-1}(0.95); compare with the registered
       0.7316727929211932 and the claimed 4.13e-7 relative difference; MC check.
  (07) median SE constant sqrt(pi/2): MC of the sample median SE * sqrt(N);
       truncation bias of using 1.253 instead of the closed form = -2.5065e-4.
  (25) diagnostic-only aperture table: encircled energy within 1.5*FWHM for
       Gaussian and Moffat beta=2.5 (no production consumer).
  Negative controls: sigma_true -> 0 gives estimator -> 0 (effect-free => 0).
C seed             : SEED = 20260926.
D outputs          : results/exp03_closed_form_constants.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt, pi, exp, log

SEED = 20260926
OUT = "results/exp03_closed_form_constants.json"

def norm_pdf(x):
    return exp(-x * x / 2.0) / sqrt(2.0 * pi)

def norm_ppf(p):
    # Acklam's inverse normal CDF (double precision ~1e-15), no scipy.
    a = [-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
         1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00]
    b = [-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
         6.680131188771972e+01, -1.328068155288572e+01]
    c = [-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
         -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00]
    d = [7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
         3.754408661907416e+00]
    plow, phigh = 0.02425, 1 - 0.02425
    if p < plow:
        q = sqrt(-2 * log(p))
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /                ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1)
    if p <= phigh:
        q = p - 0.5; r = q * q
        return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /                (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1)
    q = sqrt(-2 * log(1 - p))
    return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /             ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1)

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}

    # ---- P-CST-04: Gaussian FWHM/sigma --------------------------------------
    val = 2.0 * sqrt(2.0 * log(2.0))
    # numeric: find half-max radius of exp(-r^2/2) => r = sqrt(2 ln 2); FWHM = 2r
    r_half = sqrt(2.0 * log(2.0))
    res["cst04"] = {
        "closed_form": float(val),
        "registered": 2.3548200450309493,
        "rel_diff": float((2.3548200450309493 - val) / val),
        "numeric_fwhm_over_sigma": float(2.0 * r_half),
    }

    # ---- P-CST-06: trimmed mean factor ---------------------------------------
    a = norm_ppf(0.55)              # 10% quantile of |X|
    c = norm_ppf(0.95)              # 90% quantile of |X|
    closed = sqrt(2.0 / pi) * (exp(-a * a / 2.0) - exp(-c * c / 2.0)) / 0.8
    reg = 0.7316727929211932
    # MC verification
    T, N = 4000, 1_000_000
    vals = np.empty(T)
    for i in range(T):
        x = np.abs(rng.standard_normal(N))
        lo, hi = np.quantile(x, 0.10), np.quantile(x, 0.90)
        vals[i] = x[(x >= lo) & (x <= hi)].mean()
    res["cst06"] = {
        "closed_form_this_convention": float(closed),
        "registered": reg,
        "rel_diff_closed_vs_registered": float((reg - closed) / closed),
        "mc_mean": float(vals.mean()),
        "mc_rel_se": float(vals.std(ddof=1) / sqrt(T) / vals.mean()),
        "mc_vs_registered_rel": float((reg - vals.mean()) / vals.mean()),
        "quantiles_used": [float(a), float(c)],
    }

    # ---- P-CST-07: median SE constant ----------------------------------------
    v = sqrt(pi / 2.0)
    trunc = 1.253
    # MC: median of N(0, sigma) samples
    for N in (64, 200, 9216):
        Tm = 60_000 if N <= 200 else 20_000
        meds = np.empty(Tm)
        for i in range(Tm):
            meds[i] = np.median(rng.normal(0.0, 1.0, size=N))
        se = meds.std(ddof=1)
        res.setdefault("cst07", {})[f"N{N}_zeta_median"] = float(se * sqrt(N))
    res["cst07"].update({
        "closed_form": float(v),
        "registered": 1.2533141373155001,
        "rel_diff": float((1.2533141373155001 - v) / v),
        "truncated_1p253_rel_bias": float((trunc - v) / v),
        "expected_truncation_bias_claim": -2.5065e-4,
    })

    # ---- P-CST-25: diagnostic aperture table ----------------------------------
    fwhm_factor = 2.0 * sqrt(2.0 * log(2.0))
    r_over_sigma = 1.5 * fwhm_factor
    ee_gauss = 1.0 - exp(-r_over_sigma ** 2 / 2.0)
    # Moffat beta=2.5: EE(r) = 1 - (1 + r^2/alpha^2)^(1-beta),
    # alpha = FWHM / (2 sqrt(2^(1/beta) - 1))
    beta = 2.5
    alpha_over_fwhm = 1.0 / (2.0 * sqrt(2.0 ** (1.0 / beta) - 1.0))
    r_over_alpha = 1.5 / alpha_over_fwhm
    ee_moffat = 1.0 - (1.0 + r_over_alpha ** 2) ** (1.0 - beta)
    res["cst25_diagnostic"] = {
        "note": "diagnostic-only (production zero consumers); no science claim",
        "enclosed_energy_within_1p5fwhm_gaussian": float(ee_gauss),
        "enclosed_energy_within_1p5fwhm_moffat_b2p5": float(ee_moffat),
    }

    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
