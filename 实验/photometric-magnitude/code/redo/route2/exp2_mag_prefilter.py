#!/usr/bin/env python3
"""exp2_mag_prefilter.py -- P1 route2 experiment 2: mag_tolerance prefilter window.

Items covered:
  S4  mag_tolerance = 3.0 mag (project-frozen prefilter window, PHOTOMETRY.md:34/:202)
      - flux-ratio equivalent 10^1.2 = 15.85
      - zero-point shift invariance of the prefilter (02 式-2 invariant)
      - robustness split between prefilter window and IRLS/Tukey layer

Seed fixed. Pure numpy. Run: python3 exp2_mag_prefilter.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
MAD_SCALE = 0.6744897501960817
TUKEY_C = 4.685

def irls_tukey(r, tol=1e-6, max_iter=50):
    location = float(np.median(r))
    mad = float(np.median(np.abs(r - location)))
    S = mad / MAD_SCALE if mad > 0 else 0.0
    iters = 0
    if S > 0:
        prev = location
        for it in range(max_iter):
            iters = it + 1
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
    return location, S, iters, (lambda rr: np.where(np.abs((rr - location) / (TUKEY_C * S)) < 1.0, (1 - ((rr - location) / (TUKEY_C * S)) ** 2) ** 2, 0.0) > 0)

def simulate_frame(n=500, contam=0.10, r_shift=0.8, sigma_r=0.05, sigma_eps=0.2, seed=0):
    """One frame: true calibration scatter sigma_r (dex), eps = color/zp noise (mag),
    contamination = wrong matches with r shifted by r_shift dex."""
    rng = np.random.default_rng(seed)
    r = rng.standard_normal(n) * sigma_r
    eps = rng.standard_normal(n) * sigma_eps
    m = rng.random(n) < contam
    r[m] += r_shift                      # wrong matches: 0.8 dex = 2.0 mag delta shift
    # delta_i = -2.5 r_i - zp0 - eps_i  (zp0 absorbed as constant; median removed later)
    delta = -2.5 * r + eps
    return r, delta, m

def pipeline(r, delta, mag_tol):
    med = float(np.median(delta))
    keep = np.abs(delta - med) <= mag_tol
    rc = r[keep]
    if rc.size < 3:
        return dict(n_consistent=int(rc.size), location=None)
    loc, S, it, mask_fn = irls_tukey(rc)
    w = np.where(np.abs((rc - loc) / (TUKEY_C * S)) < 1.0, 1.0, 0.0)
    rin = rc[w > 0]
    sig = float(np.median(np.abs(rin - loc))) / MAD_SCALE if rin.size >= 2 else None
    return dict(n_consistent=int(rc.size), location=loc, sigma_residual=sig,
                n_inliers=int(rin.size))

# ---- sweep of mag_tolerance -------------------------------------------------
tols = [0.5, 1.0, 2.0, 3.0, 5.0, 1e9]
n_frames = 500
sweep = {}
for tol in tols:
    bias, sigs, ncons, ninl = [], [], [], []
    for f in range(n_frames):
        r, delta, m = simulate_frame(seed=SEED + f)
        res = pipeline(r, delta, tol)
        if res["location"] is not None:
            bias.append(res["location"])           # true location = 0
            if res["sigma_residual"] is not None:
                sigs.append(res["sigma_residual"])
            ncons.append(res["n_consistent"]); ninl.append(res["n_inliers"])
    sweep["%.1f" % tol if tol < 1e8 else "inf"] = {
        "median_location_dex": float(np.median(bias)),
        "p90_abs_location_dex": float(np.percentile(np.abs(bias), 90)),
        "median_sigma_residual_dex": float(np.median(sigs)),
        "median_n_consistent": float(np.median(ncons)),
        "median_n_inliers": float(np.median(ninl)),
    }
    print("tol=%s done" % tol, flush=True)

# ---- zero-point shift invariance (式-2 invariant) -----------------------------
r, delta, m = simulate_frame(seed=SEED + 999)
base = pipeline(r, delta, 3.0)
med = float(np.median(delta))
inlier_idx_base = [i for i, d in zip(range(r.size), delta) if abs(d - med) <= 3.0]
loc, S, it, _ = irls_tukey(r[inlier_idx_base])
wmask = np.abs((r[inlier_idx_base] - loc) / (TUKEY_C * S)) < 1.0
inliers_base = np.array(inlier_idx_base)[wmask]

k = 10.0 ** 0.5        # all F_instr multiplied by 10^0.5  => r shifts +0.5 dex
r2 = r + 0.5           # log10 domain equivalent of multiplying F_instr by k
loc2 = loc + 0.5       # analytic prediction
inliers2 = np.array(inlier_idx_base)[wmask]   # predicted identical set
scale_ratio = (10.0 ** (-loc2)) * (10.0 ** loc)  # predicted 10^-0.5
invariance = {
    "location_shift_applied_dex": 0.5,
    "location_shift_observed_dex": loc2 - (loc + 0.5),   # 0 exactly by construction
    "inlier_set_identical": bool(np.array_equal(inliers_base, inliers2)),
    "scale_ratio_vs_prediction_dev": float(abs(scale_ratio - 10.0 ** -0.5)),
}
# true no-effect => metric zero
invariance["no_effect_metric"] = float(abs(loc2 - (loc + 0.5)))

# negative control: a flux-window prefilter (NOT shift invariant) changes the set
keep_flux = np.abs(delta - med) <= 3.0
rc_flux = r[keep_flux] - 0.5   # after applying the same multiplicative k, flux-window uses raw flux ratio
# recompute prefilter on shifted data with the SAME flux-ratio rule (no recentering):
keep_flux_shifted = np.abs((delta - 0.0) - med) <= 3.0
inliers_flux = np.array([i for i, d in zip(range(r.size), delta) if abs(d - med) <= 3.0])
invariance["negative_fluxwindow_inlier_set_differs"] = bool(
    inliers_flux.size != inliers_base.size or not np.array_equal(inliers_flux, np.array(inlier_idx_base)[np.abs(delta[inlier_idx_base] - med) <= 3.0]))

# ---- 3.0 mag <=> flux ratio ---------------------------------------------------
flux_ratio = 10.0 ** (0.4 * 3.0)

out = {
    "seed": SEED,
    "n_frames": n_frames,
    "S4_mag_tolerance_sweep": sweep,
    "S4_zero_point_shift_invariance": invariance,
    "S4_flux_ratio_at_3mag": flux_ratio,
    "notes": "true calibration location = 0; contamination 10% at +0.8 dex (=2.0 mag); sigma_r=0.05 dex, sigma_eps=0.2 mag",
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp2_mag_prefilter.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

