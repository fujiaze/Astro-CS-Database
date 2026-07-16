# -*- coding: utf-8 -*-
"""
WCS 对齐调试脚本
================
功能: 比较 WCS 投影的 Gaia 星点位置与星点检测器检测出的实际星点位置,
      量化偏差 (dx, dy) 的统计信息, 诊断重投影对齐问题。
用途: 在生成可视化 PNG 前验证 WCS 坐标系是否正确对齐图像像素坐标系。

诊断逻辑:
    1. Plate solve 获取 WCS (含 SIP)
    2. 查询 Gaia 星表 (前 N 颗亮星)
    3. 用 astropy WCS world_to_pixel 投影 Gaia 星点到像素坐标 (x_wcs, y_wcs)
    4. 用 star_detector 检测图像中的星点 (x_det, y_det)
    5. 对每个 WCS 投影星点, 找最近的检测星点 (距离 < 阈值)
    6. 统计匹配对的偏差 (dx, dy) 分布:
       - 平均偏差 (mean_dx, mean_dy)
       - 中位偏差 (median_dx, median_dy)
       - 偏差直方图
    7. 输出诊断结论: 是否存在系统性偏移

用法:
    py debug_alignment.py                          # 单张测试
    py debug_alignment.py --input <path>           # 指定文件
"""

import os
import sys
import time
import math
import ctypes
import argparse
import functools

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

print = functools.partial(print, flush=True)

# ============================================================================
# 路径常量
# ============================================================================
PROJECT_ROOT = r"F:\Astro dev\Astro CS Normalization Database"
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

import numpy as np


# ============================================================================
# FITS 指向解析 (复用)
# ============================================================================

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


def get_fits_pointing(reader, fits_path):
    meta = reader.read_metadata(fits_path)
    fl = meta.observation.focallen
    ps = meta.observation.xpixsz
    w = meta.geometry.width
    h = meta.geometry.height

    img = reader.read_header_only(fits_path)
    kw_dict = {kw.name.upper(): kw.value for kw in img.keywords}
    img.close()

    ra0 = parse_ra_hms(kw_dict.get("OBJCTRA", "0"))
    dec0 = parse_dec_dms(kw_dict.get("OBJCTDEC", "0"))

    s0 = 206.265 * ps / fl if (fl and ps and fl > 0) else 0.0
    fov_x = w * s0 / 3600.0 if s0 > 0 else 0.0
    fov_y = h * s0 / 3600.0 if s0 > 0 else 0.0
    fov_deg = max(fov_x, fov_y)

    return {
        "ra": ra0, "dec": dec0, "focallen": fl, "xpixsz": ps,
        "width": w, "height": h, "s0_arcsec_per_px": s0,
        "fov_deg": fov_deg, "object": kw_dict.get("OBJECT", ""),
    }


# ============================================================================
# 环境初始化
# ============================================================================

def init_environment():
    print("\n" + "=" * 70)
    print("初始化调试环境")
    print("=" * 70)

    dr3sp = os.path.join(PROJECT_ROOT, "GaiaDR3SP")
    dr3 = os.path.join(PROJECT_ROOT, "GaiaDR3")
    if os.path.isdir(dr3sp):
        gaia_dir = dr3sp
        db_type = 2
    elif os.path.isdir(dr3):
        gaia_dir = dr3
        db_type = 1
    else:
        raise RuntimeError("未找到 GaiaDR3SP 或 GaiaDR3 目录")

    from vector_match_v2 import GaiaClientPy
    gaia_client = GaiaClientPy(gaia_dir, db_type=db_type)
    gaia_handle = gaia_client._handle
    if isinstance(gaia_handle, ctypes.c_void_p):
        gaia_handle = gaia_handle.value

    from star_detector import StarDetector, SDetParamsPy
    sdet = StarDetector(params=SDetParamsPy(fitRadius=0))
    sdet_handle = sdet._handle
    if isinstance(sdet_handle, ctypes.c_void_p):
        sdet_handle = sdet_handle.value

    from ipv_solver import IPVSolver
    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)

    from astro_image_io import ImageReader
    reader = ImageReader()

    return {"solver": solver, "gaia_client": gaia_client, "sdet": sdet, "reader": reader}


# ============================================================================
# 核心调试逻辑
# ============================================================================

