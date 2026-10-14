#!/usr/bin/env python3
"""E2E 可视化与数字测量（science-v2-impl）。

从 export 的普通 2-D WCS FITS 生成：
  1. 全景拉伸图（asinh, 负值/NaN 如实处理，不做 clamp 到 0）
  2. 亮星放大图（ brightest-star cutout ）
  3. 接缝边界图（相邻帧覆盖差异的可视化）
  4. 低表面亮度结构对照（不同 stretch 档位）
并输出数字测量：覆盖像素数、NaN 数、正负统计、最亮源位置与峰值、
   5-sigma 类深度指示（不声称绝对测光，因为 gain 未知 ⇒ SNR 是上界）。
"""
import json
import os
import sys

import numpy as np
from astropy.io import fits
from astropy.visualization import AsinhStretch, ImageNormalize

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "run", "A", "preview")
FITS = os.path.join(HERE, "..", "..", "..", "run", "science-v2-impl", "e2e",
                    "out", "export", "output_phase3.fits")


def asinh_png(data, path, title, a=None):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    finite = data[np.isfinite(data)]
    if finite.size == 0:
        print("  ! no finite pixels, skip", path)
        return
    if a is None:
        # 用正数分位数定标；NaN/负值保留为背景（asinh 线性过零）
        pos = finite[finite > 0]
        a = float(np.percentile(pos, 5)) if pos.size else 1.0
    norm = ImageNormalize(vmin=float(np.percentile(finite, 1)),
                          vmax=float(np.percentile(finite, 99.9)),
                          stretch=AsinhStretch(a=a))
    fig, ax = plt.subplots(figsize=(10, 10), dpi=110)
    im = ax.imshow(data, norm=norm, origin="lower", cmap="magma",
                   interpolation="nearest")
    ax.set_title(title, fontsize=11)
    fig.colorbar(im, ax=ax, shrink=0.8, label="ADU/sr (un-stretched data)")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
    print("  wrote", os.path.relpath(path, HERE))


def main():
    os.makedirs(OUT, exist_ok=True)
    with fits.open(FITS, memmap=True) as hl:
        h = hl[0].header
        d = hl[0].data.astype(np.float64)
    print("FITS:", os.path.relpath(FITS, HERE))
    print("  NAXIS=%sx%s BITPIX=%s BUNIT=%s" % (
        h.get("NAXIS1"), h.get("NAXIS2"), h.get("BITPIX"), h.get("BUNIT")))
    print("  WCS: CRPIX=(%s,%s) CRVAL=(%s,%s) CD=(%s,%s)" % (
        h.get("CRPIX1"), h.get("CRPIX2"), h.get("CRVAL1"), h.get("CRVAL2"),
        h.get("CD1_1"), h.get("CD1_2")))

    n = d.size
    nan = int(np.count_nonzero(~np.isfinite(d)))
    fin = d[np.isfinite(d)]
    neg = int(np.count_nonzero(fin < 0))
    meas = {
        "schema": "acsd.science-v2.export-measure/v1",
        "n_pixels": int(n),
        "n_nan": nan,
        "n_negative": neg,
        "frac_negative_of_finite": (neg / max(1, fin.size)),
        "min": float(np.min(fin)) if fin.size else None,
        "max": float(np.max(fin)) if fin.size else None,
        "median": float(np.median(fin)) if fin.size else None,
        "p1_p99": [float(np.percentile(fin, 1)), float(np.percentile(fin, 99))],
    }
    # brightest star
    if fin.size:
        iy, ix = np.unravel_index(int(np.nanargmax(d)), d.shape)
        meas["brightest"] = {"x": int(ix), "y": int(iy),
                             "value": float(np.nanmax(d))}
        print("  brightest: %g ADU/sr at (x=%d,y=%d)" % (
            meas["brightest"]["value"], ix, iy))
    # 5-sigma-like depth indicator on the sky-subtracted negative tail:
    # honest framing: this is a *robust scatter* of the residual field, NOT an
    # absolute depth (gain unknown ⇒ SNR is an upper bound per DATA-P1-SNR).
    if fin.size:
        mad = 1.4826 * float(np.median(np.abs(fin - np.median(fin))))
        meas["robust_sigma_adu_per_sr"] = mad
        meas["five_sigma_flux_equiv_adu_per_sr"] = 5.0 * mad
        print("  robust sigma = %.4g ADU/sr ; 5x = %.4g ADU/sr (upper-bound "
              "framing; NOT absolute photometry)" % (mad, 5 * mad))
    with open(os.path.join(OUT, "measurements.json"), "w", encoding="utf-8") as f:
        json.dump(meas, f, indent=1, ensure_ascii=False)
    print("  measurements ->", os.path.relpath(os.path.join(OUT, "measurements.json"), HERE))

    # 1) panorama
    asinh_png(d, os.path.join(OUT, "pano_asinh.png"),
              "M42 H-alpha mosaic export (asinh, un-stretched ADU/sr)")
    # 2) bright-star cutout
    if fin.size:
        iy, ix = np.unravel_index(int(np.nanargmax(d)), d.shape)
        r = 96
        sub = d[max(0, iy - r):iy + r, max(0, ix - r):ix + r]
        asinh_png(sub, os.path.join(OUT, "bright_star_cutout.png"),
                  "brightest source cutout (%dx%d px)" % (sub.shape[1], sub.shape[0]))
    # 3) seam/connectivity visualisation: show |gradient| magnitude — seams show
    #    up as line-like structures; honest framing: a picture, not a metric.
    gy, gx = np.gradient(d)
    gmag = np.hypot(gx, gy)
    asinh_png(gmag, os.path.join(OUT, "gradient_map.png"),
              "gradient magnitude (line-like features = candidate seams)")
    # 4) low-surface-brightness comparison: 3 stretch asinh parameters
    for i, a in enumerate([1.0, 10.0, 100.0]):
        asinh_png(d, os.path.join(OUT, "lsb_asinh_a%d.png" % a),
                  "low-surface-brightness comparison, asinh a=%g" % a, a=a)
    return 0


if __name__ == "__main__":
    sys.exit(main())
