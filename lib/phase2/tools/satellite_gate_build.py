#!/usr/bin/env python3
# lib/phase2/tools/satellite_gate_build.py — V15 卫星线真实生产门构造
#
# 用途（NON_PRODUCTION_TOOL_ONLY）：
#   以真实 Phase1 单帧 HiPS（t4_crop_v3.hips，GC Red，真实星场/背景）为
#   底图，构造 20 个独立 exposure 输入栈：
#     - 每帧施加独立乘性/加性小噪声（模拟真实帧间变化，support/NaN 结构
#       与 SNR catalogue 保持原样）；
#     - 第 10 帧受控注入大圆卫星线（signal += 0.05，≈17×背景 median，
#       宽度 ~1.5px）；
#   exposure granularity：20 个输入各自是独立 HiPS（frame_id 因 signal
#   payload 不同而不同），Phase2 rejection 前保持独立样本身份。
#
# 输出：run/temp/satgate/frameNN.hips（20 个 HiPS）+ trail_mask.json +
# stage2_satellite_auto.json + stage2_satellite_clean.json（对照无注入）。
import glob
import json
import math
import os
import random
import shutil
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy_healpix import HEALPix, lonlat_to_healpix, healpix_to_lonlat

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "run" / "temp" / "phase1_freeze" / "t4_crop_v3.hips"
OUT = ROOT / "run" / "temp" / "satgate"
N_FRAMES = 20
TRAIL_FRAME = 10   # 0-based；该帧注入卫星线
NSIDE = 1 << 16    # order 7 → nside 65536（t4_crop target order）
TRAIL_AMP = 0.05
TRAIL_WIDTH_DEG = 1.5 * (3.220769 / 3600.0)  # 1.5 px × pixel_scale


def copy_tree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def interleave(x, y):
    """NESTED local = interleave(x, y)：bit 2b ← x_b；bit 2b+1 ← y_b。"""
    local = np.zeros(np.broadcast(x, y).shape, dtype=np.int64)
    for b in range(9):
        local |= ((x >> b) & 1) << (2 * b)
        local |= ((y >> b) & 1) << (2 * b + 1)
    return local


