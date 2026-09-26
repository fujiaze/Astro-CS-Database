#!/usr/bin/env python3
"""EXP-P2-R1-11: star-mask radius vs sigma_bg bias (P-CST-15, P-ALG-04).

Claims (NOISE_MODEL 5a / established-doc 1.2c):
  r_local(F, FWHM, k*sigma_bg) solves surface brightness at r = k*sigma_bg;
  at r_local the residual variance contamination is k^2 = 1% => sigma bias <= 0.5%;
  at fixed r=10 px, bias grows with F (+0.02% -> +2.29% for F = 1e3 -> 1e6, log-law).
Theory leg: closed-form r_local for Moffat(beta=2.5) and Gaussian.
Experiment leg: MC sigma_bg bias vs mask radius and flux, 8 seeds.
Negative control: no source => mask radius has no effect (bias difference 0).
Seed fixed 20260926. Pure python3+numpy. Runtime < 4 min.
"""
import json, os
import numpy as np

SEED = 20260926
out = {"seed": SEED}
KAPPA = 1.482602218505602
k, SIG_BG, GAIN, RN, FWHM, BETA = 0.1, 5.0, 1.3, 10.0, 3.0, 2.5
# true blank-sky sigma of the simulated frame (Poisson sky + read noise), ADU
SIG_TRUE = float(np.sqrt(SIG_BG * GAIN / GAIN ** 2 + (RN / GAIN) ** 2))  # (sky+?)/g^2 + rn^2/g^2
SIG_TRUE = float(np.sqrt((SIG_BG * GAIN) / GAIN ** 2 + (RN / GAIN) ** 2))

# ---------- theory: closed-form r_local ----------
alpha = FWHM / (2.0 * np.sqrt(2.0 ** (1.0 / BETA) - 1.0))
def r_local_moffat(F_e, k=k, sigma_bg=SIG_TRUE):
    # SB(r) = F*(BETA-1)/(pi*alpha^2) * (1+r^2/alpha^2)^-BETA  [e/px]
    # threshold surface brightness = k * sigma_bg [ADU] = k*sigma_bg*GAIN [e/px]
    C = F_e * (BETA - 1) / (np.pi * alpha ** 2)
    return alpha * np.sqrt((C / (k * sigma_bg * GAIN)) ** (1.0 / BETA) - 1.0)
def r_local_gauss(F_e, k=k, sigma_bg=SIG_BG):
    s = FWHM / 2.3548200450309493
    return s * np.sqrt(2.0 * np.log(F_e / (2.0 * np.pi * s ** 2 * k * sigma_bg * GAIN)))
out["r_local_theory"] = {
    "sigma_true_adu": SIG_TRUE,
    "moffat2p5_F1e5": float(r_local_moffat(1e5)),
    "gauss_F1e5": float(r_local_gauss(1e5, k=k, sigma_bg=SIG_TRUE)),
    "repo_claim": 17.6,
    "note": "repo claim 17.6 px at F=1e5; exact comparison requires the repo's "
            "profile beta and sky level; both log-law variants shown.",
}
out["r_local_scan"] = {f"F{F:.0e}": float(r_local_moffat(F)) for F in (1e3, 1e4, 1e5, 1e6)}

# ---------- experiment: MC ----------
side, c0 = 256, 128
r = np.arange(side)
X, Y = np.meshgrid(r, r)
R2 = (X - c0) ** 2 + (Y - c0) ** 2
PROF = (1.0 + R2 / alpha ** 2) ** -BETA
PROF /= PROF.sum()

def frame(F_e, seed):
    g = np.random.default_rng(seed)
    sky_e = g.poisson(SIG_BG * GAIN, size=(side, side)).astype(float) / GAIN
    rn = g.normal(0.0, RN / GAIN, size=(side, side))
    src = g.poisson(F_e * PROF, size=(side, side)).astype(float) / GAIN if F_e > 0 else 0.0
    return sky_e + rn + src

def sigma_bg_est(img, mask_radius):
    yy, xx = np.ogrid[:side, :side]
    mask = (xx - c0) ** 2 + (yy - c0) ** 2 <= mask_radius ** 2
    vals = img[~mask]
    med = np.median(vals)
    return KAPPA * np.median(np.abs(vals - med))

def bias(F_e, r_mask, seeds=8):
    bs = []
    for s in range(seeds):
        img = frame(F_e, SEED + 31 * s + int(r_mask))
        sb = sigma_bg_est(img, r_mask)
        bs.append(sb / SIG_TRUE - 1.0)   # reference = the frame's true blank-sky sigma
    return float(np.mean(bs)), float(np.std(bs) / np.sqrt(seeds))

rows = []
for rm in (2.0, 6.0, 10.0, float(r_local_moffat(1e5)), 28.0, 60.0):
    b, e = bias(1e5, rm)
    rows.append({"r_mask": rm, "bias": b, "mc_err": e})
out["bias_vs_radius_F1e5"] = rows
rows_f = []
for F_e in (1e3, 1e4, 1e5, 1e6):
    b, e = bias(F_e, 10.0)
    rows_f.append({"F_e": F_e, "bias": b, "mc_err": e})
out["bias_vs_flux_r10"] = rows_f
out["repo_claims_flux_r10"] = {"F1e3": 0.0002, "F1e6": 0.0229,
                               "note": "+0.02% -> +2.29% at r=10 px"}

# negative control: no source
b0a, _ = bias(0.0, 2.0)
b0b, _ = bias(0.0, 60.0)
out["null_no_source"] = {"bias_r2": b0a, "bias_r60": b0b,
                         "difference": abs(b0a - b0b),
                         "expected": "~0 within MC error"}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp11_mask_radius_bias.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
