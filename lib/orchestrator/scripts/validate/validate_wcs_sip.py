# -*- coding: utf-8 -*-
"""
validate_wcs_sip.py - WCS/SIP 验证脚本
功能: 用 astropy.wcs 验证 plate_solve 后的 WCS/SIP 正确性
用途: 全链路调试阶段验证 WCS 解析结果
调用: python validate_wcs_sip.py <fits_file> [--gaia-cat <gaia_json>] [--star-det <star_json>] [--output <output_json>]

验证项:
    1. WCS 构造: 用 astropy.wcs.WCS 从 header 构造 WCS 对象（不报错）
    2. CTYPE: CTYPE1/CTYPE2 为 RA---TAN-SIP/DEC--TAN-SIP 或 RA---TAN/DEC--TAN
    3. CRVAL 范围: CRVAL1 ∈ [0,360], CRVAL2 ∈ [-90,90]
    4. CD 行列式: det(CD) > 0 (非退化)
    5. 中心投影: WCS 将图像中心像素投影到天球坐标，与 OBJCTRA/OBJCTDEC 比较 (误差 < 1 度)
    6. 星匹配 RMS: 用 WCS 将 gaia_cat ra/dec 投影到像素，与 star_det x/y 比较 (RMS < 1 角秒)

输出: JSON 到 output/pipeline_debug/<frame_name>/validate_wcs.json
作者: Astro CS Normalization Database
日期: 2026-07-13
"""

from __future__ import annotations

import os
import sys
import json
import logging
import argparse
from datetime import datetime

import numpy as np

# ============================ 日志配置 ============================

