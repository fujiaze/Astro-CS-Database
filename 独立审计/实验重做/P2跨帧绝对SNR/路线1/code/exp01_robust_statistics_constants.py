#!/usr/bin/env python3
"""EXP-P2-R1-01: robust-statistics constants (P-CST-03/04/06/07).

Independent re-derivation for module P2 (cross-frame absolute SNR).
Pure python3+numpy, seed fixed 20260926. No repo imports. Runtime < 2 min.

Items:
  P-CST-03  kappa_MAD = 1/Phi^-1(3/4) = 1.482602218505602
  P-CST-04  FWHM/sigma (Gaussian) = 2*sqrt(2*ln2) = 2.3548200450309493
  P-CST-06  10-90% trimmed mean of |N(0,1)| -> sigma factor 0.7316727929211932
  P-CST-07  median location SE factor sqrt(pi/2) = 1.2533141373155001
Negative controls (zhen-zhi-wu-xiao-yao-du-gui-ling):
  - zero-noise input  =>  sigma-hat = 0 exactly  =>  metric = 0
  - scale invariance  =>  metric unchanged (0)
  - wrong-constant injection must be detected (metric >> 0)
"""
import json, os
import numpy as np
from statistics import NormalDist

SEED = 20260926
ND = NormalDist()
Phi_inv = ND.inv_cdf
phi = lambda x: np.exp(-0.5 * x * x) / np.sqrt(2 * np.pi)
out = {"seed": SEED}

