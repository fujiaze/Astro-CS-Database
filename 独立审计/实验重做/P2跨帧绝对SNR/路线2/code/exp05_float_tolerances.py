#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-05: P-CST-12 / P-CST-13 / P-CST-20 (float-level tolerances)

A literature leg  : IEEE 754 double precision (eps = 2^-52); Goldberg 1991,
                    "What Every Computer Scientist Should Know About Floating-
                    Point Arithmetic", ACM Computing Surveys 23, 5.
B experiment leg  :
  (13) weight-identity gate 2.22e-16: w = SNR_c^2/F_ref^2 vs 1/sigma_F^2 over
       1e6 random draws in two evaluation orders; report max/99.99% rel dev.
       NEGATIVE CONTROL: inject 1e-15 relative perturbation => gate trips.
  (12) node-reproduction tolerance 1e-9: bicubic-like separable evaluation of a
       random control grid at its own nodes, two summation orders => max abs
       diff ~1e-16 << 1e-9.  Demonstrate blindness: perturb ALL node values by
       x(1+1e-10) (a systematic scale error) => self-check still ~0, i.e. the
       1e-9 gate catches bit-level differences only, not model errors (V-5).
  (20) PSF profile normalisation |sum P - 1| <= 1e-9: truncated Moffat beta=4
       and Gaussian profiles renormalised in float64 (dev ~1e-16, passes) and
       in float32 (dev ~1e-7, trips) => the tolerance is meaningful only under
       the FP64 evaluation rule.  Negative control: float32 must trip.
C seed             : SEED = 20260926.
D outputs          : results/exp05_float_tolerances.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt, log

SEED = 20260926
OUT = "results/exp05_float_tolerances.json"

def bicubic_catmull(p0, p1, p2, p3, t):
    # Catmull-Rom cubic through 4 nodes, separable use
    return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 +
          t * (3.0 * (p1 - p2) + p3 - p0)))

def eval_grid_path_a(g, i, j):
    """evaluate exactly at node (i,j) via the separable path (y then x).
    Catmull-Rom passes through its nodes p1 (t=0) and p2 (t=1); to land on
    node (i,j) with t=1 the stencil must be (i-2..i+1) x (j-2..j+1)."""
    ny, nx = g.shape
    row = np.empty(4)
    for d in range(4):
        jj = min(max(j - 2 + d, 0), nx - 1)
        col = np.empty(4)
        for e in range(4):
            ii = min(max(i - 2 + e, 0), ny - 1)
            col[e] = g[ii, jj]
        row[d] = bicubic_catmull(col[0], col[1], col[2], col[3], 1.0)  # t=1 -> p2 = node
    return bicubic_catmull(row[0], row[1], row[2], row[3], 1.0)

def eval_grid_path_b(g, i, j):
    """direct lookup path."""
    return g[i, j]

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}

    # ---- P-CST-13 ------------------------------------------------------------
    eps = float(np.finfo(np.float64).eps)
    n = 1_000_000
    F_ref = 10.0 ** rng.uniform(0.5, 2.0, size=n)      # ADU
    sigma_F = 10.0 ** rng.uniform(-1.0, 1.5, size=n)   # ADU
    # control-point semantics: SNR_c = F_ref / sigma_F
    SNR = F_ref / sigma_F
    w1 = SNR ** 2 / F_ref ** 2          # spec order
    w2 = (F_ref / sigma_F / F_ref) ** 2 # algebraically identical, different order
    w3 = 1.0 / sigma_F ** 2             # inverse-variance target
    dev_a = np.abs(w1 / w3 - 1.0)
    dev_b = np.abs(w2 / w3 - 1.0)
    res["cst13"] = {
        "eps_double": eps,
        "max_rel_dev_order1": float(dev_a.max()),
        "max_rel_dev_order2": float(dev_b.max()),
        "p9999_rel_dev": float(np.quantile(dev_a, 0.9999)),
        "gate": 2.22e-16,
        "measured_repo_claim": 3.01e-16,
        "gate_exceeded_fraction_order1": float(np.mean(dev_a > 2.22e-16)),
    }
    # negative control: 1e-15 relative perturbation must trip the gate
    w3p = w3 * (1.0 + 1e-15)
    res["cst13_negative_control_max_rel_dev"] = float(np.abs(w1 / w3p - 1.0).max())

    # ---- P-CST-12 -------------------------------------------------------------
    ny, nx = 32, 32
    g = rng.uniform(5.0, 80.0, size=(ny, nx))
    diffs = []
    for i in range(2, ny - 2):
        for j in range(2, nx - 2):
            a = eval_grid_path_a(g, i, j)
            b = eval_grid_path_b(g, i, j)
            diffs.append(abs(a - b))
    res["cst12"] = {
        "node_selfcheck_max_abs_two_orders": float(np.max(diffs)),
        "tolerance": 1e-9,
    }
    g2 = g * (1.0 + 1e-10)  # systematic scale error: invisible to the self-check
    diffs2 = []
    for i in range(2, ny - 2):
        for j in range(2, nx - 2):
            diffs2.append(abs(eval_grid_path_a(g2, i, j) - eval_grid_path_b(g2, i, j)))
    res["cst12"]["node_selfcheck_after_1e-10_scale_error"] = float(np.max(diffs2))
    res["cst12"]["true_relative_scale_error"] = 1e-10
    res["cst12"]["interpretation"] = ("gate catches bit-level/order differences only; "
        "a 1e-10 systematic scale error passes the self-check (V-5: no evidence value)")

    # ---- P-CST-20 --------------------------------------------------------------
    def profile_stack(fwhm, beta, half):
        sig = fwhm / (2.0 * sqrt(2.0 * log(2.0)))
        h = np.arange(-half, half + 1, dtype=np.float64)
        xx, yy = np.meshgrid(h, h)
        r2 = xx ** 2 + yy ** 2
        if beta is None:      # Gaussian
            P = np.exp(-r2 / (2.0 * sig ** 2))
        else:                 # Moffat, alpha = 1 px scale chosen so FWHM matches
            alpha = fwhm / (2.0 * np.sqrt(2.0 ** (1.0 / beta) - 1.0))
            P = (1.0 + r2 / alpha ** 2) ** (-beta)
        return P
    out20 = {}
    for name, fwhm, beta, half in (("gauss_fwhm3_h36", 3.0, None, 36),
                                   ("moffat4_fwhm3_h36", 3.0, 4.0, 36),
                                   ("moffat4_fwhm60_h720", 60.0, 4.0, 720)):
        P = profile_stack(fwhm, beta, half)
        P64 = P / P.sum()
        P32 = (P.astype(np.float32) / np.float32(P.sum())).astype(np.float64)
        out20[name] = {
            "abs_sumP_minus_1_float64": float(abs(P64.sum() - 1.0)),
            "abs_sumP_minus_1_float32": float(abs(P32.sum() - 1.0)),
            "passes_1e-9_float64": bool(abs(P64.sum() - 1.0) <= 1e-9),
            "passes_1e-9_float32": bool(abs(P32.sum() - 1.0) <= 1e-9),
        }
    res["cst20"] = out20
    res["cst20_tolerance"] = 1e-9

    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
