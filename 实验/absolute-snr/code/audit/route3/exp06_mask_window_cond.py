#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06: source mask radius (P-CST-15), profile window sensitivity (P-CST-23),
control-point conditioning threshold (P-CST-17).

Hypotheses:
  H1 (P-CST-15) for a beta=2.5 Moffat source, the unbiased-mask radius derived from
     "residual surface brightness at the mask edge = k * sigma_bg" is
     r_local = alpha * sqrt( (F*(beta-1)/(pi*alpha^2*k*sigma_bg))^(1/beta) - 1 ),
     alpha = FWHM / (2*sqrt(2^(1/beta)-1)). For F=1e5 e-, FWHM=3 px, k=0.1, sigma_bg=5 ADU
     this gives r_local ~= 17.6 px (matches NOISE_MODEL.md section 14 item 5).
     MC: masking at r >= r_local keeps the MAD sigma_bg bias within ~0.5% (var contamination
     < 1% sigma_bg^2 per the k=0.1 contract); masking at r < r_local shows growing positive
     bias. Zero-effect negative: star-free frame => bias consistent with 0 at every radius.
  H2 (P-CST-23) sky-limited SNR bias from PSF-weight truncation:
     SNR(w)/SNR(inf) - 1 = sqrt( sum P^2 (w) / sum P^2 (inf) ) - 1 <= 0 (too-narrow window
     overestimates sum P^2, hence underestimates sigma_F... i.e. overestimates SNR => green);
     the archived magnitude scale (+4e-6 / +2.4e-4 / +1.33e-2) is reproduced in ORDER for
     Moffat4 power-law tails at comparable window/FWHM ratios; Gaussian profiles are
     exponentially insensitive. Curve delivered over FWHM x half-window grid.
  H3 (P-CST-17) plane fit var(x,y)=a+b x+c y on control points: relative error of (b,c)
     scales linearly with the conditioning kappa = sqrt(lambda_hi/lambda_lo) of the centered
     design; the threshold lambda_lo/lambda_hi >= 1/16 (kappa <= 4) separates stable from
     inflated regimes. Negative: kappa=1 (round cloud) => error at the noise floor.

