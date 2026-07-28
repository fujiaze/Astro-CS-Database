# -*- coding: utf-8 -*-
"""
WCS 重投影可视化校核脚本
=========================
功能: 对 testdata 中的 FITS 图像执行 plate solving，将 Gaia 星表前 1000 亮星
      重投影到图像上，使用 Siril MTF 直方图拉伸，输出全尺寸 PNG 供人工校核。
用途: 通过红色十字标记与实际星点位置对比，直观验证 plate solve WCS 精度。

拉伸方式: Siril Auto Stretch (MTF, Midtone Transfer Function)
  - shadows_clipping = -2.8 sigma
  - target_background = 0.25
  - 参考: siril-1.4.3/src/filters/mtf.c

输出: 全尺寸 PNG，无图例、无边框，仅标注 RMS 等参数

用法:
    py visualize_reproject.py --single          # 单张测试
    py visualize_reproject.py --batch            # 批量处理所有 testdata
    py visualize_reproject.py --single --input <path>  # 指定文件

依赖:
    - ipv_solver.dll + ipv_solver.py
    - gaia_client.dll + vector_match_v2.py (GaiaClientPy)
    - star_detector.dll + star_detector.py
    - astro_image_io.dll + astro_image_io.py
    - Python: numpy, astropy, PIL
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

OUTPUT_DIR = os.path.join(
    PROJECT_ROOT, "lib", "plate_solve", "logs", "visualize_reproject"
)


# ============================================================================
# Siril MTF 直方图拉伸
# ============================================================================

def _mtf(x, m, lo, hi):
    """MTF (Midtone Transfer Function) - Siril src/filters/mtf.c:95"""
    if x <= lo:
        return 0.0
    if x >= hi:
        return 1.0
    xp = (x - lo) / (hi - lo)
    return ((m - 1.0) * xp) / (((2.0 * m - 1.0) * xp) - m)


def siril_autostretch(data_uint16):
    """Siril Auto Stretch (MTF)

    参考: siril-1.4.3/src/filters/mtf.c find_linked_midtones_balance()

    算法:
        1. 归一化到 [0, 1] (除以 65535)
        2. 计算 median 和 MAD (中位绝对偏差)
        3. MAD 归一化: mad_sigma = MAD * 1.4826
        4. 阴影点: c0 = max(median + (-2.8) * mad_sigma, 0)
        5. 中调: midtones = MTF(median - c0, 0.25, 0, 1)
        6. 对每个像素应用 MTF(x, midtones, c0, 1.0)
        7. 映射到 [0, 255] uint8
    """
    img_f = data_uint16.astype(np.float64) / 65535.0

    median = float(np.median(img_f))
    mad = float(np.median(np.abs(img_f - median)))
    mad_sigma = mad * 1.4826

    shadows_clipping = -2.8
    target_bg = 0.25

    c0 = median + shadows_clipping * mad_sigma
    if c0 < 0.0:
        c0 = 0.0

    m2 = median - c0
    if m2 <= 0.0:
        midtones = 0.0
    else:
        midtones = _mtf(m2, target_bg, 0.0, 1.0)

    if midtones <= 0.0 or midtones >= 1.0:
        lo_p, hi_p = np.percentile(img_f, [0.5, 99.5])
        if hi_p <= lo_p:
            hi_p = lo_p + 1e-6
        stretched = np.clip((img_f - lo_p) / (hi_p - lo_p), 0.0, 1.0)
    else:
        vec_mtf = np.vectorize(_mtf, otypes=[np.float64])
        stretched = vec_mtf(img_f, midtones, c0, 1.0)

    result = (stretched * 255.0).clip(0, 255).astype(np.uint8)
    return result


# ============================================================================
# FITS 指向解析 (复用自 run_ipv_baseline.py)
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
    print("初始化可视化环境")
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
    print(f"[1/4] GaiaClient: {gaia_dir} (db_type={db_type})")
    gaia_client = GaiaClientPy(gaia_dir, db_type=db_type)
    gaia_handle = gaia_client._handle
    if isinstance(gaia_handle, ctypes.c_void_p):
        gaia_handle = gaia_handle.value

    from star_detector import StarDetector, SDetParamsPy
    print("[2/4] StarDetector")
    sdet = StarDetector(params=SDetParamsPy(fitRadius=0))
    sdet_handle = sdet._handle
    if isinstance(sdet_handle, ctypes.c_void_p):
        sdet_handle = sdet_handle.value

    from ipv_solver import IPVSolver
    print("[3/4] IPVSolver")
    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)

    from astro_image_io import ImageReader
    print("[4/4] ImageReader")
    reader = ImageReader()

    return {
        "solver": solver, "gaia_client": gaia_client, "sdet": sdet,
        "reader": reader, "gaia_dir": gaia_dir, "db_type": db_type,
    }


# ============================================================================
# 核心可视化逻辑
# ============================================================================

import numpy as np
from PIL import Image as PILImage, ImageDraw, ImageFont


def wcs_world_to_pixel(result, ra_arr, dec_arr):
    """WCS-SIP 重投影: 天球坐标 (RA, Dec) -> 像素坐标 (x, y)

    使用 astropy.wcs.WCS 标准 WCS-SIP 实现 (可靠, 无手动投影 bug)

    坐标约定:
        - astropy world_to_pixel() 返回 0-based 像素坐标
        - y 坐标直接对应 FITS data 的行索引 (data[y, x])
        - 与 PIL Image.fromarray(data) 的坐标系一致 (无需 Y-flip)

    参数:
        result: IpvWcsResult 结构体
        ra_arr, dec_arr: 天球坐标数组 (度)

    返回:
        (x_pix, y_pix): 0-based 像素坐标数组
    """
    from ipv_solver import to_astropy_wcs
    from astropy.coordinates import SkyCoord
    import astropy.units as u

    wcs = to_astropy_wcs(result)
    sky_coords = SkyCoord(ra=ra_arr * u.deg, dec=dec_arr * u.deg)
    x_pix, y_pix = wcs.world_to_pixel(sky_coords)
    return np.asarray(x_pix, dtype=np.float64), np.asarray(y_pix, dtype=np.float64)


def visualize_frame(env, fits_path, out_path, top_n=1000, save_solved_fits=False):
    """处理单帧: plate solve -> Gaia 查询 -> WCS 重投影 -> MTF 拉伸 -> PNG 输出

    参数:
        env: init_environment() 返回的字典
        fits_path: FITS 文件路径
        out_path: 输出 PNG 路径
        top_n: 标记前 N 颗最亮星 (默认 1000)
        save_solved_fits: True=同时保存带 WCS 的 FITS 副本 (复制原文件 + 写 WCS header)

    返回:
        dict: 结果信息 (success, rms, n_pairs, n_marked, elapsed)
    """
    solver = env["solver"]
    reader = env["reader"]
    gaia_client = env["gaia_client"]

    t0 = time.time()

    # 1. 读取 FITS 指向信息
    pointing = get_fits_pointing(reader, fits_path)
    w = pointing["width"]
    h = pointing["height"]
    ra0 = pointing["ra"]
    dec0 = pointing["dec"]
    fl = pointing["focallen"]
    ps = pointing["xpixsz"]
    s0 = pointing["s0_arcsec_per_px"]
    fov_deg = pointing["fov_deg"]
    obj_name = pointing["object"]

    print(f"  图像: {w}x{h}, FOV={fov_deg:.2f}°, {obj_name}")
    print(f"  指向: RA={ra0:.4f}°, Dec={dec0:.4f}°, fl={fl}mm, ps={ps}um")

    # 2. 读取图像像素数据
    img = reader.read(fits_path)
    img_data = np.array(img.data, dtype=np.uint16)
    img.close()

    # 3. Plate Solve
    print("  Plate solving...")
    params = solver.get_default_params()
    result = solver.solve(
        image_path=fits_path,
        ra0=ra0, dec0=dec0,
        focal_length_mm=fl, pixel_size_um=ps,
        params=params,
    )

    success = bool(result.success)
    rms_arcsec = float(result.rms_arcsec)
    rms_px = float(result.rms_px)
    n_pairs = int(result.n_pairs)
    n_detected = int(result.n_detected)
    n_catalog = int(result.n_catalog)
    trans_order = int(result.trans_order)
    best_inliers = int(result.best_inliers)

    if not success:
        print(f"  !!! Plate solve FAILED: {result.error_msg.decode('utf-8', errors='ignore')}")

    # 3.5 保存带 WCS 的 FITS 副本 (可选)
    if success and save_solved_fits:
        import shutil
        from solve_and_write_wcs import write_wcs_to_fits
        solved_fits_path = out_path.replace("_reproject.png", "_solved.fits")
        print(f"  保存带 WCS 的 FITS: {solved_fits_path}")
        try:
            shutil.copy2(fits_path, solved_fits_path)
            write_wcs_to_fits(solved_fits_path, result, overwrite=True)
        except Exception as e:
            print(f"  !!! 保存 solved FITS 失败: {e}")

    # 4. 查询 Gaia 星表 (高星等上限确保覆盖足够多的星)
    query_radius = fov_deg * 0.75
    mag_limit = 18.0
    print(f"  Gaia cone_search: radius={query_radius:.2f}°, mag<={mag_limit}")
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(
        ra0, dec0, query_radius, mag_limit
    )
    print(f"  Gaia 返回: {len(ra_arr)} 颗星")

    # 5. WCS-SIP 手动重投影: (RA, Dec) -> (x_pix, y_pix)
    x_pix, y_pix = wcs_world_to_pixel(result, ra_arr, dec_arr)

    # 7. 筛选: 仅保留图像范围内的星
    in_bounds = (x_pix >= 0) & (x_pix < w) & (y_pix >= 0) & (y_pix < h)
    x_in = x_pix[in_bounds]
    y_in = y_pix[in_bounds]
    mag_in = mag_arr[in_bounds]
    print(f"  图像范围内: {len(x_in)} 颗星")

    # 8. 按星等排序 (亮星优先), 取前 top_n
    sort_idx = np.argsort(mag_in)
    n_mark = min(top_n, len(sort_idx))
    x_mark = x_in[sort_idx[:n_mark]]
    y_mark = y_in[sort_idx[:n_mark]]
    mag_mark = mag_in[sort_idx[:n_mark]]
    print(f"  标记前 {n_mark} 颗最亮星 (mag {mag_mark[0]:.2f}~{mag_mark[-1]:.2f})")

    # 9. Siril MTF 拉伸
    print("  Siril MTF 拉伸...")
    stretched = siril_autostretch(img_data)

    # 10. 渲染 PNG (PIL, 精确像素尺寸)
    # astropy world_to_pixel 返回 0-based 坐标, 直接对应 PIL Image.fromarray(data) 的坐标系
    # (PIL 的 (x, y) 对应 data[y, x], astropy 的 (x, y) 也对应 data[y, x])
    # 因此无需 Y-flip, 直接用 astropy 坐标即可对齐星点
    print("  渲染 PNG...")
    img_rgb = PILImage.fromarray(stretched, mode="L").convert("RGB")
    draw = ImageDraw.Draw(img_rgb)

    # 红色十字标记 (astropy 0-based 坐标, 直接用于 PIL)
    cross_size = max(3, min(w, h) // 400)
    cross_w = max(1, min(w, h) // 2000)
    for i in range(n_mark):
        cx = int(round(x_mark[i]))
        cy = int(round(y_mark[i]))
        draw.line(
            [(cx - cross_size, cy), (cx + cross_size, cy)],
            fill=(255, 0, 0), width=cross_w,
        )
        draw.line(
            [(cx, cy - cross_size), (cx, cy + cross_size)],
            fill=(255, 0, 0), width=cross_w,
        )

    # 参数标注 (左上角, 黄色文字 + 黑色阴影)
    font_size = max(14, min(w, h) // 60)
    try:
        font = ImageFont.truetype("arial.ttf", font_size)
    except (OSError, IOError):
        font = ImageFont.load_default()

    elapsed = time.time() - t0
    scale_arcsec = math.sqrt(abs(
        float(result.cd[0]) * float(result.cd[3]) -
        float(result.cd[1]) * float(result.cd[2])
    )) * 3600.0

    lines = [
        f"Object: {obj_name}",
        f"Success: {'YES' if success else 'NO'}",
        f"RMS: {rms_arcsec:.3f}\" ({rms_px:.3f} px)",
        f"n_pairs: {n_pairs}  (inliers: {best_inliers})",
        f"Scale: {scale_arcsec:.3f}\"/px  (s0: {s0:.3f}\"/px)",
        f"trans_order: {trans_order}  sip_order: {int(result.sip_order)}",
        f"n_detected: {n_detected}  n_catalog: {n_catalog}",
        f"n_marked: {n_mark}/{len(x_in)} (top {top_n} brightest in FOV)",
        f"FOV: {fov_deg:.2f}°  {w}x{h}",
        f"Elapsed: {elapsed:.1f}s",
    ]

    line_h = font_size + 4
    margin = max(5, min(w, h) // 200)
    for i, line in enumerate(lines):
        y_text = margin + i * line_h
        for dx, dy in [(-1, -1), (-1, 1), (1, -1), (1, 1), (0, -1), (0, 1), (-1, 0), (1, 0)]:
            draw.text((margin + dx, y_text + dy), line, fill=(0, 0, 0), font=font)
        draw.text((margin, y_text), line, fill=(255, 255, 0), font=font)

    # 保存
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    img_rgb.save(out_path, "PNG")
    print(f"  => {out_path} ({w}x{h})")

    return {
        "success": success,
        "rms_arcsec": rms_arcsec,
        "rms_px": rms_px,
        "n_pairs": n_pairs,
        "n_marked": n_mark,
        "n_in_fov": int(len(x_in)),
        "elapsed": elapsed,
        "object": obj_name,
    }


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
    parser = argparse.ArgumentParser(description="WCS 重投影可视化校核")
    parser.add_argument("--single", action="store_true", help="单张测试模式")
    parser.add_argument("--batch", action="store_true", help="批量处理模式")
    parser.add_argument("--input", type=str, default=None, help="指定输入文件 (single 模式)")
    parser.add_argument("--top-n", type=int, default=1000, help="标记前 N 颗最亮星 (默认 1000)")
    parser.add_argument("--output-dir", type=str, default=None,
                        help="输出目录 (默认 lib/plate_solve/logs/visualize_reproject)")
    parser.add_argument("--save-solved-fits", action="store_true",
                        help="同时保存带 WCS 的 FITS 副本 ({label}_solved.fits)")
    args = parser.parse_args()

    if not args.single and not args.batch:
        args.single = True

    output_dir = args.output_dir if args.output_dir else OUTPUT_DIR
    os.makedirs(output_dir, exist_ok=True)
    env = init_environment()

    if args.single:
        if args.input:
            fits_path = args.input
            label = os.path.splitext(os.path.basename(fits_path))[0]
        else:
            all_frames = scan_all_fits()
            if not all_frames:
                print("未找到 testdata 中的 .fts 文件")
                return
            label, fits_path = all_frames[0]
            print(f"\n单张测试: {label}")
            print(f"文件: {fits_path}")

        out_path = os.path.join(output_dir, f"{label}_reproject.png")
        info = visualize_frame(env, fits_path, out_path, top_n=args.top_n,
                                save_solved_fits=args.save_solved_fits)
        print(f"\n结果: success={info['success']}, RMS={info['rms_arcsec']:.3f}\", "
              f"n_marked={info['n_marked']}, elapsed={info['elapsed']:.1f}s")

    elif args.batch:
        all_frames = scan_all_fits()
        print(f"\n批量模式: 共 {len(all_frames)} 帧")
        results = []
        for i, (label, fits_path) in enumerate(all_frames):
            print(f"\n[{i+1}/{len(all_frames)}] {label}")
            out_path = os.path.join(output_dir, f"{label}_reproject.png")
            try:
                info = visualize_frame(env, fits_path, out_path, top_n=args.top_n,
                                        save_solved_fits=args.save_solved_fits)
                results.append({"label": label, **info})
            except Exception as e:
                print(f"  !!! ERROR: {e}")
                results.append({"label": label, "success": False, "error": str(e)})

        n_ok = sum(1 for r in results if r.get("success"))
        print(f"\n{'='*70}")
        print(f"批量完成: {n_ok}/{len(results)} 成功")
        print(f"输出目录: {output_dir}")

    env["solver"].close()


if __name__ == "__main__":
    main()
