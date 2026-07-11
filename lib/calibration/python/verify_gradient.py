# -*- coding: utf-8 -*-
"""
验证脚本：比较校准前后的背景梯度变化，判断 Flat 校准效果
功能：读取原始 Light、校准后、坏点修复后 FITS，比较四角与中心均值差
用途：Task 7 背景均匀性检查点辅助验证
"""
import sys
import os
import numpy as np

PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
from astro_image_io import ImageReader

EXP_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "logs", "experiment")
LIGHTS_DIR = os.path.join(PROJECT_ROOT, "testdata", "T4_data", "lights", "panel1")

reader = ImageReader()

files = {
    "原始": os.path.join(LIGHTS_DIR, "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"),
    "校准后": os.path.join(EXP_DIR, "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red_calibrated.fits"),
    "坏点修复后": os.path.join(EXP_DIR, "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red_final.fits"),
}

margin = 200  # 用更大的边距减少星点影响

print("=== 背景梯度比较（margin=%d）===" % margin)
print("%-12s %10s %10s %10s %10s %10s %10s" % ("阶段", "TL", "TR", "BL", "BR", "Center", "最大差异%"))

for label, path in files.items():
    img = reader.read(path)
    data = img.data.astype(np.float64)
    h, w = data.shape
    corners = {
        "TL": float(np.mean(data[:margin, :margin])),
        "TR": float(np.mean(data[:margin, -margin:])),
        "BL": float(np.mean(data[-margin:, :margin])),
        "BR": float(np.mean(data[-margin:, -margin:])),
    }
    center = float(np.mean(data[h // 2 - margin:h // 2 + margin, w // 2 - margin:w // 2 + margin]))
    all_vals = list(corners.values()) + [center]
    max_diff = (max(all_vals) - min(all_vals)) / abs(center) * 100 if center != 0 else 0
    print("%-12s %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f" % (
        label, corners["TL"], corners["TR"], corners["BL"], corners["BR"], center, max_diff))
    img.close()

print("\n说明：如果校准后最大差异% 小于原始，说明 Flat 校准有效（降低了梯度）。")
print("真实天文图像含天体信号，绝对值<5%阈值可能过于严格。")