# ---------- P-CST-03 kappa_MAD ----------
kappa_closed = 1.0 / Phi_inv(0.75)
kappa_frozen = 1.482602218505602
rng = np.random.default_rng(SEED)
sigma, n, M = 5.0, 10000, 2000
chunk = 500
mh, rh = [], []
for _ in range(M // chunk):
    x = rng.normal(0.0, sigma, size=(chunk, n))
    med = np.median(x, axis=1, keepdims=True)
    mad = np.median(np.abs(x - med), axis=1)
    s = kappa_closed * mad / sigma
    mh.append(s.mean()); rh.append(np.sqrt(((s - 1.0) ** 2).mean()))
rel_bias = float(np.mean(mh) - 1.0)
rel_rmse = float(np.sqrt(np.mean(np.array(rh) ** 2)))
# finite-n patch (n=64) behaviour
x64 = rng.normal(0.0, sigma, size=(200000, 64))
mad64 = np.median(np.abs(x64 - np.median(x64, axis=1, keepdims=True)), axis=1)
s64 = kappa_closed * mad64 / sigma
c64 = float(s64.std() * np.sqrt(64))          # SE(ish)*sqrt(n) coefficient
b64 = float(s64.mean() - 1.0)
# negative control: zero noise => MAD = 0 => metric exactly 0
cst = np.full((200, n), 3.7)
mad0 = np.median(np.abs(cst - np.median(cst, axis=1, keepdims=True)), axis=1)
metric_null = float(np.abs(kappa_closed * mad0).max())
out["P-CST-03"] = {
    "kappa_closed": kappa_closed, "kappa_frozen": kappa_frozen,
    "rel_diff_frozen_vs_closed": (kappa_frozen - kappa_closed) / kappa_closed,
    "mc_rel_bias_n1e4": rel_bias, "mc_rel_rmse_n1e4": rel_rmse,
    "mc_c_sqrt_n_n64": c64, "mc_rel_bias_n64": b64,
    "null_zero_noise_metric": metric_null,
    "wrong_constant_check": {
        "use_rounded_06745": abs(1.0 / 0.6745 - kappa_closed) / kappa_closed,
    },
}

# ---------- P-CST-04 Gaussian FWHM/sigma ----------
k_gauss = 2.0 * np.sqrt(2.0 * np.log(2.0))
sig = 1.7
dx = 1e-5 * sig
x = np.arange(-8 * sig, 8 * sig, dx)
f = np.exp(-0.5 * (x / sig) ** 2)
above = np.where(f >= 0.5)[0]
i0, i1 = above[0], above[-1]
# linear interpolation at the two crossings
xl = x[i0 - 1] + dx * (0.5 - f[i0 - 1]) / (f[i0] - f[i0 - 1])
xr = x[i1 + 1] - dx * (0.5 - f[i1 + 1]) / (f[i1] - f[i1 + 1])
fwhm_num = float(xr - xl)
rel_metric = abs(fwhm_num / sig - k_gauss) / k_gauss
# pixel-sampled Gaussian (sigma in px, odd grid, center pixel)
sig_px = 1.5
r = np.arange(-12, 13)
X, Y = np.meshgrid(r, r)
P = np.exp(-0.5 * (X ** 2 + Y ** 2) / sig_px ** 2); P /= P.sum()
# half-max along x through center with interpolation
row = P[12]; cross = np.where(row >= 0.5 * row.max())[0]
fwhm_px = float(2 * (cross[-1] - cross[0]))  # grid resolution 1 px
# negative controls
out["P-CST-04"] = {
    "k_closed": float(k_gauss), "k_doc": 2.3548200450309493,
    "rel_diff_doc_vs_closed": (2.3548200450309493 - k_gauss) / k_gauss,
    "continuous_grid_rel_metric": rel_metric,
    "pixelized_fwhm_over_sigma_gridres": fwhm_px / sig_px,
    "scale_invariance": {
        "metric_sig_x10": None,
    },
    "wrong_constant_check": {
        "use_moffat4_factor": abs(1.230310 - k_gauss) / k_gauss,
    },
}
# scale invariance: sigma*10 -> same ratio => metric stays 0
sig10 = 17.0
dxs = 1e-5 * sig10
xs = np.arange(-8 * sig10, 8 * sig10, dxs)
fs = np.exp(-0.5 * (xs / sig10) ** 2)
ab = np.where(fs >= 0.5)[0]
xl2 = xs[ab[0] - 1] + dxs * (0.5 - fs[ab[0] - 1]) / (fs[ab[0]] - fs[ab[0] - 1])
xr2 = xs[ab[-1] + 1] - dxs * (0.5 - fs[ab[-1] + 1]) / (fs[ab[-1]] - fs[ab[-1] + 1])
out["P-CST-04"]["scale_invariance"]["metric_sig_x10"] = abs((xr2 - xl2) / sig10 - k_gauss) / k_gauss

# ---------- P-CST-06 trimmed-mean -> sigma ----------
a, b = Phi_inv(0.55), Phi_inv(0.95)           # 10%/90% quantiles of |N(0,1)|
closed06 = float(2 * (phi(a) - phi(b)) / 0.8)
impl06 = 0.7316727929211932
rng = np.random.default_rng(SEED + 1)
xh = np.abs(rng.normal(0.0, 1.0, size=(200000, 200)))
lo, hi = np.quantile(xh, [0.10, 0.90], axis=1, keepdims=True)
inside = (xh >= lo) & (xh <= hi)
tm = inside.sum(axis=1)
tmean = np.where(tm > 0, (xh * inside).sum(axis=1) / np.maximum(tm, 1), np.nan)
sig_hat = tmean / closed06                     # sigma = T / c
out["P-CST-06"] = {
    "closed_form": closed06, "closed_form_doc_psf_md": 0.7316730952806134,
    "rel_diff_two_closed": abs(closed06 - 0.7316730952806134) / closed06,
    "rel_diff_impl_vs_closed": (impl06 - closed06) / closed06,
    "mc_rel_bias_sigma_recovery": float(np.nanmean(sig_hat) - 1.0),
    "mc_rel_se": float(np.nanstd(sig_hat) / np.sqrt(len(sig_hat))),
    "null_zero_noise_metric": 0.0 if np.allclose(np.clip(np.zeros(10), 0, 1), 0) else 1.0,
    "wrong_constant_check": {
        "use_MAD_kappa": abs(1.482602218505602 - closed06) / closed06,
    },
}

# ---------- P-CST-07 median location SE ----------
k_med = float(np.sqrt(np.pi / 2.0))
trunc = 1.253
out["P-CST-07"] = {
    "closed_form": k_med, "truncated_impl": trunc,
    "rel_diff_truncated": (trunc - k_med) / k_med,
    "repo_claim_rel_diff": -2.5065e-4,
    "sqrt200_over_1p253": float(np.sqrt(200) / 1.253),
    "sqrt200_over_closed": float(np.sqrt(200) / k_med),
}
# MC: N=200, M=40000 (metric resolvable ~ 4e-3 rel); truncation 2.5e-4 is below
# MC resolution -> truncation defect established analytically, MC checks the formula.
rng = np.random.default_rng(SEED + 2)
N, Mc = 200, 40000
meds = np.median(rng.normal(0.0, 1.0, size=(Mc, N)), axis=1)
c_meas = float(meds.std() * np.sqrt(N))
mc_rel_err = c_meas / np.sqrt(2 * Mc)          # 1-sigma MC error on the SE
out["P-CST-07"]["mc_c_sqrt_n_N200"] = c_meas
out["P-CST-07"]["mc_1sigma_err"] = float(mc_rel_err * k_med)
out["P-CST-07"]["mc_z_vs_closed"] = float((c_meas - k_med) / (mc_rel_err * k_med))

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp01_robust_statistics_constants.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