# 日志目录: orchestrator/scripts/validate/ -> ../../logs = orchestrator/logs
_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "logs"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] [%(threadName)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "validate_wcs_sip_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("validate_wcs_sip")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("validate_wcs_sip 日志初始化完成: %s", log_file)
    return lg


logger = _init_logger()


# ============================ HMS/DMS 解析 ============================

def parse_hms(hms_str):
    """解析 RA HMS 字符串为度数

    支持: "HH:MM:SS.S" / "HH MM SS.S" / 浮点度数字符串

    示例: "17:45:40.0" -> 266.4167
    """
    s = str(hms_str).strip()
    parts = s.replace(":", " ").split()
    if len(parts) >= 3:
        h, m, sec = float(parts[0]), float(parts[1]), float(parts[2])
        return 15.0 * (h + m / 60.0 + sec / 3600.0)
    elif len(parts) == 1:
        return float(parts[0])
    return 0.0


def parse_dms(dms_str):
    """解析 Dec DMS 字符串为度数

    支持: "±DD:MM:SS.S" / "±DD MM SS.S" / 浮点度数字符串

    示例: "-29:00:28" -> -29.0078
    """
    s = str(dms_str).strip()
    sign = 1.0
    if s.startswith("-"):
        sign = -1.0
        s = s[1:]
    elif s.startswith("+"):
        s = s[1:]
    parts = s.replace(":", " ").split()
    if len(parts) >= 3:
        d, m, sec = float(parts[0]), float(parts[1]), float(parts[2])
        return sign * (d + m / 60.0 + sec / 3600.0)
    elif len(parts) == 1:
        return sign * float(parts[0])
    return 0.0


# ============================ JSON 加载 ============================

def _load_json(path):
    """加载 JSON 文件（UTF-8 编码）"""
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _extract_array(obj, keys):
    """从字典中按优先顺序查找键，返回 numpy float64 数组"""
    if not isinstance(obj, dict):
        return None
    for k in keys:
        if k in obj:
            return np.asarray(obj[k], dtype=np.float64)
    return None


def parse_gaia_cat(data):
    """解析 gaia_cat JSON，返回 (ra, dec, mag) numpy 数组

    支持格式:
        1. {"ra": [...], "dec": [...], "mag": [...]}
        2. [[ra, dec, mag], ...]
        3. {"stars": [[ra, dec, mag], ...]} / {"catalog": [...]}
    """
    if data is None:
        return None, None, None

    # 字典格式
    if isinstance(data, dict):
        ra = _extract_array(data, ["ra", "RA", "ra_deg"])
        dec = _extract_array(data, ["dec", "DEC", "dec_deg"])
        mag = _extract_array(data, ["mag", "magnitude", "g"])
        if ra is not None and dec is not None:
            if mag is None:
                mag = np.zeros_like(ra)
            return ra, dec, mag
        # 嵌套数组
        for k in ("stars", "data", "catalog"):
            if k in data:
                return parse_gaia_cat(data[k])

    # 列表格式 [[ra, dec, mag], ...]
    if isinstance(data, list) and len(data) > 0 and isinstance(data[0], (list, tuple)):
        arr = np.asarray(data, dtype=np.float64)
        if arr.ndim == 2 and arr.shape[1] >= 3:
            return arr[:, 0], arr[:, 1], arr[:, 2]

    return None, None, None


def parse_star_det(data):
    """解析 star_det JSON，返回 (x, y, flux, mag) numpy 数组

    支持格式:
        1. {"x": [...], "y": [...], "flux": [...], "mag": [...]}
        2. [[x, y, flux, mag], ...]
        3. {"stars": [[x, y, flux, mag], ...]} / {"detected": [...]}
    """
    if data is None:
        return None, None, None, None

    # 字典格式
    if isinstance(data, dict):
        x = _extract_array(data, ["x", "X", "x_pix"])
        y = _extract_array(data, ["y", "Y", "y_pix"])
        flux = _extract_array(data, ["flux", "FLUX"])
        mag = _extract_array(data, ["mag", "magnitude"])
        if x is not None and y is not None:
            if flux is None:
                flux = np.zeros_like(x)
            if mag is None:
                mag = np.zeros_like(x)
            return x, y, flux, mag
        # 嵌套数组
        for k in ("stars", "data", "detected"):
            if k in data:
                return parse_star_det(data[k])

    # 列表格式 [[x, y, flux, mag], ...]
    if isinstance(data, list) and len(data) > 0 and isinstance(data[0], (list, tuple)):
        arr = np.asarray(data, dtype=np.float64)
        if arr.ndim == 2 and arr.shape[1] >= 2:
            flux = arr[:, 2] if arr.shape[1] >= 3 else np.zeros(arr.shape[0])
            mag = arr[:, 3] if arr.shape[1] >= 4 else np.zeros(arr.shape[0])
            return arr[:, 0], arr[:, 1], flux, mag

    return None, None, None, None


# ============================ 验证函数 ============================

def check_wcs_construction(header):
    """验证项1: 用 astropy.wcs.WCS 从 header 构造 WCS 对象（不报错）

    返回: (pass: bool, detail: str, wcs: WCS or None)
    """
    from astropy.wcs import WCS
    try:
        wcs = WCS(header)
        return True, "WCS 构造成功", wcs
    except Exception as e:
        return False, "WCS 构造失败: %s" % e, None


def check_ctype(header):
    """验证项2: 验证 CTYPE1/CTYPE2 为 RA---TAN-SIP/DEC--TAN-SIP 或 RA---TAN/DEC--TAN

    返回: (pass: bool, detail: str)
    """
    ctype1 = str(header.get("CTYPE1", "")).strip()
    ctype2 = str(header.get("CTYPE2", "")).strip()

    valid_pairs = [
        ("RA---TAN-SIP", "DEC--TAN-SIP"),
        ("RA---TAN", "DEC--TAN"),
    ]

    for v1, v2 in valid_pairs:
        if ctype1 == v1 and ctype2 == v2:
            return True, "%s / %s" % (ctype1, ctype2)

    return False, "CTYPE 不匹配: CTYPE1=%s, CTYPE2=%s" % (ctype1, ctype2)


def check_crval_range(header):
    """验证项3: 验证 CRVAL1 ∈ [0,360], CRVAL2 ∈ [-90,90]

    返回: (pass: bool, detail: str)
    """
    crval1 = float(header.get("CRVAL1", 0.0))
    crval2 = float(header.get("CRVAL2", 0.0))

    if 0.0 <= crval1 <= 360.0 and -90.0 <= crval2 <= 90.0:
        return True, "CRVAL1=%.4f, CRVAL2=%.4f" % (crval1, crval2)
    return False, "CRVAL 超出范围: CRVAL1=%.4f (需[0,360]), CRVAL2=%.4f (需[-90,90])" % (crval1, crval2)


def check_cd_determinant(header):
    """验证项4: 验证 CD 矩阵行列式 > 0 (非退化)

    返回: (pass: bool, detail: str, det: float)
    """
    cd11 = float(header.get("CD1_1", 0.0))
    cd12 = float(header.get("CD1_2", 0.0))
    cd21 = float(header.get("CD2_1", 0.0))
    cd22 = float(header.get("CD2_2", 0.0))

    det = cd11 * cd22 - cd12 * cd21

    if det > 0:
        return True, "det=%.6e" % det, det
    return False, "CD 行列式非正: det=%.6e" % det, det


def check_center_projection(wcs, header, width, height):
    """验证项5: WCS 将图像中心像素投影到天球坐标，与 OBJCTRA/OBJCTDEC 比较 (误差 < 1 度)

    返回: (pass: bool, detail: str)
    """
    # 图像中心像素 (使用 0-based 中心)
    cx = width / 2.0
    cy = height / 2.0

    # 投影到天球
    sky = wcs.pixel_to_world(cx, cy)
    ra_center = float(sky.ra.deg)
    dec_center = float(sky.dec.deg)

    # 解析 OBJCTRA/OBJCTDEC
    objctra_str = str(header.get("OBJCTRA", "")).strip()
    objctdec_str = str(header.get("OBJCTDEC", "")).strip()

    if not objctra_str or not objctdec_str:
        return False, "OBJCTRA/OBJCTDEC 缺失: OBJCTRA=%r, OBJCTDEC=%r" % (objctra_str, objctdec_str)

    try:
        ra_obj = parse_hms(objctra_str)
        dec_obj = parse_dms(objctdec_str)
    except Exception as e:
        return False, "OBJCTRA/OBJCTDEC 解析失败: %s" % e

    # 计算角距 (球面距离简化计算，小角度近似)
    dra = ra_center - ra_obj
    # 处理 RA 环绕 [0, 360)
    if dra > 180.0:
        dra -= 360.0
    elif dra < -180.0:
        dra += 360.0

    ddec = dec_center - dec_obj
    cos_dec = float(np.cos(np.radians(dec_center)))
    error_deg = float(np.sqrt((dra * cos_dec) ** 2 + ddec ** 2))

    detail = "center=(%.1f,%.1f) -> ra=%.4f, dec=%.4f, OBJCTRA=%.4f, OBJCTDEC=%.4f, error=%.4fdeg" % (
        cx, cy, ra_center, dec_center, ra_obj, dec_obj, error_deg)

    if error_deg < 1.0:
        return True, detail
    return False, detail + " (误差 >= 1 度)"


def check_star_matching_rms(wcs, gaia_cat_data, star_det_data, pixel_scale_deg):
    """验证项6: 用 WCS 将 gaia_cat ra/dec 投影到像素，与 star_det x/y 比较，计算 RMS

    RMS 应 < 1 角秒 (1/3600 度)。

    参数:
        wcs: astropy.wcs.WCS 对象
        gaia_cat_data: gaia_cat JSON 数据
        star_det_data: star_det JSON 数据
        pixel_scale_deg: 像素尺度 (度/像素)，用于将像素 RMS 换算为角秒

    返回: (pass: bool or None, detail: str, rms_arcsec: float or None)
        - pass=None 表示因数据缺失跳过
    """
    from astropy.coordinates import SkyCoord
    import astropy.units as u

    ra_arr, dec_arr, mag_arr = parse_gaia_cat(gaia_cat_data)
    x_det, y_det, flux_det, mag_det = parse_star_det(star_det_data)

    if ra_arr is None or x_det is None:
        return None, "gaia_cat 或 star_det 数据格式无法解析", None

    if len(ra_arr) == 0 or len(x_det) == 0:
        return None, "gaia_cat 或 star_det 为空", None

    logger.info("星匹配 RMS 计算: gaia=%d, star_det=%d", len(ra_arr), len(x_det))

    # 将 Gaia 星投影到像素坐标
    gaia_sky = SkyCoord(ra=ra_arr * u.deg, dec=dec_arr * u.deg)
    x_gaia, y_gaia = wcs.world_to_pixel(gaia_sky)
    x_gaia = np.asarray(x_gaia, dtype=np.float64)
    y_gaia = np.asarray(y_gaia, dtype=np.float64)

    # 限制到检测星的 bounding box (加 50 像素余量) 内的 Gaia 星
    x_min = float(np.min(x_det)) - 50.0
    x_max = float(np.max(x_det)) + 50.0
    y_min = float(np.min(y_det)) - 50.0
    y_max = float(np.max(y_det)) + 50.0

    in_range = (x_gaia >= x_min) & (x_gaia <= x_max) & (y_gaia >= y_min) & (y_gaia <= y_max)
    n_in_range = int(np.sum(in_range))
    logger.info("图像范围内 Gaia 星: %d / %d", n_in_range, len(ra_arr))

    if n_in_range == 0:
        return None, "无 Gaia 星落在检测星范围内", None

    x_gaia_valid = x_gaia[in_range]
    y_gaia_valid = y_gaia[in_range]

    # 对每颗 Gaia 星，找最近的检测星 (向量化计算)
    # 为避免大矩阵 O(N_gaia * N_det)，按颗循环
    match_dist_px = np.empty(n_in_range, dtype=np.float64)
    for i in range(n_in_range):
        dx = x_det - x_gaia_valid[i]
        dy = y_det - y_gaia_valid[i]
        match_dist_px[i] = float(np.min(np.sqrt(dx * dx + dy * dy)))

    # RMS (像素)
    rms_px = float(np.sqrt(np.mean(match_dist_px ** 2)))

    # 转换到角秒
    if pixel_scale_deg > 0:
        rms_arcsec = rms_px * pixel_scale_deg * 3600.0
    else:
        rms_arcsec = None

    n_matched_5px = int(np.sum(match_dist_px < 5.0))  # 5 像素内匹配数

    logger.info("RMS 计算: rms_px=%.4f, rms_arcsec=%s, n_gaia=%d, matched<5px=%d",
                rms_px, "%.4f" % rms_arcsec if rms_arcsec is not None else "N/A",
                n_in_range, n_matched_5px)

    if rms_arcsec is None:
        detail = "RMS=%.4f px (n=%d) [pixel_scale=0, 无法换算角秒]" % (rms_px, n_in_range)
        return None, detail, None

    if rms_arcsec < 1.0:
        detail = "RMS=%.4f arcsec (n=%d, matched<5px=%d)" % (rms_arcsec, n_in_range, n_matched_5px)
        return True, detail, rms_arcsec

    detail = "RMS=%.4f arcsec (>1arcsec, n=%d, matched<5px=%d)" % (rms_arcsec, n_in_range, n_matched_5px)
    return False, detail, rms_arcsec


# ============================ 主验证函数 ============================

def validate_wcs(fits_file, gaia_cat_path=None, star_det_path=None, output_path=None):
    """主验证函数

    参数:
        fits_file: FITS 文件路径
        gaia_cat_path: Gaia 星表 JSON 路径 (可选)
        star_det_path: 星点检测 JSON 路径 (可选)
        output_path: 输出 JSON 路径 (可选，默认 output/pipeline_debug/<frame_name>/validate_wcs.json)

    返回:
        tuple: (result_dict, exit_code)
            - result_dict: 验证结果字典
            - exit_code: 0=通过, 1=失败
    """
    from astropy.io import fits

    logger.info("=" * 60)
    logger.info("WCS/SIP 验证启动")
    logger.info("  FITS 文件: %s", fits_file)
    logger.info("  Gaia 星表: %s", gaia_cat_path)
    logger.info("  星点检测: %s", star_det_path)

    # 检查文件存在
    if not os.path.isfile(fits_file):
        logger.error("FITS 文件不存在: %s", fits_file)
        return {
            "fits_file": fits_file,
            "overall_pass": False,
            "checks": [{"name": "file_exists", "pass": False, "detail": "文件不存在"}],
            "wcs_info": {},
        }, 1

    # 读取 FITS 头和图像尺寸
    logger.info("-" * 40)
    logger.info("读取 FITS 头")
    with fits.open(fits_file, mode="readonly") as hdul:
        header = hdul[0].header
        # 优先从数据获取尺寸
        if hdul[0].data is not None and hdul[0].data.ndim >= 2:
            height, width = int(hdul[0].data.shape[-2]), int(hdul[0].data.shape[-1])
        else:
            # 从头信息推断
            width = int(header.get("NAXIS1", 0))
            height = int(header.get("NAXIS2", 0))
    logger.info("图像尺寸: %dx%d", width, height)

    # 提取 WCS 信息 (用于输出)
    wcs_info = {
        "ctype1": str(header.get("CTYPE1", "")),
        "ctype2": str(header.get("CTYPE2", "")),
        "crval1": float(header.get("CRVAL1", 0.0)),
        "crval2": float(header.get("CRVAL2", 0.0)),
        "crpix1": float(header.get("CRPIX1", 0.0)),
        "crpix2": float(header.get("CRPIX2", 0.0)),
        "cd_matrix": [
            [float(header.get("CD1_1", 0.0)), float(header.get("CD1_2", 0.0))],
            [float(header.get("CD2_1", 0.0)), float(header.get("CD2_2", 0.0))],
        ],
        "sip_order": int(header.get("A_ORDER", 0)),
    }

    checks = []

    # 验证项1: WCS 构造
    logger.info("-" * 40)
    logger.info("验证项1: WCS 构造")
    ok, detail, wcs = check_wcs_construction(header)
    checks.append({"name": "wcs_construction", "pass": ok, "detail": detail})
    logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)

    if not ok:
        # WCS 构造失败，无法继续后续验证
        result = {
            "fits_file": fits_file,
            "overall_pass": False,
            "checks": checks,
            "wcs_info": wcs_info,
        }
        logger.error("WCS 构造失败，终止后续验证")
        _write_result(result, fits_file, output_path)
        return result, 1

    # 验证项2: CTYPE
    logger.info("验证项2: CTYPE 检查")
    ok, detail = check_ctype(header)
    checks.append({"name": "ctype", "pass": ok, "detail": detail})
    logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)

    # 验证项3: CRVAL 范围
    logger.info("验证项3: CRVAL 范围检查")
    ok, detail = check_crval_range(header)
    checks.append({"name": "crval_range", "pass": ok, "detail": detail})
    logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)

    # 验证项4: CD 行列式
    logger.info("验证项4: CD 行列式检查")
    ok, detail, det = check_cd_determinant(header)
    checks.append({"name": "cd_determinant", "pass": ok, "detail": detail})
    logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)

    # 验证项5: 中心投影 vs OBJCTRA/OBJCTDEC
    logger.info("验证项5: 中心投影 vs OBJCTRA/OBJCTDEC")
    ok, detail = check_center_projection(wcs, header, width, height)
    checks.append({"name": "center_projection", "pass": ok, "detail": detail})
    logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)

    # 验证项6: 星匹配 RMS (可选)
    if gaia_cat_path and star_det_path:
        logger.info("验证项6: 星匹配 RMS")
        try:
            gaia_cat_data = _load_json(gaia_cat_path)
            star_det_data = _load_json(star_det_path)
            logger.info("  gaia_cat 加载: %s", gaia_cat_path)
            logger.info("  star_det 加载: %s", star_det_path)

            # 计算 pixel scale (度/像素): sqrt(|det(CD)|)
            pixel_scale_deg = float(np.sqrt(abs(det))) if det != 0 else 0.0
            logger.info("  pixel_scale=%.6e deg/px", pixel_scale_deg)

            ok, detail, rms = check_star_matching_rms(
                wcs, gaia_cat_data, star_det_data, pixel_scale_deg
            )
            if ok is None:
                # 数据缺失或无法计算，标记为跳过 (pass=null)
                checks.append({"name": "star_matching_rms", "pass": None, "detail": detail})
                logger.info("  结果: SKIP - %s", detail)
            else:
                checks.append({"name": "star_matching_rms", "pass": ok, "detail": detail})
                logger.info("  结果: %s - %s", "PASS" if ok else "FAIL", detail)
        except Exception as e:
            checks.append({"name": "star_matching_rms", "pass": None, "detail": "计算异常: %s" % e})
            logger.error("  星匹配 RMS 计算异常: %s", e, exc_info=True)
    else:
        logger.info("验证项6: 跳过 (未提供 gaia_cat/star_det)")

    # 总体判定: 仅 pass=False 视为失败，pass=None (跳过) 不影响
    overall_pass = all(c["pass"] is not False for c in checks)

    result = {
        "fits_file": fits_file,
        "overall_pass": overall_pass,
        "checks": checks,
        "wcs_info": wcs_info,
    }

    _write_result(result, fits_file, output_path)
    logger.info("总体判定: %s", "PASS" if overall_pass else "FAIL")
    logger.info("=" * 60)

    return result, (0 if overall_pass else 1)


