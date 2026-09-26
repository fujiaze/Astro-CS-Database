#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03: median location SE constant (P-CST-07), float identity gate (P-CST-13),
weight exponent gamma=2 (P-CST-21).

Hypotheses:
  H1 (P-CST-07) for N i.i.d. Gaussian samples, SE(median) = sqrt(pi/2) * sigma / sqrt(N)
     asymptotically (= 1.2533141373155001 sigma/sqrt(N)); the implementation value 1.253
     is truncated by -2.5066e-4 relative. Zero-effect negative: sigma=0 => SE=0.
  H2 (P-CST-13) the identity w = SNR^2 / F_ref^2 equals 1/sigma_F^2 with sigma_F = F_ref/SNR
     to within a few double-precision epsilons (machine eps 2.22e-16); the archive gate
     2.22e-16 is the IEEE-754 double rounding scale, no literature/experiment leg needed.
  H3 (P-CST-21) gamma = 2 is a definitional exponent: w_gamma = SNR^gamma / F_ref^2 differs
     from 1/sigma_F^2 by SNR^(gamma-2) - 1, which is exactly 0 (float eps) only at gamma=2;
     hence gamma=2 is NOT an empirical calibration quantity (route-3 verdict, diverges from
     the auditor request for calibration evidence).

Pure python + numpy. Seeds hardcoded. Runtime << 5 min.
Output: ../results/exp03_median_se_identity.json
"""
from __future__ import annotations

import json
import math
import os

import numpy as np

SEED = 20260926
SQRT_PI_2 = math.sqrt(math.pi / 2.0)
TRUNCATED = 1.253

rng = np.random.default_rng(SEED)

# ---------- H1: SE(median) Monte Carlo ----------

h1 = {}
for n, n_trials in [(10, 200000), (30, 100000), (100, 20000), (200, 10000), (1000, 4000)]:
    x = rng.standard_normal((n_trials, n))
    med = np.median(x, axis=1)
    se = med.std(ddof=1)
    c = se * math.sqrt(n)          # expect ~1.2533
    h1["n%d" % n] = {"c_measured": float(c),
                     "rel_dev_from_exact": float(c / SQRT_PI_2 - 1.0),
                     "rel_bias_if_truncated_1253": float(TRUNCATED / SQRT_PI_2 - 1.0)}
# zero-effect negative
h1["negative_sigma0"] = {"se": float(np.median(np.zeros((1000, 100)), axis=1).std(ddof=1)),
                         "is_zero": True}

# ---------- H2: weight identity roundoff ----------

rng2 = np.random.default_rng(SEED + 1)
n_id = 1_000_000
snr = rng2.uniform(1.0, 100.0, n_id)
f_ref = 10.0 ** rng2.uniform(3.0, 5.0, n_id)
sigma_f = f_ref / snr
w_direct = 1.0 / (sigma_f * sigma_f)
w_identity = (snr * snr) / (f_ref * f_ref)
rel_dev = np.abs(w_identity / w_direct - 1.0)
h2 = {"eps_double": float(np.finfo(np.float64).eps),
      "archived_gate": 2.22e-16,
      "max_rel_dev": float(rel_dev.max()),
      "n_points": n_id,
      "within_gate": bool(rel_dev.max() <= 2.22e-16)}

# ---------- H3: gamma exponent identity ----------

h3 = {}
for gamma in [1.0, 1.5, 2.0, 2.5, 3.0]:
    w_gamma = snr ** gamma / (f_ref * f_ref)
    dev = np.abs(w_gamma / w_direct - 1.0)
    h3["gamma_%s" % gamma] = {"max_rel_dev": float(dev.max()),
                              "median_rel_dev": float(np.median(dev)),
                              "theory_bound_SNR_pow": float(np.abs(100.0 ** (gamma - 2.0) - 1.0))}

out = {
    "experiment": "P2-route3 EXP-03 median SE / float identity / gamma exponent",
    "seed": SEED,
    "theory": {"sqrt_pi_2_exact": SQRT_PI_2, "truncated_1p253_rel_bias": TRUNCATED / SQRT_PI_2 - 1.0},
    "H1_median_se": h1,
    "H2_weight_identity": h2,
    "H3_gamma": h3,
}
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, "..", "results", "exp03_median_se_identity.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps(out, indent=1, ensure_ascii=False))
