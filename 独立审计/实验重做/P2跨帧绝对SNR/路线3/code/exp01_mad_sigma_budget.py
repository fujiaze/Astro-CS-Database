#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01: MAD->sigma constant (P-CST-03) + sky sample budget (P-CST-08) + 5-sigma clip (P-CST-09).

Hypotheses:
  H1 (P-CST-03) sigma_hat = kappa*MAD with kappa = 1/Phi^-1(3/4) = 1.482602218505602:
     on Gaussian samples E[sigma_hat]/sigma = 1 asymptotically; sigma=0 => sigma_hat=0 (zero-effect negative).
  H2 (P-CST-08) single-arm MAD scale estimator rel SE = c/sqrt(n); NOISE_MODEL.md 5a claims
     c ~= 1.152 (x median efficiency 1.25 => 1.44/sqrt(N_sky)); D-39 mentions a rejected 1.144 branch.
     This experiment measures c for pooled MAD and median-of-patch-variance arms and
     arbitrates 1.152 vs 1.144 vs the asymptotic value.
  H3 (P-CST-09) 5-sigma clip false-positive rate on pure Gaussian noise = 2*Phi(-5) = 5.733e-7/px;
     on an 8x8 patch (n=64) expected clipped count 3.67e-5 ~ 0 (zero-effect negative).

Pure python + numpy. Seeds hardcoded. Runtime << 5 min.
Output: ../results/exp01_mad_sigma_budget.json
"""
from __future__ import annotations

import json
import math
import os

import numpy as np

SEED = 20260926
KAPPA_FROZEN = 1.482602218505602

# ---------- closed-form theory legs (independent re-derivation) ----------

def phi_cdf(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

def phi_pdf(x):
    return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)

def norm_ppf(p, lo=-10.0, hi=10.0):
    """Bisection inverse of the standard normal CDF (numpy has no erfinv)."""
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if phi_cdf(mid) < p:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)

q75 = norm_ppf(0.75)                       # Phi^-1(3/4)
kappa_closed = 1.0 / q75
c_known_center = math.sqrt(0.25 / (2.0 * phi_pdf(q75)) ** 2) / q75
# asymptotic var of the |Z| quantile at p=0.5: p(1-p)/(n f(c)^2), f = density of |Z| = 2*phi

# ---------- H1: kappa calibration (Monte Carlo) ----------

rng = np.random.default_rng(SEED)
h1 = {}
for n, n_trials in [(64, 60000), (1024, 8000), (9216, 2400)]:
    sig_true = 5.0
    x = rng.standard_normal((n_trials, n)) * sig_true
    s_hat = KAPPA_FROZEN * np.median(np.abs(x - np.median(x, axis=1, keepdims=True)), axis=1)
    rel = s_hat / sig_true
    h1["n%d" % n] = {
        "mean_rel_bias": float(rel.mean() - 1.0),
        "c_measured_sqrt_n": float(rel.std(ddof=1) / rel.mean() * math.sqrt(n)),
    }
x = rng.standard_normal((4000, 1024)) * 5.0
s_hat_known = KAPPA_FROZEN * np.median(np.abs(x), axis=1)
rel_known = s_hat_known / 5.0
h1["n1024_known_center"] = {
    "mean_rel_bias": float(rel_known.mean() - 1.0),
    "c_measured_sqrt_n": float(rel_known.std(ddof=1) / rel_known.mean() * math.sqrt(1024)),
}

# zero-effect negative: constant input (sigma=0) => sigma_hat = 0 exactly
x0 = np.full((100, 64), 3.0)
h1["negative_sigma0"] = {"sigma_hat_max": float(np.abs(KAPPA_FROZEN * np.median(np.abs(x0[0] - np.median(x0[0]))))),
                         "metric_is_zero": bool(np.all(KAPPA_FROZEN * np.median(np.abs(x0 - np.median(x0, axis=1, keepdims=True)), axis=1) == 0.0))}

# ---------- H2: sky-sample budget coefficient arbitration ----------

def pipeline_c(n_sky, n_trials, patch=8):
    """Pooled MAD arm and median-of-patch-variance arm; c = relSE * sqrt(n_sky)."""
    sig_true = 5.0
    x = rng.standard_normal((n_trials, n_sky)) * sig_true
    s_pool = KAPPA_FROZEN * np.median(np.abs(x - np.median(x, axis=1, keepdims=True)), axis=1)
    cA = float(s_pool.std(ddof=1) / s_pool.mean() * math.sqrt(n_sky))
    n_patch = n_sky // (patch * patch)
    xb = x[:, : n_patch * patch * patch].reshape(n_trials, n_patch, patch * patch)
    patch_sig = KAPPA_FROZEN * np.median(np.abs(xb - np.median(xb, axis=2, keepdims=True)), axis=2)
    patch_var = np.median(patch_sig, axis=1) ** 2
    cB = float(np.sqrt(patch_var).std(ddof=1) / np.sqrt(patch_var).mean() * math.sqrt(n_sky))
    return {"c_pooled_mad": cA, "c_median_of_patch_vars": cB}

h2 = {
    "asymptotic_c_known_center_closed": c_known_center,
    "asymptotic_c_if_ARE_037": 1.0 / math.sqrt(2.0 * 0.37),
    "claim_1152": 1.152,
    "claim_1144": 1.144,
    "n9216": pipeline_c(9216, 1200),
    "n1024": pipeline_c(1024, 3000),
}
h2["implied_n_sky_at_eps0015"] = {k: float((v / 0.015) ** 2) for k, v in {
    "c_1152x125": 1.152 * 1.2533141373155001,
    "c_1144x125": 1.144 * 1.2533141373155001,
    "c_measured9216_poolx125": h2["n9216"]["c_pooled_mad"] * 1.2533141373155001,
}.items()}

# ---------- H3: 5-sigma clip false-positive rate (theory + MC) ----------

fp_theory = 2.0 * (1.0 - phi_cdf(5.0))
x = rng.standard_normal((2000, 64)) * 5.0
med = np.median(x, axis=1, keepdims=True)
mad = np.median(np.abs(x - med), axis=1, keepdims=True)
clipped = np.abs(x - med) > 5.0 * KAPPA_FROZEN * mad
h3 = {"fp_per_px_theory": float(fp_theory),
      "expected_clip_per_64px_patch": float(fp_theory * 64),
      "mc_frames_with_any_clip": int(clipped.any(axis=1).sum()),
      "mc_frames_total": int(clipped.shape[0]),
      "mc_clip_rate_per_px": float(clipped.mean())}

# ---------- report ----------

out = {
    "experiment": "P2-route3 EXP-01 MAD->sigma / sky budget / 5sigma clip",
    "seed": SEED,
    "theory": {
        "Phi_inv_075": q75,
        "kappa_closed_form": kappa_closed,
        "kappa_frozen_literal": KAPPA_FROZEN,
        "kappa_rel_diff": abs(kappa_closed - KAPPA_FROZEN) / kappa_closed,
        "c_known_center_asymptotic": c_known_center,
    },
    "H1_kappa": h1,
    "H2_budget": h2,
    "H3_clip": h3,
}
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, "..", "results", "exp01_mad_sigma_budget.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps(out, indent=1, ensure_ascii=False))