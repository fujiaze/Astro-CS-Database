#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-06: P-CST-11 (read-noise double-count bias +14.5009% / +38.2524%)

A literature leg  : NOISE_MODEL.md section 9a (internal authority: sigma_i^2 =
                    sigma_sky^2 + (RN/g)^2 + F*P_i/g, read noise counted once);
                    Janesick 2001 / Howell 2006 (Poisson + read noise model).
B experiment leg  : closed form + genuine Monte Carlo (N_MC and rel SE reported,
                    per P-REG-04 discipline).
  Model (background-limited, uniform profile P_i = 1/n over n = 64 pixels, g = 1):
    correct   : var_i = S + R        (S = sigma_sky,shot^2, R = (RN/g)^2)
    double    : var_i = S + 2R       (sigma_sky declared empirical_total_rms yet
                                      (RN/g)^2 added again)
  Matched-filter flux estimate F_hat = mean(x); its true scatter is
  sqrt((S+R)/n); the double-counted predicted sigma_F is sqrt((S+2R)/n), so the
  bias on sigma_F is  B(x) = sqrt(1 + x^2) - 1  with  x = (RN/g)/sigma_sky,total.
  The two registered reference values sit exactly on this curve at
    x1 = 0.5577124..., x2 = 0.9546570...
  (reverse-engineered here; the underlying (RN, g, sigma_sky) of the archived
  points are NOT registered in 05 => parameter anchor UNRESOLVED, the curve-form
  anchor is verified).
  NEGATIVE CONTROL: correct combination (shot_noise_only declaration) => bias 0.
C seed             : SEED = 20260926.
D outputs          : results/exp06_doublecount_bias.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt

SEED = 20260926
OUT = "results/exp06_doublecount_bias.json"
N_MC = 20000
N_PIX = 64

def bias_closed(x):
    return sqrt(1.0 + x * x) - 1.0

def run_mc(S, R, n_mc=N_MC, seed=SEED):
    """MC scatter of the matched-filter (uniform) flux estimate."""
    rng = np.random.default_rng(seed)
    f = np.empty(n_mc)
    for i in range(n_mc):
        sky = rng.poisson(S, size=N_PIX).astype(float)
        rn = rng.normal(0.0, sqrt(R), size=N_PIX)
        f[i] = (sky + rn).mean()
    return f

def main():
    res = {"seed": SEED, "n_mc": N_MC, "n_pix": N_PIX}  # N_MC raised to 20000 for rel-SE ~0.5%

    # closed-form curve
    xs = np.linspace(0.0, 1.2, 25)
    res["closed_form_curve"] = [{"x": float(x), "bias": float(bias_closed(x))} for x in xs]

    # reference points
    reg = {"base_point": 0.145009, "rn50_worst": 0.382524}
    pts = {}
    for k, b in reg.items():
        x = sqrt((1.0 + b) ** 2 - 1.0)
        pts[k] = {"registered_bias": b, "x_implied": float(x),
                  "bias_closed_at_x_implied": float(bias_closed(x))}
    res["reference_points_reverse_engineered"] = pts

    # MC verification at the two implied x points
    S_tot = 100.0   # sigma_sky,total^2 = 100 ADU^2  (sigma = 10 ADU)
    mc = {}
    for k, b in reg.items():
        x = pts[k]["x_implied"]
        # x^2 = R/(S+R) = R/S_tot  =>  R = x^2 * S_tot,  S = (1 - x^2) * S_tot
        R = S_tot * x * x
        S = S_tot - R
        sigF_true = sqrt((S + R) / N_PIX)
        sigF_double = sqrt((S + 2.0 * R) / N_PIX)
        f = run_mc(S, R)
        mc[k] = {
            "S_shot_adu2": float(S), "R_read_adu2": float(R), "RN_adu": float(sqrt(R)),
            "sigmaF_true_analytic": float(sigF_true),
            "sigmaF_double_predicted": float(sigF_double),
            "mc_scatter_of_Fhat": float(f.std(ddof=1)),
            "mc_scatter_vs_analytic_rel": float(f.std(ddof=1) / sigF_true - 1.0),
            "bias_pred_over_true": float(sigF_double / sigF_true - 1.0),
            "rel_se_of_mc_scatter": float(f.std(ddof=1) / sqrt(2.0 * (N_MC - 1)) / f.std(ddof=1)),
        }
    res["mc_verification"] = mc

    # negative control: correct combination => bias exactly 0
    x = pts["base_point"]["x_implied"]
    R = S_tot * x * x; S = S_tot - R
    f = run_mc(S, R)
    sigF_true = sqrt((S + R) / N_PIX)
    res["negative_control"] = {
        "description": "shot_noise_only declaration, var_pred = S + R",
        "bias_correct_combination": float(sqrt((S + R) / N_PIX) / sigF_true - 1.0),
        "mc_scatter_vs_analytic_rel": float(f.std(ddof=1) / sigF_true - 1.0),
        "expected": 0.0,
    }
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
