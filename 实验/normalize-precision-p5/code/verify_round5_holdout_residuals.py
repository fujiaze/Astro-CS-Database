#!/usr/bin/env python3
"""留出星残差复核（B3 修正版）: 逐像元模型加权 Poisson+RN 预期。

第一版用孔径平均通量预测残差尺度, 对亮星严重低估（窗内高信号像元的 σ 远大于
孔径均值）。本版按每个像元的模型面亮度预测 σ_j = sqrt(model_j/g + B/g + RN²/g²),
再对窗内取 10%–90% 截尾均值（与残差估计量同一域）, 才是可对拍的预期。
"""
from __future__ import annotations

import json
import math
import os
import sys

import numpy as np
from astropy.io import fits

WORK = "run/norm-precision-test/work"
GAIN = 1.0        # e-/ADU（run/norm-precision-test.json: snr.gain_e_per_adu）
RN_E = 9.0        # e（同配置 read_noise_e）

src = json.load(open(os.path.join(WORK, "p1_sources.json"), encoding="utf-8"))
fr = src["frames"][0]
pp = [p for p in fr["psf_params"]
      if all(k in p for k in ("A", "B", "cx", "cy", "sx", "sy", "theta", "flux"))]
noise_sigma = float(fr["noise_sigma"])
cand = [f for f in os.listdir(WORK) if f.startswith("cleaned_") and f.endswith(".fts")]
with fits.open(os.path.join(WORK, cand[0]), memmap=True) as hdul:
    data = hdul[0].data.astype(np.float64)
ny, nx = data.shape

order = np.argsort([-p["flux"] for p in pp])
sample = [pp[i] for i in order[:40]]
rows = []
for p in sample:
    cx, cy = p["cx"], p["cy"]
    sig = math.sqrt(p["sx"] * p["sy"])
    x0 = int(math.floor(cx - 3 * sig - 2)); x1 = int(math.ceil(cx + 3 * sig + 2))
    y0 = int(math.floor(cy - 3 * sig - 2)); y1 = int(math.ceil(cy + 3 * sig + 2))
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
    inw = r2 <= (3 * sig) ** 2
    res = (win - model)[inw]
    # 截尾稳健尺度（与残差同域: 10%–90% |r| 均值 / 0.7316727929211932）
    arr = np.sort(np.abs(res))
    k = len(arr)
    trim = float(arr[int(0.1 * k):int(0.9 * k)].mean()) / 0.7316727929211932
    # 逐像元预期 σ_j（模型加权; 同一窗口同权截尾）
    sig_j = np.sqrt(model[inw] / GAIN + p["B"] / GAIN + RN_E * RN_E / (GAIN * GAIN))
    sj = np.sort(sig_j)
    trim_pred = float(sj[int(0.1 * k):int(0.9 * k)].mean())
    rows.append((trim, trim_pred, trim / trim_pred, p["flux"],
                 float(model[inw].max())))

print("留出星窗内残差复核（cleaned 帧, 40 颗最亮留出星, 逐像元模型加权预期）")
print("  %10s %12s %12s %8s" % ("flux", "trim_sigma", "pred_sigma", "ratio"))
for r in rows[:12]:
    print("  %10.1f %12.3f %12.3f %8.3f" % (r[3], r[0], r[1], r[2]))
rt = np.array([r[2] for r in rows])
tr = np.array([r[0] for r in rows])
tp = np.array([r[1] for r in rows])
print("  ratio (trim/pred): median=%.3f  p10=%.3f  p90=%.3f  max=%.3f"
      % (np.median(rt), np.percentile(rt, 10), np.percentile(rt, 90), rt.max()))
print("  trim_sigma/帧noise_sigma: median=%.3f (亮星局域 Poisson 主导, 与帧级"
      "空天 σ 不可比)" % (np.median(tr) / noise_sigma))
print("  pred_sigma/帧noise_sigma: median=%.3f" % (np.median(tp) / noise_sigma))
