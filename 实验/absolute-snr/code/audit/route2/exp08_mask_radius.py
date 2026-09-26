#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-08: P-CST-15 (mask radius parameter group k=0.1, r_min, rmax=60)

A literature leg  : NOISE_MODEL.md section 5a (r_local closed forms; internal
                    derivation MASK-002); Moffat 1969 for the profile; Bertin &
                    Arnouts 1996 for robust background estimation practice.
B experiment leg  : synthetic-frame Monte Carlo (MASK-002 oracle style).
  r_local (NOISE_MODEL.md section 5a, reproduced independently):
    Gaussian : r_local = sigma_p * sqrt(2 ln( F / (2 pi sigma_p^2 k sigma_bg) ))
    Moffat b : r_local = alpha * sqrt( (F (b-1) / (pi alpha^2 k sigma_bg))^(1/b) - 1 )
               alpha = FWHM / (2 sqrt(2^(1/b) - 1))
  Frame: 512^2, N_s = 80 stars, power-law fluxes F in [2e2, 1e5] ADU,
  Moffat beta = 2.5, FWHM = 3 px, sigma_bg = 5 ADU (true).
  Arm A (adaptive, k = 0.1, r_min = max(1.5, 0.75 FWHM), rmax = 60):
    |sigma_hat/sigma - 1| <= 2% oracle + monotonicity of r_i in (F, FWHM).
  NEGATIVE CONTROL 1: fixed r = 2 px with F_max = 1e6 => must exceed 2% (red).
  NEGATIVE CONTROL 2: fixed r = 60 px on a 256^2 / 50-star frame => N_sky <
  9216 (budget violation must be detectable; sky-budget invariant).
C seed             : SEED = 20260926 (frame generation, flux draws).
D outputs          : results/exp08_mask_radius.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt, log, pi

SEED = 20260926
KAPPA = 0.1
OUT = "results/exp08_mask_radius.json"

def r_local_moffat(F, fwhm, beta, k, sigma_bg):
    alpha = fwhm / (2.0 * sqrt(2.0 ** (1.0 / beta) - 1.0))
    arg = (F * (beta - 1.0) / (pi * alpha ** 2 * k * sigma_bg)) ** (1.0 / beta) - 1.0
    return alpha * sqrt(arg) if arg > 0 else 0.0

def r_local_gauss(F, fwhm, k, sigma_bg):
    sig = fwhm / (2.0 * sqrt(2.0 * log(2.0)))
    arg = F / (2.0 * pi * sig ** 2 * k * sigma_bg)
    return sig * sqrt(2.0 * log(arg)) if arg > 1.0 else 0.0

def make_frame(rng, shape, n_stars, sigma_bg, fmax, fwhm=3.0, beta=2.5, seed=SEED):
    ny, nx = shape
    img = rng.normal(0.0, sigma_bg, size=(ny, nx))
    # power-law fluxes dN/dF ~ F^-2 in [2e2, fmax]
    u = rng.uniform(0.0, 1.0, size=n_stars)
    Finv_hi = 1.0 / 2e2
    Finv_lo = 1.0 / fmax
    F = 1.0 / (Finv_hi + u * (Finv_lo - Finv_hi))
    alpha = fwhm / (2.0 * sqrt(2.0 ** (1.0 / beta) - 1.0))
    ext = 40
    h = np.arange(-ext, ext + 1)
    xx, yy = np.meshgrid(h, h)
    rr2 = (xx ** 2 + yy ** 2).astype(float)
    prof = (1.0 + rr2 / alpha ** 2) ** (-beta)
    prof /= prof.sum()  # normalise stamp; flux F distributed over stamp
    for i in range(n_stars):
        cy, cx = rng.integers(ext, ny - ext), rng.integers(ext, nx - ext)
        img[cy - ext:cy + ext + 1, cx - ext:cx + ext + 1] += F[i] * prof
    return img, F

def sigma_bg_estimate(img, mask, sigma_bg_guess, n_clip=2, clip=5.0):
    x = img[~mask]
    med = np.median(x)
    s = 1.482602218505602 * np.median(np.abs(x - med))
    for _ in range(n_clip):
        keep = np.abs(x - med) <= clip * s
        if keep.all():
            break
        x = x[keep]
        med = np.median(x)
        s = 1.482602218505602 * np.median(np.abs(x - med))
    return float(s), int(x.size)

def build_mask(shape, stars_xyF, radii):
    ny, nx = shape
    mask = np.zeros((ny, nx), dtype=bool)
    yy, xx = np.mgrid[0:ny, 0:nx]
    for (cy, cx, F), r in zip(stars_xyF, radii):
        r2 = (yy - cy) ** 2 + (xx - cx) ** 2
        mask |= r2 <= max(r, 1.0) ** 2
    return mask

