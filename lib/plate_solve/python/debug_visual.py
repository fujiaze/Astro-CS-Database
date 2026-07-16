# -*- coding: utf-8 -*-
"""
WCS 对齐可视化调试: 在图像中心区域同时画 WCS 投影星(红)和检测星(绿)
验证坐标系方向是否一致
"""

import os
import sys
import math
import ctypes
import functools

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

print = functools.partial(print, flush=True)

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
from PIL import Image as PILImage, ImageDraw, ImageFont


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
    fov_deg = max(w, h) * s0 / 3600.0 if s0 > 0 else 0.0
    return {"ra": ra0, "dec": dec0, "focallen": fl, "xpixsz": ps,
            "width": w, "height": h, "s0_arcsec_per_px": s0, "fov_deg": fov_deg,
            "object": kw_dict.get("OBJECT", "")}


def siril_autostretch(data_uint16):
    img_f = data_uint16.astype(np.float64) / 65535.0
    median = float(np.median(img_f))
    mad = float(np.median(np.abs(img_f - median)))
    mad_sigma = mad * 1.4826
    c0 = max(median + (-2.8) * mad_sigma, 0.0)
    m2 = median - c0
    if m2 <= 0.0:
        midtones = 0.0
    else:
        xp = m2
        midtones = ((0.25 - 1.0) * xp) / (((2.0 * 0.25 - 1.0) * xp) - 0.25)
    if midtones <= 0.0 or midtones >= 1.0:
        lo_p, hi_p = np.percentile(img_f, [0.5, 99.5])
        stretched = np.clip((img_f - lo_p) / (hi_p - lo_p + 1e-9), 0.0, 1.0)
    else:
        vec = np.vectorize(lambda x: 0.0 if x <= c0 else (1.0 if x >= 1.0 else
            ((midtones - 1.0) * ((x - c0) / (1.0 - c0))) /
            (((2.0 * midtones - 1.0) * ((x - c0) / (1.0 - c0))) - midtones)))
        stretched = vec(img_f)
    return (stretched * 255.0).clip(0, 255).astype(np.uint8)


