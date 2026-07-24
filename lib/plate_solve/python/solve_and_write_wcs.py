# -*- coding: utf-8 -*-
"""
Plate Solve and Write WCS
=========================
功能: 传入校准后FITS文件路径，调用IPVSolver完成plate solving，并将WCS关键字写入FITS文件头
用途: 自动化WCS求解与写入流程，支持单文件处理和批量并行处理
依赖: astropy, numpy, ipv_solver, vector_match_v2, star_detector
调用:
    # 单文件
    python solve_and_write_wcs.py --fits path/to/image.fits
    # 指定备用参数
    python solve_and_write_wcs.py --fits image.fits --ra0 12.34 --dec0 56.78 \
        --focal-length 200 --pixel-size 6.0
    # 不覆盖原文件
    python solve_and_write_wcs.py --fits image.fits --no-overwrite
注意:
    - --ra0/--dec0/--focal-length/--pixel-size 为备用值，未传入时从FITS头读取
    - OBJCTRA/OBJCTDEC 格式为 "HH MM SS.SS" / "DD MM SS.SS"（时分秒），自动转换为度
    - WCS关键字写入时保留原数据，仅更新/追加WCS相关关键字
    - SIP系数为下三角: A[i*6+j] 对应 dx^i*dy^j (i+j<=order)
作者: Astro CS Normalization Database
日期: 2026-07-11
"""

from __future__ import annotations

import os
import sys
import json
import ctypes
import logging
import argparse
import threading
import concurrent.futures
from datetime import datetime
from typing import Optional

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

