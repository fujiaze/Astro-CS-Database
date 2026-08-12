#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
V7 G5 证据工具（NON_PRODUCTION_TOOL_ONLY）: 真实马赛克 overlap 光测定量

读取 3 个 panel Phase1 HiPS + stage2 输出的真实 mosaic（均为标准 IVOA HiPS
FITS tiles，nside=65536 NESTED），计算：

1. overlap 区域“校准前”帧差（panel 两两 signal 之差）与“校准后”
   残差（每帧 vs 已集成 mosaic）的 robust 统计（median / MAD robust sigma /
   p95 / max），区分 seam band（相邻 panel 覆盖边界 ±0.25 deg）与 interior band；
2. 低频残差（0.5 deg 网格 bin median 的 p95/max）；
3. 全 union 像素深度直方图（depth=1..3）与 control 深度直方图（stage2
   diagnostics 双 bin）。

不修改任何 Phase1/Phase2 科学代码；只做证据生成。

用法:
    py -3.12 lib/phase2/tools/overlap_photometry.py \
        --panels run/temp/p2_v7/gc/panel1_Red.hips run/temp/p2_v7/gc/panel2_Red.hips \
                    run/temp/p2_v7/gc/panel3_Red.hips \
        --mosaic run/phase2/v7/gc_3panel.mosaic.hips \
        --diagnostics run/phase2/v7/gc_3panel.mosaic.hips/diagnostics.json \
        --out run/temp/p2_v7/evidence/gc/overlap_before_after.json
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits

TW = 512          # tile width
SHIFT = 9         # tile-local nested bits
NSIDE = 65536     # leaf order 7


# ---------------------------------------------------------------------------
# HEALPix NESTED 定位（与 lib/common/healpix/healpix_core.cpp 同构的向量化实现）
# ---------------------------------------------------------------------------
def _deinterleave(ipix: np.ndarray, bits: int) -> tuple[np.ndarray, np.ndarray]:
    """ip_low -> (x, y): ix 偶数位, iy 奇数位 (逐位, 向量化)."""
    x = np.zeros(ipix.shape, dtype=np.uint32)
    y = np.zeros(ipix.shape, dtype=np.uint32)
    for i in range(bits):
        bit_x = (ipix >> np.uint64(2 * i)) & np.uint64(1)
        bit_y = (ipix >> np.uint64(2 * i + 1)) & np.uint64(1)
        x |= (bit_x.astype(np.uint32) << i)
        y |= (bit_y.astype(np.uint32) << i)
    return x, y


def tile_pixel_radec(tile_ipix: int) -> tuple[np.ndarray, np.ndarray]:
    """返回 tile 内 512x512 像素的 (ra_deg, dec_deg)，shape=(512,512)，
    numpy 索引与 FITS 文件数组一致（row=511-x, col=y）。"""
    # local = xy_to_nest(x, y, 9)；global ipix = tile<<18 | local
    locals_ = np.arange(TW * TW, dtype=np.uint64)
    ipix = (np.uint64(tile_ipix) << np.uint64(18)) | locals_
    x, y = _deinterleave(ipix, 16)      # 16 bits: 9 local + 8 tile + face 占高 2 位
    face = (ipix >> np.uint64(32)).astype(np.uint32)
    return _xyz_to_radec(face, x, y)


