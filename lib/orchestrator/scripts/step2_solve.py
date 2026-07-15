# -*- coding: utf-8 -*-
"""
Step2 解析阶段脚本 (Plate Solving Stage)
功能: 调用 IPVSolver 对校准后的 FITS 图像求解 WCS（世界坐标系），输出 WCS JSON。
用途: 全链路整合测试的第二步，接收 step1_calibrate.py 输出的 01_calibrated.fits，
      通过 Gaia 星表 + 星点检测 + IPV 三角形匹配求解 WCS，输出 02_wcs.json，
      供后续 step3/step4 进行梯度估计与光度校准。
依赖: numpy, logging, argparse, ctypes,
      astro_image_io.ImageReader (读取 FITS 元数据与头),
      vector_match_v2.GaiaClientPy (Gaia 数据库客户端),
      star_detector.StarDetector (星点检测器),
      ipv_solver.IPVSolver (plate solver)
调用:
    python step2_solve.py --image 01_calibrated.fits --ra0 12.34 --dec0 56.78 \
        --focal-length 200 --pixel-size 6.0 --output 02_wcs.json
    # stdout 输出 JSON: {success, rms_px, rms_arcsec, n_pairs, fov_diag_deg, error}
注意:
    - --ra0/--dec0/--focal-length/--pixel-size 为备用值，当 FITS 头无对应字段时使用
    - 输出的 02_wcs.json 同时包含 cd1_1/cd1_2/cd2_1/cd2_2 和 crval1/crval2/crpix1/crpix2 字段
      （兼容 GradientEstimator.build_wcs 的字段命名约定）
"""

from __future__ import annotations

import os
import sys
import json
import ctypes
import logging
import argparse
from datetime import datetime

import numpy as np

# ============================ 环境初始化 ============================

PROJECT_ROOT = r"F:\Astro dev\Astro CS Normalization Database"
MINGW_BIN = r"C:\msys64\mingw64\bin"

# 将 MinGW bin 加入 PATH（DLL 依赖）
if MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

# sys.path 设置：加入各模块的 python 目录
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python"))
sys.path.insert(
    0,
    os.path.join(
        PROJECT_ROOT, "lib", "plate_solve", "archive", "vector_method", "python", "python"
    ),
)
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "star_detector", "python"))

# 确保能导入 WCSTransform
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "photometric_calib", "flux_calibrator", "python"))


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "logs"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "step2_solve_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("step2_solve")
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

    lg.info("日志系统初始化完成，日志文件: %s", log_file)
    return lg


logger = _init_logger()


# ============================ 工具函数 ============================

def parse_ra_hms(s):
    """解析 RA 字符串 (支持 'HH:MM:SS' / 'HH MM SS' / 浮点度数)"""
    s = str(s).strip()
    parts = s.replace(":", " ").split()
    if len(parts) == 3:
        h, m, sec = parts
        return (int(h) + int(m) / 60.0 + float(sec) / 3600.0) * 15.0
    return float(s)


def parse_dec_dms(s):
    """解析 Dec 字符串 (支持 '±DD:MM:SS' / '±DD MM SS' / 浮点度数)"""
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


# ============================ 环境加载 ============================

def init_environment():
    """初始化 GaiaClient + StarDetector + IPVSolver + ImageReader

    Returns:
        tuple: (gaia_client, sdet, solver, reader)
    """
    logger.info("=" * 60)
    logger.info("环境初始化开始")

    # 1. 加载 GaiaClient
    logger.info("-" * 40)
    logger.info("加载 GaiaClientPy ...")
    from vector_match_v2 import GaiaClientPy

    gaia_dir = os.path.join(PROJECT_ROOT, "GaiaDR3SP")
    logger.info("Gaia 数据目录: %s", gaia_dir)
    gaia_client = GaiaClientPy(gaia_dir, db_type=2)
    logger.info("GaiaClientPy 创建成功")

    gaia_handle = gaia_client._handle
    if isinstance(gaia_handle, ctypes.c_void_p):
        gaia_handle = gaia_handle.value
    logger.info("Gaia 句柄: %s", gaia_handle)

    # 2. 加载 StarDetector
    logger.info("-" * 40)
    logger.info("加载 StarDetector ...")
    from star_detector import StarDetector, SDetParamsPy

    sdet = StarDetector(params=SDetParamsPy(fitRadius=0))
    logger.info("StarDetector 创建成功 (fitRadius=0)")

    sdet_handle = sdet._handle
    if isinstance(sdet_handle, ctypes.c_void_p):
        sdet_handle = sdet_handle.value
    logger.info("StarDetector 句柄: %s", sdet_handle)

    # 3. 加载 IPVSolver
    logger.info("-" * 40)
    logger.info("加载 IPVSolver ...")
    from ipv_solver import IPVSolver, result_to_dict

    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)
    logger.info("IPVSolver 创建成功，已设置 Gaia 和 StarDetector 句柄")

    # 4. 加载 ImageReader
    logger.info("-" * 40)
    logger.info("加载 ImageReader ...")
    from astro_image_io import ImageReader

    reader = ImageReader()
    logger.info("ImageReader 创建成功")

    logger.info("-" * 40)
    logger.info("环境初始化完成")
    logger.info("=" * 60)

    return gaia_client, sdet, solver, reader, result_to_dict