def main():
    res = {"seed": SEED}
    sigma_bg_true = 5.0

    # ---- monotonicity of r_local in F and FWHM (invariant) -------------------
    Fs = np.logspace(2.3, 5.0, 20)
    r_F = [r_local_moffat(f, 3.0, 2.5, KAPPA, sigma_bg_true) for f in Fs]
    fwhms = np.linspace(2.0, 8.0, 13)
    r_W = [r_local_moffat(1e4, w, 2.5, KAPPA, sigma_bg_true) for w in fwhms]
    res["r_local_monotone_in_F"] = bool(np.all(np.diff(r_F) > 0))
    res["r_local_monotone_in_FWHM"] = bool(np.all(np.diff(r_W) > 0))
    res["r_local_at_F1e5_fwhm3"] = float(r_local_moffat(1e5, 3.0, 2.5, KAPPA, sigma_bg_true))
    res["r_local_at_F1e5_gauss"] = float(r_local_gauss(1e5, 3.0, KAPPA, sigma_bg_true))

    # ---- frame MC: adaptive mask vs fixed small mask -------------------------
    def one_frame(shape, n_stars, fmax, radius_mode, seed):
        rng = np.random.default_rng(seed)
        ny, nx = shape
        img = rng.normal(0.0, sigma_bg_true, size=(ny, nx))
        u = rng.uniform(0.0, 1.0, size=n_stars)
        Finv_hi, Finv_lo = 1.0 / 2e2, 1.0 / fmax
        F = 1.0 / (Finv_hi + u * (Finv_lo - Finv_hi))
        fwhm, beta = 3.0, 2.5
        alpha = fwhm / (2.0 * sqrt(2.0 ** (1.0 / beta) - 1.0))
        ext = 30
        h = np.arange(-ext, ext + 1)
        xx, yy = np.meshgrid(h, h)
        rr2 = (xx ** 2 + yy ** 2).astype(float)
        prof = (1.0 + rr2 / alpha ** 2) ** (-beta)
        prof /= prof.sum()
        pos = []
        for i in range(n_stars):
            cy, cx = int(rng.integers(ext + 1, ny - ext - 1)), int(rng.integers(ext + 1, nx - ext - 1))
            pos.append((cy, cx, F[i]))
            img[cy - ext:cy + ext + 1, cx - ext:cx + ext + 1] += F[i] * prof
        if radius_mode == "adaptive":
            # pass 1: sigma_bg with 4*FWHM masks
            r1 = [4.0 * fwhm] * n_stars
            m1 = np.zeros((ny, nx), dtype=bool)
            Y, X = np.mgrid[0:ny, 0:nx]
            for (cy, cx, Fi), r in zip(pos, r1):
                m1 |= (Y - cy) ** 2 + (X - cx) ** 2 <= r ** 2
            sig1, _ = sigma_bg_estimate(img, m1, sigma_bg_true)
            # pass 2: r_local from closed form, clipped
            radii = [float(np.clip(r_local_moffat(Fi, fwhm, beta, KAPPA, sig1),
                                   max(1.5, 0.75 * fwhm), 60.0)) for (cy, cx, Fi) in pos]
        elif radius_mode == "fixed2":
            radii = [2.0] * n_stars
        elif radius_mode == "fixed60":
            radii = [60.0] * n_stars
        mask = np.zeros((ny, nx), dtype=bool)
        Y, X = np.mgrid[0:ny, 0:nx]
        for (cy, cx, Fi), r in zip(pos, radii):
            mask |= (Y - cy) ** 2 + (X - cx) ** 2 <= r ** 2
        sig, n_sky = sigma_bg_estimate(img, mask, sigma_bg_true)
        return {"sigma_hat": sig, "rel_bias": float(sig / sigma_bg_true - 1.0),
                "n_sky_used": n_sky,
                "sky_budget_violation": bool(n_sky < 9216),
                "r_median": float(np.median(radii)), "r_max_used": float(np.max(radii))}

    res["armA_adaptive_512_80"] = one_frame((512, 512), 80, 1e5, "adaptive", seed=SEED)
    res["negative_control_fixed2_Fmax1e6"] = one_frame((512, 512), 60, 1e6, "fixed2", seed=SEED + 1)
    res["negative_control_fixed60_256_50"] = one_frame((256, 256), 50, 1e5, "fixed60", seed=SEED + 2)
    res["armA_adaptive_256_50"] = one_frame((256, 256), 50, 1e5, "adaptive", seed=SEED + 3)
    # dense frame: the fixed-2px negative control must actually trip (>2%)
    res["negative_control_fixed2_dense_256_200_Fmax1e6"] = one_frame(
        (256, 256), 200, 1e6, "fixed2", seed=SEED + 4)
    res["armA_adaptive_dense_256_200_Fmax1e6"] = one_frame(
        (256, 256), 200, 1e6, "adaptive", seed=SEED + 4)

    # multi-seed spread for arm A (12 seeds, per NOISE_MODEL 11 oracle style)
    biases = []
    for s in range(SEED + 10, SEED + 22):
        biases.append(one_frame((512, 512), 80, 1e5, "adaptive", seed=s)["rel_bias"])
    res["armA_12seed_rel_bias"] = {"values": [float(b) for b in biases],
                                   "max_abs": float(np.max(np.abs(biases)))}
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()