def _xyz_to_radec(face: np.ndarray, x: np.ndarray, y: np.ndarray,
                  ns: int = NSIDE) -> tuple[np.ndarray, np.ndarray]:
    ns64 = np.float64(ns)
    xf = x.astype(np.float64) + 0.5
    yf = y.astype(np.float64) + 0.5
    north = (face <= 3)
    south = (face >= 8)
    equatorial = ~(north | south)

    ra = np.zeros(x.shape, dtype=np.float64)
    dec = np.zeros(x.shape, dtype=np.float64)

    # ---- 赤道路径 (faces 4..7 及极面赤道三角) ----
    eq = equatorial | ((north | south) & ((north & ((x + y) <= ns - 1)) |
                                          (south & ((x + y) >= ns))))
    # 上面等价条件与 C++ 一致：north 极面 x+y<=ns 走赤道公式, south 极面 x+y>=ns 走赤道公式
    if eq.any():
        f = face[eq]
        xx = xf[eq] / ns64
        yy = yf[eq] / ns64
        zoff = np.zeros(f.shape, dtype=np.float64)
        phioff = np.zeros(f.shape, dtype=np.float64)
        chp = f.astype(np.int64)
        m1 = f <= 3
        phioff[m1] = 1.0
        m2 = (f >= 4) & (f <= 7)
        zoff[m2] = -1.0
        chp[m2] -= 4
        m3 = f >= 8
        phioff[m3] = 1.0
        zoff[m3] = -2.0
        chp[m3] -= 8
        z = (2.0 / 3.0) * (xx + yy + zoff)
        phi = (math.pi / 4.0) * (xx - yy + phioff + 2.0 * chp.astype(np.float64))
        rad = np.sqrt(np.clip(1.0 - z * z, 0.0, None))
        rx = rad * np.cos(phi)
        ry = rad * np.sin(phi)
        rz = z
        ra[eq] = np.arctan2(ry, rx) % (2.0 * math.pi)
        dec[eq] = np.arcsin(np.clip(rz, -1.0, 1.0))

    # ---- 极面三角路径 ----
    pol = ~eq
    if pol.any():
        f = face[pol]
        xx = xf[pol].copy()
        yy = yf[pol].copy()
        zf = np.ones(f.shape, dtype=np.float64)
        southp = f >= 8
        if southp.any():
            tmp = xx[southp].copy()
            xx[southp] = yy[southp].copy()
            yy[southp] = ns64 - tmp
            xx[southp] = ns64 - xx[southp]
            zf[southp] = -1.0
        # phi_t
        denom = 2.0 * ((ns64 - xx) + (ns64 - yy))
        phi_t = np.where((yy == ns64) & (xx == ns64), 0.0,
                         math.pi * (ns64 - yy) / np.where(denom == 0.0, 1.0, denom))
        small = phi_t < math.pi / 4.0
        vv = np.zeros(xx.shape, dtype=np.float64)
        v1 = np.abs(math.pi * (ns64 - xx) / ((2.0 * phi_t - math.pi) * ns64) / math.sqrt(3.0))
        v2 = np.abs(math.pi * (ns64 - yy) / (2.0 * phi_t * ns64) / math.sqrt(3.0))
        vv[small] = v1[small]
        vv[~small] = v2[~small]
        z = (1.0 - vv) * (1.0 + vv)
        rad = np.sqrt(np.clip(1.0 + z, 0.0, None)) * vv
        z = z * zf
        f64 = f.astype(np.float64)
        phi = np.where(southp, math.pi / 2.0 * (f64 - 8.0) + phi_t,
                       math.pi / 2.0 * f64 + phi_t)
        phi = phi % (2.0 * math.pi)
        rx = rad * np.cos(phi)
        ry = rad * np.sin(phi)
        rz = z
        ra[pol] = np.arctan2(ry, rx) % (2.0 * math.pi)
        dec[pol] = np.arcsin(np.clip(rz, -1.0, 1.0))

    return (np.degrees(ra).reshape(TW, TW), np.degrees(dec).reshape(TW, TW))


# ---------------------------------------------------------------------------
# HiPS tile 读取
# ---------------------------------------------------------------------------
_NPIX_RE = re.compile(r"Npix(\d+)\.fits$")
_DIR_RE = re.compile(r"^Dir(\d+)$")


def tile_rel_path(tile_ipix: int, order: int = 7) -> str:
    """与 aio_hips_writer/reader 一致的 tile 相对路径约定
    (Dir = ipix//10000, Npix = ipix%10000)。"""
    return f"Norder{order}/Dir{tile_ipix // 10000}/Npix{tile_ipix % 10000}.fits"