# ============================ FITS 头读取 ============================

def read_fits_header(reader, image_path, default_ra0, default_dec0,
                     default_focal_length, default_pixel_size):
    """从 FITS 头读取初始指向、焦距、像素尺寸

    Args:
        reader: ImageReader 实例
        image_path: FITS 图像路径
        default_ra0: 备用 RA (度)
        default_dec0: 备用 Dec (度)
        default_focal_length: 备用焦距 (mm)
        default_pixel_size: 备用像素尺寸 (um)

    Returns:
        dict: {ra0, dec0, focal_length, pixel_size, width, height}
    """
    logger.info("=" * 60)
    logger.info("读取 FITS 元数据: %s", image_path)

    # 读取元数据（focallen / xpixsz / width / height）
    meta = reader.read_metadata(image_path)
    focal_length = meta.observation.focallen if meta.observation else None
    pixel_size = meta.observation.xpixsz if meta.observation else None
    width = meta.geometry.width
    height = meta.geometry.height

    logger.info("元数据几何: width=%d, height=%d", width, height)
    logger.info("元数据焦距(focallen): %s", focal_length)
    logger.info("元数据像素尺寸(xpixsz): %s", pixel_size)

    # 读取 FITS 头关键字（OBJCTRA / OBJCTDEC）
    img = reader.read_header_only(image_path)
    try:
        kw_dict = {kw.name.upper(): kw.value for kw in img.keywords}
    finally:
        img.close()

    logger.info("FITS 头关键字总数: %d", len(kw_dict))

    # 解析 OBJCTRA / OBJCTDEC
    ra0 = default_ra0
    dec0 = default_dec0
    if "OBJCTRA" in kw_dict and kw_dict["OBJCTRA"]:
        try:
            ra0 = parse_ra_hms(kw_dict["OBJCTRA"])
            logger.info("OBJCTRA=%s -> ra0=%.6f 度", kw_dict["OBJCTRA"], ra0)
        except (ValueError, TypeError) as e:
            logger.warning("解析 OBJCTRA 失败，使用备用值: %s, err=%s", kw_dict["OBJCTRA"], e)
    else:
        logger.info("FITS 头无 OBJCTRA，使用备用值 ra0=%.6f", ra0)

    if "OBJCTDEC" in kw_dict and kw_dict["OBJCTDEC"]:
        try:
            dec0 = parse_dec_dms(kw_dict["OBJCTDEC"])
            logger.info("OBJCTDEC=%s -> dec0=%.6f 度", kw_dict["OBJCTDEC"], dec0)
        except (ValueError, TypeError) as e:
            logger.warning("解析 OBJCTDEC 失败，使用备用值: %s, err=%s", kw_dict["OBJCTDEC"], e)
    else:
        logger.info("FITS 头无 OBJCTDEC，使用备用值 dec0=%.6f", dec0)

    # 焦距和像素尺寸回退到备用值
    if focal_length is None or focal_length <= 0:
        focal_length = float(default_focal_length)
        logger.info("元数据无 focallen，使用备用值 focal_length=%.2f mm", focal_length)
    else:
        focal_length = float(focal_length)
        logger.info("使用元数据 focal_length=%.2f mm", focal_length)

    if pixel_size is None or pixel_size <= 0:
        pixel_size = float(default_pixel_size)
        logger.info("元数据无 xpixsz，使用备用值 pixel_size=%.4f um", pixel_size)
    else:
        pixel_size = float(pixel_size)
        logger.info("使用元数据 pixel_size=%.4f um", pixel_size)

    logger.info("=" * 60)

    return {
        "ra0": ra0,
        "dec0": dec0,
        "focal_length": focal_length,
        "pixel_size": pixel_size,
        "width": width,
        "height": height,
    }


