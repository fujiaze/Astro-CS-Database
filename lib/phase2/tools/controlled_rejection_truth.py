#!/usr/bin/env python3
# lib/phase2/tools/controlled_rejection_truth.py — V17 受控 clean rejection truth
#
# 已知真值（known sky + stars/PSF + faint extended structure + 独立噪声，
# 零 outlier）→ 20 帧 per-exposure HiPS → 生产 Phase2（truth=none 与
# wbpp_2_9_1 auto）→ 测 true sample FPR / pixel FPR / star flux bias /
# PSF 宽度 / faint structure transfer / 背景噪声效率。
#
# V17：合成 HiPS 必须满足生产 reader/coverage/frame_id 全部契约：
#   - signal/support/snr 子产品 properties 含 hips_version/hips_order/
#     hips_tile_width/hips_frame/obs_filter；
#   - 每子产品 Moc.fits（UNIQ order-7 编码）→ aio_hips_open 能枚举 tile；
#   - snr TSV（star_id ra dec snr quality_flags photometric_status）→
#     帧级 SNR median 与局部 SNR 映射有真实来源。
import json
import shutil
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy_healpix import healpix_to_lonlat

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "run" / "temp" / "v17_control_truth"
NSIDE = 1 << 16
N_FRAMES = 20
ORDER = 7
TILE = 0
STAR_TABLE = []


def write_moc(path):
    """order-7 单 tile（ipix=0）的 UNIQ MOC FITS（reader 可枚举）。"""
    uniq = np.array([4 * (1 << (2 * ORDER)) + TILE], dtype=np.int64)
    c = fits.Column(name="UNIQ", format="K", array=uniq)
    hdu = fits.BinTableHDU.from_columns([c])
    hdu.name = "MOC"
    hdu.writeto(path, overwrite=True)


def write_properties(d):
    (d / "properties").write_text(
        "creator_did=ivo://astrocs/phase1\n"
        "obs_title=V17 controlled rejection truth\n"
        "obs_filter=Red\n"
        "obs_exptime=1200\n"
        "obs_date=2026-08-14T00:00:00\n"
        "hips_version=1.4\n"
        "hips_order=7\n"
        "hips_tile_width=512\n"
        "hips_frame=equatorial\n"
        "hips_pixel_scale=0.0001125\n"
        "moc_sky_fraction=1.0\n", encoding="utf-8")


def build_truth():
    """已知真值：常数天空 0.01 + 平滑 extended 结构 + 8 个 PSF 星（0 离群）。"""
    w = h = 512
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    sky = 0.01 + 0.0 * xx
    ext = 0.02 * np.exp(-((xx - 180.0) ** 2 + (yy - 200.0) ** 2) / (2 * 60.0 ** 2))
    ext += 0.015 * np.exp(-((xx - 320.0) ** 2 + (yy - 300.0) ** 2) / (2 * 90.0 ** 2))
    stars = np.zeros((h, w))
    rng = np.random.default_rng(7)
    star_rows = []
    for i in range(8):
        cx, cy = rng.uniform(80, w - 80, 2)
        amp = rng.uniform(0.3, 0.9)
        fwhm = rng.uniform(2.5, 4.0)
        s = fwhm / 2.3548
        stars += amp * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * s ** 2))
        star_rows.append({"id": i, "x": float(cx), "y": float(cy),
                          "amp": float(amp), "fwhm": float(fwhm)})
    truth = (sky + ext + stars).astype(np.float32)
    return truth, star_rows


def pixel_to_sky(rows, cols):
    """FITS (row, col) 像素 → order-7 tile 内 NESTED leaf → (ra, dec)°。"""
    rows = np.asarray(rows, dtype=np.int64)
    cols = np.asarray(cols, dtype=np.int64)
    x_nest = 511 - rows
    y_nest = cols
    local = np.zeros(np.broadcast(x_nest, y_nest).shape, dtype=np.int64)
    for b in range(9):
        local |= ((x_nest >> b) & 1) << (2 * b)
        local |= ((y_nest >> b) & 1) << (2 * b + 1)
    leaf = (np.int64(TILE) << 18) + local
    lon, lat = healpix_to_lonlat(leaf, NSIDE, order="nested")
    return np.asarray(lon.deg), np.asarray(lat.deg)