Pure python + numpy. Seeds hardcoded. Runtime << 5 min.
Output: ../results/exp06_mask_window_cond.json
"""
from __future__ import annotations

import json
import math
import os

import numpy as np

SEED = 20260926
KAPPA = 1.482602218505602

# ---------- H1: mask radius ----------

def r_local(F, fwhm, beta, k, sigma_bg):
    alpha = fwhm / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
    inside = (F * (beta - 1.0) / (math.pi * alpha ** 2 * k * sigma_bg)) ** (1.0 / beta) - 1.0
    return alpha * math.sqrt(inside)

RL_PRED = r_local(1e5, 3.0, 2.5, 0.1, 5.0)

def make_frame(rng, n_stars, shape=(512, 512), fwhm=3.0, beta=2.5, sigma_bg=5.0,
               flux_lo=200.0, flux_hi=1e5):
    img = rng.normal(0.0, sigma_bg, shape)
    alpha = fwhm / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
    ys = rng.integers(62, shape[0] - 62, n_stars)
    xs = rng.integers(62, shape[1] - 62, n_stars)
    fluxes = 10.0 ** rng.uniform(math.log10(flux_lo), math.log10(flux_hi), n_stars)
    rad = 60  # paint out to 60 px (beyond any tested radius)
    yy, xx = np.mgrid[-rad:rad + 1, -rad:rad + 1].astype(float)
    prof = (1.0 + (yy ** 2 + xx ** 2) / alpha ** 2) ** (-beta)
    prof /= prof.sum()
    for y, x, f in zip(ys, xs, fluxes):
        img[y - rad:y + rad + 1, x - rad:x + rad + 1] += f * prof
    return img, ys, xs, fluxes

h1 = {"r_local_pred_F1e5": float(RL_PRED)}
for radius in [2.0, 4.0, 6.0, 10.0, 17.6, 28.0, 60.0]:
    biases = []
    for s in range(3):
        rng = np.random.default_rng(SEED + 100 + s)
        img, ys, xs, fluxes = make_frame(rng, 40)
        msk = np.ones(img.shape, bool)
        yy, xx = np.mgrid[0:512, 0:512].astype(float)
        for y, x in zip(ys, xs):
            msk &= (np.hypot(yy - y, xx - x) > radius)
        vals = img[msk]
        sig_hat = KAPPA * np.median(np.abs(vals - np.median(vals)))
        biases.append(sig_hat / 5.0 - 1.0)
    h1["bias_at_r_%.1f" % radius] = float(np.mean(biases))
# zero-effect negative: star-free frames
biases0 = []
for s in range(3):
    rng = np.random.default_rng(SEED + 200 + s)
    img = rng.normal(0.0, 5.0, (512, 512))
    sig_hat = KAPPA * np.median(np.abs(img - np.median(img)))
    biases0.append(sig_hat / 5.0 - 1.0)
h1["negative_starsfree_bias"] = float(np.mean(biases0))
h1["negative_pass_lt_0p2pct"] = bool(abs(np.mean(biases0)) < 0.002)

# ---------- H2: window sensitivity ----------

def sum_p2_trunc(profile, sigma, half):
    xs = np.arange(-half, half + 1, dtype=float)
    xx, yy = np.meshgrid(xs, xs)
    P = profile(xx, yy, sigma)
    P /= P.sum()
    return float((P ** 2).sum())

def gauss2(xx, yy, s):
    return np.exp(-0.5 * (xx ** 2 + yy ** 2) / s ** 2)

def moffat2d(xx, yy, s):
    a = math.sqrt(2.0) * s
    return (1.0 + (xx ** 2 + yy ** 2) / a ** 2) ** (-4)

h2 = {}
for name, prof in [("gaussian", gauss2), ("moffat4", moffat2d)]:
    for fwhm in [2.0, 3.0, 6.0, 12.0, 60.0]:
        sigma = fwhm / (2.3548200450309493 if name == "gaussian" else 1.230307652590102)
        s_inf = sum_p2_trunc(prof, sigma, 800)
        row = {}
        for w in [15, 30, 60, 120, 256]:
            s_w = sum_p2_trunc(prof, sigma, w)
            # SNR(w)/SNR(inf) - 1 = sqrt(S_w/S_inf) - 1  (negative = SNR overestimated = green)
            row["w%d" % w] = float(math.sqrt(s_w / s_inf) - 1.0)
        h2["%s_fwhm_%g" % (name, fwhm)] = row

# ---------- H3: control-point conditioning ----------

h3 = {}
rng3 = np.random.default_rng(SEED + 9)
noise = 0.05
for compress in [1.0, 4.0, 8.0, 16.0]:   # x-compression => lambda_lo/lambda_hi = compress^-2
    errs_b, errs_c = [], []
    for t in range(200):
        gx, gy = np.meshgrid(np.linspace(32, 480, 8), np.linspace(32, 480, 8))
        pts = np.stack([(gx.ravel() - 256.0), (gy.ravel() - 256.0) * 1.0], axis=1)
        pts[:, 0] /= compress
        a, b, c = 1.0, 0.002, -0.001
        y = a + b * pts[:, 0] + c * pts[:, 1] + rng3.normal(0.0, noise, 64)
        X = np.stack([np.ones(64), pts[:, 0], pts[:, 1]], axis=1)
        coef, *_ = np.linalg.lstsq(X, y, rcond=None)
        errs_b.append(abs(coef[1] - b) / abs(b))
        errs_c.append(abs(coef[2] - c) / abs(c))
    cen = pts - pts.mean(axis=0)
    ev = np.linalg.eigvalsh(cen.T @ cen)
    kap = float(math.sqrt(ev.max() / ev.min()))
    h3["compress_%g" % compress] = {
        "kappa": kap,
        "lambda_lo_over_hi": 1.0 / kap ** 2,
        "rel_err_b_mean": float(np.mean(errs_b)),
        "rel_err_c_mean": float(np.mean(errs_c)),
        "err_over_kappa_b": float(np.mean(errs_b) / kap),
    }
h3["threshold_note"] = "lambda_lo/lambda_hi >= 1/16 <=> kappa <= 4"
h3["negative_kappa1_noise_floor"] = h3["compress_1"]["rel_err_b_mean"]

out = {
    "experiment": "P2-route3 EXP-06 mask radius / window sensitivity / conditioning",
    "seed": SEED,
    "H1_mask_radius": h1,
    "H2_window_sensitivity": h2,
    "H3_conditioning": h3,
}
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, "..", "results", "exp06_mask_window_cond.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps(out, indent=1, ensure_ascii=False))
