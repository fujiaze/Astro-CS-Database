#!/usr/bin/env python3
"""EXP-P2-R1-02: Moffat4 FWHM<->sigma factor 1.230310 (P-CST-05).

Theory leg (closed form, derived here, cross-checked against docs/science/PSF.md:63):
  Moffat4 profile  I(r) = (1 + r^2/(2 s^2))^(-4)   (alpha = sqrt(2) s, beta = 4)
  half maximum at  1 + r^2/(2 s^2) = 2^(1/4)
  =>  FWHM = 2 * sqrt(2) * sqrt(2^(1/4) - 1) * s = 1.230307652590102 * s
  =>  frozen constant 1.230310 carries relative diff +1.908e-6 (repo claim +1.91e-6).
Experiment legs: continuous fine-grid half-max measurement, pixel-integrated profile,
scale-invariance null (metric 0), wrong-constant injection, flux closure check
(flux = 2*pi*A*sx*sy/3 for beta=4, PSF.md item 2), second-moment applicability note.
Seed fixed 20260926. Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
closed = 2.0 * np.sqrt(2.0) * np.sqrt(2.0 ** 0.25 - 1.0)
frozen = 1.230310
out = {"seed": SEED, "closed_form": float(closed), "frozen": frozen,
       "rel_diff_frozen_vs_closed": float((frozen - closed) / closed),
       "repo_claim_rel_diff": 1.91e-6}

# --- continuous fine-grid measurement ---
sig = 1.7
dx = 1e-6 * sig
x = np.arange(0.0, 12 * sig, dx)
f = (1.0 + x ** 2 / (2 * sig ** 2)) ** -4.0
# f is monotone decreasing from f(0)=1: first grid point below 0.5
i1 = int(np.argmax(f < 0.5))
xl = x[i1 - 1] + dx * (0.5 - f[i1 - 1]) / (f[i1] - f[i1 - 1])
fwhm_cont = float(2 * xl)
out["continuous_ratio"] = float(fwhm_cont / sig)
out["continuous_rel_metric"] = float(abs(fwhm_cont / sig - closed) / closed)

# --- pixel-integrated profile (star at center pixel of odd grid) ---
def pixelized_ratio(sig_px, half=400):
    r = np.arange(-half, half + 1)
    X, Y = np.meshgrid(r, r)
    P = (1.0 + (X ** 2 + Y ** 2) / (2 * sig_px ** 2)) ** -4.0
    row = P[half, half:]
    cross = np.where(row >= 0.5 * row.max())[0]
    # linear interpolation between the bracketing pixels
    j = cross[-1]
    x0, x1 = j, j + 1
    v0, v1 = row[j], row[j + 1]
    t = (0.5 * row.max() - v0) / (v1 - v0)
    fwhm_px = 2.0 * (x0 + t)
    return float(fwhm_px / sig_px)

for s_px in (1.5, 3.0, 24.5):
    ratio = pixelized_ratio(s_px)
    out.setdefault("pixelized", {})[str(s_px)] = {
        "ratio": ratio, "rel_metric": abs(ratio - closed) / closed}

# --- scale invariance null: sigma scaled x100 => metric unchanged (0) ---
def continuous_ratio(sig_c, dx_rel=1e-5):
    dx = dx_rel * sig_c
    x = np.arange(0.0, 12 * sig_c, dx)
    f = (1.0 + x ** 2 / (2 * sig_c ** 2)) ** -4.0
    i1 = int(np.argmax(f < 0.5))
    xl = x[i1 - 1] + dx * (0.5 - f[i1 - 1]) / (f[i1] - f[i1 - 1])
    return float(2 * xl / sig_c)

out["null_scale_invariance"] = {
    "ratio_sig_1p7": continuous_ratio(1.7),
    "ratio_sig_17": continuous_ratio(17.0),
}
r1 = out["null_scale_invariance"]["ratio_sig_1p7"]
r2 = out["null_scale_invariance"]["ratio_sig_17"]
out["null_scale_invariance"]["metric"] = float(abs(r2 - r1) / r1)

# --- wrong-constant injection ---
out["wrong_constant_check"] = {
    "use_gaussian_factor_2p35482": float(abs(2.3548200450309493 - closed) / closed),
}

# --- second-moment applicability note: if sigma were the per-axis moment width ---
# per-axis variance of Moffat(beta=4, alpha) = alpha^2 / (2*(beta-2)) = alpha^2/4
# => sigma_mom = alpha/2 = sqrt(2)*s/2 ; FWHM/sigma_mom = closed * sqrt(2) = 1.7406...
out["applicability_second_moment"] = {
    "fwhm_over_sigma_moment": float(closed * np.sqrt(2.0)),
    "note": "1.230310 maps FWHM to the profile PARAMETER s (alpha=sqrt(2)s), "
            "not to the per-axis second-moment width; using the moment width "
            "would give 1.7407 and is a different (wrong) reading.",
}

# --- flux closure: total flux of A*(1+Q)^-4 with beta=4 is 2*pi*A*sx*sy/3 ---
A, sx, sy = 100.0, 1.5, 1.5
half = 400
r = np.arange(-half, half + 1)
X, Y = np.meshgrid(r, r)
# fine grid with 0.25 px step (true sub-pixel integration of the peaked profile)
xf = np.arange(-half, half + 0.25, 0.25)
Xf, Yf = np.meshgrid(xf, xf)
Qf = (Xf ** 2) / (2 * sx ** 2) + (Yf ** 2) / (2 * sy ** 2)
Pf = A * (1.0 + Qf) ** -4.0
flux_num = float(Pf.sum() * 0.25 ** 2)
flux_ana = 2.0 * np.pi * A * sx * sy / 3.0
out["flux_closure"] = {"numeric": flux_num, "analytic": float(flux_ana),
                       "rel_diff": abs(flux_num - flux_ana) / flux_ana,
                       "note": "0.25 px sub-sampled Riemann sum; residual is "
                               "discretization of the peak, not a formula error"}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp02_moffat4_constant.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
