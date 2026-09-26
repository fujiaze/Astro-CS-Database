#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-02: P-CST-03 / P-CST-08 / P-CST-09 (MAD->sigma scale, sky budget, 5-sigma clip)

A literature leg  : Rousseeuw & Croux 1993, JASA 88, 1273 (MAD asymptotics,
                    efficiency ~37%); Huber 1981, Robust Statistics.
B experiment leg  : MC with fixed seed.
  (03) kappa = 1.482602218505602 recovery: relative bias of kappa*MAD at
       N = 64 and N = 9216; asymptotic SE constant zeta = sqrt(N)*SE(sigma_hat/sigma)
       compared with (a) repo constant 1.152, (b) quantile-derivation
       zeta_q = 1/(2*0.674490*f_norm(0.674490))... computed from first principles,
       (c) efficiency vs sample sd (literature ~37%).
  (08) sky budget: n_min = (zeta*eff_median/eps_target)^2 with eps=0.015,
       eff_median = 1.25 => repo 9216; measured zeta => measured n_min;
       direct MC check of SE(sigma_hat)/sigma at N = 9216 vs 1.44/sqrt(N).
  (09) 5-sigma clip (<=2 rounds) bias at N = 64 and N = 9216 (~0 expected);
       NEGATIVE CONTROL: 2-sigma clip must produce a large negative bias.
  Negative control (both 03/08): constant input => sigma_hat = 0 exactly
       (true-zero-effect => metric zero).
C seed             : SEED = 20260926.
D outputs          : results/exp02_robust_scale_mad.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt

SEED = 20260926
KAPPA = 1.482602218505602        # P-CST-03
EFF_MEDIAN = 1.25                # median efficiency factor (P-CST-08)
EPS_TARGET = 0.015
REPO_ZETA = 1.152
OUT = "results/exp02_robust_scale_mad.json"
SIGMA = 5.0

def sigma_hat_mad(x, clip=None, rounds=2):
    med = np.median(x)
    mad = np.median(np.abs(x - med))
    s = KAPPA * mad
    if clip is not None:
        for _ in range(rounds):
            if s <= 0:
                break
            keep = np.abs(x - med) <= clip * s
            if keep.all():
                break
            x = x[keep]
            med = np.median(x)
            mad = np.median(np.abs(x - med))
            s = KAPPA * mad
    return s

def zeta_quantile_derivation():
    # MAD ~ median of |X| (symmetric, large N). Quantile estimator asymptotics:
    # Var(q_p) = p(1-p) / (n f(q_p)^2), p = 1/2, on the |X| (half-normal) density.
    from math import pi, exp
    xp = 0.6744897501960817                      # Phi^{-1}(3/4)
    phi = exp(-xp * xp / 2.0) / sqrt(2.0 * pi)   # standard normal pdf at xp
    f_abs = 2.0 * phi                            # half-normal density at its median
    var_mad = 0.25 / f_abs ** 2                  # n * Var(MAD) / sigma^2
    return sqrt(var_mad) / xp                    # zeta for sigma_hat = MAD/xp

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "sigma_true": SIGMA}

    # ---- P-CST-03: bias & SE constant --------------------------------------
    for N, T in ((64, 200_000), (9216, 20_000)):
        est = np.empty(T)
        for i in range(T):
            x = rng.normal(0.0, SIGMA, size=N)
            est[i] = sigma_hat_mad(x)
        rel = est / SIGMA
        res[f"N{N}"] = {
            "trials": T,
            "rel_bias": float(np.mean(rel) - 1.0),
            "rel_se": float(np.std(rel, ddof=1)),
            "zeta_measured_sqrtN": float(np.std(rel, ddof=1) * sqrt(N)),
            "rel_se_of_zeta": float(np.std(rel, ddof=1) * sqrt(N) / sqrt(2.0 * T)),
        }
    res["zeta_repo"] = REPO_ZETA
    res["zeta_quantile_derivation"] = float(zeta_quantile_derivation())

    # efficiency vs sample sd (Gaussian): sd should be ~sqrt(2)/sqrt(N) better
    N = 4096; T = 20_000
    e_mad = np.empty(T); e_sd = np.empty(T)
    for i in range(T):
        x = rng.normal(0.0, SIGMA, size=N)
        e_mad[i] = sigma_hat_mad(x)
        e_sd[i] = np.std(x, ddof=1)
    v_mad = np.var(e_mad / SIGMA, ddof=1)
    v_sd = np.var(e_sd / SIGMA, ddof=1)
    res["efficiency_mad_vs_sd"] = float(v_sd / v_mad)   # literature ~0.37
    res["efficiency_reference"] = 0.37

    # ---- P-CST-08: budget ----------------------------------------------------
    n_repo = (REPO_ZETA * EFF_MEDIAN / EPS_TARGET) ** 2
    zeta_m = res["N9216"]["zeta_measured_sqrtN"]
    n_meas = (zeta_m * EFF_MEDIAN / EPS_TARGET) ** 2
    res["budget"] = {
        "n_min_repo": float(n_repo),
        "n_min_measured": float(n_meas),
        "se_over_sigma_at_9216_measured": float(res["N9216"]["rel_se"]),
        "se_over_sigma_repo_formula": float(EFF_MEDIAN * REPO_ZETA / sqrt(9216)),
        "budget_ratio_measured_over_repo": float(n_meas / n_repo),
    }

    # ---- P-CST-09: clip bias --------------------------------------------------
    for N, T in ((64, 200_000), (9216, 20_000)):
        out = {}
        for label, clip in (("clip5", 5.0), ("clip2_negative_control", 2.0), ("noclip", None)):
            est = np.empty(T)
            for i in range(T):
                x = rng.normal(0.0, SIGMA, size=N)
                est[i] = sigma_hat_mad(x, clip=clip)
            out[label] = float(np.mean(est / SIGMA) - 1.0)
        res[f"clip_bias_N{N}"] = out

    # ---- negative control: constant input -------------------------------------
    const = np.full(9216, 3.7)
    res["constant_input_sigma_hat"] = float(sigma_hat_mad(const))

    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