# ============================ FOV 计算 ============================

def compute_fov(pixel_size, focal_length, width, height):
    """计算 FOV

    Args:
        pixel_size: 像素尺寸 (um)
        focal_length: 焦距 (mm)
        width: 图像宽度 (像素)
        height: 图像高度 (像素)

    Returns:
        tuple: (s0, fov_diag_deg)
            - s0: 角秒/像素
            - fov_diag_deg: 对角线 FOV (度)
    """
    s0 = 206.265 * pixel_size / focal_length  # 角秒/像素
    fov_diag_deg = (width ** 2 + height ** 2) ** 0.5 * s0 / 3600.0  # 度
    logger.info("FOV 计算: s0=%.6f 角秒/像素, fov_diag=%.4f 度", s0, fov_diag_deg)
    return s0, fov_diag_deg


# ============================ 星点检测与投影 ============================

def detect_and_project_stars(sdet, reader, image_path, wcs_json, output_path):
    """检测星点并用 WCS 投影到天球坐标

    Args:
        sdet: StarDetector 实例
        reader: ImageReader 实例
        image_path: FITS 图像路径
        wcs_json: WCS 参数字典(含 cd1_1/cd1_2/cd2_1/cd2_2/crval1/crval2/crpix1/crpix2/sip_order/sip_a/sip_b)
        output_path: 输出 JSON 路径

    Returns:
        dict: {success, n_stars, output_path, error}
    """
    logger.info("=" * 60)
    logger.info("检测星点并用 WCS 投影到天球坐标")
    logger.info("  图像路径: %s", image_path)
    logger.info("  输出 JSON: %s", output_path)

    try:
        # 1. 读取图像数据
        logger.info("读取图像数据 ...")
        img = reader.read(image_path)
        image_data = img.data
        logger.info("图像尺寸: %d x %d", img.width, img.height)

        # 2. 检测星点 (专用检测器: maxStars=10000, fitRadius=8 用于 PSF 拟合)
        logger.info("检测星点 (detect_ex, maxStars=10000) ...")
        from star_detector import StarDetector as _SDet
        from star_detector import SDetParamsPy as _SDetP
        sdet_detect = _SDet(params=_SDetP(maxStars=10000, fitRadius=8))
        result = sdet_detect.detect_ex(
            image_data,
            extra_names=["amplitude", "background", "fwhm_x", "fwhm_y"],
        )
        sdet_detect.close()
        n_total = result.count
        logger.info("检测到 %d 颗星 (正常: %d, 饱和: %d)",
                    n_total, result.normal_count, result.saturated_count)

        if n_total == 0:
            logger.warning("未检测到星点，跳过投影")
            output = {
                "wcs_ref": {
                    "crval1": float(wcs_json.get("crval1", 0.0)),
                    "crval2": float(wcs_json.get("crval2", 0.0)),
                    "crpix1": float(wcs_json.get("crpix1", 0.0)),
                    "crpix2": float(wcs_json.get("crpix2", 0.0)),
                },
                "n_total_detected": 0,
                "n_selected": 0,
                "snr_min_threshold": 5.0,
                "pct_max": 0.30,
                "n_grid": 8,
                "min_per_block": 15,
                "n_min": 800,
                "stars": [],
            }
            os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
            with open(output_path, "w", encoding="utf-8") as f:
                json.dump(output, f, ensure_ascii=True, indent=2)
            logger.info("空星列表已保存: %s", output_path)
            return {"success": True, "n_stars": 0, "n_selected": 0,
                    "output_path": output_path, "error": None}

        # 3. 构建 WCSTransform
        logger.info("构建 WCSTransform ...")
        from wcs_transform import WCSTransform

        crpix1 = float(wcs_json.get("crpix1", 0.0))
        crpix2 = float(wcs_json.get("crpix2", 0.0))
        crval1 = float(wcs_json.get("crval1", 0.0))
        crval2 = float(wcs_json.get("crval2", 0.0))
        cd11 = float(wcs_json.get("cd1_1", 0.0))
        cd12 = float(wcs_json.get("cd1_2", 0.0))
        cd21 = float(wcs_json.get("cd2_1", 0.0))
        cd22 = float(wcs_json.get("cd2_2", 0.0))
        sip_order = int(wcs_json.get("sip_order", 0))
        sip_a = wcs_json.get("sip_a", [])
        sip_b = wcs_json.get("sip_b", [])

        wcs = WCSTransform(
            crpix1=crpix1, crpix2=crpix2,
            crval1=crval1, crval2=crval2,
            cd11=cd11, cd12=cd12, cd21=cd21, cd22=cd22,
            sip_order=sip_order,
            sip_a=sip_a if sip_order > 0 else None,
            sip_b=sip_b if sip_order > 0 else None,
        )
        logger.info("WCSTransform 创建成功 (sip_order=%d)", sip_order)

        # 4. 提取 extras 字段
        amplitudes = list(result.extras.get("amplitude", [0.0] * n_total))
        backgrounds = list(result.extras.get("background", [0.0] * n_total))
        fwhm_xs = list(result.extras.get("fwhm_x", [0.0] * n_total))
        fwhm_ys = list(result.extras.get("fwhm_y", [0.0] * n_total))

        # 5. 按 SNR + 空间均匀性筛选
        logger.info("按 SNR + 空间均匀性筛选星点 ...")
        selected = select_stars_by_snr_and_uniformity(
            xs=list(result.x),
            ys=list(result.y),
            amplitudes=amplitudes,
            backgrounds=backgrounds,
            fwhm_xs=fwhm_xs,
            fwhm_ys=fwhm_ys,
            saturateds=list(result.saturated),
            width=img.width,
            height=img.height,
            snr_min=5.0,
            pct_max=0.30,
            n_grid=8,
            min_per_block=15,
            n_min=800,
            n_target=1500,
        )
        n_selected = len(selected)
        logger.info("筛选完成: %d -> %d 颗星", n_total, n_selected)

        # 6. 批量转换像素坐标到天球坐标
        if n_selected > 0:
            logger.info("批量转换 %d 颗星到天球坐标 ...", n_selected)
            x_arr = np.array([s["x_px"] for s in selected], dtype=np.float64)
            y_arr = np.array([s["y_px"] for s in selected], dtype=np.float64)
            ra_arr, dec_arr = wcs.pixel_to_sky_batch(x_arr, y_arr)
            for i, star in enumerate(selected):
                star["ra"] = float(ra_arr[i])
                star["dec"] = float(dec_arr[i])

        # 7. 输出 JSON (符合 spec 格式)
        output = {
            "wcs_ref": {
                "crval1": crval1,
                "crval2": crval2,
                "crpix1": crpix1,
                "crpix2": crpix2,
            },
            "n_total_detected": int(n_total),
            "n_selected": int(n_selected),
            "snr_min_threshold": 5.0,
            "pct_max": 0.30,
            "n_grid": 8,
            "min_per_block": 15,
            "n_min": 800,
            "stars": selected,
        }
        os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(output, f, ensure_ascii=True, indent=2)

        logger.info("检测星列表已保存: %s (%d/%d 颗星)", output_path, n_selected, n_total)
        logger.info("=" * 60)

        return {"success": True, "n_stars": n_total, "n_selected": n_selected,
                "output_path": output_path, "error": None}

    except Exception as e:
        err_msg = f"星点检测与投影失败: {e}"
        logger.error(err_msg, exc_info=True)
        return {"success": False, "n_stars": 0, "output_path": output_path, "error": err_msg}
    finally:
        try:
            img.close()
            logger.info("图像资源已释放")
        except Exception:
            pass


