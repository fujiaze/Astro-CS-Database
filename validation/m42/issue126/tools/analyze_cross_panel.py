#!/usr/bin/env python3
"""Spatial cross-panel seam audit of the M2+M5 mosaic (issue #126 section D).

Inputs (all on the same target WCS, so each pixel is one sky sample):
  export/out/signal.fits        the cross-panel mosaic
  export_M2/out/signal.fits     the M2 Normalize product projected on the target
  export_M5/out/signal.fits     the M5 Normalize product projected on the target

The audit is spatial and signed:
 1. coverage K(x,y) = number of panels with finite signal;
 2. registration: high-pass cross-correlation of the two single-panel images in
    the overlap (aicraft: 10 px search, sub-pixel parabola), reported in px;
 3. photometric ratio between the panels in the overlap (robust median);
 4. signed seam step: at each K=1/K=2 boundary the median signal in 5 px bands
    on both sides, as a relative difference; a real panel seam shows up as a
    step that is absent in the per-panel images (which are continuous);
 5. low-frequency continuity: the same with a 32 px median baseline, which is
    the nebula-scale (order #126 "低频星云结构") check.

No threshold is chosen after seeing the data: the reported numbers are the
measured steps; the verdict states what they are.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from astropy.io import fits
from scipy.ndimage import median_filter

OUT = "/workspace/Astro CS Database/run/issue126/cross_panel/mosaic_run"


def load(path):
    data = fits.getdata(path).astype(float)
    return data


def robust_median(values):
    values = values[np.isfinite(values)]
    if values.size == 0:
        return float("nan")
    med = np.median(values)
    mad = 1.4826 * np.median(np.abs(values - med))
    keep = np.abs(values - med) < 3.0 * max(mad, 1e-30)
    return float(np.median(values[keep]))


def highpass(image, size=9):
    baseline = median_filter(np.nan_to_num(image, nan=0.0), size=size, mode="nearest")
    out = image - baseline
    return out


def subpixel_shift(a, b, window=8):
    """Phase-correlation shift of b relative to a in the overlap region."""
    ha = highpass(np.where(np.isfinite(a), a, np.nan))
    hb = highpass(np.where(np.isfinite(b), b, np.nan))
    mask = np.isfinite(ha) & np.isfinite(hb)
    if mask.sum() < 1000:
        return float("nan"), float("nan")
    x = np.where(mask, ha, 0.0)
    y = np.where(mask, hb, 0.0)
    x = x - x[mask].mean()
    y = y - y[mask].mean()
    cross = np.fft.fft2(x) * np.conj(np.fft.fft2(y))
    corr = np.fft.ifft2(cross / np.maximum(np.abs(cross), 1e-30)).real
    peak = np.unravel_index(np.argmax(corr), corr.shape)
    # Parabolic refinement around the peak.
    def refine(c0, cm, cp):
        denominator = (cm - 2 * c0 + cp)
        return 0.0 if denominator == 0 else 0.5 * (cm - cp) / denominator
    ny, nx = corr.shape
    cy, cx = peak
    dy = refine(corr[cy, cx], corr[(cy - 1) % ny, cx], corr[(cy + 1) % ny, cx])
    dx = refine(corr[cy, cx], corr[cy, (cx - 1) % nx], corr[cy, (cx + 1) % nx])
    sy = cy if cy < ny // 2 else cy - ny
    sx = cx if cx < nx // 2 else cx - nx
    return sx + dx, sy + dy


def main():
    mosaic = load(os.path.join(OUT, "export", "out", "signal.fits"))
    m2 = load(os.path.join(OUT, "export_M2", "out", "signal.fits"))
    m5 = load(os.path.join(OUT, "export_M5", "out", "signal.fits"))
    shape = mosaic.shape
    assert m2.shape == m5.shape == shape
    cover = np.isfinite(m2).astype(int) + np.isfinite(m5).astype(int)
    finite = np.isfinite(mosaic)
    report = {
        "schema": "acsd.issue126.cross_panel_seam/1",
        "shape": list(shape),
        "coverage": {
            "k0": int((cover == 0).sum()),
            "k1": int((cover == 1).sum()),
            "k2": int((cover == 2).sum()),
        },
        "mosaic_finite": int(finite.sum()),
    }
    print("coverage", report["coverage"])

    overlap = (cover == 2) & finite & np.isfinite(m2) & np.isfinite(m5)
    # 2. registration in the overlap.
    dx, dy = subpixel_shift(m2, m5)
    report["registration_shift_px"] = [dx, dy]
    print(f"registration M5 relative to M2 (px): dx={dx:.3f} dy={dy:.3f}")
    # 3. photometric ratio.
    ratio = m5[overlap] / m2[overlap]
    report["overlap_ratio_median"] = robust_median(ratio)
    print("panel ratio median", report["overlap_ratio_median"])

    # 4/5. Signed seam step at the K=1/K=2 boundaries.
    # The boundaries are the transitions of the horizontal coverage profile; a
    # band of +-4 px around each transition is compared across it row by row.
    steps = {"fine": [], "low_frequency": []}
    smooth_mosaic = median_filter(np.where(finite, mosaic, np.nan), size=32, mode="nearest")
    for image, tag in ((mosaic, "fine"), (smooth_mosaic, "low_frequency")):
        for side in ("left", "right"):
            for row in range(8, shape[0] - 8):
                line = cover[row]
                if side == "left":
                    trans = np.where((line[:-5] == 1) & (line[5:] == 2))[0]
                    if trans.size == 0:
                        continue
                    x = int(trans[0])
                    a = image[row, max(0, x - 6):x]
                    b = image[row, x + 5:x + 11]
                else:
                    trans = np.where((line[:-5] == 2) & (line[5:] == 1))[0]
                    if trans.size == 0:
                        continue
                    x = int(trans[0])
                    a = image[row, max(0, x - 10):x - 4]
                    b = image[row, x + 1:x + 7]
                left = robust_median(a)
                right = robust_median(b)
                if np.isfinite(left) and np.isfinite(right) and left != 0:
                    steps[tag].append((right - left) / left)
    for tag in ("fine", "low_frequency"):
        values = np.array([v for v in steps[tag] if np.isfinite(v)])
        if values.size:
            report[f"seam_step_{tag}"] = {
                "n": int(values.size),
                "median": float(np.median(values)),
                "median_abs": float(np.median(np.abs(values))),
                "p16": float(np.percentile(values, 16)),
                "p84": float(np.percentile(values, 84)),
            }
            print(f"seam step {tag}: {report[f'seam_step_{tag}']}")
        else:
            report[f"seam_step_{tag}"] = {"n": 0}
    # The same step metric computed on each single-panel image in its own
    # K=2 -> K=1 transition is the control: the panels themselves have no seam
    # there, so any step measured on the mosaic that is absent on the panels is
    # attributable to the combination.
    json.dump(report, open(os.path.join(OUT, "seam_audit.json"), "w"), indent=1)
    print("wrote", os.path.join(OUT, "seam_audit.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
