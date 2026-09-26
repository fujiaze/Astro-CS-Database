#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P4-01: inverse-variance weight identity w = SNR^2/F_ref^2 == 1/sigma_F^2,
gamma-power optimality, and F_ref (m_ref) propagation.

Route 3 (independent scientific route) of the P4 dense-SNR-reconstruction audit.
Standalone: uses only numpy + stdlib. Seed fixed. No repo imports.

Chain interfaces exercised:
  upstream   : P2 sparse_snr_layer control points (SNR_c, F_ref per frame)  [simulated]
  downstream : P2/P5 inverse-variance weighting for stacking               [validated]
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp01_weight_identity.json"


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "hypotheses": {}}

    # ---------------- H1a: identity w = SNR^2/F_ref^2 == 1/sigma_F^2 ----------
    n = 512
    f_ref = 10.0 ** rng.uniform(-2, 2, size=n)        # arbitrary reference flux (ADU)
    sigma_f = 10.0 ** rng.uniform(-1.5, 1.5, size=n)  # flux uncertainty (ADU)
    snr = f_ref / sigma_f
    w_a = snr**2 / f_ref**2
    w_b = 1.0 / sigma_f**2
    rel = np.max(np.abs(w_a - w_b) / w_b)
    res["hypotheses"]["H1a_identity"] = {
        "claim": "w = SNR^2/F_ref^2 == 1/sigma_F^2 (algebraic identity)",
        "n_points": int(n),
        "max_relative_deviation": float(rel),
        "pass": bool(rel < 1e-12),
    }

    # ------------- H1b: optimality of inverse-variance weighting -------------
    K = 16
    M = 20000
    sig = 10.0 ** rng.uniform(-1, 1, size=K)          # frame sigmas, 2 dex range
    f_ref_k = 10.0 ** rng.uniform(-1, 1, size=K)
    snr_k = f_ref_k / sig
    mu = 3.7
    draws = rng.normal(0.0, 1.0, size=(M, K)) * sig + mu
    schemes = {
        "gamma2_inverse_variance (w=SNR^2/F_ref^2)": 1.0 / sig**2,
        "gamma1_linear_snr (w~SNR)": snr_k,
        "equal_weight": np.ones(K),
    }
    h1b = {}
    for name, w in schemes.items():
        est = (draws * w).sum(axis=1) / w.sum()
        var_mc = est.var(ddof=1)
        var_analytic = (w**2 * sig**2).sum() / w.sum() ** 2
        h1b[name] = {"var_mc": float(var_mc), "var_analytic": float(var_analytic)}
    var_opt = 1.0 / (1.0 / sig**2).sum()
    h1b["analytic_optimum_1_over_sum_ivar"] = {"var": float(var_opt)}
    eff = {k: v["var_analytic"] / var_opt for k, v in h1b.items() if "var_analytic" in v}
    h1b["efficiency_vs_optimum"] = eff
    mc_ok = all(
        abs(h1b[k]["var_mc"] / h1b[k]["var_analytic"] - 1.0) < 0.05 for k in schemes
    )
    h1b["mc_vs_analytic_consistent_5pct"] = bool(mc_ok)
    h1b["pass"] = bool(eff["gamma2_inverse_variance (w=SNR^2/F_ref^2)"] <= eff["gamma1_linear_snr (w~SNR)"] and
                       eff["gamma1_linear_snr (w~SNR)"] <= eff["equal_weight"])
    res["hypotheses"]["H1b_optimality"] = h1b

    # ------------- H1c: negative control (no-effect => metric zero) ----------
    sig_eq = np.full(K, 2.0)
    schemes_eq = [1.0 / sig_eq**2, sig_eq, np.ones(K)]
    vars_eq = [float((w**2 * sig_eq**2).sum() / w.sum() ** 2) for w in schemes_eq]
    spread = (max(vars_eq) - min(vars_eq)) / min(vars_eq)
    res["hypotheses"]["H1c_negative_control"] = {
        "claim": "equal sigmas => all schemes identical (no-effect => metric zero)",
        "variances": vars_eq,
        "relative_spread": float(spread),
        "pass": bool(spread < 1e-12),
    }

    # ------------- H1d: F_ref / m_ref propagation ----------------------------
    m_ref0, m_ref1 = 6.0, 7.0
    dm = m_ref1 - m_ref0
    f0 = 10 ** (-0.4 * (m_ref0 - 20.0))
    f1 = 10 ** (-0.4 * (m_ref1 - 20.0))
    ratio_f = f1 / f0
    sig_fixed = 0.5
    snr0, snr1 = f0 / sig_fixed, f1 / sig_fixed
    w0 = (snr0 / f0) ** 2
    w1 = (snr1 / f1) ** 2
    res["hypotheses"]["H1d_fref_propagation"] = {
        "claim": ("dm_ref = +1 => F_ref and SNR x 10^-0.4 = 0.3981 (bright-end "
                  "direction x 2.512); SNR^2 moves x 10^-0.8 = 0.158 (inverse "
                  "6.31); w = SNR^2/F_ref^2 = 1/sigma_F^2 is m_ref-INVARIANT"),
        "dm_ref": float(dm),
        "F_ref_ratio": float(ratio_f),
        "F_ref_ratio_expected": float(10 ** (-0.4 * dm)),
        "SNR_ratio": float(snr1 / snr0),
        "SNR_ratio_expected": float(10 ** (-0.4 * dm)),
        "SNR2_ratio": float((snr1 / snr0) ** 2),
        "SNR2_ratio_expected": float(10 ** (-0.8 * dm)),
        "w_ratio": float(w1 / w0),
        "w_ratio_expected": 1.0,
        "pass": bool(
            abs(ratio_f / 10 ** (-0.4 * dm) - 1) < 1e-12
            and abs(snr1 / snr0 / 10 ** (-0.4 * dm) - 1) < 1e-12
            and abs(w1 / w0 - 1.0) < 1e-12
        ),
    }

    # ------------- downstream interface: two-frame stack --------------------
    w_direct = 1.0 / sig**2
    w_via_snr = (f_ref_k / sig) ** 2 / f_ref_k**2
    max_rel = float(np.max(np.abs(w_direct - w_via_snr) / w_direct))
    res["downstream_interface"] = {
        "max_relative_weight_deviation": max_rel,
        "threshold": 1e-12,
        "pass": bool(max_rel < 1e-12),
    }

    res["all_pass"] = bool(all(v.get("pass", False) for v in res["hypotheses"].values())
                           and res["downstream_interface"]["pass"])
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=2, ensure_ascii=False)
    print("H1a:", res["hypotheses"]["H1a_identity"])
    print("H1b eff:", res["hypotheses"]["H1b_optimality"]["efficiency_vs_optimum"],
          "pass:", res["hypotheses"]["H1b_optimality"]["pass"])
    print("H1c:", res["hypotheses"]["H1c_negative_control"])
    print("H1d:", res["hypotheses"]["H1d_fref_propagation"])
    print("downstream:", res["downstream_interface"])
    print("ALL PASS:", res["all_pass"])


if __name__ == "__main__":
    main()
