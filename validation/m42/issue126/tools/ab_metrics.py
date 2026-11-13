#!/usr/bin/env python3
"""Same-ROI order-18 vs order-13 detail metrics (issue #126).

Definitions are fixed here before the numbers (the earlier ad-hoc script's
high-pass/HF definitions were not reproducible; the independent reviewer's
definitions are adopted):

  * exact-zero second differences: fraction of d2 along BOTH axes that are
    exactly 0.0 (a piecewise-constant 26-px upsampled block map gives values
    near 1 for order 13);
  * high-pass correlation: image - median_filter(image, 9) correlated with the
    same high-pass of the calibrated input frame (the target WCS equals the
    input WCS, so the export and the input sample the same sky);
  * high-frequency power fraction: azimuthal radial FFT power, sum(k=8..60) /
    sum(k>=1);
  * interior ratio: order18 / order13 over finite pixels with a 2-px edge.

Usage: ab_metrics.py <order18-export.fits> <order13-export.fits> <input-frame.fits> <out.json>
"""
from __future__ import annotations

import json
import sys

import numpy as np
from astropy.io import fits
from scipy.ndimage import median_filter


def hp(x, size=9):
    return x - median_filter(np.nan_to_num(x, nan=0.0), size=size, mode="nearest")


def zero_d2(image):
    values = []
    for axis in (0, 1):
        d2 = np.diff(image, 2, axis=axis)
        values.append(float(np.mean(d2[np.isfinite(d2)] == 0.0)))
    both = np.diff(np.diff(image, 2, axis=0), 2, axis=1)
    values.append(float(np.mean(both[np.isfinite(both)] == 0.0)))
    return {"axis0": values[0], "axis1": values[1], "both_axes": values[2]}


def radial_power(image):
    y = image - np.nanmean(image)
    y = np.nan_to_num(y)
    power = np.fft.fftshift(np.abs(np.fft.fft2(y)) ** 2)
    n = y.shape[0]
    c = n // 2
    yy, xx = np.indices(y.shape)
    rr = np.hypot(yy - c, xx - c).astype(int)
    return np.bincount(rr.ravel(), power.ravel()) / np.maximum(1, np.bincount(rr.ravel()))


def corr(x, y, mask):
    x = x[mask] - x[mask].mean()
    y = y[mask] - y[mask].mean()
    return float((x * y).sum() / np.sqrt((x * x).sum() * (y * y).sum()))


def main():
    a, b, raw, out = sys.argv[1:5]
    a = fits.getdata(a).astype(float)
    b = fits.getdata(b).astype(float)
    raw = fits.getdata(raw).astype(float)
    finite = np.isfinite(a) & np.isfinite(b)
    mask = np.zeros_like(finite)
    mask[2:-2, 2:-2] = True
    mask &= finite
    ha, hb, hr = hp(a), hp(b), hp(raw)
    pa, pb, pr = radial_power(a), radial_power(b), radial_power(raw)

    def hf(p):
        return float(p[8:61].sum() / p[1:].sum())

    ratio = a[mask] / b[mask]
    report = {
        "schema": "acsd.issue126.ab_metrics/1",
        "order18_export": sys.argv[1],
        "order13_export": sys.argv[2],
        "input_frame": sys.argv[3],
        "zero_second_differences": {
            "order18": zero_d2(a), "order13": zero_d2(b), "raw": zero_d2(raw),
        },
        "highpass_correlation_with_raw": {
            "order18": corr(ha, hr, mask), "order13": corr(hb, hr, mask),
        },
        "high_frequency_power_fraction": {
            "order18": hf(pa), "order13": hf(pb), "raw": hf(pr),
        },
        "interior_ratio_18_over_13": {
            "n": int(mask.sum()),
            "median": float(np.median(ratio)),
            "p16": float(np.percentile(ratio, 16)),
            "p84": float(np.percentile(ratio, 84)),
        },
        "note": "the input frame is the calibrated crop; its WCS equals the export target WCS. "
                "The high-pass/HF definitions are the reviewer's; the earlier ad-hoc values "
                "(0.188 / 0.608 / 0.108) were not reproducible and are superseded.",
    }
    json.dump(report, open(out, "w"), indent=1)
    print(json.dumps(report, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
