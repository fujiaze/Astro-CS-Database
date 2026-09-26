#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04: reference magnitude m_ref (P-CST-19) + end-to-end chain interface use case.

Chain position (P1 -> P2 -> P3/P4):
  upstream P1 (photometric magnitude coordinate system) supplies per-frame zero point ZP_k [mag];
  P2 consumes ZP_k -> F_ref,k = 10^(-0.4*(m_ref - ZP_k)) [ADU], frame_snr (dimensionless),
     and sparse absolute-SNR control points (dimensionless);
  downstream P3/P4 consume the sparse SNR control points (IDW) to a dense SNR field and
     the inverse-variance weights for stacking.

Hypotheses:
  H1 monotonicity/scaling: frame_snr(m_ref) is monotonically decreasing in m_ref with
     ratio 10^(-0.4) = 0.39811 per mag (delta frame_snr values 2.512x per mag; 6.31x per 2 mag),
     exactly as the 05 spec's pending-confirmation entry claims.
  H2 pairing invariance: w_k = SNR_k^2/F_ref,k^2 = 1/sigma_F,k^2 independent of m_ref AND ZP_k
     (the numerator and denominator of the weight are same-frame paired quantities).
  H3 interface: with control-point SNR noise set to the P-CST-08 budget (1.5% at N_sky=9216),
     the downstream IDW reconstruction of the dense SNR field reaches ~1% relative RMS on a
     smooth field (precision convention passes through the interface).

Pure python + numpy. Seeds hardcoded. Runtime << 5 min.
Output: ../results/exp04_refmag_chain.json
"""
from __future__ import annotations

import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

# ---------- upstream P1: per-frame zero points ----------

n_frames = 8
zp_true = 25.0
zp_scatter = 0.02
zp_k = zp_true + rng.normal(0.0, zp_scatter, n_frames)   # P1 product: frame ZP [mag]
sigma_f_k = np.array([0.05, 0.048, 0.052, 0.051, 0.049, 0.053, 0.050, 0.047])  # P2 noise model output [ADU]

# ---------- H1: monotonicity + per-mag scaling ----------

h1 = {}
m_grid = [4.0, 5.0, 6.0, 7.0, 8.0, 9.0]
snr_by_m = []
for m_ref in m_grid:
    f_ref = 10.0 ** (-0.4 * (m_ref - zp_k))
    snr = f_ref / sigma_f_k
    snr_by_m.append(float(snr.mean()))
snr_by_m = np.array(snr_by_m)
ratios = snr_by_m[:-1] / snr_by_m[1:]
h1 = {
    "snr_by_m_ref": dict(zip(map(str, m_grid), snr_by_m.tolist())),
    "is_monotonic_decreasing": bool(np.all(np.diff(snr_by_m) < 0)),
    "per_mag_ratio_measured": float(np.prod(ratios) ** (1.0 / len(ratios))),
    "per_mag_ratio_theory": 10.0 ** 0.4,
    "two_mag_ratio_measured": float(snr_by_m[0] / snr_by_m[2]),
    "two_mag_ratio_theory": 10.0 ** 0.8,
    "frame_snr_at_m_ref_6_first3": [float(s) for s in (10.0 ** (-0.4 * (6.0 - zp_k)) / sigma_f_k)[:3]],
}

# ---------- H2: pairing invariance of the weight ----------

m_ref = 6.0
f_ref = 10.0 ** (-0.4 * (m_ref - zp_k))
snr_k = f_ref / sigma_f_k
w_pair = (snr_k ** 2) / (f_ref ** 2)
w_direct = 1.0 / (sigma_f_k ** 2)
h2 = {"max_rel_dev": float(np.abs(w_pair / w_direct - 1.0).max()),
      "invariant_to_zp_and_mref": bool(np.abs(w_pair / w_direct - 1.0).max() < 1e-15)}
# invariance across m_ref choices
devs = []
for m in m_grid:
    fr = 10.0 ** (-0.4 * (m - zp_k))
    s = fr / sigma_f_k
    devs.append(float(np.abs((s ** 2 / fr ** 2) / w_direct - 1.0).max()))
h2["max_rel_dev_across_m_grid"] = max(devs)

# ---------- H3: downstream interface (sparse SNR control points -> dense SNR field) ----------

frame = 512
delta = 64                       # P-CST-16: hips.tile_width(512)/8, structural constant
yy, xx = np.mgrid[0:frame, 0:frame].astype(float)
true_field = 20.0 * (1.0 + 0.3 * np.sin(2 * np.pi * xx / frame) * np.cos(2 * np.pi * yy / frame))
ctrl_pts = []
vals = []
ctrl_noise_rel = 0.015            # P-CST-08 budget: SE/sigma at N_sky = 9216
rng3 = np.random.default_rng(SEED + 7)
for cy in range(delta // 2, frame, delta):
    for cx in range(delta // 2, frame, delta):
        ctrl_pts.append((cx, cy))
        vals.append(true_field[cy, cx] * (1.0 + rng3.normal(0.0, ctrl_noise_rel)))
ctrl_pts = np.array(ctrl_pts, float)
vals = np.array(vals)

def idw_reconstruct(pts, v, grid_xx, grid_yy, power=2.0, k=8):
    out = np.empty(grid_xx.shape)
    flat_g = np.stack([grid_xx.ravel(), grid_yy.ravel()], axis=1)
    for i, g in enumerate(flat_g):
        d2 = ((pts - g) ** 2).sum(axis=1)
        order = np.argsort(d2)[:k]
        w = 1.0 / np.maximum(d2[order], 1e-12) ** (power / 2.0)
        out.ravel()[i] = float((w * v[order]).sum() / w.sum())
    return out

rec = idw_reconstruct(ctrl_pts, vals, xx, yy)
rel_rms = float(np.sqrt(((rec - true_field) ** 2).mean()) / true_field.std())
# zero-effect negative: noiseless control points => reconstruction error is pure IDW discretization
rec0 = idw_reconstruct(ctrl_pts, true_field[ctrl_pts[:, 1].astype(int), ctrl_pts[:, 0].astype(int)], xx, yy)
rel_rms_noiseless = float(np.sqrt(((rec0 - true_field) ** 2).mean()) / true_field.std())

h3 = {"n_control_points": int(len(vals)), "delta_px": delta,
      "control_noise_rel": ctrl_noise_rel,
      "dense_rel_rms_with_ctrl_noise": rel_rms,
      "dense_rel_rms_noiseless_negative": rel_rms_noiseless,
      "noise_inflation_over_noiseless": rel_rms / max(rel_rms_noiseless, 1e-12)}

out = {
    "experiment": "P2-route3 EXP-04 reference magnitude + chain interface",
    "seed": SEED,
    "upstream_P1": {"zp_k": [float(z) for z in zp_k], "zp_scatter_mag": zp_scatter},
    "H1_monotonic_scaling": h1,
    "H2_pairing_invariance": h2,
    "H3_downstream_interface": h3,
}
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, "..", "results", "exp04_refmag_chain.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps(out, indent=1, ensure_ascii=False))
