#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-004 — 验证 CRPIX 0.5 px 偏移假设

假设:
    IPV 求解器内部 U 坐标中心 = img_w / 2.0 = 2048.0 (0-based)
    WCS 输出 CRPIX = width/2.0 + 0.5 = 2048.5 (1-based)
    astropy world_to_pixel 把 CRPIX 当 1-based, 转 0-based 得 2047.5
    => 投影位置比 IPV 内部中心偏左 0.5 px
    => detector - predicted ≈ +0.5 px (系统性正偏移)

验证方法:
    对 T2_RED_LDN43_solved.fits, 分别用不同 CRPIX 偏移重新投影 Gaia 亮星,
    匹配检测星点, 计算残差. 若 offset=+0.5 时残差最小, 则假设成立.

用法:
    python verify_crpix_offset.py
"""
import os
import sys
import json
import math
import ctypes
import numpy as np
from pathlib import Path

PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
MINGW_BIN = r"C:\msys64\mingw64\bin"

if MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "archive", "vector_method", "python", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "star_detector", "python"))


def parse_ra_hms(s):
    s = str(s).strip()
    parts = s.replace(":", " ").split()
    if len(parts) == 3:
        h, m, sec = parts
        return (int(h) + int(m) / 60.0 + float(sec) / 3600.0) * 15.0
    return float(s)


def parse_dec_dms(s):
    s = str(s).strip()
    sign = 1.0
    if s.startswith("-"):
        sign = -1.0
        s = s[1:]
    elif s.startswith("+"):
        s = s[1:]
    parts = s.replace(":", " ").split()
    if len(parts) == 3:
        d, m, sec = parts
        return sign * (int(d) + int(m) / 60.0 + float(sec) / 3600.0)
    return sign * float(s)


def main():
    from astropy.io import fits
    from astropy.wcs import WCS
    from astropy.coordinates import SkyCoord
    import astropy.units as u
    from scipy.spatial import cKDTree

    fits_path = os.path.join(PROJECT_ROOT, "T2_RED_LDN43_solved.fits")
    print(f"读取 FITS: {fits_path}")

    with fits.open(fits_path, mode="readonly", memmap=False) as hdul:
        header = hdul[0].header.copy()
        pixels = hdul[0].data.astype(np.float32)
    h, w = pixels.shape
    print(f"图像尺寸: {w}x{h}")

    # 原始 WCS
    wcs_orig = WCS(header)
    crpix1_orig = float(wcs_orig.wcs.crpix[0])
    crpix2_orig = float(wcs_orig.wcs.crpix[1])
    print(f"原始 CRPIX: ({crpix1_orig}, {crpix2_orig}) (1-based)")
    print(f"原始 CRPIX 0-based: ({crpix1_orig - 1}, {crpix2_orig - 1})")
    print(f"IPV 内部中心 (img_w/2.0): ({w/2.0}, {h/2.0}) (0-based)")
    print(f"差异: CRPIX_0based - IPV_center = ({crpix1_orig - 1 - w/2.0}, {crpix2_orig - 1 - h/2.0})")

    # 初始化 PlateSolve 环境 (拿 Gaia + 检测星点)
    from vector_match_v2 import GaiaClientPy
    from star_detector import StarDetector, SDetParamsPy
    from ipv_solver import IPVSolver
    from astro_image_io import ImageReader

    dr3sp = os.path.join(PROJECT_ROOT, "GaiaDR3SP")
    db_type = 2 if os.path.isdir(dr3sp) else 1
    gaia_dir = dr3sp if os.path.isdir(dr3sp) else os.path.join(PROJECT_ROOT, "GaiaDR3")
    print(f"GaiaClient: {gaia_dir} (db_type={db_type})")
    gaia_client = GaiaClientPy(gaia_dir, db_type=db_type)
    gaia_handle = gaia_client._handle
    if isinstance(gaia_handle, ctypes.c_void_p):
        gaia_handle = gaia_handle.value

    sdet = StarDetector(params=SDetParamsPy(fitRadius=0))
    sdet_handle = sdet._handle
    if isinstance(sdet_handle, ctypes.c_void_p):
        sdet_handle = sdet_handle.value

    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)

    reader = ImageReader()

    # 读 FITS 指向
    meta = reader.read_metadata(fits_path)
    fl = meta.observation.focallen
    ps = meta.observation.xpixsz
    img_hdr = reader.read_header_only(fits_path)
    kw_dict = {kw.name.upper(): kw.value for kw in img_hdr.keywords}
    img_hdr.close()
    ra0 = parse_ra_hms(kw_dict.get("OBJCTRA", "0"))
    dec0 = parse_dec_dms(kw_dict.get("OBJCTDEC", "0"))
    s0 = 206.265 * ps / fl if (fl and ps and fl > 0) else 0.0
    fov_deg = max(w, h) * s0 / 3600.0 if s0 > 0 else 0.0
    print(f"指向: RA={ra0:.4f}°, Dec={dec0:.4f}°, fl={fl}mm, ps={ps}um, fov={fov_deg:.2f}°")

    # Gaia 查询 (取最亮 200 颗)
    query_radius = fov_deg * 0.75
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(ra0, dec0, query_radius, 18.0)
    print(f"Gaia 返回: {len(ra_arr)} 颗")

    # PlateSolve 求解拿检测星点
    params = solver.get_default_params()
    result = solver.solve(
        image_path=fits_path,
        ra0=ra0, dec0=dec0,
        focal_length_mm=fl, pixel_size_um=ps,
        params=params,
    )
    print(f"PlateSolve: success={result.success}, rms_px={result.rms_px:.4f}, n_pairs={result.n_pairs}")

    # 用 callback 拿检测星点
    detected_holder = []
    def callback(detections, n, user_data):
        if n > 0:
            detected_holder.append(detections.copy())

    result2 = solver.solve_from_memory_with_callback(
        pixels, w, h, ra0, dec0, fl, ps,
        callback, user_data=None, params=params,
    )
    if not detected_holder:
        print("ERROR: 未检测到星点")
        return
    detections = detected_holder[0]
    det_xy = detections[:, :2].astype(np.float64)
    print(f"检测星点: {len(det_xy)} 颗")

    # 取最亮 60 颗 Gaia 星
    n_bright = 60
    bright_idx = np.argsort(mag_arr)[:n_bright]
    ra_bright = ra_arr[bright_idx]
    dec_bright = dec_arr[bright_idx]
    mag_bright = mag_arr[bright_idx]
    print(f"取最亮 {n_bright} 颗 Gaia 星 (mag {mag_bright[0]:.2f}~{mag_bright[-1]:.2f})")

    # 测试不同 CRPIX 偏移
    offsets = [-1.0, -0.5, 0.0, +0.5, +1.0]
    print("\n" + "=" * 80)
    print(f"{'offset':>8} | {'n_match':>8} | {'median':>8} | {'mean':>8} | {'p68':>8} | {'res_x_mean':>11} | {'res_y_mean':>11}")
    print("=" * 80)

    results = []
    for offset in offsets:
        # 构建修改后的 WCS
        wcs_mod = WCS(header)
        wcs_mod.wcs.crpix = [crpix1_orig + offset, crpix2_orig + offset]

        # 投影 Gaia 亮星
        sky = SkyCoord(ra_bright * u.deg, dec_bright * u.deg)
        pred_x, pred_y = wcs_mod.world_to_pixel(sky)
        pred_xy = np.column_stack([pred_x, pred_y])

        # kd-tree 匹配
        valid = np.isfinite(pred_x) & np.isfinite(pred_y)
        if np.sum(valid) == 0:
            continue
        pred_xy_v = pred_xy[valid]
        mag_v = mag_bright[valid]

        tree_det = cKDTree(det_xy)
        tree_pred = cKDTree(pred_xy_v)
        matches = []
        dists_pred, idxs_pred = tree_det.query(pred_xy_v, k=1, distance_upper_bound=3.0)
        for i, (d, det_idx) in enumerate(zip(dists_pred, idxs_pred)):
            if not np.isfinite(d) or d > 3.0:
                continue
            d_back, idx_back = tree_pred.query(det_xy[det_idx], k=1, distance_upper_bound=3.0)
            if idx_back == i and d_back <= 3.0:
                matches.append((int(det_idx), i, float(d)))

        if len(matches) == 0:
            print(f"{offset:+8.2f} | {0:>8} | N/A")
            continue

        res_x = np.array([det_xy[m[0], 0] - pred_xy_v[m[1], 0] for m in matches])
        res_y = np.array([det_xy[m[0], 1] - pred_xy_v[m[1], 1] for m in matches])
        dist = np.array([m[2] for m in matches])

        median = float(np.median(dist))
        mean = float(np.mean(dist))
        p68 = float(np.percentile(dist, 68.3))
        rx_mean = float(np.mean(res_x))
        ry_mean = float(np.mean(res_y))

        print(f"{offset:+8.2f} | {len(matches):>8} | {median:>8.4f} | {mean:>8.4f} | {p68:>8.4f} | {rx_mean:>+11.4f} | {ry_mean:>+11.4f}")
        results.append({
            "offset": offset,
            "n_match": len(matches),
            "median_px": median,
            "mean_px": mean,
            "p68_px": p68,
            "res_x_mean_px": rx_mean,
            "res_y_mean_px": ry_mean,
        })

    # 找最优偏移
    if results:
        best = min(results, key=lambda r: r["median_px"])
        print(f"\n最优偏移: {best['offset']:+.2f} (median={best['median_px']:.4f} px)")
        print(f"原始偏移 0.00 对应 median={next((r for r in results if r['offset']==0.0), {}).get('median_px', 'N/A')}")

    # 输出 JSON
    out_path = os.path.join(PROJECT_ROOT, "engineering_v1.2", "evidence", "P11-004", "reports", "crpix_offset_verification.json")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump({
            "fits": fits_path,
            "crpix_orig_1based": [crpix1_orig, crpix2_orig],
            "crpix_orig_0based": [crpix1_orig - 1, crpix2_orig - 1],
            "ipv_internal_center_0based": [w / 2.0, h / 2.0],
            "diff_crpix0based_vs_ipv": [crpix1_orig - 1 - w / 2.0, crpix2_orig - 1 - h / 2.0],
            "results": results,
        }, f, indent=2)
    print(f"\n报告: {out_path}")


if __name__ == "__main__":
    main()
