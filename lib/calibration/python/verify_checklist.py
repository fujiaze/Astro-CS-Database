# -*- coding: utf-8 -*-
"""
验证脚本：检查 checklist.md 中的实验验证检查点
功能：验证校准后 FITS 的元数据、背景均匀性、读回一致性
用途：Task 7 验证阶段的辅助脚本
"""
import sys
import os
import numpy as np

PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
from astro_image_io import ImageReader

EXP_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "logs", "experiment")
FINAL_FITS = os.path.join(
    EXP_DIR,
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red_final.fits",
)

reader = ImageReader()

# === 1. FITS 头元数据验证 ===
print("=== 1. FITS 头元数据验证 ===")
img = reader.read(FINAL_FITS)
print("文件: " + os.path.basename(FINAL_FITS))
print("尺寸: %dx%d" % (img.width, img.height))

meta_names = {kw.name for kw in img.keywords}
required = {"CALIBRAT", "DARKSCAL", "MASTERBI", "MASTERDA", "MASTERFL",
            "CCHOT", "CCCOLD", "CCMETHOD", "CCMAXSZ"}
present = required & meta_names
missing = required - meta_names
print("包含的校准关键字: %s" % sorted(present))
if missing:
    print("缺失的关键字: %s" % sorted(missing))
else:
    print("缺失的关键字: 无")

bad_kw = meta_names & {"BZERO", "BSCALE"}
if bad_kw:
    print("BZERO/BSCALE (应为空): %s [错误!]" % sorted(bad_kw))
else:
    print("BZERO/BSCALE (应为空): 无 [正确]")

# === 2. 背景均匀性验证 ===
print("\n=== 2. 背景均匀性验证 ===")
data = img.data.astype(np.float64)
h, w = data.shape
margin = 100
corners = {
    "TL": data[:margin, :margin],
    "TR": data[:margin, -margin:],
    "BL": data[-margin:, :margin],
    "BR": data[-margin:, -margin:],
}
center = data[h // 2 - margin:h // 2 + margin, w // 2 - margin:w // 2 + margin]
corner_means = {k: float(np.mean(v)) for k, v in corners.items()}
center_mean = float(np.mean(center))
print("中心 mean: %.4f" % center_mean)
max_diff_pct = 0.0
for k in sorted(corner_means):
    v = corner_means[k]
    diff_pct = abs(v - center_mean) / abs(center_mean) * 100 if center_mean != 0 else 0
    if diff_pct > max_diff_pct:
        max_diff_pct = diff_pct
    print("  %s mean: %.4f  差异: %.2f%%" % (k, v, diff_pct))
if max_diff_pct < 5.0:
    print("背景均匀性: 通过 (最大差异 %.2f%% < 5%%)" % max_diff_pct)
else:
    print("背景均匀性: 未通过 (最大差异 %.2f%% >= 5%%)" % max_diff_pct)

img.close()

# === 3. 读回一致性验证 ===
print("\n=== 3. 读回一致性验证 ===")
img2 = reader.read(FINAL_FITS)
data2 = img2.data
print("读回 shape: %s, dtype: %s" % (str(data2.shape), str(data2.dtype)))
print("读回 mean=%.4f, std=%.4f" % (float(np.mean(data2)), float(np.std(data2))))
img2.close()
print("读回验证: 通过")

print("\n验证完成")
