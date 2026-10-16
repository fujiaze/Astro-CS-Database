#!/usr/bin/env python3
"""留出星拟合优度（χ²/dof）与邻星污染诊断。

对 40 颗最亮留出星, 在 cleaned 帧上逐像元计算 (win−model)²/σ_j² 的 χ²/dof
（σ_j = sqrt(model_j/g + B/g + RN²/g²), 七参数拟合 ⇒ dof = n−7）, 并统计窗内
其它已拟合星的数量（邻星污染）。χ²/dof≈1 表示模型+噪声自洽; ≫1 归因于邻星/
模型失配; 邻星计数用于区分两者。
"""
from __future__ import annotations

import json
import math
import os

import numpy as np
from astropy.io import fits

WORK = "run/norm-precision-test/work"
GAIN, RN_E = 1.0, 9.0

src = json.load(open(os.path.join(WORK, "p1_sources.json"), encoding="utf-8"))
pp = [p for p in src["frames"][0]["psf_params"]
      if all(k in p for k in ("A", "B", "cx", "cy", "sx", "sy", "theta", "flux"))]
cand = [f for f in os.listdir(WORK) if f.startswith("cleaned_") and f.endswith(".fts")]
with fits.open(os.path.join(WORK, cand[0]), memmap=True) as hdul:
    data = hdul[0].data.astype(np.float64)
ny, nx = data.shape
stars = np.array([[p["cx"], p["cy"]] for p in pp])

order = np.argsort([-p["flux"] for p in pp])
rows = []
for i in order[:40]:
    p = pp[i]
    cx, cy = p["cx"], p["cy"]
    sig = math.sqrt(p["sx"] * p["sy"])
    rr = 3 * sig
    x0 = int(math.floor(cx - rr - 2)); x1 = int(math.ceil(cx + rr + 2))
    y0 = int(math.floor(cy - rr - 2)); y1 = int(math.ceil(cy + rr + 2))
    if x0 < 0 or y0 < 0 or x1 >= nx or y1 >= ny:
        continue
    win = data[y0:y1 + 1, x0:x1 + 1]
    yy, xx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    dx, dy = xx - cx, yy - cy
    c, s = math.cos(p["theta"]), math.sin(p["theta"])
    q = ((c * c / (2 * p["sx"] ** 2) + s * s / (2 * p["sy"] ** 2)) * dx * dx
         + 2 * (math.sin(2 * p["theta"]) / (4 * p["sx"] ** 2) -
                math.sin(2 * p["theta"]) / (4 * p["sy"] ** 2)) * dx * dy
         + (s * s / (2 * p["sx"] ** 2) + c * c / (2 * p["sy"] ** 2)) * dy * dy)
    model = p["B"] + p["A"] / (1.0 + q) ** 4
    r2 = dx * dx + dy * dy
    inw = r2 <= rr * rr
    n = int(inw.sum())
    sig_j = np.sqrt(model[inw] / GAIN + p["B"] / GAIN + RN_E * RN_E / (GAIN * GAIN))
    res = (win - model)[inw]
    chi2 = float(np.sum((res / sig_j) ** 2)) / max(n - 7, 1)
    # 邻星:  centres 在 3σ 窗内的其它拟合星
    d = np.hypot(stars[:, 0] - cx, stars[:, 1] - cy)
    neigh = int(np.count_nonzero((d < rr) & (d > 1e-6)))
    rows.append((p["flux"], chi2, neigh, n, float(np.abs(res).max()),
                 float(sig_j.max())))

print("留出星 χ²/dof 与邻星（cleaned 帧, 40 颗最亮留出星, σ_j=√(model/g+B/g+RN²)）")
print("  %10s %8s %6s %5s %10s %10s" % ("flux", "chi2/dof", "neigh", "n",
                                        "max|res|", "max σ_j"))
for r in rows[:14]:
    print("  %10.1f %8.2f %6d %5d %10.1f %10.1f" % r)
ch = np.array([r[1] for r in rows])
nb = np.array([r[2] for r in rows])
print("  chi2/dof: median=%.2f  p10=%.2f  p90=%.2f  max=%.2f"
      % (np.median(ch), np.percentile(ch, 10), np.percentile(ch, 90), ch.max()))
print("  邻星数:  median=%.1f  max=%d" % (np.median(nb), nb.max()))
iso = ch[nb == 0]
if len(iso):
    print("  无邻星子集 (n=%d): chi2/dof median=%.2f max=%.2f"
          % (len(iso), np.median(iso), iso.max()))
