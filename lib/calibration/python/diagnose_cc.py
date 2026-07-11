# -*- coding: utf-8 -*-
"""
诊断脚本：分析坏点检测问题
功能：检查 Dark 帧数据范围、局部统计检测的星点误删问题
"""
import sys
import os
import numpy as np
import scipy.ndimage as ndi

PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
from astro_image_io import ImageReader

EXP_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "logs", "experiment")
CALIB_DIR = os.path.join(PROJECT_ROOT, "testdata", "T4_data", "calibration files")

reader = ImageReader()

# 1. 检查 Dark 帧数据范围
dark_path = os.path.join(CALIB_DIR, "masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf")
img = reader.read(dark_path)
dark = img.data.astype(np.float64)
img.close()
print("=== Dark 帧统计 ===")
print("min=%.6f, max=%.6f, mean=%.6f, median=%.6f, std=%.6f" % (
    dark.min(), dark.max(), dark.mean(), np.median(dark), dark.std()))
dark_med = np.median(dark)
dark_mad = np.median(np.abs(dark - dark_med))
print("全局 MAD=%.6f, sigma=%.6f, 3sigma阈值=%.6f" % (
    dark_mad, 1.4826 * dark_mad, dark_med + 3 * 1.4826 * dark_mad))
print("Dark 帧中 > 阈值的像素数: %d / %d (%.2f%%)" % (
    np.sum(dark > dark_med + 3 * 1.4826 * dark_mad),
    dark.size,
    100.0 * np.sum(dark > dark_med + 3 * 1.4826 * dark_mad) / dark.size))

# 2. 检查局部统计 vs 全局统计
# 取一个 200x200 的区域分析
region = dark[1000:1200, 1000:1200]
local_med = ndi.median_filter(region, size=5)
local_resid = region - local_med
local_mad = np.median(np.abs(local_resid - np.median(local_resid)))
print("\n=== 局部 vs 全局统计（200x200 区域）===")
print("局部 MAD=%.6f, 全局 MAD=%.6f" % (local_mad, dark_mad))
print("局部 sigma=%.6f, 全局 sigma=%.6f" % (1.4826 * local_mad, 1.4826 * dark_mad))

# 3. 检查校准后图像的局部检测问题
cal_path = os.path.join(EXP_DIR, "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red_calibrated.fits")
img = reader.read(cal_path)
cal = img.data.astype(np.float64)
img.close()

print("\n=== 校准后图像统计 ===")
print("min=%.4f, max=%.4f, mean=%.4f, median=%.4f, std=%.4f" % (
    cal.min(), cal.max(), cal.mean(), np.median(cal), cal.std()))

# 4. 模拟局部检测，看星点是否被误删
# 找一个有暗弱星点的区域（高值孤立区域）
local_med_cal = ndi.median_filter(cal, size=5)
residual = cal - local_med_cal
res_med = np.median(residual)
res_mad = np.median(np.abs(residual - res_med))
res_sigma = 1.4826 * res_mad
print("\n=== 校准后图像局部检测统计 ===")
print("残差 median=%.4f, MAD=%.4f, sigma=%.4f" % (res_med, res_mad, res_sigma))
print("3sigma 阈值: 残差 > %.4f" % (3 * res_sigma))
print("5sigma 阈值: 残差 > %.4f" % (5 * res_sigma))

# 统计不同阈值下的检测数
for sigma in [3, 5, 8, 12]:
    hot = residual > sigma * res_sigma
    # 过滤大结构（星点）
    structure = np.ones((3, 3), dtype=int)
    labeled, n = ndi.label(hot, structure=structure)
    sizes = np.bincount(labeled.ravel())
    # 只保留 size < 4 的
    small_mask = sizes < 4
    small_mask[0] = False
    small_hot = small_mask[labeled]
    n_small = int(small_hot.sum())
    print("  %dσ: 原始=%d, 过滤后(小结构)=%d" % (sigma, int(hot.sum()), n_small))

# 5. 检查星点区域的残差特征
# 找几个亮像素（可能是星点中心）
print("\n=== 星点 vs 坏点形态分析 ===")
# 找局部峰值
from scipy.ndimage import maximum_filter
local_max = maximum_filter(cal, size=7)
peaks = (cal == local_max) & (cal > cal.mean() + 5 * cal.std())
peak_coords = np.argwhere(peaks)
print("检测到 %d 个局部峰值（可能的星点）" % len(peak_coords))

# 分析前10个峰值的形态
if len(peak_coords) > 0:
    print("\n前10个峰值的形态分析:")
    print("%-6s %-10s %-10s %-10s %-10s %-10s" % ("序号", "峰值", "3x3中值", "残差", "残差/sigma", "邻居>中值数"))
    for i, (y, x) in enumerate(peak_coords[:10]):
        patch = cal[max(0,y-2):y+3, max(0,x-2):x+3]
        patch_med = np.median(patch)
        resid = cal[y, x] - patch_med
        # 统计8个邻居中有几个也高于局部中值
        neighbors = []
        for dy in [-1, 0, 1]:
            for dx in [-1, 0, 1]:
                if dy == 0 and dx == 0:
                    continue
                ny, nx = y + dy, x + dx
                if 0 <= ny < cal.shape[0] and 0 <= nx < cal.shape[1]:
                    neighbors.append(cal[ny, nx] > patch_med)
        n_above = sum(neighbors)
        print("%-6d %-10.2f %-10.2f %-10.2f %-10.2f %-10d" % (
            i+1, cal[y, x], patch_med, resid, resid / res_sigma if res_sigma > 0 else 0, n_above))