# astro_image_io.dll 所在目录（C++ plate solver 运行时依赖）
_ASTRO_IO_DIR = os.path.join(PROJECT_ROOT, "lib", "astro_image_io")
if _ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
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
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "photometric_calib", "flux_calibrator", "python"))


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "logs"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] [%(threadName)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "solve_and_write_wcs_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("solve_and_write_wcs")
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
    """解析 RA 字符串为度数

    支持格式:
        "HH:MM:SS.SS" / "HH MM SS.SS" / 浮点度数字符串

    参数:
        s: RA 字符串

    返回:
        float: RA 度数
    """
    s = str(s).strip()
    parts = s.replace(":", " ").split()
    if len(parts) == 3:
        h, m, sec = parts
        return (int(h) + int(m) / 60.0 + float(sec) / 3600.0) * 15.0
    return float(s)


def parse_dec_dms(s):
    """解析 Dec 字符串为度数

    支持格式:
        "±DD:MM:SS.SS" / "±DD MM SS.SS" / 浮点度数字符串

    参数:
        s: Dec 字符串

    返回:
        float: Dec 度数
    """
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
    """初始化 GaiaClient + StarDetector + IPVSolver

    每次调用创建独立实例，适用于单文件处理或线程内复用。

    返回:
        tuple: (gaia_client, sdet, solver)
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
    from ipv_solver import IPVSolver

    solver = IPVSolver()
    solver.set_gaia_handle(gaia_handle)
    solver.set_detector_handle(sdet_handle)
    logger.info("IPVSolver 创建成功，已设置 Gaia 和 StarDetector 句柄")

    logger.info("-" * 40)
    logger.info("环境初始化完成")
    logger.info("=" * 60)

    return gaia_client, sdet, solver


def _close_environment(gaia_client, sdet, solver):
    """释放环境资源

    参数:
        gaia_client: GaiaClientPy 实例
        sdet: StarDetector 实例
        solver: IPVSolver 实例
    """
    for obj, name in [(solver, "IPVSolver"), (sdet, "StarDetector"), (gaia_client, "GaiaClient")]:
        try:
            obj.close()
            logger.info("%s 资源已释放", name)
        except Exception:
            pass


# ============================ FITS 头读取 ============================

def read_fits_header(fits_path, default_ra0=0.0, default_dec0=0.0,
                     default_focal_length=0.0, default_pixel_size=0.0):
    """从 FITS 头读取初始指向、焦距、像素尺寸

    使用 astropy.io.fits 读取 OBJCTRA / OBJCTDEC / FOCALLEN / XPIXSZ 关键字。
    如果参数传入非零值，优先使用传入值；否则从 FITS 头读取。

    参数:
        fits_path: FITS 文件路径
        default_ra0: 备用 RA (度)，传入非零值优先使用
        default_dec0: 备用 Dec (度)，传入非零值优先使用
        default_focal_length: 备用焦距 (mm)，传入非零值优先使用
        default_pixel_size: 备用像素尺寸 (um)，传入非零值优先使用

    返回:
        dict: {ra0, dec0, focal_length, pixel_size}
    """
    from astropy.io import fits

    logger.info("=" * 60)
    logger.info("读取 FITS 头: %s", fits_path)

    with fits.open(fits_path, mode='readonly') as hdul:
        header = hdul[0].header
        objctra = header.get('OBJCTRA')
        objctdec = header.get('OBJCTDEC')
        focallen = header.get('FOCALLEN')
        xpixsz = header.get('XPIXSZ')

    logger.info("FITS 头值: OBJCTRA=%s, OBJCTDEC=%s, FOCALLEN=%s, XPIXSZ=%s",
                objctra, objctdec, focallen, xpixsz)

    # RA: 传入值优先
    if default_ra0 != 0.0:
        ra0 = float(default_ra0)
        logger.info("使用传入值 ra0=%.6f 度", ra0)
    elif objctra:
        try:
            ra0 = parse_ra_hms(objctra)
            logger.info("OBJCTRA=%s -> ra0=%.6f 度", objctra, ra0)
        except (ValueError, TypeError) as e:
            ra0 = 0.0
            logger.warning("解析 OBJCTRA 失败，ra0=0: %s, err=%s", objctra, e)
    else:
        ra0 = 0.0
        logger.warning("FITS 头无 OBJCTRA 且未传入备用值，ra0=0")

    # Dec: 传入值优先
    if default_dec0 != 0.0:
        dec0 = float(default_dec0)
        logger.info("使用传入值 dec0=%.6f 度", dec0)
    elif objctdec:
        try:
            dec0 = parse_dec_dms(objctdec)
            logger.info("OBJCTDEC=%s -> dec0=%.6f 度", objctdec, dec0)
        except (ValueError, TypeError) as e:
            dec0 = 0.0
            logger.warning("解析 OBJCTDEC 失败，dec0=0: %s, err=%s", objctdec, e)
    else:
        dec0 = 0.0
        logger.warning("FITS 头无 OBJCTDEC 且未传入备用值，dec0=0")

    # 焦距: 传入值优先
    if default_focal_length != 0.0:
        focal_length = float(default_focal_length)
        logger.info("使用传入值 focal_length=%.2f mm", focal_length)
    elif focallen is not None and float(focallen) > 0:
        focal_length = float(focallen)
        logger.info("使用 FITS 头 FOCALLEN=%.2f mm", focal_length)
    else:
        focal_length = 0.0
        logger.warning("FITS 头无 FOCALLEN 且未传入备用值，focal_length=0")

    # 像素尺寸: 传入值优先
    if default_pixel_size != 0.0:
        pixel_size = float(default_pixel_size)
        logger.info("使用传入值 pixel_size=%.4f um", pixel_size)
    elif xpixsz is not None and float(xpixsz) > 0:
        pixel_size = float(xpixsz)
        logger.info("使用 FITS 头 XPIXSZ=%.4f um", pixel_size)
    else:
        pixel_size = 0.0
        logger.warning("FITS 头无 XPIXSZ 且未传入备用值，pixel_size=0")

    logger.info("=" * 60)

    return {
        "ra0": ra0,
        "dec0": dec0,
        "focal_length": focal_length,
        "pixel_size": pixel_size,
    }


# ============================ WCS 关键字写入 ============================

# 需要删除的 WCS 相关关键字前缀
_WCS_KEYWORD_PATTERNS = [
    "CTYPE", "CRVAL", "CRPIX", "CDELT", "CD", "RADESYS", "EQUINOX",
    "LONPOLE", "LATPOLE", "CROTA",
    "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
    "PV1_", "PV2_", "PS1_", "PS2_",
]

# 需要精确删除的 WCS 关键字
_WCS_EXACT_KEYWORDS = {
    "CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
    "CDELT1", "CDELT2", "CD1_1", "CD1_2", "CD2_1", "CD2_2",
    "RADESYS", "EQUINOX", "LONPOLE", "LATPOLE",
    "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
}


def _remove_wcs_keywords(header):
    """删除 FITS 头中已有的 WCS 相关关键字

    删除范围: CTYPE/CRVAL/CRPIX/CD/CDELT/RADESYS/EQUINOX/LONPOLE/LATPOLE
              以及 SIP 系数 (A_i_j, B_i_j, AP_i_j, BP_i_j)

    参数:
        header: astropy.io.fits.Header 对象
    """
    # 收集所有需要删除的关键字名
    keys_to_delete = []

    for key in list(header.keys()):
        if key in ("", "COMMENT", "HISTORY"):
            continue
        key_upper = key.strip().upper()

        # 精确匹配
        if key_upper in _WCS_EXACT_KEYWORDS:
            keys_to_delete.append(key)
            continue

        # SIP 系数模式: A_i_j, B_i_j, AP_i_j, BP_i_j
        for prefix in ("A_", "B_", "AP_", "BP_"):
            if key_upper.startswith(prefix):
                remainder = key_upper[len(prefix):]
                parts = remainder.split("_")
                if len(parts) == 2 and all(p.isdigit() for p in parts):
                    keys_to_delete.append(key)
                    break

    # 执行删除
    for key in keys_to_delete:
        try:
            del header[key]
        except (KeyError, Exception):
            pass

    if keys_to_delete:
        logger.info("已删除 %d 个旧 WCS 关键字", len(keys_to_delete))


def write_wcs_to_fits(fits_path, result, overwrite=True):
    """将 WCS 关键字写入 FITS 文件头

    使用 astropy.io.fits 直接修改文件头，保留原数据，仅更新/追加 WCS 关键字。

    参数:
        fits_path: FITS 文件路径
        result: IpvWcsResult 结构体
        overwrite: True=覆盖原文件, False=不修改文件

    返回:
        dict: 写入的 WCS 关键字字典
    """
    from astropy.io import fits

    logger.info("-" * 40)
    logger.info("写入 WCS 关键字到 FITS 文件")
    logger.info("  文件: %s", fits_path)
    logger.info("  overwrite=%s", overwrite)

    # 解析 result 字段
    cd = list(result.cd)
    crval = list(result.crval)
    crpix = list(result.crpix)
    sip_order = int(result.sip_order)

    ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
    ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')

    # 如果 ctype 为空，根据 sip_order 设置默认值
    if not ctype1:
        ctype1 = "RA---TAN-SIP" if sip_order > 0 else "RA---TAN"
    if not ctype2:
        ctype2 = "DEC--TAN-SIP" if sip_order > 0 else "DEC--TAN"

    logger.info("  CTYPE1=%s, CTYPE2=%s", ctype1, ctype2)
    logger.info("  CRVAL1=%.8f, CRVAL2=%.8f", crval[0], crval[1])
    logger.info("  CRPIX1=%.4f, CRPIX2=%.4f", crpix[0], crpix[1])
    logger.info("  CD1_1=%.10e, CD1_2=%.10e", cd[0], cd[1])
    logger.info("  CD2_1=%.10e, CD2_2=%.10e", cd[2], cd[3])
    logger.info("  SIP_ORDER=%d", sip_order)

    # 构建 WCS 关键字字典（用于返回）
    wcs_dict = {
        "CTYPE1": ctype1,
        "CTYPE2": ctype2,
        "CRVAL1": crval[0],
        "CRVAL2": crval[1],
        "CRPIX1": crpix[0],
        "CRPIX2": crpix[1],
        "CD1_1": cd[0],
        "CD1_2": cd[1],
        "CD2_1": cd[2],
        "CD2_2": cd[3],
        "RADESYS": "ICRS",
        "EQUINOX": 2000.0,
    }

    # 收集 SIP 系数（下三角: i+j <= order, A[i*6+j] 对应 dx^i*dy^j）
    sip_keywords = {}
    if sip_order > 0:
        sip_keywords["A_ORDER"] = sip_order
        sip_keywords["B_ORDER"] = sip_order
        for i in range(sip_order + 1):
            for j in range(sip_order + 1 - i):
                idx = i * 6 + j
                if idx < 36:
                    a_val = float(result.sip_a[idx])
                    b_val = float(result.sip_b[idx])
                    # 跳过 0 0 项（A_0_0=0 是标准约定，但仍写入）
                    sip_keywords["A_%d_%d" % (i, j)] = a_val
                    sip_keywords["B_%d_%d" % (i, j)] = b_val

        logger.info("  SIP A/B 系数: %d 个 (order=%d)",
                    len([k for k in sip_keywords if k.startswith("A_")]), sip_order)

    # 逆向 SIP (AP/BP): world2pix 反投影用, 缺失会导致边缘投影退化到数值迭代
    ap_order = int(result.sip_ap_order)
    if ap_order > 0:
        sip_keywords["AP_ORDER"] = ap_order
        sip_keywords["BP_ORDER"] = ap_order
        for i in range(ap_order + 1):
            for j in range(ap_order + 1 - i):
                idx = i * 6 + j
                if idx < 36:
                    ap_val = float(result.sip_ap[idx])
                    bp_val = float(result.sip_bp[idx])
                    sip_keywords["AP_%d_%d" % (i, j)] = ap_val
                    sip_keywords["BP_%d_%d" % (i, j)] = bp_val

        logger.info("  SIP AP/BP 系数: %d 个 (order=%d)",
                    len([k for k in sip_keywords if k.startswith("AP_")]), ap_order)
    else:
        logger.warning("  逆向 SIP (AP/BP) 缺失: sip_ap_order=%d, world2pix 将退化到数值迭代", ap_order)

    # 如果不覆盖，直接返回 WCS 字典，不写入文件
    if not overwrite:
        logger.info("  overwrite=False，不修改文件，仅返回 WCS 字典")
        wcs_dict.update(sip_keywords)
        return wcs_dict

    # 写入 FITS 文件头
    with fits.open(fits_path, mode='update') as hdul:
        header = hdul[0].header

        # 1. 删除已有的 WCS 关键字
        _remove_wcs_keywords(header)

        # 2. 写入基础 WCS 关键字
        header["CTYPE1"] = (ctype1, "WCS projection axis 1")
        header["CTYPE2"] = (ctype2, "WCS projection axis 2")
        header["CRVAL1"] = (float(crval[0]), "Reference RA (deg)")
        header["CRVAL2"] = (float(crval[1]), "Reference Dec (deg)")
        header["CRPIX1"] = (float(crpix[0]), "Reference pixel X (1-based)")
        header["CRPIX2"] = (float(crpix[1]), "Reference pixel Y (1-based)")
        header["CD1_1"] = (float(cd[0]), "CD matrix element 1_1 (deg/pix)")
        header["CD1_2"] = (float(cd[1]), "CD matrix element 1_2 (deg/pix)")
        header["CD2_1"] = (float(cd[2]), "CD matrix element 2_1 (deg/pix)")
        header["CD2_2"] = (float(cd[3]), "CD matrix element 2_2 (deg/pix)")
        header["RADESYS"] = ("ICRS", "Coordinate reference system")
        header["EQUINOX"] = (2000.0, "Equinox (year)")

        # 3. 写入 SIP 系数
        if sip_order > 0:
            header["A_ORDER"] = (sip_order, "SIP forward polynomial order (A)")
            header["B_ORDER"] = (sip_order, "SIP forward polynomial order (B)")

            for i in range(sip_order + 1):
                for j in range(sip_order + 1 - i):
                    idx = i * 6 + j
                    if idx < 36:
                        a_val = float(result.sip_a[idx])
                        b_val = float(result.sip_b[idx])
                        header["A_%d_%d" % (i, j)] = (a_val, "SIP A coefficient")
                        header["B_%d_%d" % (i, j)] = (b_val, "SIP B coefficient")

            # 逆向 SIP (AP/BP): world2pix 反投影用
            ap_order_write = int(result.sip_ap_order)
            if ap_order_write > 0:
                header["AP_ORDER"] = (ap_order_write, "SIP inverse polynomial order (AP)")
                header["BP_ORDER"] = (ap_order_write, "SIP inverse polynomial order (BP)")
                for i in range(ap_order_write + 1):
                    for j in range(ap_order_write + 1 - i):
                        idx = i * 6 + j
                        if idx < 36:
                            ap_val = float(result.sip_ap[idx])
                            bp_val = float(result.sip_bp[idx])
                            header["AP_%d_%d" % (i, j)] = (ap_val, "SIP AP coefficient")
                            header["BP_%d_%d" % (i, j)] = (bp_val, "SIP BP coefficient")

        # 4. 刷新写入磁盘
        hdul.flush()

    logger.info("  WCS 关键字写入完成")

    # 合并 SIP 关键字到返回字典
    wcs_dict.update(sip_keywords)
    return wcs_dict


# ============================ 主函数 ============================

def solve_and_write_wcs(fits_path, ra0=0.0, dec0=0.0, focal_length=0.0,
                        pixel_size=0.0, overwrite=True, env=None):
    """对单个 FITS 文件执行 plate solving 并写入 WCS 关键字

    流程:
        1. 读取 FITS 头获取 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ
        2. 初始化 GaiaClient + StarDetector + IPVSolver
        3. 调用 solver.solve() 完成解析
        4. 如果成功，将 WCS 关键字写入 FITS 文件头
        5. 返回结果字典

    参数:
        fits_path: FITS 文件路径
        ra0: 备用 RA (度)，传入非零值优先使用 (默认 0.0)
        dec0: 备用 Dec (度)，传入非零值优先使用 (默认 0.0)
        focal_length: 备用焦距 mm，传入非零值优先使用 (默认 0.0)
        pixel_size: 备用像素尺寸 um，传入非零值优先使用 (默认 0.0)
        overwrite: True=覆盖原文件写入WCS, False=不修改文件 (默认 True)
        env: 可选，(gaia_client, sdet, solver) 元组。如果提供则复用，
             None 则创建新实例并在完成后关闭。

    返回:
        dict: {
            success: bool,        # 求解是否成功
            rms_px: float,        # RMS (像素)
            rms_arcsec: float,    # RMS (角秒)
            n_pairs: int,         # 匹配对数
            wcs_json: dict,       # WCS 参数字典 (含 cd/crval/crpix/sip 等)
            error: str or None,   # 错误信息
        }
    """
    logger.info("=" * 60)
    logger.info("solve_and_write_wcs 启动")
    logger.info("  文件: %s", fits_path)
    logger.info("  ra0=%s, dec0=%s, focal_length=%s, pixel_size=%s, overwrite=%s",
                ra0, dec0, focal_length, pixel_size, overwrite)

    # 校验输入文件
    if not os.path.isfile(fits_path):
        err = "FITS 文件不存在: %s" % fits_path
        logger.error(err)
        return {
            "success": False, "rms_px": 0.0, "rms_arcsec": 0.0,
            "n_pairs": 0, "wcs_json": None, "error": err,
        }

    # 判断是否使用外部环境
    use_external_env = env is not None
    if use_external_env:
        gaia_client, sdet, solver = env
        logger.info("使用外部传入的环境实例")
    else:
        try:
            gaia_client, sdet, solver = init_environment()
        except Exception as e:
            err = "环境初始化失败: %s" % e
            logger.error(err, exc_info=True)
            return {
                "success": False, "rms_px": 0.0, "rms_arcsec": 0.0,
                "n_pairs": 0, "wcs_json": None, "error": err,
            }

    try:
        # 1. 读取 FITS 头
        header_info = read_fits_header(
            fits_path, default_ra0=ra0, default_dec0=dec0,
            default_focal_length=focal_length, default_pixel_size=pixel_size,
        )
        ra0_eff = header_info["ra0"]
        dec0_eff = header_info["dec0"]
        focal_length_eff = header_info["focal_length"]
        pixel_size_eff = header_info["pixel_size"]

        # 2. 调用 solver.solve()
        logger.info("-" * 40)
        logger.info("调用 solver.solve() ...")
        logger.info("  ra0=%.6f 度, dec0=%.6f 度", ra0_eff, dec0_eff)
        logger.info("  focal_length=%.2f mm, pixel_size=%.4f um",
                    focal_length_eff, pixel_size_eff)

        try:
            result = solver.solve(
                fits_path, ra0_eff, dec0_eff, focal_length_eff, pixel_size_eff
            )
        except Exception as e:
            err = "solver.solve() 异常: %s" % e
            logger.error(err, exc_info=True)
            return {
                "success": False, "rms_px": 0.0, "rms_arcsec": 0.0,
                "n_pairs": 0, "wcs_json": None, "error": err,
            }

        logger.info("solver.solve() 返回: success=%d", result.success)
        logger.info("  rms_px=%.4f, rms_arcsec=%.4f", result.rms_px, result.rms_arcsec)
        logger.info("  n_pairs=%d, n_detected=%d, n_catalog=%d",
                    result.n_pairs, result.n_detected, result.n_catalog)
        if result.error_msg:
            err_msg = result.error_msg.decode("utf-8", errors="ignore").strip()
            if err_msg:
                logger.info("  error_msg=%s", err_msg)

        # 3. 检查求解结果
        if result.success == 0:
            err_msg = result.error_msg.decode("utf-8", errors="ignore").strip()
            err = "plate solving 失败: %s" % (err_msg if err_msg else "未收敛")
            logger.warning(err)
            # 不修改 FITS 文件头
            logger.info("success=0，不修改 FITS 文件头")
            return {
                "success": False,
                "rms_px": float(result.rms_px),
                "rms_arcsec": float(result.rms_arcsec),
                "n_pairs": int(result.n_pairs),
                "wcs_json": None,
                "error": err,
            }

        # 4. 写入 WCS 关键字到 FITS 文件
        wcs_dict = write_wcs_to_fits(fits_path, result, overwrite=overwrite)

        # 5. 构建 wcs_json 返回字典
        cd = list(result.cd)
        crval = list(result.crval)
        crpix = list(result.crpix)
        ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
        ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
        if not ctype1:
            ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
        if not ctype2:
            ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"

        wcs_json = {
            "success": True,
            "cd1_1": cd[0],
            "cd1_2": cd[1],
            "cd2_1": cd[2],
            "cd2_2": cd[3],
            "crval1": crval[0],
            "crval2": crval[1],
            "crpix1": crpix[0],
            "crpix2": crpix[1],
            "ctype1": ctype1,
            "ctype2": ctype2,
            "sip_order": int(result.sip_order),
            "sip_a": list(result.sip_a),
            "sip_b": list(result.sip_b),
            "rms_px": float(result.rms_px),
            "rms_arcsec": float(result.rms_arcsec),
            "n_pairs": int(result.n_pairs),
            "n_detected": int(result.n_detected),
            "n_catalog": int(result.n_catalog),
            "best_inliers": int(result.best_inliers),
            "trans_order": int(result.trans_order),
        }

        logger.info("-" * 40)
        logger.info("solve_and_write_wcs 完成")
        logger.info("  success=True, rms_px=%.4f, rms_arcsec=%.4f, n_pairs=%d",
                    result.rms_px, result.rms_arcsec, result.n_pairs)
        logger.info("=" * 60)

        return {
            "success": True,
            "rms_px": float(result.rms_px),
            "rms_arcsec": float(result.rms_arcsec),
            "n_pairs": int(result.n_pairs),
            "wcs_json": wcs_json,
            "error": None,
        }

    finally:
        # 仅在使用内部环境时关闭资源
        if not use_external_env:
            _close_environment(gaia_client, sdet, solver)


# ============================ 批量函数 ============================

# 线程本地存储：每线程复用环境实例
_thread_local = threading.local()


def _get_thread_env():
    """获取当前线程的环境实例（线程本地存储）

    每个线程首次调用时创建独立实例，后续复用。

    返回:
        tuple: (gaia_client, sdet, solver)
    """
    if not hasattr(_thread_local, 'env'):
        logger.info("[线程 %s] 创建线程本地环境实例", threading.current_thread().name)
        _thread_local.env = init_environment()
    return _thread_local.env


def _cleanup_thread_env():
    """清理当前线程的环境实例"""
    if hasattr(_thread_local, 'env'):
        gaia_client, sdet, solver = _thread_local.env
        _close_environment(gaia_client, sdet, solver)
        del _thread_local.env


def _batch_worker(fits_path, ra0, dec0, focal_length, pixel_size, overwrite):
    """批量处理的工作线程函数

    使用线程本地环境实例，每帧独立处理。

    参数:
        fits_path: FITS 文件路径
        ra0: 备用 RA (度)
        dec0: 备用 Dec (度)
        focal_length: 备用焦距 (mm)
        pixel_size: 备用像素尺寸 (um)
        overwrite: 是否覆盖原文件

    返回:
        dict: solve_and_write_wcs 的返回结果
    """
    thread_name = threading.current_thread().name
    logger.info("[线程 %s] 处理文件: %s", thread_name, fits_path)
    try:
        env = _get_thread_env()
        result = solve_and_write_wcs(
            fits_path, ra0=ra0, dec0=dec0,
            focal_length=focal_length, pixel_size=pixel_size,
            overwrite=overwrite, env=env,
        )
        result["fits_path"] = fits_path
        return result
    except Exception as e:
        err = "[线程 %s] 处理异常: %s" % (thread_name, e)
        logger.error(err, exc_info=True)
        return {
            "success": False, "rms_px": 0.0, "rms_arcsec": 0.0,
            "n_pairs": 0, "wcs_json": None, "error": err,
            "fits_path": fits_path,
        }


def solve_batch(fits_paths, ra0=0.0, dec0=0.0, focal_length=0.0,
                pixel_size=0.0, overwrite=True, max_workers=16):
    """批量对多个 FITS 文件执行 plate solving 并写入 WCS

    使用 ThreadPoolExecutor 并行处理，每线程独立 GaiaClient/StarDetector/Solver 实例。
    开发环境 16 线程 CPU，默认 max_workers=16。

    参数:
        fits_paths: FITS 文件路径列表
        ra0: 备用 RA (度)，传入非零值优先使用 (默认 0.0)
        dec0: 备用 Dec (度)，传入非零值优先使用 (默认 0.0)
        focal_length: 备用焦距 mm，传入非零值优先使用 (默认 0.0)
        pixel_size: 备用像素尺寸 um，传入非零值优先使用 (默认 0.0)
        overwrite: True=覆盖原文件写入WCS (默认 True)
        max_workers: 最大并行线程数 (默认 16)

    返回:
        list[dict]: 每个文件的结果列表，每项格式同 solve_and_write_wcs 返回值，
                   额外包含 fits_path 字段
    """
    logger.info("=" * 60)
    logger.info("solve_batch 启动")
    logger.info("  文件数: %d", len(fits_paths))
    logger.info("  max_workers=%d, overwrite=%s", max_workers, overwrite)

    if not fits_paths:
        logger.warning("文件列表为空")
        return []

    results = []
    n_total = len(fits_paths)

    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
        # 提交所有任务
        future_to_path = {
            executor.submit(
                _batch_worker, path, ra0, dec0, focal_length, pixel_size, overwrite
            ): path
            for path in fits_paths
        }

        # 按完成顺序收集结果
        for i, future in enumerate(concurrent.futures.as_completed(future_to_path), 1):
            path = future_to_path[future]
            try:
                result = future.result()
            except Exception as e:
                err = "线程异常: %s" % e
                logger.error("[文件 %s] %s", path, err, exc_info=True)
                result = {
                    "success": False, "rms_px": 0.0, "rms_arcsec": 0.0,
                    "n_pairs": 0, "wcs_json": None, "error": err,
                    "fits_path": path,
                }
            results.append(result)

            # 进度日志
            n_success = sum(1 for r in results if r.get("success"))
            logger.info("[进度 %d/%d] 成功=%d, 失败=%d, 当前文件=%s",
                        i, n_total, n_success, i - n_success, path)

    # 清理所有线程的环境实例
    _cleanup_thread_env()

    # 统计
    n_success = sum(1 for r in results if r.get("success"))
    n_fail = n_total - n_success
    logger.info("-" * 40)
    logger.info("solve_batch 完成: 总数=%d, 成功=%d, 失败=%d",
                n_total, n_success, n_fail)
    logger.info("=" * 60)

    return results


# ============================ 命令行入口 ============================

def main():
    """命令行入口

    参数:
        --fits: FITS文件路径（必填）
        --ra0: 备用RA（度，默认0）
        --dec0: 备用Dec（度，默认0）
        --focal-length: 备用焦距mm（默认0）
        --pixel-size: 备用像素尺寸um（默认0）
        --no-overwrite: 不覆盖原文件（默认覆盖）
    """
    parser = argparse.ArgumentParser(
        description="Plate Solve and Write WCS: 调用IPVSolver求解WCS并写入FITS文件头",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
    # 基本用法（从FITS头读取参数）
    python solve_and_write_wcs.py --fits image.fits

    # 指定备用参数
    python solve_and_write_wcs.py --fits image.fits --ra0 12.34 --dec0 56.78 \\
        --focal-length 200 --pixel-size 6.0

    # 不覆盖原文件（仅求解，不写入）
    python solve_and_write_wcs.py --fits image.fits --no-overwrite

    # 批量处理
    python solve_and_write_wcs.py --fits file1.fits file2.fits file3.fits
        """,
    )
    parser.add_argument("--fits", required=True, nargs='+',
                        help="FITS文件路径（必填，支持多个文件批量处理）")
    parser.add_argument("--ra0", type=float, default=0.0,
                        help="备用RA（度，默认0，传入非零值优先于FITS头）")
    parser.add_argument("--dec0", type=float, default=0.0,
                        help="备用Dec（度，默认0，传入非零值优先于FITS头）")
    parser.add_argument("--focal-length", type=float, default=0.0,
                        help="备用焦距mm（默认0，传入非零值优先于FITS头）")
    parser.add_argument("--pixel-size", type=float, default=0.0,
                        help="备用像素尺寸um（默认0，传入非零值优先于FITS头）")
    parser.add_argument("--no-overwrite", action="store_true",
                        help="不覆盖原文件（默认覆盖，即修改原FITS文件头）")
    parser.add_argument("--max-workers", type=int, default=16,
                        help="批量处理最大线程数（默认16）")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    overwrite = not args.no_overwrite
    fits_paths = args.fits

    if len(fits_paths) == 1:
        # 单文件处理
        result = solve_and_write_wcs(
            fits_path=fits_paths[0],
            ra0=args.ra0,
            dec0=args.dec0,
            focal_length=args.focal_length,
            pixel_size=args.pixel_size,
            overwrite=overwrite,
        )
    else:
        # 批量处理
        result = solve_batch(
            fits_paths=fits_paths,
            ra0=args.ra0,
            dec0=args.dec0,
            focal_length=args.focal_length,
            pixel_size=args.pixel_size,
            overwrite=overwrite,
            max_workers=args.max_workers,
        )

    # 输出 JSON 到 stdout
    print(json.dumps(result, ensure_ascii=True, default=str, indent=2))
    return 0 if (result.get("success") if isinstance(result, dict) else
                 any(r.get("success") for r in result)) else 1


if __name__ == "__main__":
    sys.exit(main())