def main():
    random.seed(20260814)
    rng = np.random.default_rng(20260814)
    if not (BASE / "signal" / "properties").exists():
        sys.exit("base HiPS missing: " + str(BASE))
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    tiles = sorted(
        glob.glob(str(BASE / "signal" / "Norder7" / "**" / "Npix*.fits"),
                  recursive=True))
    tile_ipix = [int(Path(t).stem[4:]) for t in tiles]
    print(f"base tiles: {len(tiles)}")
    # 读真实 signal/support 底图
    sig_tiles = {}
    sup_tiles = {}
    for t, ip in zip(tiles, tile_ipix):
        with fits.open(t) as h:
            sig_tiles[ip] = h[0].data.astype(np.float64).copy()
        with fits.open(str(BASE / "support" / "Norder7" /
                           Path(t).parent.name /
                           Path(t).name)) as h:
            sup_tiles[ip] = h[0].data.astype(np.float64).copy()

    # 构造 20 帧：逐帧噪声 + 第 10 帧注入卫星线
    trail_mask = {}   # tile ipix -> list of (x,y) 注入像素
    # 场中心（有限像素平均 lon/lat，切平面参考）
    sample = sig_tiles[tile_ipix[len(tile_ipix) // 2]]
    ys, xs = np.nonzero(np.isfinite(sample))
    leaf_c = (np.int64(tile_ipix[len(tile_ipix) // 2]) << 18) + \
        interleave(511 - ys.astype(np.int64), xs.astype(np.int64))
    lon_c, lat_c = healpix_to_lonlat(leaf_c, NSIDE, order="nested")
    lon_c = float(np.asarray(lon_c.deg).mean())
    lat_c = float(np.asarray(lat_c.deg).mean())
    print(f"field center: lon={lon_c:.5f} lat={lat_c:.5f}")
    # 大圆弧（切平面直线近似，场小 <1°）：(u=lon·cos, v=lat) 从
    # (c-0.4°, c+0.35°) 到 (c+0.4°, c-0.35°)；宽度按角距。
    arc_u0, arc_v0 = lon_c - 0.4, lat_c + 0.35
    arc_u1, arc_v1 = lon_c + 0.4, lat_c - 0.35
    arc_du, arc_dv = arc_u1 - arc_u0, arc_v1 - arc_v0
    arc_len2 = arc_du * arc_du + arc_dv * arc_dv
    # 每 tile 像素网格（FITS 数组坐标 (row, col)）
    col_m = np.arange(512, dtype=np.int64)[None, :]
    row_m = np.arange(512, dtype=np.int64)[:, None]
    x_nest = 511 - row_m
    y_nest = col_m
    local_m = np.zeros((512, 512), dtype=np.int64)
    for b in range(9):
        local_m |= ((x_nest >> b) & 1) << (2 * b)
        local_m |= ((y_nest >> b) & 1) << (2 * b + 1)
    clean_sig = {}   # TRAIL_FRAME 的无注入版本（bias 对照）
    for f in range(N_FRAMES):
        frame = OUT / f"frame{f:02d}.hips"
        copy_tree(BASE, frame)
        mult = 1.0 + rng.uniform(-0.01, 0.01)
        add = rng.normal(0.0, 3e-5)
        mask_f = {}
        for ip in tile_ipix:
            sig = sig_tiles[ip]
            sup = sup_tiles[ip]
            out = sig * mult + add
            # 保持 NaN/support<=0 结构与底图一致
            out[~(np.isfinite(sig))] = sig[~(np.isfinite(sig))]
            out[sup <= 0.0] = sig[sup <= 0.0]
            if f == TRAIL_FRAME:
                leaf = (np.int64(ip) << 18) + local_m
                lon, lat = healpix_to_lonlat(leaf.ravel(), NSIDE,
                                             order="nested")
                lon = np.asarray(lon.deg).reshape(512, 512)
                lat = np.asarray(lat.deg).reshape(512, 512)
                u = lon  # 场小：u 直接用 lon（cos 因子并入阈值）
                v = lat
                # 点到线段垂直距离（投影钳制在线段内）
                t = ((u - arc_u0) * arc_du + (v - arc_v0) * arc_dv) / arc_len2
                t = np.clip(t, 0.0, 1.0)
                pu = arc_u0 + t * arc_du
                pv = arc_v0 + t * arc_dv
                dist = np.hypot(u - pu, v - pv)
                trail = (dist <= TRAIL_WIDTH_DEG) & np.isfinite(sig) & \
                    (sup > 0.0)
                n_trail = int(np.count_nonzero(trail))
                if n_trail:
                    out[trail] += TRAIL_AMP
                    rows_a, cols_a = np.nonzero(trail)
                    mask_f[str(ip)] = [
                        [int(c), int(r)] for c, r in zip(cols_a, rows_a)]
            with fits.open(tiles[tile_ipix.index(ip)]) as h:
                hdr = h[0].header
            hdu = fits.PrimaryHDU(out.astype(np.float32), header=hdr)
            out_path = frame / "signal" / "Norder7" / \
                Path(tiles[tile_ipix.index(ip)]).parent.name / \
                Path(tiles[tile_ipix.index(ip)]).name
            hdu.writeto(out_path, overwrite=True)
            if f == TRAIL_FRAME:
                clean_sig[ip] = out.copy()
        if mask_f:
            trail_mask[str(f)] = mask_f
        print(f"frame{f:02d} done (mult={mult:.4f} add={add:.2e})")

    # 无注入对照帧（同噪声、无 trail）
    clean_frame = OUT / f"frame{TRAIL_FRAME:02d}_clean.hips"
    copy_tree(OUT / f"frame{TRAIL_FRAME:02d}.hips", clean_frame)
    for ip, sig in clean_sig.items():
        with fits.open(tiles[tile_ipix.index(ip)]) as h:
            hdr = h[0].header
        hdu = fits.PrimaryHDU(sig.astype(np.float32), header=hdr)
        hdu.writeto(clean_frame / "signal" / "Norder7" /
                    Path(tiles[tile_ipix.index(ip)]).parent.name /
                    Path(tiles[tile_ipix.index(ip)]).name,
                    overwrite=True)

    with open(OUT / "trail_mask.json", "w", encoding="utf-8") as f:
        json.dump({
            "trail_frame": TRAIL_FRAME,
            "trail_amp": TRAIL_AMP,
            "trail_width_deg": TRAIL_WIDTH_DEG,
            "tiles": trail_mask,
            "n_frames": N_FRAMES,
            "base": str(BASE),
        }, f, indent=2)

    inputs = [f"run/temp/satgate/frame{f:02d}.hips" for f in range(N_FRAMES)]
    inputs_clean = list(inputs)
    inputs_clean[TRAIL_FRAME] = f"run/temp/satgate/frame{TRAIL_FRAME:02d}_clean.hips"
    for name, out_hips in [("stage2_satellite_auto.json",
                            "run/temp/satgate/mosaic_auto.hips"),
                           ("stage2_satellite_clean.json",
                            "run/temp/satgate/mosaic_clean.hips")]:
        in_list = inputs_clean if name.endswith("clean.json") else inputs
        cfg = {
            "version": 1,
            "inputs": {"hips": in_list, "target_order": "auto"},
            "model": {
                "control_grid_per_tile": 8, "patch_radius_pixels": 2,
                "min_samples": 5, "snr_search_radius_deg": 0.05,
                "robust_loss": "huber", "snr_weight_mode": "snr2_normalized",
                "huber_delta": 1.345, "smoothing": 0.0,
                "zero_anchor_weight": 0.001, "sigma_floor": 0.001,
                "support_power": 1.0
            },
            "integration": {
                "precision": "fp32",
                "memory_limit_mb": 16384,
                "rejection": {"method": "auto", "profile": "wbpp_current",
                              "underdetermined_n": 2},
                "weight_mode": "auto",
                "acr_route": "cpu"
            },
            "output": {"hips": out_hips},
            "diagnostics": {"enabled": True}
        }
        with open(OUT / name, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)
    print("satgate built:", OUT)
    print("trail pixels per tile:", {
        k: sum(len(v) for v in m.values())
        for k, m in trail_mask.items()})


if __name__ == "__main__":
    main()