def leaf_tile_ipix(product_dir: Path) -> set[int]:
    """枚举 signal/Norder7 下的 leaf tile ipix。"""
    out: set[int] = set()
    n7 = product_dir / "Norder7"
    if not n7.exists():
        return out
    for p in n7.rglob("Npix*.fits"):
        m = _NPIX_RE.search(p.name)
        dm = _DIR_RE.match(p.parent.name)
        if m and dm:
            out.add(int(dm.group(1)) * 10000 + int(m.group(1)))
    return out


def read_tile(product: Path, tile_ipix: int, order: int = 7) -> np.ndarray | None:
    """读取单个 leaf tile FITS 图像数组 (512,512)；失败返回 None。"""
    cand = product / tile_rel_path(tile_ipix, order)
    try:
        with fits.open(cand, memmap=False) as hdul:
            return hdul[0].data.astype(np.float32)
    except Exception:
        return None


def robust_stats(d: np.ndarray) -> dict:
    d = d[np.isfinite(d)]
    if d.size == 0:
        return {"n": 0}
    med = float(np.median(d))
    mad = float(np.median(np.abs(d - med)))
    sigma = 1.4826 * mad
    return {
        "n": int(d.size),
        "median": med,
        "mad": mad,
        "robust_sigma": sigma,
        "p95_abs": float(np.percentile(np.abs(d), 95)),
        "max_abs": float(np.max(np.abs(d))),
    }