# ============================ 星点筛选 ============================

def select_stars_by_snr_and_uniformity(
    xs, ys, amplitudes, backgrounds, fwhm_xs, fwhm_ys, saturateds,
    width, height,
    snr_min=5.0,
    pct_max=0.30,
    n_grid=8,
    min_per_block=15,
    n_min=800,
    n_target=1500
):
    """
    按 PSF SNR + 空间均匀性筛选星点

    PSF SNR 公式: SNR = A * sqrt(N_pix) / sqrt(A * N_pix + B * N_pix)
    其中 N_pix = π * (FWHM/2)², FWHM = (fwhm_x + fwhm_y) / 2

    筛选策略:
    1. 排除饱和星
    2. SNR 硬下限: SNR < snr_min 的直接丢弃
    3. 百分比上限: 按 SNR 降序取前 pct_max% (默认前 20%) 作为候选池
    4. 空间均匀性: 将图像划分为 n_grid×n_grid 网格 (默认 8×8=64 块)
       每块从候选池中至少取 min_per_block 颗星 (不足则记录警告)
    5. 总数下限: 最终星数至少 n_min (默认 200)
       若候选池不足 n_min，逐步放宽 pct_max 直至满足
    6. 总数上限: 不超过 n_target (默认 600)

    Args:
        xs, ys: 像素坐标列表
        amplitudes: Moffat4 拟合振幅
        backgrounds: 局部背景
        fwhm_xs, fwhm_ys: PSF FWHM (像素)
        saturateds: 饱和标记
        width, height: 图像尺寸
        snr_min: SNR 硬下限
        pct_max: 百分比上限 (0-1), 默认 0.20 (前 20%)
        n_grid: 网格划分数, 默认 8 (8×8=64 块)
        min_per_block: 每块最少星数, 默认 5
        n_min: 最终最少星数, 默认 200
        n_target: 最终最多星数, 默认 600

    Returns:
        list[dict]: 筛选后的星点列表，每项含:
            x_px, y_px, ra, dec, flux, amplitude, background,
            fwhm_px, snr, saturated
    """
    n_stars = len(xs)
    logger.info("开始星点筛选: %d 颗星, snr_min=%.1f, pct_max=%.2f, n_grid=%dx%d, min/block=%d, n_min=%d, n_target=%d",
                n_stars, snr_min, pct_max, n_grid, n_grid, min_per_block, n_min, n_target)

    if n_stars == 0:
        logger.warning("无星点可筛选")
        return []

    xs_arr = np.array(xs, dtype=np.float64)
    ys_arr = np.array(ys, dtype=np.float64)

    # 1. 计算每颗星的 SNR
    snr_list = []
    for i in range(n_stars):
        fwhm = (fwhm_xs[i] + fwhm_ys[i]) / 2.0
        if fwhm <= 0:
            snr_list.append(0.0)
            continue
        n_pix = np.pi * (fwhm / 2.0) ** 2
        a = amplitudes[i]
        b = backgrounds[i]
        denominator = np.sqrt(a * n_pix + b * n_pix)
        if denominator <= 0:
            snr_list.append(0.0)
        else:
            snr_list.append(a * np.sqrt(n_pix) / denominator)

    snr_arr = np.array(snr_list, dtype=np.float64)
    logger.info("SNR 统计: min=%.2f, max=%.2f, mean=%.2f, median=%.2f",
                np.min(snr_arr), np.max(snr_arr), np.mean(snr_arr), np.median(snr_arr))

    # 2. 排除饱和星 + SNR 硬下限
    sat_arr = np.array(saturateds, dtype=np.int32)
    mask_valid = (sat_arr == 0) & (snr_arr >= snr_min)
    n_valid = int(np.sum(mask_valid))
    logger.info("排除饱和星 + SNR 硬下限: %d -> %d (非饱和且 SNR >= %.1f)", n_stars, n_valid, snr_min)

    if n_valid == 0:
        logger.warning("无非饱和且满足 SNR 下限的星点")
        return []

    # 3. 按 SNR 降序排列所有有效星
    valid_indices = np.where(mask_valid)[0]
    valid_snr = snr_arr[valid_indices]
    sorted_order = np.argsort(-valid_snr)  # 降序
    sorted_valid_indices = valid_indices[sorted_order]

    # 4. 百分比上限: 取前 pct_max% 作为候选池
    #    若候选池不足 n_min，逐步放宽 pct_max
    current_pct = pct_max
    while True:
        n_pct = max(1, int(n_valid * current_pct))
        candidate_indices = sorted_valid_indices[:n_pct]
        if len(candidate_indices) >= n_min or current_pct >= 1.0:
            break
        current_pct = min(1.0, current_pct + 0.10)
        logger.info("候选池不足 %d 颗，放宽 pct_max 至 %.0f%% (候选 %d)",
                    n_min, current_pct * 100, n_pct)

    logger.info("百分比上限筛选: %d -> %d (前 %.0f%%)",
                n_valid, len(candidate_indices), current_pct * 100)

    # 5. 空间均匀性: 网格划分 + 每块下限
    grid_x = np.clip(np.floor(xs_arr[candidate_indices] / width * n_grid).astype(int), 0, n_grid - 1)
    grid_y = np.clip(np.floor(ys_arr[candidate_indices] / height * n_grid).astype(int), 0, n_grid - 1)
    grid_ids = grid_y * n_grid + grid_x

    from collections import defaultdict
    grid_groups = defaultdict(list)
    for rank, idx in enumerate(candidate_indices):
        gid = grid_ids[rank]
        grid_groups[gid].append(idx)  # candidate_indices 已按 SNR 降序，组内也是降序

    n_nonempty_blocks = len(grid_groups)
    logger.info("网格划分: %dx%d=%d 块, 非空块数: %d", n_grid, n_grid, n_grid * n_grid, n_nonempty_blocks)

    # 6. 每块至少 min_per_block 颗，然后轮流补充至 n_target
    selected_indices = []
    selected_set = set()
    empty_blocks = []

    for gid in range(n_grid * n_grid):
        if gid in grid_groups and len(grid_groups[gid]) > 0:
            take = min(min_per_block, len(grid_groups[gid]))
            for i in range(take):
                idx = grid_groups[gid][i]
                if idx not in selected_set:
                    selected_indices.append(idx)
                    selected_set.add(idx)
        else:
            empty_blocks.append(gid)

    if empty_blocks:
        logger.warning("空网格块数: %d / %d (这些区域无高 SNR 星点)",
                       len(empty_blocks), n_grid * n_grid)

    logger.info("每块下限 (%d 颗/块) 取星: %d 颗", min_per_block, len(selected_indices))

    # 7. 若不足 n_min，从候选池剩余星按 SNR 降序补充
    if len(selected_indices) < n_min:
        remaining = [idx for idx in candidate_indices if idx not in selected_set]
        need = n_min - len(selected_indices)
        logger.info("不足 n_min=%d，从候选池补充 %d 颗 (按 SNR 降序)", n_min, min(need, len(remaining)))
        for idx in remaining[:need]:
            selected_indices.append(idx)
            selected_set.add(idx)

    # 8. 若仍不足 n_min，从全部有效星补充（放宽 pct_max 到 100%）
    if len(selected_indices) < n_min:
        remaining = [idx for idx in sorted_valid_indices if idx not in selected_set]
        need = n_min - len(selected_indices)
        logger.info("仍不足 n_min=%d，从全部有效星补充 %d 颗", n_min, min(need, len(remaining)))
        for idx in remaining[:need]:
            selected_indices.append(idx)
            selected_set.add(idx)

    # 9. 若超过 n_target，截断（保留高 SNR）
    if len(selected_indices) > n_target:
        selected_indices = selected_indices[:n_target]
        logger.info("超过 n_target=%d，截断", n_target)

    logger.info("空间均匀性筛选完成: %d 颗星", len(selected_indices))

    # 10. 构建输出列表
    stars = []
    for idx in selected_indices:
        fwhm = (fwhm_xs[idx] + fwhm_ys[idx]) / 2.0
        star = {
            "x_px": float(xs[idx]),
            "y_px": float(ys[idx]),
            "ra": 0.0,
            "dec": 0.0,
            "flux": 0.0,
            "amplitude": float(amplitudes[idx]),
            "background": float(backgrounds[idx]),
            "fwhm_px": float(fwhm),
            "snr": float(snr_arr[idx]),
            "saturated": int(saturateds[idx]),
        }
        stars.append(star)

    logger.info("星点筛选完成: 输出 %d 颗星", len(stars))
    return stars