def debug_frame(env, fits_path, top_n_gaia=200, match_threshold_px=5.0):
    """调试单帧对齐: 比较 WCS 投影星点与检测星点

    参数:
        env: 环境字典
        fits_path: FITS 文件路径
        top_n_gaia: 取前 N 颗 Gaia 亮星做投影比较
        match_threshold_px: 匹配阈值 (像素), 超过此距离视为未匹配
    """
    solver = env["solver"]
    reader = env["reader"]
    gaia_client = env["gaia_client"]
    sdet = env["sdet"]

    # 1. 读取 FITS 指向
    pointing = get_fits_pointing(reader, fits_path)
    w = pointing["width"]
    h = pointing["height"]
    ra0 = pointing["ra"]
    dec0 = pointing["dec"]
    fl = pointing["focallen"]
    ps = pointing["xpixsz"]
    fov_deg = pointing["fov_deg"]
    obj_name = pointing["object"]

    print(f"\n图像: {w}x{h}, FOV={fov_deg:.2f}°, {obj_name}")
    print(f"指向: RA={ra0:.4f}°, Dec={dec0:.4f}°")

    # 2. 读取图像像素数据 (用于星点检测)
    img = reader.read(fits_path)
    img_data = np.array(img.data, dtype=np.uint16)
    img.close()

    # 3. Plate Solve
    print("Plate solving...")
    params = solver.get_default_params()
    result = solver.solve(
        image_path=fits_path,
        ra0=ra0, dec0=dec0,
        focal_length_mm=fl, pixel_size_um=ps,
        params=params,
    )

    if not result.success:
        print(f"!!! Plate solve FAILED: {result.error_msg.decode('utf-8', errors='ignore')}")
        return

    print(f"Plate solve 成功: RMS={result.rms_arcsec:.3f}\", n_pairs={result.n_pairs}")
    print(f"  CD=[{result.cd[0]:.6e}, {result.cd[1]:.6e}, {result.cd[2]:.6e}, {result.cd[3]:.6e}]")
    print(f"  CRVAL=({result.crval[0]:.6f}, {result.crval[1]:.6f})")
    print(f"  CRPIX=({result.crpix[0]:.3f}, {result.crpix[1]:.3f})  (1-based FITS 约定)")
    print(f"  sip_order={result.sip_order}, sip_ap_order={result.sip_ap_order}")
    print(f"  ctype=[{result.ctype1.decode('utf-8', errors='ignore').rstrip(chr(0))}, "
          f"{result.ctype2.decode('utf-8', errors='ignore').rstrip(chr(0))}]")

    # 4. 查询 Gaia 星表
    query_radius = fov_deg * 0.75
    mag_limit = 18.0
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(ra0, dec0, query_radius, mag_limit)
    print(f"Gaia 返回: {len(ra_arr)} 颗星")

    # 5. WCS 投影: (RA, Dec) -> (x_wcs, y_wcs) via astropy
    from ipv_solver import to_astropy_wcs
    from astropy.coordinates import SkyCoord
    import astropy.units as u

    wcs = to_astropy_wcs(result)

    # 验证 astropy WCS 正确性: 中心点 (CRPIX-1, 0-based) 应对应 CRVAL
    cx_0based = float(result.crpix[0]) - 1.0
    cy_0based = float(result.crpix[1]) - 1.0
    center_sky = wcs.pixel_to_world(cx_0based, cy_0based)
    print(f"\n--- astropy WCS 验证 ---")
    print(f"  CRPIX (1-based): ({result.crpix[0]:.3f}, {result.crpix[1]:.3f})")
    print(f"  中心点 (0-based): ({cx_0based:.3f}, {cy_0based:.3f})")
    print(f"  pixel_to_world(中心) = RA={center_sky.ra.deg:.6f}, Dec={center_sky.dec.deg:.6f}")
    print(f"  CRVAL               = RA={result.crval[0]:.6f}, Dec={result.crval[1]:.6f}")
    dra = center_sky.ra.deg - result.crval[0]
    ddec = center_sky.dec.deg - result.crval[1]
    print(f"  差值: dRA={dra:+.6f}°, dDec={ddec:+.6f}°")

    # 验证 4 个角点
    print(f"\n  4 个角点 pixel_to_world (0-based):")
    for name, px, py in [("TL", 0, 0), ("TR", w-1, 0), ("BL", 0, h-1), ("BR", w-1, h-1)]:
        sky = wcs.pixel_to_world(px, py)
        print(f"    {name} ({px},{py}) -> RA={sky.ra.deg:.6f}, Dec={sky.dec.deg:.6f}")

    sky_coords = SkyCoord(ra=ra_arr * u.deg, dec=dec_arr * u.deg)
    x_wcs, y_wcs = wcs.world_to_pixel(sky_coords)
    x_wcs = np.asarray(x_wcs, dtype=np.float64)
    y_wcs = np.asarray(y_wcs, dtype=np.float64)

    # 筛选图像范围内的星
    in_bounds = (x_wcs >= 0) & (x_wcs < w) & (y_wcs >= 0) & (y_wcs < h)
    x_wcs_in = x_wcs[in_bounds]
    y_wcs_in = y_wcs[in_bounds]
    mag_in = mag_arr[in_bounds]
    print(f"WCS 投影后图像范围内: {len(x_wcs_in)} 颗星")

    # 取前 top_n_gaia 颗亮星做比较 (亮星检测可靠)
    sort_idx = np.argsort(mag_in)
    n_cmp = min(top_n_gaia, len(sort_idx))
    x_wcs_top = x_wcs_in[sort_idx[:n_cmp]]
    y_wcs_top = y_wcs_in[sort_idx[:n_cmp]]
    mag_top = mag_in[sort_idx[:n_cmp]]
    print(f"取前 {n_cmp} 颗亮星做比较 (mag {mag_top[0]:.2f}~{mag_top[-1]:.2f})")

    # 6. 星点检测 (直接用 star_detector, 返回 [(x, y), ...] 列表)
    print("星点检测...")
    det_coords = sdet.detect(img_data)
    x_det = np.array([c[0] for c in det_coords], dtype=np.float64)
    y_det = np.array([c[1] for c in det_coords], dtype=np.float64)
    print(f"检测到 {len(x_det)} 颗星")

    # 7. 匹配: 对每个 WCS 投影星点找最近的检测星点
    print(f"\n匹配 (阈值 {match_threshold_px}px)...")
    matched_dx = []
    matched_dy = []
    matched_dist = []
    matched_wcs_x = []
    matched_wcs_y = []
    matched_det_x = []
    matched_det_y = []
    matched_mag = []
    unmatched_count = 0

    for i in range(n_cmp):
        wx = x_wcs_top[i]
        wy = y_wcs_top[i]
        # 找最近的检测星点
        if len(x_det) == 0:
            break
        dx_all = x_det - wx
        dy_all = y_det - wy
        dist_all = np.sqrt(dx_all * dx_all + dy_all * dy_all)
        j_min = int(np.argmin(dist_all))
        d_min = float(dist_all[j_min])

        if d_min < match_threshold_px:
            matched_dx.append(float(dx_all[j_min]))
            matched_dy.append(float(dy_all[j_min]))
            matched_dist.append(d_min)
            matched_wcs_x.append(wx)
            matched_wcs_y.append(wy)
            matched_det_x.append(float(x_det[j_min]))
            matched_det_y.append(float(y_det[j_min]))
            matched_mag.append(float(mag_top[i]))
        else:
            unmatched_count += 1

    n_matched = len(matched_dx)
    print(f"匹配: {n_matched}/{n_cmp} (未匹配 {unmatched_count})")

    if n_matched == 0:
        print("!!! 无匹配对, 无法诊断")
        return

    dx_arr = np.array(matched_dx)
    dy_arr = np.array(matched_dy)
    dist_arr = np.array(matched_dist)

    # 8. 统计偏差
    print("\n" + "=" * 70)
    print("偏差统计 (det - wcs, 像素)")
    print("=" * 70)
    print(f"  dx: mean={np.mean(dx_arr):+.3f}  median={np.median(dx_arr):+.3f}  std={np.std(dx_arr):.3f}  "
          f"[min={np.min(dx_arr):+.3f}, max={np.max(dx_arr):+.3f}]")
    print(f"  dy: mean={np.mean(dy_arr):+.3f}  median={np.median(dy_arr):+.3f}  std={np.std(dy_arr):.3f}  "
          f"[min={np.min(dy_arr):+.3f}, max={np.max(dy_arr):+.3f}]")
    print(f"  dist: mean={np.mean(dist_arr):.3f}  median={np.median(dist_arr):.3f}  "
          f"[min={np.min(dist_arr):.3f}, max={np.max(dist_arr):.3f}]")

    # 9. 偏差直方图 (粗略)
    print("\ndx 直方图 (像素, bin=1.0):")
    _print_histogram(dx_arr, -match_threshold_px, match_threshold_px, 1.0)
    print("\ndy 直方图 (像素, bin=1.0):")
    _print_histogram(dy_arr, -match_threshold_px, match_threshold_px, 1.0)

    # 10. 前 10 个匹配对详情
    print("\n前 10 个匹配对详情 (按星等亮→暗):")
    print(f"  {'mag':>6}  {'wcs_x':>8}  {'wcs_y':>8}  {'det_x':>8}  {'det_y':>8}  {'dx':>7}  {'dy':>7}  {'dist':>6}")
    for i in range(min(10, n_matched)):
        print(f"  {matched_mag[i]:6.2f}  {matched_wcs_x[i]:8.2f}  {matched_wcs_y[i]:8.2f}  "
              f"{matched_det_x[i]:8.2f}  {matched_det_y[i]:8.2f}  {matched_dx[i]:+7.3f}  "
              f"{matched_dy[i]:+7.3f}  {matched_dist[i]:6.3f}")

    # 11. 诊断结论
    print("\n" + "=" * 70)
    print("诊断结论")
    print("=" * 70)
    median_dx = float(np.median(dx_arr))
    median_dy = float(np.median(dy_arr))
    median_dist = float(np.median(dist_arr))
    print(f"  中位偏差: dx={median_dx:+.3f}px, dy={median_dy:+.3f}px, dist={median_dist:.3f}px")

    if median_dist < 1.0:
        print("  => 对齐良好 (中位偏差 < 1px), 重投影坐标系正确")
    elif median_dist < 3.0:
        print("  => 轻微偏差 (1-3px), 可能有 sub-pixel 系统性偏移")
    else:
        print("  => 显著偏差 (>3px), 存在坐标系问题")

    # 系统性偏移检测
    if abs(median_dx) > 1.0 and abs(median_dx) > 2 * np.std(dx_arr):
        print(f"  => X 方向系统性偏移: {median_dx:+.3f}px")
    if abs(median_dy) > 1.0 and abs(median_dy) > 2 * np.std(dy_arr):
        print(f"  => Y 方向系统性偏移: {median_dy:+.3f}px")
        if median_dy < -1.0:
            print("     (dy<0: det 在 wcs 上方, 可能需要 Y-flip)")
        elif median_dy > 1.0:
            print("     (dy>0: det 在 wcs 下方)")

    # 检查是否为整数像素偏移 (可能是 1-based vs 0-based)
    if abs(median_dx - round(median_dx)) < 0.2 and abs(median_dx) > 0.5:
        print(f"  => X 偏移接近整数 {round(median_dx)}px, 可能是 1-based/0-based 问题")
    if abs(median_dy - round(median_dy)) < 0.2 and abs(median_dy) > 0.5:
        print(f"  => Y 偏移接近整数 {round(median_dy)}px, 可能是 1-based/0-based 问题")


