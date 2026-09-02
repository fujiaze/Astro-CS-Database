# -*- coding: utf-8 -*-
"""
P11-004 WCS 构建方式等价性验证脚本
==================================
功能: 对 P11-004 已生成的 8 个 solved.fits, 分别用两种方式构建 astropy WCS,
      对同一组 Gaia 亮星做 world_to_pixel 投影, 比较输出差异。

方式A (导出脚本用的方式):
    重新运行 PlateSolve 拿 IpvWcsResult, 然后 to_astropy_wcs(result)

方式B (诊断工具用的方式):
    直接从 solved.fits 的 header 读 WCS(header)

验证内容:
    1. crpix_a vs crpix_b (1-based FITS)
    2. cd_a vs cd_b
    3. sip_a vs sip_b (A/B/AP/BP 矩阵和 crpix)
    4. world_to_pixel 差异 (max/mean/p99)
    5. 输出 JSON 报告

依赖: ipv_solver, vector_match_v2 (GaiaClientPy), star_detector, astro_image_io
运行: python compare_wcs_construction.py
"""

import os
import sys
import json
import time
import ctypes
import logging
import functools
import traceback
from datetime import datetime

# ============================================================================
# Windows 控制台 UTF-8
# ============================================================================
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

print = functools.partial(print, flush=True)

# ============================================================================
# 路径常量 (照搬 visualize_reproject.py)
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

# astro_image_io.dll 目录 (C++ plate solver 运行时依赖)
_ASTRO_IO_DIR = os.path.join(PROJECT_ROOT, "lib", "astro_image_io")
if _ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python"))
sys.path.insert(
    0,
    os.path.join(
        PROJECT_ROOT,
        "lib", "plate_solve", "archive", "vector_method", "python", "python",
    ),
)
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "star_detector", "python"))

# ============================================================================
# 输出路径
# ============================================================================
_EVIDENCE_DIR = os.path.join(
    PROJECT_ROOT, "engineering_v1.2", "evidence", "P11-004"
)
_LOG_DIR = os.path.join(_EVIDENCE_DIR, "raw_logs")
_REPORT_DIR = os.path.join(_EVIDENCE_DIR, "reports")
os.makedirs(_LOG_DIR, exist_ok=True)
os.makedirs(_REPORT_DIR, exist_ok=True)

_LOG_FILE = os.path.join(
    _LOG_DIR,
    "compare_wcs_construction_"
    + datetime.now().strftime("%Y%m%d_%H%M%S")
    + ".log",
)
REPORT_PATH = os.path.join(_REPORT_DIR, "wcs_construction_comparison.json")

# ============================================================================
# 日志配置
# ============================================================================
logger = logging.getLogger("compare_wcs")
logger.setLevel(logging.DEBUG)
logger.propagate = False

_fh = logging.FileHandler(_LOG_FILE, encoding="utf-8")
_fh.setLevel(logging.DEBUG)
_fh.setFormatter(
    logging.Formatter("[%(asctime)s] [%(levelname)s] %(message)s",
                      datefmt="%Y-%m-%d %H:%M:%S")
)
logger.addHandler(_fh)

_ch = logging.StreamHandler(sys.stdout)
_ch.setLevel(logging.INFO)
_ch.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
logger.addHandler(_ch)

# ============================================================================
# 8 帧映射 (frame_id -> 源 .fts 相对路径)
# 与 run_diagnosis.ps1 一致
# ============================================================================
FRAMES = [
    {
        "id": "T2_RED_LDN43",
        "src": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts",
    },
    {
        "id": "T2_GREEN_LDN43",
        "src": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts",
    },
    {
        "id": "T2_BLUE_LDN43",
        "src": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts",
    },
    {
        "id": "T2_HA_LDN43",
        "src": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts",
    },
    {
        "id": "T3_RED_NGC55",
        "src": r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts",
    },
    {
        "id": "T3_GREEN_NGC55",
        "src": r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts",
    },
    {
        "id": "T3_BLUE_NGC55",
        "src": r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts",
    },
    {
        "id": "T3_LUM_NGC55",
        "src": r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts",
    },
]

