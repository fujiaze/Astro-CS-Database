#!/usr/bin/env python3
"""exp6_sigma_kappa.py -- P1 route2 experiment 6: zero-point standard error & sample limits.

Items covered:
  S9  sigma_kappa,stat ~= 1.253 * sigma_residual / sqrt(N)  (02 式-4 consumption form;
      snr_estimator.h:295) -- SE of the median of N normal samples = sqrt(pi/(2N)) * sigma
  S13 ZP_syn sample lower limit 3 (01 C9/B13): sampling error of a 3-star median
  plus composition with the MAD finite-sample bias (S10).

Seed fixed. Run: python3 exp6_sigma_kappa.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

SQRT_PI_2 = math.sqrt(math.pi / 2.0)          # 1.2533141...
E_ABS_NORMAL = math.sqrt(2.0 / math.pi)       # 0.7978845...
inv_eabs = 1.0 / E_ABS_NORMAL

# analytic check: SE(median_N) = sqrt(pi/(2N)) sigma
def mc_median_se(n, sigma=1.0, reps=400000, chunk=100000):
    acc = 0.0; acc2 = 0.0; remaining = reps
    while remaining > 0:
        b = min(remaining, chunk)
        z = rng.standard_normal((b, n)) * sigma
        med = np.median(z, axis=1)
        acc += float(med.sum()); acc2 += float((med * med).sum())
        remaining -= b
    m = acc / reps
    v = acc2 / reps - m * m
    return math.sqrt(max(v, 0.0))

se_table = {}
for n in [3, 10, 50, 200, 1000, 2338]:
    se = mc_median_se(n, reps=200000 if n <= 200 else 100000)
    pred = SQRT_PI_2 / math.sqrt(n)
    se_table[str(n)] = {
        "mc_se_of_median": se,
        "analytic_sqrt_pi_over_2N": pred,
        "ratio": se / pred,
    }
    print("median SE n=%d done" % n, flush=True)

# negative control: sigma = 0 => SE = 0 (no-effect truth => zero)
z0 = np.zeros((100000, 5))
se_zero = float(np.median(z0, axis=1).std())
neg = {"sigma": 0.0, "mc_se": se_zero, "metric_zero": se_zero == 0.0}

# S13: ZP_syn from 3 stars -- 68% CI of the median estimate
se3 = SQRT_PI_2 / math.sqrt(3)
# composition: sigma_residual itself is biased low at n=3 by ~1.495x (Croux & Rousseeuw)
bias3 = 1.495
comp = {
    "n": 3,
    "se_of_median_in_units_of_true_sigma_mag": se3,
    "reported_sigma_residual_underestimate_factor": bias3,
    "total_underestimate_factor_of_reported_se": bias3 * 1.0,
    "example_true_sigma_mag_0.10": {
        "true_se_of_zp_median_mag": se3 * 0.10,
        "reported_se_if_sigma_mag_underestimated_mag": se3 * 0.10 / bias3,
    },
}
# measured typical zero_point_n_stars 2338 (01 C9) for contrast
se2338 = SQRT_PI_2 / math.sqrt(2338)
comp["n2338_se_of_median_in_units_of_sigma"] = se2338

out = {
    "seed": SEED,
    "S9_constants": {
        "sqrt_pi_2": SQRT_PI_2, "E_abs_normal": E_ABS_NORMAL, "inv_E_abs": inv_eabs,
        "code_coefficient_1253_relative_dev": abs(1.253 - SQRT_PI_2) / SQRT_PI_2,
    },
    "S9_se_of_median_table": se_table,
    "S9_no_effect_negative": neg,
    "S13_zp_sample_limit_3": comp,
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp6_sigma_kappa.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