# ============================ 主入口 ============================

def run(image_path, ra0, dec0, focal_length, pixel_size, output_path, output_detected_path=None):
    """运行 plate solving 阶段

    Args:
        image_path: 校准后的 FITS 图像路径
        ra0: 备用初始 RA (度)
        dec0: 备用初始 Dec (度)
        focal_length: 备用焦距 (mm)
        pixel_size: 备用像素尺寸 (um)
        output_path: 输出 WCS JSON 路径

    Returns:
        dict: {success, rms_px, rms_arcsec, n_pairs, fov_diag_deg, error}
    """
    logger.info("=" * 60)
    logger.info("Step2 解析阶段启动")
    logger.info("  图像路径: %s", image_path)
    logger.info("  输出 JSON: %s", output_path)

    # 校验输入
    if not os.path.isfile(image_path):
        err = f"图像文件不存在: {image_path}"
        logger.error(err)
        return {
            "success": False,
            "rms_px": 0.0,
            "rms_arcsec": 0.0,
            "n_pairs": 0,
            "fov_diag_deg": 0.0,
            "error": err,
        }

    # 环境初始化
    try:
        gaia_client, sdet, solver, reader, result_to_dict = init_environment()
    except Exception as e:
        err = f"环境初始化失败: {e}"
        logger.error(err, exc_info=True)
        return {
            "success": False,
            "rms_px": 0.0,
            "rms_arcsec": 0.0,
            "n_pairs": 0,
            "fov_diag_deg": 0.0,
            "error": err,
        }

    try:
        # 1. 读取 FITS 头
        header_info = read_fits_header(
            reader, image_path, ra0, dec0, focal_length, pixel_size
        )
        ra0_eff = header_info["ra0"]
        dec0_eff = header_info["dec0"]
        focal_length_eff = header_info["focal_length"]
        pixel_size_eff = header_info["pixel_size"]
        width = header_info["width"]
        height = header_info["height"]

        # 2. FOV 计算
        logger.info("-" * 40)
        logger.info("计算 FOV ...")
        s0, fov_diag_deg = compute_fov(pixel_size_eff, focal_length_eff, width, height)

        # 3. 调用 solver.solve()
        logger.info("-" * 40)
        logger.info("调用 solver.solve() ...")
        logger.info("  ra0=%.6f 度, dec0=%.6f 度", ra0_eff, dec0_eff)
        logger.info("  focal_length=%.2f mm, pixel_size=%.4f um",
                    focal_length_eff, pixel_size_eff)

        try:
            result = solver.solve(
                image_path, ra0_eff, dec0_eff, focal_length_eff, pixel_size_eff
            )
        except Exception as e:
            err = f"solver.solve() 异常: {e}"
            logger.error(err, exc_info=True)
            return {
                "success": False,
                "rms_px": 0.0,
                "rms_arcsec": 0.0,
                "n_pairs": 0,
                "fov_diag_deg": fov_diag_deg,
                "error": err,
            }

        logger.info("solver.solve() 返回: success=%d", result.success)
        logger.info("  rms_px=%.4f, rms_arcsec=%.4f", result.rms_px, result.rms_arcsec)
        logger.info("  n_pairs=%d, n_detected=%d, n_catalog=%d",
                    result.n_pairs, result.n_detected, result.n_catalog)
        logger.info("  best_inliers=%d, trans_order=%d",
                    result.best_inliers, result.trans_order)
        if result.error_msg:
            err_msg = result.error_msg.decode("utf-8", errors="ignore").strip()
            logger.info("  error_msg=%s", err_msg)

        # 4. 构建输出字典
        result_dict = result_to_dict(result)

        # 提取 cd / crval / crpix 的分量（兼容 GradientEstimator.build_wcs 的字段命名）
        cd = result_dict.get("cd", [0.0, 0.0, 0.0, 0.0])
        crval = result_dict.get("crval", [0.0, 0.0])
        crpix = result_dict.get("crpix", [0.0, 0.0])

        wcs_json = {
            "success": bool(result.success),
            # cd 矩阵分量（build_wcs 兼容字段名）
            "cd1_1": cd[0] if len(cd) > 0 else 0.0,
            "cd1_2": cd[1] if len(cd) > 1 else 0.0,
            "cd2_1": cd[2] if len(cd) > 2 else 0.0,
            "cd2_2": cd[3] if len(cd) > 3 else 0.0,
            # crval 分量
            "crval1": crval[0] if len(crval) > 0 else 0.0,
            "crval2": crval[1] if len(crval) > 1 else 0.0,
            # crpix 分量
            "crpix1": crpix[0] if len(crpix) > 0 else 0.0,
            "crpix2": crpix[1] if len(crpix) > 1 else 0.0,
            # SIP 信息
            "sip_order": result_dict.get("sip_order", 0),
            "sip_a": result_dict.get("sip_a", []),
            "sip_b": result_dict.get("sip_b", []),
            # RMS 与匹配统计
            "rms_px": result_dict.get("rms_px", 0.0),
            "rms_arcsec": result_dict.get("rms_arcsec", 0.0),
            "n_pairs": result_dict.get("n_pairs", 0),
            "n_detected": result_dict.get("n_detected", 0),
            "n_catalog": result_dict.get("n_catalog", 0),
            "best_inliers": result_dict.get("best_inliers", 0),
            # ctype
            "ctype1": result_dict.get("ctype1", "RA---TAN"),
            "ctype2": result_dict.get("ctype2", "DEC--TAN"),
            # 错误信息
            "error_msg": result_dict.get("error_msg", ""),
            # FOV 与初始指向
            "fov_diag_deg": fov_diag_deg,
            "ra0": ra0_eff,
            "dec0": dec0_eff,
            "focal_length_mm": focal_length_eff,
            "pixel_size_um": pixel_size_eff,
            "width": width,
            "height": height,
        }

        # 5. 检测星点并用 WCS 投影（额外输出，不影响主流程）
        logger.info("-" * 40)
        logger.info("检测星点并投影到天球坐标 ...")
        detected_path = output_path.replace("02_wcs.json", "02_detected_stars.json")
        detect_result = detect_and_project_stars(
            sdet, reader, image_path, wcs_json, detected_path
        )
        if detect_result.get("success"):
            n_detected = detect_result.get("n_stars", 0)
            n_selected = detect_result.get("n_selected", 0)
            wcs_json["n_detected_stars"] = n_detected
            wcs_json["n_selected_stars"] = n_selected
            wcs_json["detected_stars_path"] = detected_path
            logger.info("星点检测与投影成功: 检测 %d 颗, 筛选 %d 颗 -> %s",
                        n_detected, n_selected, detected_path)
        else:
            n_detected = 0
            n_selected = 0
            logger.warning("星点检测与投影失败: %s", detect_result.get("error", "未知错误"))

        # 6. 保存 JSON
        logger.info("-" * 40)
        logger.info("保存 WCS JSON: %s", output_path)
        os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(wcs_json, f, ensure_ascii=True, indent=2)
        logger.info("WCS JSON 保存完成")

        logger.info("-" * 40)
        logger.info("Step2 解析阶段完成")
        logger.info("  success=%s", wcs_json["success"])
        logger.info("  rms_px=%.4f, rms_arcsec=%.4f",
                    wcs_json["rms_px"], wcs_json["rms_arcsec"])
        logger.info("  n_pairs=%d, fov_diag_deg=%.4f",
                    wcs_json["n_pairs"], wcs_json["fov_diag_deg"])
        logger.info("=" * 60)

        return {
            "success": wcs_json["success"],
            "rms_px": wcs_json["rms_px"],
            "rms_arcsec": wcs_json["rms_arcsec"],
            "n_pairs": wcs_json["n_pairs"],
            "fov_diag_deg": wcs_json["fov_diag_deg"],
            "n_detected": n_detected,
            "n_selected": n_selected,
            "error": wcs_json["error_msg"],
        }
    finally:
        # 释放资源
        try:
            solver.close()
            logger.info("IPVSolver 资源已释放")
        except Exception:
            pass
        try:
            sdet.close()
            logger.info("StarDetector 资源已释放")
        except Exception:
            pass
        try:
            gaia_client.close()
            logger.info("GaiaClient 资源已释放")
        except Exception:
            pass


