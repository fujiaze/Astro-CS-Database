#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VIS-501 成品帧渲染与自检（R 通道）。

依据：控制包 VIS-501 任务书（整幅 FITS + 分块 PNG + 逐块自检 + 接缝度量）；
      AGENTS §9（视觉层提交前自行逐块检查，分辨率不足时裁剪放大再读；分段计时找热点）。

判据（fail-closed，都能红）：
  V1 finite_fraction     有限像素占比 == 1.0（NaN/Inf 即红）；
  V2 nonzero_fraction    非零像素占比 > 0（全零产品 = 空图，判红）；
  V3 dynamic_range       有限像素的 p99.9/p0.1 > 1（常数图判红）；
  V4 seam_metric         相邻分块边界处的一阶差分中位数与块内同向差分中位数之比
                         （seam_ratio）；无接缝叠加要求 seam_ratio 不显著大于 1。
                         判据：seam_ratio <= 1.5（远超则说明块间接缝存在）；
  V5 coverage            非零像素的包围盒面积 / 全图面积 >= 0.5（导出中心必须落在覆盖内）。

输出：out/<group>/{full.png, tiles/tile_r{r}_c{c}.png, vis_report.json}
"""
from __future__ import annotations

import argparse
import io
import json
import os
import sys

import numpy as np
from astropy.io import fits
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def stretch(a, lo_pct=0.1, hi_pct=99.9, mode="asinh", asinh_a=0.05):
    """返回 uint8 图 + 拉伸参数（确定性：分位数来自有限像素）。"""
    fin = np.isfinite(a)
    vals = a[fin]
    lo = float(np.percentile(vals, lo_pct))
    hi = float(np.percentile(vals, hi_pct))
    if not np.isfinite(lo) or not np.isfinite(hi) or hi <= lo:
        hi = lo + 1.0
    x = np.clip((np.nan_to_num(a, nan=lo, posinf=hi, neginf=lo) - lo) / (hi - lo), 0.0, 1.0)
    if mode == "asinh":
        x = np.arcsinh(x / asinh_a) / np.arcsinh(1.0 / asinh_a)
    return (np.clip(x, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), lo, hi


def seam_metric(img, tile):
    """接缝度量：分块边界处 |Δ| 中位数 / 块内同向 |Δ| 中位数。

    只在**有效（非零）行/列**上取，避免把空白天区当成接缝。
    """
    a = img.astype(np.float64)
    h, w = a.shape
    valid_rows = np.where(np.abs(a).sum(axis=1) > 0)[0]
    valid_cols = np.where(np.abs(a).sum(axis=0) > 0)[0]
    if valid_rows.size < 4 or valid_cols.size < 4:
        return None, None
    r0, r1 = int(valid_rows[0]), int(valid_rows[-1])
    c0, c1 = int(valid_cols[0]), int(valid_cols[-1])

    def med(v):
        v = v[np.isfinite(v)]
        return float(np.median(np.abs(v))) if v.size else 0.0

    # 竖直接缝（列方向分块边界）
    cols = [c for c in range(c0 + tile, c1, tile)]
    seam_v, inner_v = [], []
    for c in cols:
        seam_v.append(a[r0:r1 + 1, c] - a[r0:r1 + 1, c - 1])
        inner_v.append(a[r0:r1 + 1, c - 3] - a[r0:r1 + 1, c - 4])
    # 水平接缝（行方向分块边界）
    rows = [r for r in range(r0 + tile, r1, tile)]
    seam_h, inner_h = [], []
    for r in rows:
        seam_h.append(a[r, c0:c1 + 1] - a[r - 1, c0:c1 + 1])
        inner_h.append(a[r - 3, c0:c1 + 1] - a[r - 4, c0:c1 + 1])
    sv = med(np.concatenate(seam_v)) if seam_v else 0.0
    iv = med(np.concatenate(inner_v)) if inner_v else 0.0
    sh = med(np.concatenate(seam_h)) if seam_h else 0.0
    ih = med(np.concatenate(inner_h)) if inner_h else 0.0
    num = max(sv, sh)
    den = max(iv, ih)
    ratio = (num / den) if den > 0 else float("inf") if num > 0 else 1.0
    return ratio, {"seam_v": sv, "inner_v": iv, "seam_h": sh, "inner_h": ih,
                   "n_col_seams": len(cols), "n_row_seams": len(rows)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fits", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--tile", type=int, default=512)
    ap.add_argument("--group", default="gc")
    ap.add_argument("--max-seam-ratio", type=float, default=1.5)
    args = ap.parse_args()

    os.makedirs(os.path.join(args.out_dir, "tiles"), exist_ok=True)
    with fits.open(args.fits) as hdul:
        hdr = hdul[0].header
        data = np.asarray(hdul[0].data, dtype=np.float64)

    fin = np.isfinite(data)
    n = data.size
    findings = []
    finite_fraction = float(fin.sum()) / n
    if finite_fraction != 1.0:
        findings.append("V1 finite_fraction=%.6f != 1.0" % finite_fraction)
    nz = int(np.count_nonzero(data[fin]))
    nonzero_fraction = nz / n
    if not nonzero_fraction > 0:
        findings.append("V2 nonzero_fraction == 0（空图）")
    vals = data[fin]
    p001 = float(np.percentile(vals, 0.1)) if vals.size else 0.0
    p999 = float(np.percentile(vals, 99.9)) if vals.size else 0.0
    dr = (p999 / p001) if p001 > 0 else float("inf")
    if not dr > 1.0:
        findings.append("V3 dynamic_range p99.9/p0.1 = %s <= 1（常数图）" % dr)
    ratio, detail = seam_metric(np.nan_to_num(data), args.tile)
    if ratio is None:
        findings.append("V4 无法计算接缝度量（有效区太小）")
    elif not ratio <= args.max_seam_ratio:
        findings.append("V4 seam_ratio=%.4f > %.2f（分块接缝）" % (ratio, args.max_seam_ratio))

    nz_rows = np.where(np.any(data != 0, axis=1))[0]
    nz_cols = np.where(np.any(data != 0, axis=0))[0]
    if nz_rows.size and nz_cols.size:
        bbox_frac = ((nz_rows[-1] - nz_rows[0] + 1) * (nz_cols[-1] - nz_cols[0] + 1)) / n
    else:
        bbox_frac = 0.0
    if not bbox_frac >= 0.5:
        findings.append("V5 coverage bbox_frac=%.4f < 0.5（导出中心未落在覆盖内）" % bbox_frac)

    png, lo, hi = stretch(data)
    Image.fromarray(png, mode="L").save(os.path.join(args.out_dir, "full.png"))
    h, w = data.shape
    ntile = 0
    for r in range(0, h, args.tile):
        for c in range(0, w, args.tile):
            sub = png[r:r + args.tile, c:c + args.tile]
            Image.fromarray(sub, mode="L").save(
                os.path.join(args.out_dir, "tiles", "tile_r%d_c%d.png" % (r // args.tile, c // args.tile)))
            ntile += 1

    rec = {"schema": "astrocs.vis-report/v1", "group": args.group, "fits": args.fits,
           "shape": [int(h), int(w)], "tile_px": args.tile, "n_tiles": ntile,
           "finite_fraction": finite_fraction, "nonzero_fraction": nonzero_fraction,
           "p0_1": p001, "p99_9": p999, "dynamic_range": dr,
           "min": float(vals.min()) if vals.size else 0.0,
           "max": float(vals.max()) if vals.size else 0.0,
           "median": float(np.median(vals)) if vals.size else 0.0,
           "stretch": {"mode": "asinh", "a": 0.05, "lo": lo, "hi": hi},
           "bbox_fraction": bbox_frac,
           "seam_ratio": ratio, "seam_detail": detail, "max_seam_ratio": args.max_seam_ratio,
           "findings": findings, "verdict": "PASS" if not findings else "FAIL"}
    io.open(os.path.join(args.out_dir, "vis_report.json"), "w", encoding="utf-8").write(
        json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
    print("VIS_%s[%s]: %dx%d tiles=%d finite=%.4f nonzero=%.4f seam_ratio=%s -> %s"
          % (rec["verdict"], args.group, w, h, ntile, finite_fraction, nonzero_fraction,
             ("%.4f" % ratio) if ratio is not None else "n/a", args.out_dir))
    for f in findings:
        print("  " + f)
    return 0 if not findings else 1


if __name__ == "__main__":
    sys.exit(main())