def build_hips_from_truth(truth, frame_idx, out_dir):
    """把真值+独立噪声写入单帧 HiPS（signal tiles；support=1 全覆盖）。"""
    d = out_dir / f"frame{frame_idx:02d}.hips"
    if d.exists():
        shutil.rmtree(d)
    (d / "signal" / "Norder7" / "Dir0").mkdir(parents=True)
    (d / "support" / "Norder7" / "Dir0").mkdir(parents=True)
    (d / "snr" / "Norder7" / "Dir0").mkdir(parents=True)
    rng = np.random.default_rng(1000 + frame_idx)
    noise = rng.normal(0.0, 0.002, truth.shape)  # 独立 Gaussian（0 离群）
    sig = truth + noise
    # 覆盖全 tile 的简化 HiPS：单 order-7 tile（含 512² 全覆盖）
    fits.PrimaryHDU(sig).writeto(
        d / "signal" / "Norder7" / "Dir0" / f"Npix{TILE}.fits",
        overwrite=True)
    fits.PrimaryHDU(np.ones((512, 512), dtype=np.float32)).writeto(
        d / "support" / "Norder7" / "Dir0" / f"Npix{TILE}.fits",
        overwrite=True)
    write_properties(d / "signal")
    write_properties(d / "support")
    write_properties(d / "snr")
    write_moc(d / "signal" / "Moc.fits")
    write_moc(d / "support" / "Moc.fits")
    write_moc(d / "snr" / "Moc.fits")
    # SNR catalogue：8 颗真值星（位置从像素换算到天球），snr 常量 8.0
    rows = np.array([int(s["y"]) for s in STAR_TABLE], dtype=np.int64)
    cols = np.array([int(s["x"]) for s in STAR_TABLE], dtype=np.int64)
    ra, dec = pixel_to_sky(rows, cols)
    lines = []
    for i, (r0, d0) in enumerate(zip(ra, dec)):
        lines.append(f"{i} {r0:.8f} {d0:.8f} 8.0 0 0")
    (d / "snr" / "Norder7" / "Dir0" / f"Npix{TILE}.tsv").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")
    (d / "snr" / "metadata.xml").write_text(
        '<?xml version="1.0"?><VOTABLE version="1.3"/>', encoding="utf-8")


def make_config(method, out):
    inputs = [f"run/temp/v17_control_truth/frame{n:02d}.hips"
              for n in range(N_FRAMES)]
    return {"version": 1, "inputs": {"hips": inputs, "target_order": "auto"},
            "model": {"control_grid_per_tile": 8, "patch_radius_pixels": 2,
                      "min_samples": 5, "snr_search_radius_deg": 0.05,
                      "robust_loss": "huber", "snr_weight_mode": "snr2_normalized",
                      "huber_delta": 1.345, "smoothing": 0.0,
                      "zero_anchor_weight": 0.001, "sigma_floor": 0.001,
                      "support_power": 1.0},
            "integration": {"precision": "fp32", "memory_limit_mb": 4096,
                            "rejection": {"method": method,
                                          "profile": "wbpp_2_9_1",
                                          "normalization": "astrocs_median_center_v1",
                                          "underdetermined_n": 2},
                            "weight_mode": "auto", "acr_route": "cpu"},
            "output": {"hips": out}, "diagnostics": {"enabled": True}}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    global STAR_TABLE
    truth, STAR_TABLE = build_truth()
    for i in range(N_FRAMES):
        build_hips_from_truth(truth, i, OUT)
    json.dump({"truth": truth.tolist(), "stars": STAR_TABLE,
               "n_frames": N_FRAMES, "order": ORDER, "tile": TILE,
               "sky": 0.01, "noise_sigma": 0.002,
               "structure": [
                   {"x": 180.0, "y": 200.0, "sigma": 60.0, "amp": 0.02},
                   {"x": 320.0, "y": 300.0, "sigma": 90.0, "amp": 0.015}]},
              open(OUT / "truth.json", "w"), indent=2)
    for name, method in [("truth", "none"), ("auto", "auto")]:
        json.dump(make_config(method, f"run/temp/v17_control_truth/mosaic_{name}.hips"),
                  open(OUT / f"stage2_{name}.json", "w"), indent=2)
    print("controlled truth built:", N_FRAMES, "frames")


if __name__ == "__main__":
    main()
