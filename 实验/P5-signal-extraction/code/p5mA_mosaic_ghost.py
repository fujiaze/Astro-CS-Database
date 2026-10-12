"""11″ 在哪一层：量生产成品在【跨组重叠区】与【单组区】的星像宽度。
图像层错位 11″ ⇒ 重叠区星像被拉宽/重影（FWHM 显著变大）；图像对齐而源表错 ⇒ 两区 FWHM 一致。
"""
import json, os, sys
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
PROD = "/workspace/Astro CS Database/run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits"

hdu = fits.open(PROD)[0]; img = hdu.data.astype(np.float64); wh = WCS(hdu.header)
print("生产成品 %s  shape=%s  NaN=%.2f%%" % (os.path.basename(PROD), img.shape, 100 * np.mean(~np.isfinite(img))))

def frame_poly_prod(tag, fid):
    wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
    f, _, _ = m5.fit_geom(wj)
    n = 4095.0
    b = []
    for t in np.linspace(0, 1, 17): b.append((t * n, 0))
    for t in np.linspace(0, 1, 17): b.append((n, t * n))
    for t in np.linspace(1, 0, 17): b.append((t * n, n))
    for t in np.linspace(1, 0, 17): b.append((0, t * n))
    b = np.array(b); ra, dec = f(b[:, 0], b[:, 1])
    x, y = wh.all_world2pix(ra, dec, 0)
    return np.stack([x, y], 1)

def box_center(poly):
    return float(np.mean(poly[:, 0])), float(np.mean(poly[:, 1]))

def fwhm_stats(cx, cy, half=200, label=""):
    x0, x1 = int(cx - half), int(cx + half); y0, y1 = int(cy - half), int(cy + half)
    sub = img[y0:y1, x0:x1]
    sub = np.where(np.isfinite(sub), sub, np.nan)
    if np.mean(np.isfinite(sub)) < 0.5:
        print("   [%s] 有效像素不足，跳过" % label); return None
    med = np.nanmedian(sub); sig = 1.4826 * np.nanmedian(np.abs(sub - med))
    sm = sub - med
    # 局部极大且 >8σ
    peaks = []
    q = sm[2:-2, 2:-2]
    thr = 8 * sig
    cand = np.argwhere((q > thr) & (q == np.maximum.reduce([sm[i:i + q.shape[0], j:j + q.shape[1]]
              for i in range(5) for j in range(5)])))
    for yy, xx in cand[:400]:
        w = sm[yy:yy + 5, xx:xx + 5] - np.min(sm[yy:yy + 5, xx:xx + 5])
        tot = np.sum(w)
        if tot <= 0: continue
        gy, gx = np.mgrid[0:5, 0:5]
        my = np.sum(w * gy) / tot; mx = np.sum(w * gx) / tot
        vy = np.sum(w * (gy - my) ** 2) / tot; vx = np.sum(w * (gx - mx) ** 2) / tot
        peaks.append(np.sqrt(0.5 * (vx + vy)) * 2.3548)
    if not peaks:
        print("   [%s] 无合格星像" % label); return None
    peaks = np.array(peaks)
    r = {"label": label, "n_peaks": int(peaks.size), "fwhm_median_px": float(np.median(peaks)),
         "fwhm_p16": float(np.percentile(peaks, 16)), "fwhm_p84": float(np.percentile(peaks, 84)),
         "box": [x0, y0, x1, y1], "median_level": float(med)}
    print("   [%s] 星像 %d 个 | FWHM 中位 %.2f px（p16/p84 %.2f/%.2f）| 局部中位 %.1f" % (
        label, r["n_peaks"], r["fwhm_median_px"], r["fwhm_p16"], r["fwhm_p84"], med))
    return r

res = {"product": PROD}
# 跨组重叠区：M2_T2_20251212 × M3_T2_20251224（§18 记录 15% 重叠）
try:
    pa = frame_poly_prod("t2", "M42_M2_T2_flying_dutchman-20251212_012404-300S-Red")
    pb = frame_poly_prod("t3", "M42_M3_T2_flying_dutchman-20251224_042543-300S-Red")
    from matplotlib.path import Path
    ax, ay = box_center(pa); bx, by = box_center(pb)
    # 交点近似：两帧中心连线中点处即重叠中心（重叠 15%）
    cx, cy = (ax + bx) / 2, (ay + by) / 2
    print("M2 中心生产像素 (%.0f, %.0f)，M3 中心 (%.0f, %.0f) ⇒ 重叠中点 (%.0f, %.0f)" % (ax, ay, bx, by, cx, cy))
    res["cross_group"] = fwhm_stats(cx, cy, label="跨组重叠区 M2×M3")
except Exception as e:
    print("跨组重叠区失败:", type(e).__name__, e); res["cross_group"] = None
# 单组区对照：另一指向组（M5）帧中心
try:
    pc = frame_poly_prod("t2", "M42_M5_T2_flying_dutchman-20260108_030531-300S-Red")
    cx2, cy2 = box_center(pc)
    print("M5 中心生产像素 (%.0f, %.0f)" % (cx2, cy2))
    res["single_group"] = fwhm_stats(cx2, cy2, label="单组区 M5")
except Exception as e:
    print("单组区失败:", type(e).__name__, e); res["single_group"] = None
if res.get("cross_group") and res.get("single_group"):
    r1 = res["cross_group"]["fwhm_median_px"]; r0 = res["single_group"]["fwhm_median_px"]
    res["ratio"] = r1 / r0
    print("⇒ 跨组重叠区/单组区 FWHM 比 = %.3f（≈1 ⇒ 图像层对齐，11″ 在源表/元数据侧；≫1 ⇒ 图像层错位）" % res["ratio"])
json.dump(res, open(f"{OUT}/p5mA_mosaic_ghost.json", "w"), indent=1)
print("写入", f"{OUT}/p5mA_mosaic_ghost.json")