def _print_histogram(arr, lo, hi, bin_width):
    bins = np.arange(lo, hi + bin_width, bin_width)
    counts, _ = np.histogram(arr, bins=bins)
    max_count = max(counts.max(), 1)
    for i, c in enumerate(counts):
        bar = "#" * int(c * 40 / max_count)
        print(f"  [{bins[i]:+5.1f}, {bins[i+1]:+5.1f}): {c:4d} {bar}")


# ============================================================================
# 主入口
# ============================================================================

def scan_all_fits():
    testdata_dir = os.path.join(PROJECT_ROOT, "testdata")
    frames = []
    for root, dirs, files in os.walk(testdata_dir):
        for f in files:
            if f.lower().endswith(".fts"):
                full_path = os.path.join(root, f)
                label = os.path.splitext(f)[0]
                frames.append((label, full_path))
    return frames


def main():
    parser = argparse.ArgumentParser(description="WCS 对齐调试")
    parser.add_argument("--input", type=str, default=None, help="指定输入文件")
    parser.add_argument("--top-n", type=int, default=200, help="比较前 N 颗 Gaia 亮星")
    parser.add_argument("--threshold", type=float, default=5.0, help="匹配阈值 (像素)")
    args = parser.parse_args()

    env = init_environment()

    if args.input:
        fits_path = args.input
        label = os.path.splitext(os.path.basename(fits_path))[0]
    else:
        all_frames = scan_all_fits()
        if not all_frames:
            print("未找到 testdata 中的 .fts 文件")
            return
        label, fits_path = all_frames[0]

    print(f"\n调试帧: {label}")
    print(f"文件: {fits_path}")

    debug_frame(env, fits_path, top_n_gaia=args.top_n, match_threshold_px=args.threshold)


if __name__ == "__main__":
    main()
