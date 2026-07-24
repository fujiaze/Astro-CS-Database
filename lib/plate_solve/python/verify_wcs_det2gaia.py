# -*- coding: utf-8 -*-
"""
WCS 反向验证: 检测星 -> (RA,Dec) -> Gaia 最近星
验证 WCS 是否正确 (RMS 应 < 1")
"""

import os, sys, ctypes, functools
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
        sign = -1.0; s = s[1:]
    elif s.startswith("+"):
        s = s[1:]
    parts = s.replace(":", " ").split()
    if len(parts) == 3:
        d, m, sec = parts
        return sign * (int(d) + int(m) / 60.0 + float(sec) / 3600.0)
    return sign * float(s)

def get_fits_pointing(reader, fits_path):
    meta = reader.read_metadata(fits_path)
    fl, ps = meta.observation.focallen, meta.observation.xpixsz
    w, h = meta.geometry.width, meta.geometry.height
    img = reader.read_header_only(fits_path)
    kw_dict = {kw.name.upper(): kw.value for kw in img.keywords}
    img.close()
    ra0 = parse_ra_hms(kw_dict.get("OBJCTRA", "0"))
    dec0 = parse_dec_dms(kw_dict.get("OBJCTDEC", "0"))
    s0 = 206.265 * ps / fl if (fl and ps and fl > 0) else 0.0
    fov_deg = max(w, h) * s0 / 3600.0 if s0 > 0 else 0.0
    return {"ra": ra0, "dec": dec0, "focallen": fl, "xpixsz": ps,
            "width": w, "height": h, "s0_arcsec_per_px": s0, "fov_deg": fov_deg}