# ============================================================================
# 判定阈值
# ============================================================================
# world_to_pixel 差异阈值 (像素). 1e-3 px = 1 毫像素, 远低于任何实际精度需求。
# 方式A 与方式B 理论上应数值等价, 差异应接近浮点精度 (~1e-12)。
VERDICT_THRESHOLD_PX = 1e-3

# ============================================================================
# FITS 指向解析 (照搬 visualize_reproject.py)
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
    """从 FITS 头读 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ, 计算 FOV (度)"""
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
# 环境初始化 (照搬 visualize_reproject.py)
# ============================================================================

def init_environment():
    logger.info("=" * 70)
    logger.info("初始化环境")
    logger.info("=" * 70)

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
    logger.info("[1/4] GaiaClient: %s (db_type=%d)", gaia_dir, db_type)
    gaia_client = GaiaClientPy(gaia_dir, db_type=db_type)
    gaia_handle = gaia_client._handle
    if isinstance(gaia_handle, ctypes.c_void_p):
        gaia_handle = gaia_handle.value

    from star_detector import StarDetector, SDetParamsPy
    logger.info("[2/4] StarDetector")
    sdet = StarDetector(params=SDetParamsPy(fitRadius=0))
    sdet_handle = sdet._handle
    if isinstance(sdet_handle, ctypes.c_void_p):
        sdet_handle = sdet_handle.value

    from ipv_solver import IPVSolver
    logger.info("[3/4] IPVSolver")
    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)

    from astro_image_io import ImageReader
    logger.info("[4/4] ImageReader")
    reader = ImageReader()

    return {
        "solver": solver, "gaia_client": gaia_client, "sdet": sdet,
        "reader": reader, "gaia_dir": gaia_dir, "db_type": db_type,
    }


# ============================================================================
# WCS 比较核心
# ============================================================================

import numpy as np


def _safe_list(arr):
    """把 numpy 数组或 None 转为 JSON 可序列化的 list"""
    if arr is None:
        return None
    return np.asarray(arr, dtype=float).tolist()


def _sip_to_dict(sip):
    """把 astropy.wcs.Sip 转为可比较的 dict"""
    if sip is None:
        return None
    return {
        "a": np.asarray(sip.a, dtype=float),
        "b": np.asarray(sip.b, dtype=float),
        "ap": (np.asarray(sip.ap, dtype=float)
               if sip.ap is not None else None),
        "bp": (np.asarray(sip.bp, dtype=float)
               if sip.bp is not None else None),
        "crpix": np.asarray(sip.crpix, dtype=float),
    }


def _sip_equal(sip_a, sip_b, atol=1e-12):
    """比较两个 Sip 是否等价 (A/B/AP/BP 矩阵和 crpix)"""
    da = _sip_to_dict(sip_a)
    db = _sip_to_dict(sip_b)
    if da is None and db is None:
        return True
    if da is None or db is None:
        return False
    # A/B 矩阵形状必须一致
    if da["a"].shape != db["a"].shape:
        return False
    if not np.allclose(da["a"], db["a"], atol=atol, rtol=0):
        return False
    if not np.allclose(da["b"], db["b"], atol=atol, rtol=0):
        return False
    # AP/BP (可能为 None)
    if (da["ap"] is None) != (db["ap"] is None):
        return False
    if da["ap"] is not None:
        if da["ap"].shape != db["ap"].shape:
            return False
        if not np.allclose(da["ap"], db["ap"], atol=atol, rtol=0):
            return False
        if not np.allclose(da["bp"], db["bp"], atol=atol, rtol=0):
            return False
    # crpix
    if not np.allclose(da["crpix"], db["crpix"], atol=atol, rtol=0):
        return False
    return True


