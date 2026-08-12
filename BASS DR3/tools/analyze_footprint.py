# -*- coding: utf-8 -*-
"""BASS DR3 覆盖天区与体积分析工具

输入: coords.csv (每帧四角坐标), index.csv (体积)
输出: 覆盖 RA/Dec 范围、指向/曝光数、足迹面积(栅格化 0.05°)、
      归档体积(按 kind)、funpack 后体积、HiPS 数据库体积估算(按层级)

用法:
  py -3.12 analyze_footprint.py [--dir ..]
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np

CELL_DEG = 0.05
NAX = 4096
NAY = 4032
PIX_DEG = 0.454968 / 3600.0  # 90Prime 像素尺度


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", type=Path, default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args()
    base: Path = args.dir.resolve()

    rows = list(csv.DictReader(open(base / "coords.csv", encoding="utf-8-sig")))
    n = len(rows)
    print(f"[coords] science entries={n}")

    ra = np.array([float(r["ra"]) for r in rows])
    dec = np.array([float(r["dec"]) for r in rows])
    print(f"[coords] RA range={ra.min():.3f}..{ra.max():.3f}  DEC range={dec.min():.3f}..{dec.max():.3f}")
    print(f"[coords] DEC>20: {(dec>20).sum()} ({(dec>20).mean()*100:.1f}%)  DEC<=20: {(dec<=20).sum()}")

    pointings = set()
    for r in rows:
        pointings.add(r["f"].rsplit("_", 1)[0])
    print(f"[coords] unique pointings={len(pointings)} (每个指向 4 CCD)")

    # ---- 足迹栅格化 (0.05°, RA 支持跨 0 环绕) ----
    dec_min, dec_max = -55.0, 85.0
    n_dec = int(round((dec_max - dec_min) / CELL_DEG))
    n_ra = int(round(360.0 / CELL_DEG))
    covered = np.zeros((n_dec, n_ra), dtype=np.int16)
    dec_idx0 = np.arange(n_dec)
    ra_idx0 = np.arange(n_ra)
    dec_vals = dec_min + (dec_idx0 + 0.5) * CELL_DEG
    ra_vals = ra_idx0 * CELL_DEG

    def mark_frame(corners):
        """corners: 4x2 [[ra,dec],...] lb,lt,rt,rb; RA 已按帧解包(可>360)."""
        xs, ys = corners[:, 0], corners[:, 1]
        # 规范化绕序 (逆时针)
        area = 0.0
        for i in range(4):
            j = (i + 1) % 4
            area += xs[i] * ys[j] - xs[j] * ys[i]
        if area < 0:
            xs, ys = xs[::-1], ys[::-1]
        ymin, ymax = ys.min(), ys.max()
        xmin, xmax = xs.min(), xs.max()
        if xmax - xmin > 350 or ymax - ymin > 5:  # 异常角点, 跳过
            return
        i0 = max(int(np.searchsorted(dec_vals, ymin)) - 1, 0)
        i1 = min(int(np.searchsorted(dec_vals, ymax)) + 1, n_dec)
        # RA 列 (含跨0解包, 列号取模)
        c0 = int(np.floor((xmin - CELL_DEG) / CELL_DEG))
        c1 = int(np.ceil((xmax + CELL_DEG) / CELL_DEG))
        if c1 - c0 > n_ra:
            return
        for i in range(i0, i1):
            yy = dec_vals[i]
            if yy < ymin or yy > ymax:
                continue
            col_idx = []
            test_x = []
            for c in range(c0, c1 + 1):
                xx = c * CELL_DEG
                col_idx.append(c % n_ra)
                test_x.append(xx)
            test_x = np.array(test_x)
            col_idx = np.array(col_idx)
            # 点在四边形内 (凸): 各边叉积同号
            inside = np.ones(len(test_x), dtype=bool)
            for k in range(4):
                x1, y1 = xs[k], ys[k]
                x2, y2 = xs[(k + 1) % 4], ys[(k + 1) % 4]
                cross = (x2 - x1) * (yy - y1) - (y2 - y1) * (test_x - x1)
                inside &= cross >= 0
            if inside.any():
                covered[i, col_idx[inside]] += 1

    skip = 0
    degenerate = 0
    for r in rows:
        base_ra = float(r["ra"])
        corners = []
        for k in ("ra_lb", "dec_lb", "ra_lt", "dec_lt", "ra_rt", "dec_rt", "ra_rb", "dec_rb"):
            corners.append(float(r[k]))
        xs = [corners[0], corners[2], corners[4], corners[6]]
        ys = [corners[1], corners[3], corners[5], corners[7]]
        xs = np.array(xs)
        ys = np.array(ys)
        span_x = xs.max() - xs.min()
        span_y = ys.max() - ys.min()
        if span_x < 0.1 or span_y < 0.1 or span_x > 1.0 or span_y > 1.0:
            degenerate += 1
            continue
        # RA 解包: 以中心为基准, 角点相对中心偏差 >180 则 ±360
        d = xs - base_ra
        xs = np.where(d > 180, xs - 360, np.where(d < -180, xs + 360, xs))
        mark_frame(np.stack([xs, ys], axis=1))

    print(f"[footprint] skipped degenerate frames={degenerate}")
    area_deg2 = float((covered > 0).sum()) * CELL_DEG * CELL_DEG
    mean_depth = float(covered.sum()) / float((covered > 0).sum()) if (covered > 0).any() else 0
    print(f"[footprint] raster={CELL_DEG}deg  union_cells={(covered>0).sum()}  area~{area_deg2:,.0f} deg2  mean_depth~{mean_depth:.2f}")
    # 各 DEC 带覆盖
    for band in ((20, 90), (-10, 20), (-55, -10)):
        lo, hi = band
        mask = (dec_vals >= lo) & (dec_vals < hi)
        a = float((covered[mask] > 0).sum()) * CELL_DEG * CELL_DEG
        print(f"[footprint] dec {lo}..{hi}: {a:,.0f} deg2")
    # 深度分布
    counts, edges = np.histogram(covered[covered > 0], bins=[0.5, 1.5, 2.5, 3.5, 4.5, 8.5, 16.5, 32.5, 1e9])
    for c, e in zip(counts, edges[:-1]):
        if c:
            nxt = edges[list(edges[:-1]).index(e) + 1]
            lbl = f"{e:.1f}-{nxt:.1f}" if nxt < 1e8 else f">{e:.1f}"
            print(f"[footprint] depth {lbl} : {(c*CELL_DEG*CELL_DEG):,.0f} deg2")

    # ---- 归档体积 ----
    sizes = {}
    counts = {}
    with open(base / "index.csv", encoding="utf-8-sig", newline="") as fh:
        for r in csv.DictReader(fh):
            k = r["k"]
            sizes[k] = sizes.get(k, 0) + int(r["s"])
            counts[k] = counts.get(k, 0) + 1
    total = sum(sizes.values())
    print("\n[volume] archive (.fits.fz, 服务端大小)")
    for k in ("science", "weight", "od"):
        print(f"  {k:8s} {counts[k]:>7,}  {sizes[k]/1e12:8.2f} TB")
    print(f"  {'total':8s} {sum(counts.values()):>7,}  {total/1e12:8.2f} TB")

    # funpack 后体积 (按 ZNAXIS 4096x4032; science/weight float32, od uint8)
    bytes_map = {
        "science": NAX * NAY * 4,
        "weight": NAX * NAY * 4,
        "od": NAX * NAY * 1,
    }
    print("\n[volume] funpack 后 (标准 FITS)")
    for k in ("science", "weight", "od"):
        b = bytes_map[k] * counts[k]
        print(f"  {k:8s} {b/1e12:8.2f} TB")
    total_u = sum(bytes_map[k] * counts[k] for k in ("science", "weight", "od"))
    print(f"  {'total':8s} {total_u/1e12:8.2f} TB")

    # ---- HiPS 估算 ----
    print(f"\n[hips] footprint={area_deg2:,.0f} deg2 (全天天区=41253 deg2)")
    frac = area_deg2 / 41253.0
    tile_px = 512 * 512
    for lvl in (6, 7, 8, 9, 10):
        nside = 512 * (2**lvl)
        allsky_px = 12 * nside * nside
        px_area_deg2 = 41253.0 / allsky_px
        px_scale_arcsec = np.sqrt(px_area_deg2) * 3600.0
        tiles_lvl = 12 * (4**lvl)
        fp_px = allsky_px * frac
        fp_tiles = tiles_lvl * frac
        bytes_f32 = fp_px * 4
        bytes_f64 = fp_px * 8
        print(
            f"  L{lvl}: 像素尺度~{px_scale_arcsec:.2f}\"  "
            f"覆盖像素~{fp_px/1e9:,.1f}G 瓦片~{fp_tiles/1e3:,.0f}k  "
            f"float32~{bytes_f32/1e9:,.0f}GB float64~{bytes_f64/1e9:,.0f}GB"
        )
    lvl = 8
    total_all = 0.0
    nside8 = 512 * (2**8)
    for l in range(lvl + 1):
        total_all += 12 * (512 * (2**l)) ** 2 * frac
    print(
        f"  L0-L8 金字塔累计(1.6\"/px 最深): ~{total_all*4/1e9:,.0f}GB float32 "
        f"/ ~{total_all*8/1e9:,.0f}GB float64"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