def main():
    from vector_match_v2 import GaiaClientPy
    from star_detector import StarDetector, SDetParamsPy
    from ipv_solver import IPVSolver, to_astropy_wcs
    from astro_image_io import ImageReader
    from astropy.coordinates import SkyCoord
    import astropy.units as u

    dr3sp = os.path.join(PROJECT_ROOT, "GaiaDR3SP")
    gaia_client = GaiaClientPy(dr3sp, db_type=2)
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
    print(f"RMS={result.rms_arcsec:.3f}\", n_pairs={result.n_pairs}, sip_order={result.sip_order}")

    # WCS
    wcs = to_astropy_wcs(result)

    # 查询 Gaia (用 solver 类似的 mag_limit)
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(ra0, dec0, fov_deg * 0.75, 18.0)
    print(f"Gaia: {len(ra_arr)} stars")

    # 检测星
    print("Detecting stars...")
    det_coords = sdet.detect(img_data)
    x_det = np.array([c[0] for c in det_coords], dtype=np.float64)
    y_det = np.array([c[1] for c in det_coords], dtype=np.float64)
    print(f"Detected: {len(x_det)} stars")

    # === 验证 1: WCS pixel_to_world 互逆性 ===
    print("\n=== 验证 1: WCS pixel_to_world / world_to_pixel 互逆性 ===")
    test_pixels = [(100, 200), (1000, 1500), (2047.5, 2047.5), (3000, 3500), (3900, 200)]
    for px, py in test_pixels:
        sky = wcs.pixel_to_world(px, py)
        x_back, y_back = wcs.world_to_pixel(sky)
        print(f"  ({px:.1f},{py:.1f}) -> RA={sky.ra.deg:.6f}, Dec={sky.dec.deg:.6f} -> ({x_back:.3f},{y_back:.3f})  "
              f"dx={x_back-px:+.4f}, dy={y_back-py:+.4f}")

    # === 验证 2: 检测星 -> (RA,Dec) -> Gaia 最近星距离 ===
    print("\n=== 验证 2: 检测星 -> (RA,Dec) -> Gaia 最近星 ===")
    # 取前 50 颗亮检测星 (按检测亮度排序, 这里简单取前 50)
    # 检测星没有 mag, 用峰值亮度近似
    # 直接取中心区域前 50 颗检测星
    center_mask = (x_det >= 500) & (x_det < 3500) & (y_det >= 500) & (y_det < 3500)
    x_det_c = x_det[center_mask]
    y_det_c = y_det[center_mask]
    n_test = min(50, len(x_det_c))
    print(f"取中心区域 {n_test} 颗检测星验证")

    # Gaia 星的 SkyCoord
    gaia_sky = SkyCoord(ra=ra_arr * u.deg, dec=dec_arr * u.deg)

    match_distances_arcsec = []
    for i in range(n_test):
        # 检测星 -> (RA, Dec)
        sky = wcs.pixel_to_world(x_det_c[i], y_det_c[i])
        # 找最近的 Gaia 星 (计算到所有 Gaia 星的角距)
        seps = gaia_sky.separation(sky)
        dist_arcsec = float(np.min(seps.arcsec))
        match_distances_arcsec.append(dist_arcsec)

    dist_arr = np.array(match_distances_arcsec)
    print(f"  匹配距离 (角秒):")
    print(f"    median={np.median(dist_arr):.3f}\"  mean={np.mean(dist_arr):.3f}\"  "
          f"[min={np.min(dist_arr):.3f}\", max={np.max(dist_arr):.3f}\"]")
    print(f"    <1\": {np.sum(dist_arr < 1.0)}/{n_test}  <3\": {np.sum(dist_arr < 3.0)}/{n_test}  <10\": {np.sum(dist_arr < 10.0)}/{n_test}")

    if np.median(dist_arr) < 1.0:
        print(f"  => WCS 正确! 检测星投影到天球后与 Gaia 星匹配良好 (中位 < 1\")")
        print(f"     问题在正向投影 (Gaia->像素) 的坐标解释上")
    else:
        print(f"  => WCS 有问题! 检测星投影到天球后与 Gaia 星不匹配")

    # === 验证 3: Gaia 星 -> 像素 -> 检测星 ===
    print("\n=== 验证 3: Gaia 星 -> 像素 -> 最近检测星 ===")
    # 取前 50 颗亮 Gaia 星
    sort_idx = np.argsort(mag_arr)
    n_gaia_test = min(50, len(sort_idx))
    gaia_match_dist_px = []

    for i in range(n_gaia_test):
        gi = sort_idx[i]
        # Gaia 星 -> 像素
        sky = SkyCoord(ra=ra_arr[gi] * u.deg, dec=dec_arr[gi] * u.deg)
        x_pix, y_pix = wcs.world_to_pixel(sky)
        # 找最近的检测星
        if len(x_det) == 0:
            break
        dx_all = x_det - float(x_pix)
        dy_all = y_det - float(y_pix)
        dist_all = np.sqrt(dx_all**2 + dy_all**2)
        d_min = float(np.min(dist_all))
        gaia_match_dist_px.append(d_min)

    gaia_dist_arr = np.array(gaia_match_dist_px)
    print(f"  Gaia 星 -> 像素 -> 最近检测星距离 (像素):")
    print(f"    median={np.median(gaia_dist_arr):.3f}px  mean={np.mean(gaia_dist_arr):.3f}px  "
          f"[min={np.min(gaia_dist_arr):.3f}px, max={np.max(gaia_dist_arr):.3f}px]")
    print(f"    <1px: {np.sum(gaia_dist_arr < 1.0)}/{n_gaia_test}  <3px: {np.sum(gaia_dist_arr < 3.0)}/{n_gaia_test}  <5px: {np.sum(gaia_dist_arr < 5.0)}/{n_gaia_test}")

    # === 验证 4: Gaia 星 -> 像素 -> 翻转后 -> 检测星 ===
    print("\n=== 验证 4: Gaia 星 -> 像素 -> Y翻转 -> 最近检测星 ===")
    gaia_match_dist_yflip = []

    for i in range(n_gaia_test):
        gi = sort_idx[i]
        sky = SkyCoord(ra=ra_arr[gi] * u.deg, dec=dec_arr[gi] * u.deg)
        x_pix, y_pix = wcs.world_to_pixel(sky)
        # Y 翻转
        y_flip = float(h - 1 - y_pix)
        # 找最近的检测星
        if len(x_det) == 0:
            break
        dx_all = x_det - float(x_pix)
        dy_all = y_det - y_flip
        dist_all = np.sqrt(dx_all**2 + dy_all**2)
        d_min = float(np.min(dist_all))
        gaia_match_dist_yflip.append(d_min)

    yflip_dist_arr = np.array(gaia_match_dist_yflip)
    print(f"  Gaia 星 -> 像素 -> Y翻转 -> 最近检测星距离 (像素):")
    print(f"    median={np.median(yflip_dist_arr):.3f}px  mean={np.mean(yflip_dist_arr):.3f}px  "
          f"[min={np.min(yflip_dist_arr):.3f}px, max={np.max(yflip_dist_arr):.3f}px]")
    print(f"    <1px: {np.sum(yflip_dist_arr < 1.0)}/{n_gaia_test}  <3px: {np.sum(yflip_dist_arr < 3.0)}/{n_gaia_test}  <5px: {np.sum(yflip_dist_arr < 5.0)}/{n_gaia_test}")

    # === 验证 5: 翻转 data 后重新检测, 验证 WCS 匹配 ===
    print("\n=== 验证 5: 翻转 data (data[::-1]) 后重新检测, WCS 直接匹配 ===")
    img_data_flip = img_data[::-1, :].copy()
    det_coords_flip = sdet.detect(img_data_flip)
    x_det_f = np.array([c[0] for c in det_coords_flip], dtype=np.float64)
    y_det_f = np.array([c[1] for c in det_coords_flip], dtype=np.float64)
    print(f"翻转后检测: {len(x_det_f)} stars")

    gaia_match_dist_flip = []
    for i in range(n_gaia_test):
        gi = sort_idx[i]
        sky = SkyCoord(ra=ra_arr[gi] * u.deg, dec=dec_arr[gi] * u.deg)
        x_pix, y_pix = wcs.world_to_pixel(sky)
        if len(x_det_f) == 0:
            break
        dx_all = x_det_f - float(x_pix)
        dy_all = y_det_f - float(y_pix)
        dist_all = np.sqrt(dx_all**2 + dy_all**2)
        d_min = float(np.min(dist_all))
        gaia_match_dist_flip.append(d_min)

    flip_dist_arr = np.array(gaia_match_dist_flip)
    print(f"  翻转data后 Gaia->检测星距离 (像素):")
    print(f"    median={np.median(flip_dist_arr):.3f}px  mean={np.mean(flip_dist_arr):.3f}px  "
          f"[min={np.min(flip_dist_arr):.3f}px, max={np.max(flip_dist_arr):.3f}px]")
    print(f"    <1px: {np.sum(flip_dist_arr < 1.0)}/{n_gaia_test}  <3px: {np.sum(flip_dist_arr < 3.0)}/{n_gaia_test}  <5px: {np.sum(flip_dist_arr < 5.0)}/{n_gaia_test}")

    print("\n=== 总结 ===")
    print(f"  验证 2 (检测星->Gaia, 反向): median={np.median(dist_arr):.3f}\"")
    print(f"  验证 3 (Gaia->检测星, 正向): median={np.median(gaia_dist_arr):.3f}px")
    print(f"  验证 4 (Gaia->Y翻转->检测星): median={np.median(yflip_dist_arr):.3f}px")
    print(f"  验证 5 (翻转data后直接匹配): median={np.median(flip_dist_arr):.3f}px")
    print()
    if np.median(flip_dist_arr) < 1.0:
        print("  => 翻转 data 后 WCS 直接匹配! 解决方案: img_data = img_data[::-1].copy()")
    elif np.median(yflip_dist_arr) < np.median(gaia_dist_arr):
        print("  => Y 翻转有效但 SIP 可能受影响, 翻转 data 是更优方案")


if __name__ == "__main__":
    main()
