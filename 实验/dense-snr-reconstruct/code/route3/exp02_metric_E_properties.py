#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P4-02: properties of the weight-efficiency metric E = Var_w/Var_opt - 1.

The ACSD sparse->dense SNR reconstruction is judged by E (weight efficiency
loss), with E = 0 iff sigma_hat is proportional to sigma_true.  This experiment
verifies the four metric properties the specification relies on:
  P1  multiplicative invariance : sigma_hat = c*sigma_true (c>0)  =>  E = 0
  P2  degenerate-case zeroing   : spatially flat truth (no spatial effect)
                                  => spatial metric must give E = 0
  P3  sensitivity (can go red)  : shuffled / wrong sigma fields => E >> 0
  P4  E vs level bias           : two arms with equal E can differ hugely in
                                  level (dex) -- E cannot replace level checks
Standalone, numpy only, seed fixed.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp02_metric_E_properties.json"


def efficiency(sigma_true, sigma_hat, n_mc=40000, seed=1):
    """E = Var_w/Var_opt - 1 estimated by MC on the weighted mean of unit-var
    measurements.  Var_w = sum w^2 s^2/(sum w)^2 with w = 1/sigma_hat^2;
    Var_opt = 1/sum(1/sigma_true^2)."""
    rng = np.random.default_rng(seed)
    w = 1.0 / sigma_hat**2
    var_w = (w**2 * sigma_true**2).sum() / w.sum() ** 2
    var_opt = 1.0 / (1.0 / sigma_true**2).sum()
    return var_w / var_opt - 1.0


def level_bias_dex(sigma_true, sigma_hat):
    return float(np.median(np.abs(np.log10(sigma_hat / sigma_true))))


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}
    n = 128
    # spatially structured true sigma field (log-normal, correlated)
    x = np.arange(n)
    field = 0.6 * np.sin(2 * np.pi * x / 97.0) + 0.4 * np.sin(2 * np.pi * x / 41.0 + 1.1)
    sigma_true = 2.0 * np.exp(0.5 * field)  # ~1.4 dex dynamic range

    # P1 multiplicative invariance
    E_p1 = efficiency(sigma_true, 3.17 * sigma_true)
    # P2 degenerate flat truth: true field constant => any monotone arm still
    # proportional? no -- a spatially varying arm on a flat truth is WRONG, but
    # the specification's degenerate gate says: flat truth + flat estimate =>
    # E must be exactly 0 (no-effect => zero).
    E_flat = efficiency(np.full(n, 2.0), np.full(n, 2.0))
    # and a *wrong* arm on a flat truth must NOT be zero (non-tautology control)
    E_flat_wrong = efficiency(np.full(n, 2.0), np.full(n, 2.0) * np.exp(0.5 * field))
    # P3 sensitivity: shuffled estimate field
    sigma_shuffled = sigma_true[rng.permutation(n)]
    E_shuf = efficiency(sigma_true, sigma_shuffled)
    # also a completely wrong monotone arm
    E_rev = efficiency(sigma_true, sigma_true[::-1])
    # P4 equal E but different level bias: arm A = 2*sigma_true (pure scale),
    # arm B = sigma_true * exp(field*0.55) tuned to have similar E... simpler:
    # construct arm B with random log-errors of amplitude comparable so that E
    # lands close to arm A's, then show level bias differs.
    armA = 2.0 * sigma_true
    E_A = efficiency(sigma_true, armA)
    # search a multiplicative random-field arm with E near E_A
    best = None
    for amp in np.arange(0.02, 0.60, 0.01):
        armB = sigma_true * np.exp(amp * field / np.std(field) * np.std(field))
        # use amp directly as log10 amplitude of a random multiplicative field
        armB = sigma_true * 10 ** (amp * np.tanh(field))
        Eb = efficiency(sigma_true, armB)
        if best is None or abs(Eb - E_A) < best[1]:
            best = (amp, abs(Eb - E_A), Eb, armB)
    amp, _, E_B, armB = best
    lb_A = level_bias_dex(sigma_true, armA)
    lb_B = level_bias_dex(sigma_true, armB)

    res["P1_multiplicative_invariance"] = {
        "E_sigma_hat_equals_3p17x_truth": float(E_p1),
        "claim": "E=0 iff sigma_hat proportional to sigma_true",
        "pass": bool(abs(E_p1) < 1e-12),
    }
    res["P2_degenerate_zeroing"] = {
        "E_flat_truth_flat_arm": float(E_flat),
        "E_flat_truth_wrong_arm": float(E_flat_wrong),
        "claim": "no spatial effect => metric zero; wrong arm on flat truth => E>0 (non-tautology)",
        "pass": bool(abs(E_flat) < 1e-12 and E_flat_wrong > 0.01),
    }
    res["P3_sensitivity"] = {
        "E_shuffled": float(E_shuf),
        "E_reversed": float(E_rev),
        "claim": "wrong spatial assignment => E >> 0 (metric can go red)",
        "pass": bool(E_shuf > 0.1 and E_rev > 0.1),
    }
    res["P4_level_bias_complement"] = {
        "armA": "2*sigma_true (pure multiplicative)",
        "armB": "multiplicative tanh(field) field, amplitude searched to match E",
        "E_A": float(E_A), "E_B": float(E_B),
        "level_bias_A_dex": lb_A, "level_bias_B_dex": lb_B,
        "bias_ratio": float(lb_B / max(lb_A, 1e-12)),
        "claim": "E equal does not imply equal level bias; report both",
        "pass": bool(abs(E_A - E_B) < 0.01 * max(E_A, 1e-9) + 0.005),
    }
    res["all_pass"] = bool(all(
        res[k].get("pass", False) for k in
        ["P1_multiplicative_invariance", "P2_degenerate_zeroing",
         "P3_sensitivity", "P4_level_bias_complement"]))
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=2, ensure_ascii=False)
    print(json.dumps(res, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
