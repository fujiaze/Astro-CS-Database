#!/usr/bin/env python3
"""留出星 χ²/dof 流量分层复核: F_ref=1000 ADU 量级（参考精度实际关心的人群）
vs 亮星（Trapezium 混合星结构）。

分层: [200,2000) / [2000,10000) / [10000,50000) / [50000,300000) / ≥300000 ADU。
每层报告 χ²/dof 中位数与最大邻星污染, 用以区分「模型+噪声自洽」的人群与
「混合星/复杂结构」的人群——前者支撑参考 SNR 的通量标度真实性, 后者是 known
observation condition（星密高）, 不是求解器缺陷。
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

bins = [(200, 2000), (2000, 10000), (10000, 50000), (50000, 300000), (300000, 1e12)]
rng = np.random.default_rng(20261016)
print("留出星 χ²/dof 流量分层（cleaned 帧; σ_j=√(model/g+B/g+RN²); dof=n−7）")
for lo, hi in bins:
    idx = [i for i, p in enumerate(pp) if lo <= p["flux"] < hi]
    if not idx:
        continue
    sel = rng.choice(idx, size=min(25, len(idx)), replace=False)
    ch, nb = [], []
    for i in sel:
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
        inw = (dx * dx + dy * dy) <= rr * rr
        n = int(inw.sum())
        if n < 12:
            continue
        sig_j = np.sqrt(model[inw] / GAIN + p["B"] / GAIN + RN_E ** 2 / GAIN ** 2)
        res = (win - model)[inw]
        ch.append(float(np.sum((res / sig_j) ** 2)) / max(n - 7, 1))
        d = np.hypot(stars[:, 0] - cx, stars[:, 1] - cy)
        nb.append(int(np.count_nonzero((d < rr) & (d > 1e-6))))
    if ch:
        ch = np.array(ch); nb = np.array(nb)
        print("  flux [%8.0f,%8.0f): n=%2d  chi2/dof median=%7.2f  p90=%8.2f  "
              "max=%9.2f  邻星 median=%.1f max=%d"
              % (lo, hi, len(ch), np.median(ch), np.percentile(ch, 90),
                 ch.max(), np.median(nb), nb.max()))