def _write_result(result, fits_file, output_path):
    """将验证结果写入 JSON 文件 (UTF-8 编码)

    参数:
        result: 验证结果字典
        fits_file: FITS 文件路径 (用于推导默认输出路径)
        output_path: 显式输出路径，None 则使用默认路径
    """
    if output_path is None:
        # 默认输出路径: output/pipeline_debug/<frame_name>/validate_wcs.json
        frame_name = os.path.splitext(os.path.basename(fits_file))[0]
        output_path = os.path.join("output", "pipeline_debug", frame_name, "validate_wcs.json")

    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=4)
    logger.info("验证结果已写入: %s", output_path)


# ============================ 命令行入口 ============================

def main():
    """命令行入口

    参数:
        fits_file: FITS 文件路径 (必填)
        --gaia-cat: Gaia 星表 JSON 路径 (可选)
        --star-det: 星点检测 JSON 路径 (可选)
        --output: 输出 JSON 路径 (可选)
    """
    parser = argparse.ArgumentParser(
        description="WCS/SIP 验证脚本: 用 astropy.wcs 验证 plate_solve 后的 WCS/SIP 正确性",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
    # 基本验证 (仅 WCS 头部)
    python validate_wcs_sip.py image.fits

    # 带 Gaia 星表和星点检测的完整验证
    python validate_wcs_sip.py image.fits --gaia-cat gaia.json --star-det stars.json

    # 指定输出路径
    python validate_wcs_sip.py image.fits --output result.json
        """,
    )
    parser.add_argument("fits_file", help="FITS 文件路径")
    parser.add_argument("--gaia-cat", default=None, help="Gaia 星表 JSON 路径 (可选)")
    parser.add_argument("--star-det", default=None, help="星点检测 JSON 路径 (可选)")
    parser.add_argument("--output", default=None,
                        help="输出 JSON 路径 (默认 output/pipeline_debug/<frame_name>/validate_wcs.json)")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8 避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    result, exit_code = validate_wcs(
        fits_file=args.fits_file,
        gaia_cat_path=args.gaia_cat,
        star_det_path=args.star_det,
        output_path=args.output,
    )

    # 输出结果 JSON 到 stdout
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