def compare_wcs_for_frame(env, frame):
    """对单帧执行方式A vs 方式B WCS 构建比较

    参数:
        env: init_environment() 返回的 dict
        frame: {"id": ..., "src": 相对路径}

    返回:
        dict: 报告字段 (符合任务要求的 JSON schema)
    """
    frame_id = frame["id"]
    src_fits = os.path.join(PROJECT_ROOT, frame["src"])
    solved_fits = os.path.join(PROJECT_ROOT, frame_id + "_solved.fits")

    logger.info("-" * 70)
    logger.info("帧: %s", frame_id)
    logger.info("  源 FITS: %s", src_fits)
    logger.info("  solved FITS: %s", solved_fits)

    if not os.path.isfile(src_fits):
        raise FileNotFoundError(f"源 FITS 不存在: {src_fits}")
    if not os.path.isfile(solved_fits):
        raise FileNotFoundError(f"solved FITS 不存在: {solved_fits}")

    solver = env["solver"]
    reader = env["reader"]
    gaia_client = env["gaia_client"]

    # ----------------------------------------------------------------------
    # 1. 读源 FITS 指向
    # ----------------------------------------------------------------------
    pointing = get_fits_pointing(reader, src_fits)
    w_px = pointing["width"]
    h_px = pointing["height"]
    ra0 = pointing["ra"]
    dec0 = pointing["dec"]
    fl = pointing["focallen"]
    ps = pointing["xpixsz"]
    fov_deg = pointing["fov_deg"]
    logger.info("  图像: %dx%d, FOV=%.3f deg, %s",
                w_px, h_px, fov_deg, pointing["object"])
    logger.info("  指向: RA=%.5f deg, Dec=%.5f deg, fl=%s mm, ps=%s um",
                ra0, dec0, fl, ps)

    # ----------------------------------------------------------------------
    # 2. 方式A: 重新运行 PlateSolve -> IpvWcsResult -> to_astropy_wcs
    # ----------------------------------------------------------------------
    t_a = time.time()
    logger.info("  [方式A] 重新运行 PlateSolve ...")
    params = solver.get_default_params()
    result = solver.solve(
        image_path=src_fits,
        ra0=ra0, dec0=dec0,
        focal_length_mm=fl, pixel_size_um=ps,
        params=params,
    )
    if not bool(result.success):
        err = result.error_msg.decode("utf-8", errors="ignore").strip()
        raise RuntimeError(f"plate solve 失败: {err or '未收敛'}")
    logger.info("  [方式A] solve 成功: rms=%.4f arcsec, n_pairs=%d, "
                "sip_order=%d, sip_ap_order=%d, trans_order=%d",
                float(result.rms_arcsec), int(result.n_pairs),
                int(result.sip_order), int(result.sip_ap_order),
                int(result.trans_order))

    from ipv_solver import to_astropy_wcs
    wcs_a = to_astropy_wcs(result)
    logger.info("  [方式A] to_astropy_wcs 完成 (%.2fs)", time.time() - t_a)

    # ----------------------------------------------------------------------
    # 3. 方式B: 从 solved.fits header 读 WCS
    # ----------------------------------------------------------------------
    t_b = time.time()
    logger.info("  [方式B] 从 solved.fits header 读 WCS ...")
    from astropy.io import fits
    from astropy.wcs import WCS

    with fits.open(solved_fits, mode="readonly") as hdul:
        header = hdul[0].header.copy()
    wcs_b = WCS(header)
    logger.info("  [方式B] WCS(header) 完成 (%.2fs)", time.time() - t_b)

    # ----------------------------------------------------------------------
    # 4. 比较 crpix / cd / sip
    # ----------------------------------------------------------------------
    crpix_a = np.asarray(wcs_a.wcs.crpix, dtype=float)
    crpix_b = np.asarray(wcs_b.wcs.crpix, dtype=float)
    crpix_diff = (crpix_a - crpix_b).tolist()
    logger.info("  crpix_a = %s", crpix_a.tolist())
    logger.info("  crpix_b = %s", crpix_b.tolist())
    logger.info("  crpix_diff = %s", crpix_diff)

    cd_a = np.asarray(wcs_a.wcs.cd, dtype=float)
    cd_b = np.asarray(wcs_b.wcs.cd, dtype=float)
    cd_diff_max = float(np.max(np.abs(cd_a - cd_b))) if cd_a.size > 0 else 0.0
    logger.info("  cd_a = %s", cd_a.tolist())
    logger.info("  cd_b = %s", cd_b.tolist())
    logger.info("  cd_diff_max = %.3e", cd_diff_max)

    sip_a = wcs_a.sip
    sip_b = wcs_b.sip
    sip_a_eq_b = _sip_equal(sip_a, sip_b)
    sip_a_crpix = (np.asarray(sip_a.crpix, dtype=float).tolist()
                   if sip_a is not None else None)
    sip_b_crpix = (np.asarray(sip_b.crpix, dtype=float).tolist()
                   if sip_b is not None else None)
    logger.info("  sip_a_eq_b = %s", sip_a_eq_b)
    logger.info("  sip_a_crpix = %s", sip_a_crpix)
    logger.info("  sip_b_crpix = %s", sip_b_crpix)
    if sip_a is not None and sip_b is not None:
        da = _sip_to_dict(sip_a)
        db = _sip_to_dict(sip_b)
        a_diff = float(np.max(np.abs(da["a"] - db["a"]))) if da["a"].size else 0.0
        b_diff = float(np.max(np.abs(da["b"] - db["b"]))) if da["b"].size else 0.0
        logger.info("  sip A_diff_max=%.3e, B_diff_max=%.3e", a_diff, b_diff)
        if da["ap"] is not None and db["ap"] is not None:
            ap_diff = float(np.max(np.abs(da["ap"] - db["ap"]))) if da["ap"].size else 0.0
            bp_diff = float(np.max(np.abs(da["bp"] - db["bp"]))) if da["bp"].size else 0.0
            logger.info("  sip AP_diff_max=%.3e, BP_diff_max=%.3e",
                        ap_diff, bp_diff)

    # ----------------------------------------------------------------------
    # 5. 查询 Gaia 星, 取前 1000 亮星, world_to_pixel 比较
    # ----------------------------------------------------------------------
    query_radius = fov_deg * 0.75
    mag_limit = 18.0
    logger.info("  Gaia cone_search: radius=%.3f deg, mag<=%.1f",
                query_radius, mag_limit)
    ra_arr, dec_arr, mag_arr = gaia_client.cone_search(
        ra0, dec0, query_radius, mag_limit
    )
    n_total = int(len(ra_arr))
    logger.info("  Gaia 返回: %d 颗星", n_total)

    if n_total == 0:
        raise RuntimeError("Gaia 查询返回 0 颗星, 无法比较 world_to_pixel")

    # 按星等升序 (亮星优先), 取前 1000
    sort_idx = np.argsort(mag_arr)
    top_n = min(1000, n_total)
    sel = sort_idx[:top_n]
    ra_sel = ra_arr[sel]
    dec_sel = dec_arr[sel]
    mag_sel = mag_arr[sel]
    logger.info("  取前 %d 颗亮星 (mag %.2f~%.2f) 做 world_to_pixel 比较",
                top_n, float(mag_sel[0]), float(mag_sel[-1]))

    from astropy.coordinates import SkyCoord
    import astropy.units as u

    sky = SkyCoord(ra=ra_sel * u.deg, dec=dec_sel * u.deg)
    x_a, y_a = wcs_a.world_to_pixel(sky)
    x_b, y_b = wcs_b.world_to_pixel(sky)
    x_a = np.asarray(x_a, dtype=np.float64)
    y_a = np.asarray(y_a, dtype=np.float64)
    x_b = np.asarray(x_b, dtype=np.float64)
    y_b = np.asarray(y_b, dtype=np.float64)

    dist = np.sqrt((x_a - x_b) ** 2 + (y_a - y_b) ** 2)
    diff_max = float(np.max(dist))
    diff_mean = float(np.mean(dist))
    diff_p99 = float(np.percentile(dist, 99))
    logger.info("  world_to_pixel 差异 (px): max=%.6e, mean=%.6e, p99=%.6e",
                diff_max, diff_mean, diff_p99)

    # ----------------------------------------------------------------------
    # 6. 判定
    # ----------------------------------------------------------------------
    verdict = "EQUIVALENT" if diff_max < VERDICT_THRESHOLD_PX else "DIVERGENT"
    logger.info("  verdict = %s (阈值=%.3e px, max=%.3e px)",
                verdict, VERDICT_THRESHOLD_PX, diff_max)

    return {
        "frame_id": frame_id,
        "solved_fits": solved_fits,
        "source_fits": src_fits,
        "crpix_a": _safe_list(crpix_a),
        "crpix_b": _safe_list(crpix_b),
        "crpix_diff": _safe_list(crpix_diff),
        "cd_a": _safe_list(cd_a),
        "cd_b": _safe_list(cd_b),
        "cd_diff_max": cd_diff_max,
        "sip_a_eq_b": bool(sip_a_eq_b),
        "sip_a_crpix": sip_a_crpix,
        "sip_b_crpix": sip_b_crpix,
        "sip_order_a": int(result.sip_order),
        "sip_ap_order_a": int(result.sip_ap_order),
        "n_gaia_stars": int(top_n),
        "n_gaia_total": int(n_total),
        "world_to_pixel_diff_max_px": diff_max,
        "world_to_pixel_diff_mean_px": diff_mean,
        "world_to_pixel_diff_p99_px": diff_p99,
        "rms_arcsec": float(result.rms_arcsec),
        "n_pairs": int(result.n_pairs),
        "verdict": verdict,
    }


