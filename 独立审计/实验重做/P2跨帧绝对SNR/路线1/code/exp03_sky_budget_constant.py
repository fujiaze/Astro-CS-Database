#!/usr/bin/env python3
"""EXP-P2-R1-03: sky sample budget constant 9216 (P-CST-08).

Chain to adjudicate:  N_min = (k_eff/eps_target)^2,  1.44 = 1.152 x 1.25,
(1.44/0.015)^2 = 9216  vs  (1.144/0.015)^2 = 5811/5816 branch.
Measured quantities (MC, seed fixed 20260926):
  c1: single 8x8 patch (n=64), sigma-hat = kappa*MAD (+5 sigma clip <=2 rounds)
  c2: same with clipped second moment
  c3: pooled MAD over all N_sky pixels of one frame (asymptotic MAD constant ~1.649)
  c4: frame pipeline sigma-hat = sqrt(median of patch variances), c_eff = relSE*sqrt(N_sky)
Negative control: sigma_true = 0 (flat frame) => sigma-hat = 0 => metric = 0.
Pure python3+numpy. Runtime < 2 min.
"""
import json, os
import numpy as np
from statistics import NormalDist

SEED = 20260926
kappa = 1.0 / NormalDist().inv_cdf(0.75)
out = {"seed": SEED, "kappa": kappa}
out["budget_arithmetic"] = {
    "(1.44/0.015)^2": (1.44 / 0.015) ** 2,
    "(1.152*1.25/0.015)^2": (1.152 * 1.25 / 0.015) ** 2,
    "(1.144/0.015)^2": (1.144 / 0.015) ** 2,
    "doc_claim_1p144_branch": 5811,
    "note": "1.144 branch evaluates to 5816.6, not 5811 as documented; "
            "9216 = (1.44/0.015)^2 is exact.",
}

def mad_sigma(x, clip=True, rounds=2):
    """per-row MAD sigma with optional 5-sigma clipping (<= rounds).
    Clipped pixels are excluded (NaN), never zero-filled."""
    x = np.atleast_2d(x).astype(float)
    med = np.median(x, axis=1, keepdims=True)
    for _ in range(rounds if clip else 0):
        s = kappa * np.nanmedian(np.abs(x - med), axis=1, keepdims=True)
        keep = np.abs(x - med) <= 5.0 * s
        if np.all(keep | np.isnan(x)):
            break
        x = np.where(keep, x, np.nan)
        med = np.nanmedian(x, axis=1, keepdims=True)
    return kappa * np.nanmedian(np.abs(x - med), axis=1)

# ---------- c1: single patch MAD, n=64 ----------
rng = np.random.default_rng(SEED)
M = 200000
s1 = mad_sigma(rng.normal(0.0, 5.0, size=(M, 64)), clip=False) / 5.0
s1c = mad_sigma(rng.normal(0.0, 5.0, size=(M, 64)), clip=True) / 5.0
out["c1_patch_mad_n64"] = {"c_sqrt_n": float(s1.std() * 8), "rel_bias": float(s1.mean() - 1.0)}
out["c1b_patch_mad_clip_n64"] = {"c_sqrt_n": float(s1c.std() * 8), "rel_bias": float(s1c.mean() - 1.0)}

# ---------- c2: single patch clipped second moment, n=64 ----------
x = rng.normal(0.0, 5.0, size=(M, 64))
med = np.median(x, axis=1, keepdims=True)
s0 = kappa * np.median(np.abs(x - med), axis=1, keepdims=True)
xc = np.where(np.abs(x - med) <= 5.0 * s0, x, np.nan)
v = np.nanmean((xc - np.nanmean(xc, axis=1, keepdims=True)) ** 2, axis=1)
s2 = np.sqrt(v) / 5.0
out["c2_patch_clip2nd_n64"] = {"c_sqrt_n": float(np.nanstd(s2) * 8),
                               "rel_bias": float(np.nanmean(s2) - 1.0)}

# ---------- c3: pooled MAD, N_sky = 65536 ----------
Nsky = 65536
Mf = 1200
s3 = []
for _ in range(Mf):
    fr = rng.normal(0.0, 5.0, size=Nsky)
    med = np.median(fr)
    s3.append(kappa * np.median(np.abs(fr - med)) / 5.0)
s3 = np.array(s3)
out["c3_pooled_mad"] = {"N_sky": Nsky, "c_sqrt_n": float(s3.std() * np.sqrt(Nsky)),
                        "rel_bias": float(s3.mean() - 1.0),
                        "asymptotic_theory_1_over_sqrt_ARE": 1.6492}

# ---------- c4: frame pipeline (median of patch variances), 256^2 frame ----------
side, patch = 256, 8
npix = side * side
npat = (side // patch) ** 2
Mf = 2000
s4 = []
for _ in range(Mf):
    fr = rng.normal(0.0, 5.0, size=(side, side)).reshape(npat, patch, patch).reshape(npat, 64)
    pv = mad_sigma(fr, clip=True) ** 2
    s4.append(np.sqrt(np.median(pv)) / 5.0)
s4 = np.array(s4)
out["c4_pipeline_median_patchvar"] = {
    "N_sky": npix, "c_eff_sqrt_Nsky": float(s4.std() * np.sqrt(npix)),
    "rel_bias": float(s4.mean() - 1.0)}

# ---------- negative control ----------
flat = np.full((50, 64), 2.9)
out["null_zero_noise"] = {"metric": float(np.abs(mad_sigma(flat, clip=True)).max())}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp03_sky_budget_constant.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
