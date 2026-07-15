"""验证 Ridge + 高匹配星数效果"""
import csv, os, json
import numpy as np
from astropy.io import fits
from scipy.spatial import cKDTree

frame_dir = r"f:\Astro dev\Astro CS Normalization Database\testdata\Galaxy_Center_T4 全链路测试数据\results\panel1_Red"

xs, ys, r_obs, r_fit = [], [], [], []
with open(os.path.join(frame_dir, "04_residuals_mult.csv"), "r", encoding="utf-8") as f:
    rd = csv.reader(f); next(rd)
    for row in rd:
        xs.append(float(row[0])); ys.append(float(row[1]))
        r_obs.append(float(row[2])); r_fit.append(float(row[3]))
xs = np.array(xs); ys = np.array(ys)
r_obs = np.array(r_obs); r_fit = np.array(r_fit)

print("=== r_obs 统计 ===")
print(f"  n={len(xs)}, median={np.median(r_obs):.4f}, std={np.std(r_obs):.4f}")

tree = cKDTree(np.column_stack([xs, ys]))
_, idxs = tree.query(np.column_stack([xs, ys]), k=6)
ndiffs = [abs(r_obs[i] - r_obs[idxs[i,j]]) for i in range(len(xs)) for j in range(1,6) if idxs[i,j] < len(xs)]
print(f"  最近邻|差| median={np.median(ndiffs):.4f}, 比值={np.median(ndiffs)/np.std(r_obs):.4f}")

residuals = r_obs - r_fit
print(f"  拟合解释方差: {100*(1-np.var(residuals)/np.var(r_obs)):.1f}%")

before = fits.getdata(os.path.join(frame_dir, "01_calibrated.fits")).astype(np.float64)
after = fits.getdata(os.path.join(frame_dir, "04_calibrated_final.fits")).astype(np.float64)
h, w = before.shape
margin = 200
c = np.median(after[h//2-margin:h//2+margin, w//2-margin:w//2+margin])
print(f"\n=== 校正后图像 ===")
print(f"  负值比例: {np.sum(after<0)/after.size*100:.2f}%")
for name, y0, y1, x0, x1 in [
    ("中心", h//2-margin, h//2+margin, w//2-margin, w//2+margin),
    ("左", h//2-margin, h//2+margin, 0, 2*margin),
    ("右", h//2-margin, h//2+margin, w-2*margin, w),
    ("上", 0, 2*margin, w//2-margin, w//2+margin),
    ("下", h-2*margin, h, w//2-margin, w//2+margin),
    ("左上", 0, 2*margin, 0, 2*margin),
    ("右上", 0, 2*margin, w-2*margin, w),
    ("左下", h-2*margin, h, 0, 2*margin),
    ("右下", h-2*margin, h, w-2*margin, w),
]:
    e = np.median(after[y0:y1, x0:x1])
    print(f"  {name}: {e:.1f}/{c:.1f} = {e/c:.4f}")

# 校正前对比
c_b = np.median(before[h//2-margin:h//2+margin, w//2-margin:w//2+margin])
print(f"\n=== 校正前 边缘/中心 ===")
for name, y0, y1, x0, x1 in [
    ("右下", h-2*margin, h, w-2*margin, w),
    ("左上", 0, 2*margin, 0, 2*margin),
]:
    e = np.median(before[y0:y1, x0:x1])
    print(f"  {name}: {e:.1f}/{c_b:.1f} = {e/c_b:.4f}")
