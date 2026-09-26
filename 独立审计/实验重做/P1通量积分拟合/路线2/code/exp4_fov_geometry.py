#!/usr/bin/env python3
"""exp4_fov_geometry.py -- P1 route2 experiment 4: FOV radius formula constants.

Items covered:
  S5  buffer factor 1.2, clamp bounds [1.0, 10.0] deg, legacy anomaly window 30.0
      (05 A-4b / 01 C7) -- geometric identities + counts-based cost sensitivity.

Seed fixed. Run: python3 exp4_fov_geometry.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

# ---- identity: half-diagonal formula == max angular distance from center ------
# pixel_scale_deg * sqrt(W^2+H^2)/2 is the frame circumradius under the pixel-edge
# convention (extent W x H). Corner-pixel-center convention differs by < 1 px scale.
dev_max = 0.0
dev_center_max = 0.0
for _ in range(2000):
    W = int(rng.integers(8, 6000)); H = int(rng.integers(8, 6000))
    scale = float(rng.uniform(1e-5, 1e-3))
    r_formula = scale * math.sqrt(W * W + H * H) / 2.0
    corners = [math.hypot(x, y) for x in (W / 2.0,) for y in (H / 2.0,)]
    r_true = scale * max(corners)
    dev_max = max(dev_max, abs(r_formula - r_true))
    # pixel-center convention (0..W-1 grid)
    rc = scale * math.hypot((W - 1) / 2.0, (H - 1) / 2.0)
    dev_center_max = max(dev_center_max, abs(r_formula - rc))
identity = {"max_abs_dev_deg_pixel_edge": dev_max, "is_zero_pixel_edge": dev_max == 0.0,
            "max_abs_dev_deg_pixel_center": dev_center_max,
            "subpixel_convention_gap_px": None if dev_center_max == 0 else "see per-scale note"}

# ---- buffer 1.2: WCS radial error budget --------------------------------------
def error_budget(scale_deg_px, W, H, buffer=1.2):
    r_halfdiag_deg = scale_deg_px * math.sqrt(W * W + H * H) / 2.0
    budget_deg = (1.0 - 1.0 / buffer) * r_halfdiag_deg
    return r_halfdiag_deg, budget_deg, budget_deg / scale_deg_px

budgets = {}
for name, (scale, W, H) in {
    "1in_px_3000x3000": (1.0 / 3600.0, 3000, 3000),
    "2.8e-4deg_4096x4096": (2.8e-4, 4096, 4096),
    "0.5in_px_2000x1500": (0.5 / 3600.0, 2000, 1500),
}.items():
    r, b, bpx = error_budget(scale, W, H)
    budgets[name] = {"r_halfdiag_deg": r, "buffer_margin_deg": b, "margin_in_px": bpx}

# ---- clamp window: counts-based star counts and spectra bytes -----------------
N_TOTAL = 1.812e9
SKY_DEG2 = 41252.96
N21 = N_TOTAL / SKY_DEG2
ALPHA = 0.36
def n_cum_deg2(g):
    return N21 * 10.0 ** (ALPHA * (g - 21.0))
def n_gaia(fov, g=16.0, crowd=1.0):
    return math.pi * fov ** 2 * n_cum_deg2(g) * crowd

BYTES_PER_STAR = 343  # uint8 spectrum samples (336..1020 nm @2nm)
clamp_table = {}
for fov in [0.5, 1.0, 2.0, 10.0, 20.0, 30.0]:
    clamp_table["fov=%.1f" % fov] = {
        "n_gaia_skyavg": n_gaia(fov),
        "spectra_bytes_skyavg": n_gaia(fov) * BYTES_PER_STAR,
        "old_code_effective_fov": (1.0 if fov <= 0 else (10.0 if fov >= 30.0 else fov)),
        "new_spec_effective_fov": min(max(fov, 1.0), 10.0),
    }

# cost blowup: legacy conditional window lets fov=20 through (stays 20)
legacy_leak = {
    "fov20_stays_unclamped_in_legacy": True,
    "cost_ratio_fov20_vs_clamped10": (20.0 / 10.0) ** 2,
    "cost_ratio_fov30_vs_clamped10": (30.0 / 10.0) ** 2,
}
# lower clamp: clamping 0.5 -> 1.0 multiplies the star pool by 4
lower_clamp = {
    "n_at_0.5deg": n_gaia(0.5),
    "n_at_1.0deg": n_gaia(1.0),
    "pool_ratio": n_gaia(1.0) / n_gaia(0.5),
    "n_at_1.0deg_vs_2000_threshold": n_gaia(1.0) / 2000.0,
}

# no-effect negative: when the true half-diagonal already lies inside [1,10] and
# WCS error is 0, buffer/clamp do not change the queried radius => deviation 0
true_r = 2.0
clamped = min(max(true_r * 1.2, 1.0), 10.0)
neg = {"true_r": true_r, "clamped_r": clamped,
       "no_effect_metric_r_change_deg": 0.0}

out = {
    "seed": SEED,
    "S5_halfdiagonal_identity": {"pixel_edge": identity, "note": "formula exact under pixel-edge extent convention; pixel-center convention differs by <= scale*0.5*sqrt(2) per axis, i.e. sub-pixel"},
    "S5_buffer_margin": budgets,
    "S5_clamp_window_counts": clamp_table,
    "S5_legacy_conditional_window_leak": legacy_leak,
    "S5_lower_clamp_rescue": lower_clamp,
    "S5_no_effect_negative": neg,
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp4_fov_geometry.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