# ============================================================================
# 主入口
# ============================================================================

def main():
    logger.info("=" * 70)
    logger.info("P11-004 WCS 构建方式等价性验证")
    logger.info("报告输出: %s", REPORT_PATH)
    logger.info("日志文件: %s", _LOG_FILE)
    logger.info("判定阈值: %.3e px", VERDICT_THRESHOLD_PX)
    logger.info("=" * 70)

    env = init_environment()
    results = []
    n_total = len(FRAMES)

    for i, frame in enumerate(FRAMES, 1):
        logger.info("")
        logger.info("########## [%d/%d] %s ##########",
                    i, n_total, frame["id"])
        t0 = time.time()
        try:
            rep = compare_wcs_for_frame(env, frame)
            rep["elapsed_sec"] = round(time.time() - t0, 2)
            results.append(rep)
        except Exception as e:
            logger.error("帧 %s 处理失败: %s", frame["id"], e, exc_info=True)
            results.append({
                "frame_id": frame["id"],
                "solved_fits": os.path.join(
                    PROJECT_ROOT, frame["id"] + "_solved.fits"),
                "source_fits": os.path.join(PROJECT_ROOT, frame["src"]),
                "verdict": "ERROR",
                "error": str(e),
                "traceback": traceback.format_exc(),
                "elapsed_sec": round(time.time() - t0, 2),
            })

    # 输出 JSON 报告
    n_eq = sum(1 for r in results if r.get("verdict") == "EQUIVALENT")
    n_div = sum(1 for r in results if r.get("verdict") == "DIVERGENT")
    n_err = sum(1 for r in results if r.get("verdict") == "ERROR")

    report = {
        "task": "P11-004 WCS construction comparison",
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "method_a": "to_astropy_wcs(IpvWcsResult) from re-run plate solve",
        "method_b": "WCS(solved.fits header)",
        "verdict_threshold_px": VERDICT_THRESHOLD_PX,
        "total_frames": n_total,
        "n_equivalent": n_eq,
        "n_divergent": n_div,
        "n_error": n_err,
        "frames": results,
    }

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    logger.info("")
    logger.info("=" * 70)
    logger.info("报告已写入: %s", REPORT_PATH)
    logger.info("汇总: EQUIVALENT=%d, DIVERGENT=%d, ERROR=%d (共 %d)",
                n_eq, n_div, n_err, n_total)
    logger.info("=" * 70)
    logger.info("每帧 verdict 与最大差异:")
    for r in results:
        v = r.get("verdict")
        if v == "ERROR":
            logger.info("  %-18s ERROR  err=%s", r["frame_id"],
                        r.get("error", ""))
        else:
            logger.info("  %-18s %-10s max_diff=%.6e px  p99=%.6e px  "
                        "crpix_diff=%s  cd_diff=%.3e  sip_eq=%s",
                        r["frame_id"], v,
                        r["world_to_pixel_diff_max_px"],
                        r["world_to_pixel_diff_p99_px"],
                        r["crpix_diff"],
                        r["cd_diff_max"],
                        r["sip_a_eq_b"])

    # 释放资源
    try:
        env["solver"].close()
    except Exception:
        pass

    return 0 if (n_err == 0 and n_div == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
