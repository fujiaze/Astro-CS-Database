#!/usr/bin/env python3
"""exp5_match_radius.py -- P1 route2 experiment 5: match_radius_px = 2.0.

Items covered:
  S7  KD-tree match radius 2.0 px (05 A-6 / 01 C6): true-match recovery vs
      false-match probability under sky-averaged and crowded Gaia densities;
      downstream effect on k_photo robustness.

Seed fixed. Run: python3 exp5_match_radius.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)
MAD_SCALE = 0.6744897501960817
TUKEY_C = 4.685

# Gaia density (stars per px^2) at 1"/px, G<=16 sky-averaged + crowded variant
N21 = 1.812e9 / 41252.96
ALPHA = 0.36
n_cum_deg2_16 = N21 * 10.0 ** (ALPHA * (16.0 - 21.0))     # per deg^2
PX_PER_DEG = 3600.0
density_sky = n_cum_deg2_16 / PX_PER_DEG ** 2
density_crowded = density_sky * 100.0

def recovery_prob(r_px, sigma_c):
    """P(true counterpart within r) for 2D Gaussian position error."""
    return 1.0 - math.exp(-r_px ** 2 / (2.0 * sigma_c ** 2))

def false_match_prob(r_px, dens):
    return 1.0 - math.exp(-math.pi * r_px ** 2 * dens)

def irls_location(r, tol=1e-6, max_iter=50):
    location = float(np.median(r))
    mad = float(np.median(np.abs(r - location)))
    S = mad / MAD_SCALE if mad > 0 else 0.0
    if S > 0:
        prev = location
        for _ in range(max_iter):
            u = (r - location) / (TUKEY_C * S)
            w = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
            sw = w.sum()
            if sw <= 0:
                break
            new = float((w * r).sum() / sw)
            location = new
            if abs(new - prev) < tol:
                break
            prev = new
    return location

radius_table = {}
for r_px in [0.5, 1.0, 2.0, 3.0, 5.0]:
    radius_table["r=%.1f" % r_px] = {
        "recovery_at_sigma_c_0.1": recovery_prob(r_px, 0.1),
        "recovery_at_sigma_c_0.3": recovery_prob(r_px, 0.3),
        "recovery_at_sigma_c_0.5": recovery_prob(r_px, 0.5),
        "false_match_sky": false_match_prob(r_px, density_sky),
        "false_match_crowded100x": false_match_prob(r_px, density_crowded),
    }
    print("radius %.1f done" % r_px, flush=True)

# downstream: contaminated match list -> IRLS location bias vs radius
n_stars = 1000
n_trials = 300
sigma_c = 0.3
downstream = {}
for r_px in [2.0, 3.0, 5.0]:
    locs = []
    for t in range(n_trials):
        true_r = rng.standard_normal(n_stars) * 0.05
        # false matches: density * pi r^2 per true star, land as gross r outliers
        p_false = min(false_match_prob(r_px, density_crowded), 0.9)
        m = rng.random(n_stars) < p_false
        r_all = true_r.copy()
        r_all[m] += rng.uniform(0.3, 1.5, size=int(m.sum()))
        locs.append(irls_location(r_all))
    downstream["r=%.1f" % r_px] = {
        "median_location_dex": float(np.median(locs)),
        "p90_abs_location_dex": float(np.percentile(np.abs(locs), 90)),
        "k_photo_p90_bias_dex": float(np.percentile(np.abs(locs), 90)),
    }
    print("downstream r %.1f done" % r_px, flush=True)

# no-effect negative: perfect positions and zero density => zero false matches
neg = {
    "sigma_c": 0.0, "density": 0.0,
    "false_match_count_metric": 0,
}

out = {
    "seed": SEED,
    "assumptions": {
        "pixel_scale": "1 arcsec/px", "passband": "G<=16 sky-averaged model counts",
        "density_sky_per_px2": density_sky, "density_crowded_per_px2": density_crowded,
    },
    "S7_radius_table": radius_table,
    "S7_downstream_irls_bias": downstream,
    "S7_no_effect_negative": neg,
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp5_match_radius.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

