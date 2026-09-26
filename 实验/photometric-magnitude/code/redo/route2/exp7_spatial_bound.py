#!/usr/bin/env python3
"""exp7_spatial_bound.py -- P1 route2 experiment 7: max|log10 m| <= 1.0 dex hard bound.

Items covered:
  S8  spatial-gain amplitude hard bound 1.0 dex (05 式-5 / 01 C3):
      - documented real contamination scale 0.1 dex (PHOTOMETRY.md 2a.5 class)
      - bound is non-binding for realistic fields; triggers only for pathological ones
      - no-effect negative: zero field => fitted amplitude at noise floor

Seed fixed. Run: python3 exp7_spatial_bound.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)
MAD_SCALE = 0.6744897501960817
TUKEY_C = 4.685

def basis(order, x, y):
    """Quadratic (order<=2) 2D polynomial basis, normalized coords [-1,1]."""
    cols = [np.ones_like(x)]
    if order >= 1:
        cols += [x, y]
    if order >= 2:
        cols += [x * x, x * y, y * y]
    return np.stack(cols, axis=1)

def fit_spatial_irls(x, y, r, order=2, c=TUKEY_C, iters=50):
    """Equal-weight IRLS + Tukey on r(x,y) over polynomial basis (式-3 style)."""
    B = basis(order, x, y)
    w = np.ones(r.size)
    coef = np.linalg.lstsq(B * w[:, None], r, rcond=None)[0]
    for _ in range(iters):
        model = B @ coef
        resid = r - model
        med = float(np.median(resid))
        mad = float(np.median(np.abs(resid - med)))
        S = mad / MAD_SCALE if mad > 0 else 0.0
        if S <= 0:
            break
        u = resid / (c * S)
        w = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
        if w.sum() <= 0:
            break
        coef = np.linalg.lstsq(B * w[:, None], r, rcond=None)[0]
    return coef, B

n_stars = 300
x = rng.uniform(-1, 1, n_stars)
y = rng.uniform(-1, 1, n_stars)

table = {}
for A_dex in [0.0, 0.03, 0.05, 0.1, 0.3, 1.0, 1.5]:
    amps = []
    for t in range(100):
        field = A_dex * (x * x - 0.3 * y * y)          # smooth quadratic field, range ~A
        r = rng.standard_normal(n_stars) * 0.02 + field  # 0.02 dex per-star scatter
        coef, B = fit_spatial_irls(x, y, r)
        m = B @ coef                                    # fitted m(x,y) in dex
        amps.append(float(np.max(np.abs(m))))
    table["A=%.2f" % A_dex] = {
        "median_max_abs_log10m_dex": float(np.median(amps)),
        "p90_max_abs_log10m_dex": float(np.percentile(amps, 90)),
        "bound_1.0dex_binding": bool(np.percentile(amps, 90) >= 1.0),
    }
    print("A=%.2f done" % A_dex, flush=True)

out = {
    "seed": SEED,
    "setup": {"n_stars": n_stars, "scatter_dex": 0.02, "order": 2,
              "field_shape": "A*(x^2-0.3y^2) on [-1,1]^2"},
    "S8_amplitude_sweep": table,
    "S8_reading": {
        "documented_real_scale_dex": 0.1,
        "bound_vs_documented_scale_ratio": 1.0 / 0.1,
        "non_binding_for_documented_scale": not table["A=0.10"]["bound_1.0dex_binding"],
    },
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp7_spatial_bound.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