def low_freq_residual(ra: np.ndarray, dec: np.ndarray, d: np.ndarray,
                      bin_deg: float = 0.5) -> dict:
    """按 ra/dec 粗网格取 bin median，返回 bin median 绝对值的 p95/max。"""
    sel = np.isfinite(d)
    if not sel.any():
        return {"n": 0}
    rb = np.floor(ra[sel] / bin_deg).astype(np.int64)
    db = np.floor(dec[sel] / bin_deg).astype(np.int64)
    keys = rb * 100000 + db
    meds: dict[int, list[float]] = {}
    vals = d[sel]
    for k, v in zip(keys.tolist(), vals.tolist()):
        meds.setdefault(k, []).append(v)
    bin_med = np.array([float(np.median(v)) for v in meds.values()])
    return {
        "n_bins": int(bin_med.size),
        "bin_deg": bin_deg,
        "p95_abs": float(np.percentile(np.abs(bin_med), 95)),
        "max_abs": float(np.max(np.abs(bin_med))),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--panels", nargs=3, required=True)
    ap.add_argument("--mosaic", required=True)
    ap.add_argument("--diagnostics", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    panels = [Path(p) for p in args.panels]
    mosaic = Path(args.mosaic)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    with open(args.diagnostics, "r", encoding="utf-8") as f:
        diag = json.load(f)

    sig_panels = [p / "signal" for p in panels]
    sup_panels = [p / "support" for p in panels]
    sig_mosaic = mosaic / "signal"
    sup_mosaic = mosaic / "support"

    # 以 mosaic leaf tiles 为 union 全集
    union_tiles = sorted(leaf_tile_ipix(sig_mosaic))
    print(f"[overlap] mosaic leaf tiles: {len(union_tiles)}")

    # ---- 全 union 像素深度直方图（depth = 有效帧数）----
    depth_hist = {1: 0, 2: 0, 3: 0}
    for t in union_tiles:
        masks = []
        for sp, si in zip(sup_panels, sig_panels):
            sup = read_tile(sp, t)
            sig = read_tile(si, t)
            if sup is None or sig is None:
                masks.append(np.zeros((TW, TW), dtype=bool))
            else:
                masks.append(np.isfinite(sig) & (sup > 0.0))
        depth = sum(masks)
        for k in (1, 2, 3):
            depth_hist[k] += int(np.count_nonzero(depth == k))
    print(f"[overlap] pixel depth hist: {depth_hist}")

    # ---- 两两 overlap（相邻 panel；边界 Dec 由数据驱动 = overlap 像素 Dec
    #      中位线。三 panel 同 RA、南北排布，seam 近似恒 Dec 线）----
    pairs = [(0, 1), (1, 2)]
    pair_results = {}
    for i, j in pairs:
        shared = leaf_tile_ipix(sig_panels[i]) & leaf_tile_ipix(sig_panels[j])
        d_before_all: list[np.ndarray] = []
        d_after_all: list[np.ndarray] = []
        res_after_all: list[np.ndarray] = []
        ra_all, dec_all, d_before_all_f = [], [], []
        for t in sorted(shared):
            si = read_tile(sig_panels[i], t)
            sj = read_tile(sig_panels[j], t)
            ui = read_tile(sup_panels[i], t)
            uj = read_tile(sup_panels[j], t)
            sm = read_tile(sig_mosaic, t)
            um = read_tile(sup_mosaic, t)
            if any(x is None for x in (si, sj, ui, uj, sm, um)):
                continue
            vi = np.isfinite(si) & (ui > 0.0)
            vj = np.isfinite(sj) & (uj > 0.0)
            vm = np.isfinite(sm) & (um > 0.0)
            ov = vi & vj & vm
            if not ov.any():
                continue
            d_before = si[ov] - sj[ov]
            d_after = 0.5 * (si[ov] + sj[ov]) - sm[ov]
            res_after = np.concatenate([si[ov] - sm[ov], sj[ov] - sm[ov]])
            d_before_all.append(d_before)
            d_after_all.append(d_after)
            res_after_all.append(res_after)
            ra_t, dec_t = tile_pixel_radec(t)
            ra_all.append(ra_t[ov])
            dec_all.append(dec_t[ov])
            d_before_all_f.append(d_before)
        if not d_before_all:
            pair_results[f"panel{i+1}-panel{j+1}"] = {"shared_tiles": len(shared), "n_overlap_pixels": 0}
            continue
        d_before = np.concatenate(d_before_all)
        d_after = np.concatenate(d_after_all)
        res_after = np.concatenate(res_after_all)
        ra_v = np.concatenate(ra_all)
        dec_v = np.concatenate(dec_all)

        boundary_dec = float(np.median(dec_v))
        seam = np.abs(dec_v - boundary_dec) <= 0.25
        interior = ~seam
        d_before_seam = d_before[seam]
        d_after_seam = d_after[seam]
        d_before_int = d_before[interior]
        d_after_int = d_after[interior]

        lf_before = low_freq_residual(ra_v, dec_v, d_before)
        lf_after = low_freq_residual(ra_v, dec_v, d_after)

        pair_results[f"panel{i+1}-panel{j+1}"] = {
            "shared_tiles": len(shared),
            "n_overlap_pixels": int(d_before.size),
            "boundary_dec_deg": boundary_dec,
            "seam_band_deg": 0.25,
            "before_calibration": {
                "frame_pair_diff": robust_stats(d_before),
                "seam_band": robust_stats(d_before_seam),
                "interior_band": robust_stats(d_before_int),
                "low_frequency": lf_before,
            },
            "after_calibration": {
                "frame_avg_vs_mosaic": robust_stats(d_after),
                "frame_vs_mosaic_residual_pooled": robust_stats(res_after),
                "seam_band": robust_stats(d_after_seam),
                "interior_band": robust_stats(d_after_int),
                "low_frequency": lf_after,
            },
        }
        print(f"[overlap] pair {i+1}-{j+1}: px={d_before.size} "
              f"before_med={float(np.median(np.abs(d_before))):.4g} "
              f"after_med={float(np.median(np.abs(d_after))):.4g}")

    result = {
        "_description": "V7 G5: 银心三 panel 真实 mosaic overlap 光测定量 "
                        "(校准前=输入 HiPS 帧间差; 校准后=帧/均值 vs 已集成 mosaic)",
        "panels": [str(p) for p in panels],
        "mosaic": str(mosaic),
        "diagnostics_ref": str(args.diagnostics),
        "pixel_depth_histogram": depth_hist,
        "control_depth_from_stage2": {
            "controls_with_depth_1": diag.get("controls_with_depth_1"),
            "controls_with_depth_ge_2": diag.get("controls_with_depth_ge_2"),
        },
        "pairs": pair_results,
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(f"[overlap] written: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
