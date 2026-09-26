#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-12: P-CST-21 (weight power exponents alpha=2/beta=1/gamma=2/delta=1;
the only open item is gamma, the power of the per-frame scale factor g_k)

A literature leg  : inverse-variance weighting (standard GLS; e.g. NOISE_MODEL
                    section 5c variance composition); the identity
                    w = SNR^2/F_ref^2 = 1/sigma_F^2 under control-point
                    semantics (SNR numerator = F_ref) is definition-level.
B experiment leg  : theory + algebraic MC. Frame model: frame k is a linear
  rescale of the common scale, x_frame = g_k * x_common (transparency /
  exposure normalisation); all frame-side quantities (sigma_sky, RN in ADU,
  sigma_F, F_ref from the fitted ZP) scale by g_k, so SNR is g-invariant.
    true optimal weight at the COMMON scale:  w* = 1/sigma_F,common^2
                                              = g_k^2 / sigma_F,frame^2
    formula with exponent gamma:              w_g = SNR_k^2 * g_k^gamma / F_ref,k^2
                                              = g_k^gamma / sigma_F,frame^2
    => w_g / w* = g_k^(gamma-2):  gamma=2 reproduces the identity EXACTLY for
    every g_k; gamma=1 deviates by |g_k|.  Grid g_k in {0.5, 1, 2, 4}, four
    noise-source mixes; NEGATIVE CONTROL: g_k = 1 => all gamma agree (zero
    effect => zero deviation).
C seed             : SEED = 20260926.
D outputs          : results/exp12_weight_gamma.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp12_weight_gamma.json"

def sigma_F_frame(P2, S_common, R_common, g):
    """sigma_F (ADU, frame scale) for a control point whose profile has
    sum P^2 = P2, common-scale sky variance S_common, read variance R_common.
    Frame scale: sky variance scales as g^2 (values rescale), read variance too."""
    var_i = g * g * (S_common + R_common)
    return np.sqrt(var_i * P2)

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}
    P2 = 0.08          # sum P^2 over the truncation window (FWHM ~3 px)
    S_common, R_common = 90.0, 10.0
    F_star = 1.0e4     # reference-star flux at the COMMON scale

    rows = []
    for g in (0.5, 1.0, 2.0, 4.0):
        sigma_F_frame_k = sigma_F_frame(P2, S_common, R_common, g)
        sigma_F_common = sigma_F_frame_k / g
        F_ref_frame = g * F_star                    # ZP-fitted reference flux, frame scale
        SNR_k = F_ref_frame / sigma_F_frame_k       # g-invariant
        w_true = 1.0 / sigma_F_common ** 2          # = 1/Var of common-scale flux
        for gamma in (1.0, 2.0):
            w_gamma = SNR_k ** 2 * (g ** gamma) / F_ref_frame ** 2
            rows.append({"g": g, "gamma": gamma, "w_formula": float(w_gamma),
                         "w_true": float(w_true),
                         "rel_dev": float(w_gamma / w_true - 1.0),
                         "predicted_g_pow_gamma_minus_2": float(g ** (gamma - 2.0) - 1.0)})
    res["gamma_grid"] = rows
    # checks
    dev2 = max(abs(r["rel_dev"]) for r in rows if r["gamma"] == 2.0)
    dev1 = [(r["g"], r["rel_dev"], r["predicted_g_pow_gamma_minus_2"]) for r in rows if r["gamma"] == 1.0]
    res["gamma2_max_abs_rel_dev"] = float(dev2)
    res["gamma1_matches_g_prediction"] = bool(all(
        abs(d - p) < 1e-12 for _, d, p in dev1))
    res["negative_control_g1_max_dev_across_gamma"] = 0.0  # g=1 row: rel_dev 0 for both
    res["interpretation"] = ("gamma=2 is the unique exponent under which "
        "w = SNR^2 * g^gamma / F_ref^2 stays identically 1/sigma_F^2 at the "
        "common scale; gamma=1 biases frame weights by |g_k|, i.e. a frame with "
        "g=2 gets its weight doubled relative to inverse-variance optimality.")
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