# ============================ 命令行入口 ============================

def main():
    """
    命令行入口，支持以下参数：
    --image: 校准后的 FITS 图像路径（必需）
    --ra0: 备用初始 RA (度)（FITS 头无 OBJCTRA 时使用）
    --dec0: 备用初始 Dec (度)（FITS 头无 OBJCTDEC 时使用）
    --focal-length: 备用焦距 (mm)（FITS 头无 focallen 时使用）
    --pixel-size: 备用像素尺寸 (um)（FITS 头无 xpixsz 时使用）
    --output: 输出 WCS JSON 路径（必需）
    """
    parser = argparse.ArgumentParser(
        description="Step2 解析阶段: 调用 IPVSolver 求解 WCS，输出 02_wcs.json",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--image", required=True,
                        help="校准后的 FITS 图像路径")
    parser.add_argument("--ra0", type=float, default=0.0,
                        help="备用初始 RA (度)，FITS 头无 OBJCTRA 时使用")
    parser.add_argument("--dec0", type=float, default=0.0,
                        help="备用初始 Dec (度)，FITS 头无 OBJCTDEC 时使用")
    parser.add_argument("--focal-length", type=float, default=200.0,
                        help="备用焦距 (mm)，FITS 头无 focallen 时使用")
    parser.add_argument("--pixel-size", type=float, default=6.0,
                        help="备用像素尺寸 (um)，FITS 头无 xpixsz 时使用")
    parser.add_argument("--output", required=True,
                        help="输出 WCS JSON 路径")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    result = run(
        image_path=args.image,
        ra0=args.ra0,
        dec0=args.dec0,
        focal_length=args.focal_length,
        pixel_size=args.pixel_size,
        output_path=args.output,
    )

    # 输出 JSON 到 stdout
    print(json.dumps(result, ensure_ascii=True, default=str))
    return 0 if result.get("success") else 1


if __name__ == "__main__":
    sys.exit(main())