def main():
    from vector_match_v2 import GaiaClientPy
    from star_detector import StarDetector, SDetParamsPy
    from ipv_solver import IPVSolver, to_astropy_wcs
    from astro_image_io import ImageReader
    from astropy.coordinates import SkyCoord
    import astropy.units as u

    dr3sp = os.path.join(PROJECT_ROOT, "GaiaDR3SP")
    db_type = 2
    gaia_client = GaiaClientPy(dr3sp, db_type=db_type)
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

    fits_path = os.path.join(PROJECT_ROOT, "testdata", "lights",
                             "LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts")
    pointing = get_fits_pointing(reader, fits_path)
    w, h = pointing["width"], pointing["height"]
    ra0, dec0 = pointing["ra"], pointing["dec"]
    fl, ps = pointing["focallen"], pointing["xpixsz"]
    fov_deg = pointing["fov_deg"]

    img = reader.read(fits_path)
    img_data = np.array(img.data, dtype=np.uint16)
    img.close()

    # Plate solve
    print("Plate solving...")
    params = solver.get_default_params()
    result = solver.solve(image_path=fits_path, ra0=ra0, dec0=dec0,
                          focal_length_mm=fl, pixel_size_um=ps, params=params)
    print(f"RMS={result.rms_arcsec:.3f}\", n_pairs={result.n_pairs}")

    # WCS 投影 Gaia 星
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(ra0, dec0, fov_deg * 0.75, 18.0)
    wcs = to_astropy_wcs(result)
    sky_coords = SkyCoord(ra=ra_arr * u.deg, dec=dec_arr * u.deg)
    x_wcs, y_wcs = wcs.world_to_pixel(sky_coords)
    x_wcs = np.asarray(x_wcs, dtype=np.float64)
    y_wcs = np.asarray(y_wcs, dtype=np.float64)

    # 检测星
    print("Detecting stars...")
    det_coords = sdet.detect(img_data)
    x_det = np.array([c[0] for c in det_coords], dtype=np.float64)
    y_det = np.array([c[1] for c in det_coords], dtype=np.float64)
    print(f"Detected {len(x_det)} stars, WCS projected {len(x_wcs)} stars")

    # 取中心区域 (1500-2600, 1500-2600), 大小 1100x1100
    cx_lo, cx_hi = 1500, 2600
    cy_lo, cy_hi = 1500, 2600

    # WCS 星 (红色) - 取该区域内的前 50 颗亮星
    in_wcs = (x_wcs >= cx_lo) & (x_wcs < cx_hi) & (y_wcs >= cy_lo) & (y_wcs < cy_hi)
    x_wcs_reg = x_wcs[in_wcs]
    y_wcs_reg = y_wcs[in_wcs]
    mag_reg = mag_arr[in_wcs]
    sort_idx = np.argsort(mag_reg)
    n_wcs_show = min(50, len(sort_idx))
    x_wcs_show = x_wcs_reg[sort_idx[:n_wcs_show]]
    y_wcs_show = y_wcs_reg[sort_idx[:n_wcs_show]]

    # 检测星 (绿色) - 取该区域内所有星
    in_det = (x_det >= cx_lo) & (x_det < cx_hi) & (y_det >= cy_lo) & (y_det < cy_hi)
    x_det_show = x_det[in_det]
    y_det_show = y_det[in_det]

    print(f"区域 [{cx_lo}:{cx_hi}, {cy_lo}:{cy_hi}]:")
    print(f"  WCS 星: {n_wcs_show} 颗 (红)")
    print(f"  检测星: {len(x_det_show)} 颗 (绿)")

    # 拉伸 + 裁剪
    stretched = siril_autostretch(img_data)
    region = stretched[cy_lo:cy_hi, cx_lo:cx_hi]
    img_rgb = PILImage.fromarray(region, mode="L").convert("RGB")
    draw = ImageDraw.Draw(img_rgb)

    # 画 WCS 星 (红色十字)
    cross_size = 8
    cross_w = 2
    for i in range(n_wcs_show):
        # 坐标转换到裁剪区域
        cx = int(round(x_wcs_show[i] - cx_lo))
        cy = int(round(y_wcs_show[i] - cy_lo))
        if 0 <= cx < (cx_hi - cx_lo) and 0 <= cy < (cy_hi - cy_lo):
            draw.line([(cx - cross_size, cy), (cx + cross_size, cy)], fill=(255, 0, 0), width=cross_w)
            draw.line([(cx, cy - cross_size), (cx, cy + cross_size)], fill=(255, 0, 0), width=cross_w)

    # 画检测星 (绿色圆圈)
    r = 6
    for i in range(len(x_det_show)):
        cx = int(round(x_det_show[i] - cx_lo))
        cy = int(round(y_det_show[i] - cy_lo))
        if 0 <= cx < (cx_hi - cx_lo) and 0 <= cy < (cy_hi - cy_lo):
            draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(0, 255, 0), width=1)

    # 标注
    try:
        font = ImageFont.truetype("arial.ttf", 24)
    except (OSError, IOError):
        font = ImageFont.load_default()
    draw.text((10, 10), "RED=WCS projected Gaia  GREEN=detected stars", fill=(255, 255, 0), font=font)

    out_path = os.path.join(PROJECT_ROOT, "lib", "plate_solve", "logs",
                            "visualize_reproject", "debug_center_region.png")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    img_rgb.save(out_path, "PNG")
    print(f"\n调试图: {out_path}")

    # 同时画一个 Y 翻转的版本 (检测星 y -> h - y)
    img_rgb2 = PILImage.fromarray(region, mode="L").convert("RGB")
    draw2 = ImageDraw.Draw(img_rgb2)
    for i in range(n_wcs_show):
        cx = int(round(x_wcs_show[i] - cx_lo))
        cy = int(round(y_wcs_show[i] - cy_lo))
        if 0 <= cx < (cx_hi - cx_lo) and 0 <= cy < (cy_hi - cy_lo):
            draw2.line([(cx - cross_size, cy), (cx + cross_size, cy)], fill=(255, 0, 0), width=cross_w)
            draw2.line([(cx, cy - cross_size), (cx, cy + cross_size)], fill=(255, 0, 0), width=cross_w)
    # 检测星 Y 翻转: y -> h - 1 - y
    for i in range(len(x_det_show)):
        cx = int(round(x_det_show[i] - cx_lo))
        cy_orig = y_det_show[i]
        cy = int(round((h - 1 - cy_orig) - cy_lo))
        if 0 <= cx < (cx_hi - cx_lo) and 0 <= cy < (cy_hi - cy_lo):
            draw2.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(0, 255, 0), width=1)
    draw2.text((10, 10), "RED=WCS  GREEN=detected(Y-flipped: h-1-y)", fill=(255, 255, 0), font=font)
    out_path2 = out_path.replace("debug_center", "debug_center_yflip")
    img_rgb2.save(out_path2, "PNG")
    print(f"Y翻转版: {out_path2}")


if __name__ == "__main__":
    main()
