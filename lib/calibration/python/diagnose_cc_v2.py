# -*- coding: utf-8 -*-
"""
诊断脚本v2：分析校准后Light的残留坏点特征
"""
import sys
import os
import numpy as np
import scipy.ndimage as ndi

PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
from astro_image_io import ImageReader

EXP_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "logs", "experiment")

reader = ImageReader()

# 读取校准后的Red帧
cal_path = os.path.join(EXP_DIR, "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red_calibrated.fits")
img = reader.read(cal_path)
cal = img.data.astype(np.float64)
img.close()

print("=== 校准后Red帧统计 ===")
print("min=%.4f, max=%.4f, mean=%.4f, median=%.4f, std=%.4f" % (
    cal.min(), cal.max(), cal.mean(), np.median(cal), cal.std()))

# 局部统计
med = ndi.median_filter(cal, size=5, mode="reflect")
residual = cal - med
res_med = np.median(residual)
res_mad = np.median(np.abs(residual - res_med))
res_sigma = 1.4826 * res_mad
print("\n=== 残差统计 ===")
print("median=%.4f, MAD=%.4f, sigma=%.4f" % (res_med, res_mad, res_sigma))

# 不同sigma阈值下的候选数
for sigma in [3, 5, 8, 10, 15, 20]:
    hot = residual > sigma * res_sigma
    cold = residual < -sigma * res_sigma
    # 孤立性检查：邻居中候选数=0
    kernel = np.ones((3, 3), dtype=np.float32)
    kernel[1, 1] = 0.0
    n_hot_neighbors = ndi.convolve(hot.astype(np.float32), kernel, mode="reflect")
    n_cold_neighbors = ndi.convolve(cold.astype(np.float32), kernel, mode="reflect")
    hot_isolated = hot & (n_hot_neighbors == 0)
    cold_isolated = cold & (n_cold_neighbors == 0)
    print("  %dσ: 热候选=%d, 冷候选=%d, 孤立热=%d, 孤立冷=%d" % (
        sigma, int(hot.sum()), int(cold.sum()),
        int(hot_isolated.sum()), int(cold_isolated.sum())))

# 检查孤立热像素的值分布
hot5 = residual > 5 * res_sigma
kernel = np.ones((3, 3), dtype=np.float32)
kernel[1, 1] = 0.0
n_neighbors = ndi.convolve(hot5.astype(np.float32), kernel, mode="reflect")
hot5_isolated = hot5 & (n_neighbors == 0)

if hot5_isolated.sum() > 0:
    isolated_vals = cal[hot5_isolated]
    isolated_resid = residual[hot5_isolated]
    print("\n=== 孤立热像素(5σ)值分布 ===")
    print("像素数: %d" % len(isolated_vals))
    print("原始值: min=%.2f, max=%.2f, median=%.2f" % (
        isolated_vals.min(), isolated_vals.max(), np.median(isolated_vals)))
    print("残差值: min=%.2f, max=%.2f, median=%.2f" % (
        isolated_resid.min(), isolated_resid.max(), np.median(isolated_resid)))
    # 残差/sigma分布
    ratio = isolated_resid / res_sigma
    print("残差/σ: min=%.1f, max=%.1f, median=%.1f" % (
        ratio.min(), ratio.max(), np.median(ratio)))
    # 分位数
    for p in [50, 75, 90, 95, 99]:
        print("  %d%%分位: 残差=%.2f (%.1fσ), 原始值=%.2f" % (
            p, np.percentile(isolated_resid, p),
            np.percentile(isolated_resid, p) / res_sigma,
            np.percentile(isolated_vals, p)))

# 检查冷像素
cold5 = residual < -5 * res_sigma
n_cold_neighbors = ndi.convolve(cold5.astype(np.float32), kernel, mode="reflect")
cold5_isolated = cold5 & (n_cold_neighbors == 0)

if cold5_isolated.sum() > 0:
    isolated_cold_vals = cal[cold5_isolated]
    isolated_cold_resid = residual[cold5_isolated]
    print("\n=== 孤立冷像素(5σ)值分布 ===")
    print("像素数: %d" % len(isolated_cold_vals))
    print("原始值: min=%.2f, max=%.2f, median=%.2f" % (
        isolated_cold_vals.min(), isolated_cold_vals.max(), np.median(isolated_cold_vals)))
    print("残差值: min=%.2f, max=%.2f, median=%.2f" % (
        isolated_cold_resid.min(), isolated_cold_resid.max(), np.median(isolated_cold_resid)))
