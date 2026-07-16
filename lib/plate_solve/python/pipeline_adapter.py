# -*- coding: utf-8 -*-
"""
plate_solve 模块管线适配器 (命名块容器版)
功能: 将 IPVSolver 包装为 PipelineStageHandler
用途: 在管线引擎中注册 STAGE_PLATESOLVE 阶段处理器，通过 PipelineFrame 命名块容器传递数据

数据流:
  输入: frame "data" 块 (FLOAT32[H,W]) + frame "header" KV块 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
  输出: frame "header" KV块追加 WCS/SIP 字段 + frame "star_det" 块 + frame "gaia_cat" 块

注意: 使用 ipv_solve_from_memory 内存接口直接传递像素数据到 C++ DLL，无需临时 FITS 文件。
"""

from __future__ import annotations

import ctypes
import logging
import os
from dataclasses import dataclass
from datetime import datetime
from typing import Optional, Tuple

import numpy as np

from astro_image_io import (
    PipelineFramePy, PipelineStageHandlerC, STAGE_PLATESOLVE,
)

# ============================================================================
# 日志初始化
# ============================================================================

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
        "pipeline_adapter_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("pipeline_adapter")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("plate_solve pipeline_adapter 日志初始化完成: %s", log_file)
    return lg


logger = _init_logger()


# ============================================================================
# RA/DEC 解析函数
# ============================================================================

def _parse_ra(ra_str: str) -> float:
    """解析 RA: 'HH MM SS.S' -> 度

    支持格式: "HH MM SS.S" / "HH:MM:SS.S" / 浮点度数字符串
    """
    s = str(ra_str).strip()
    parts = s.replace(":", " ").split()
    if len(parts) >= 3:
        h, m, sec = float(parts[0]), float(parts[1]), float(parts[2])
        return (h + m / 60.0 + sec / 3600.0) * 15.0
    elif len(parts) == 1:
        return float(parts[0])
    return 0.0


def _parse_dec(dec_str: str) -> float:
    """解析 Dec: 'DD MM SS' -> 度

    支持格式: "±DD MM SS.S" / "±DD:MM:SS.S" / 浮点度数字符串
    """
    s = str(dec_str).strip()
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


# ============================================================================
# Gaia 锥形查询辅助函数
# ============================================================================

def _gaia_cone_search_for_solver(gaia_client, ra: float, dec: float,
                                  radius_deg: float, mag_high: float):
    """调用 gaia_client 的 cone_search_for_solver C API

    返回: (ra_arr, dec_arr, mag_arr) numpy float64 数组, 失败时返回空数组
    """
    dll = gaia_client._dll
    handle = gaia_client._handle

    # 设置函数签名（如果尚未设置）
    if not getattr(dll, "_cone_search_for_solver_configured", False):
        dll.gaia_client_cone_search_for_solver.argtypes = [
            ctypes.c_void_p, ctypes.c_double, ctypes.c_double,
            ctypes.c_double, ctypes.c_double,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
            ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
            ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
            ctypes.POINTER(ctypes.c_int),
        ]
        dll.gaia_client_cone_search_for_solver.restype = ctypes.c_int
        dll._cone_search_for_solver_configured = True

    out_ra = ctypes.POINTER(ctypes.c_double)()
    out_dec = ctypes.POINTER(ctypes.c_double)()
    out_mag = ctypes.POINTER(ctypes.c_float)()
    out_count = ctypes.c_int(0)

    ret = dll.gaia_client_cone_search_for_solver(
        handle, ra, dec, radius_deg, mag_high,
        ctypes.byref(out_ra), ctypes.byref(out_dec),
        ctypes.byref(out_mag), ctypes.byref(out_count),
    )

    if ret != 0 or out_count.value <= 0:
        logger.warning("Gaia 锥形查询返回空: ret=%d, count=%d", ret, out_count.value)
        return (np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64))

    count = out_count.value
    ra_arr = np.array([out_ra[i] for i in range(count)], dtype=np.float64)
    dec_arr = np.array([out_dec[i] for i in range(count)], dtype=np.float64)
    mag_arr = np.array([float(out_mag[i]) for i in range(count)], dtype=np.float64)

    # 释放 C 端内存
    gaia_client._msvcrt.free(out_ra)
    gaia_client._msvcrt.free(out_dec)
    gaia_client._msvcrt.free(out_mag)

    logger.info("Gaia 锥形查询完成: center=(%.4f, %.4f), radius=%.2f°, mag<%.1f, n_stars=%d",
                ra, dec, radius_deg, mag_high, count)
    return ra_arr, dec_arr, mag_arr


# ============================================================================
# 错误缓冲区写入
# ============================================================================

def _write_err(err_buf, err_cap: int, msg: str) -> None:
    """将错误信息写入 err_buf (c_char_p 回调参数)

    注意: c_char_p 在 ctypes 回调中会被转换为 Python bytes (不可变)。
    此函数尝试通过 ctypes.memmove 写入, 失败时静默跳过 (错误已通过 logger 记录)。
    """
    if not err_buf or err_cap <= 0:
        return
    try:
        encoded = msg.encode("utf-8")[:err_cap - 1]
        # err_buf 可能是 int (原始指针) 或 bytes (ctypes 转换后)
        if isinstance(err_buf, int):
            ctypes.memmove(err_buf, encoded, len(encoded))
    except Exception:
        pass


# ============================================================================
# 参数类
# ============================================================================

@dataclass
class PlateSolveParams:
    """求解阶段参数

    env: (gaia_client, sdet, solver) 可复用环境, None 时自动初始化
    gaia_mag_high: Gaia 星表查询星等上限 (默认 18.0)

    注意: 使用 ipv_solve_from_memory 内存接口, 无临时 FITS 文件
    """
    env: Optional[Tuple] = None
    gaia_mag_high: float = 18.0


# ============================================================================
# 注册函数
# ============================================================================

def register_platesolve_handler(engine, params: PlateSolveParams):
    """注册 plate solve 阶段处理器到管线引擎

    engine: PipelineEngine 实例
    params: PlateSolveParams 参数

    用法:
        params = PlateSolveParams()
        register_platesolve_handler(engine, params)
    """
    # 初始化环境（重资源，只创建一次）
    if params.env is None:
        from solve_and_write_wcs import init_environment
        gaia_client, sdet, solver = init_environment()
        env = (gaia_client, sdet, solver)
        logger.info("plate_solve 环境已初始化")
    else:
        env = params.env
        gaia_client, sdet, solver = env

    gaia_mag_high = params.gaia_mag_high

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        frame = PipelineFramePy.from_c_ptr(c_frame_ptr)
        try:
            # 1. 读取像素数据
            pixels = frame.get_block_data("data")
            if pixels is None:
                msg = "frame 缺少 'data' 块"
                logger.error(msg)
                _write_err(err_buf, err_cap, msg)
                return -1
            pixels = np.ascontiguousarray(pixels, dtype=np.float32)
            if pixels.ndim != 2:
                msg = "data 块必须为 2D 数组, 当前 ndim=%d" % pixels.ndim
                logger.error(msg)
                _write_err(err_buf, err_cap, msg)
                return -1
            h, w = pixels.shape
            logger.info("plate_solve 开始: %dx%d", w, h)

            # 2. 从 header KV 块读取初始指向
            if not frame.has_block("header"):
                msg = "frame 缺少 'header' KV 块"
                logger.error(msg)
                _write_err(err_buf, err_cap, msg)
                return -1

            ra_str = frame.kv_get("header", "OBJCTRA") or ""
            dec_str = frame.kv_get("header", "OBJCTDEC") or ""
            focal_length = frame.kv_get_double("header", "FOCALLEN", 0.0)
            pixel_size = frame.kv_get_double("header", "XPIXSZ", 0.0)

            ra0 = _parse_ra(ra_str) if ra_str else 0.0
            dec0 = _parse_dec(dec_str) if dec_str else 0.0

            logger.info("初始指向: OBJCTRA='%s' -> ra0=%.6f°, OBJCTDEC='%s' -> dec0=%.6f°",
                        ra_str, ra0, dec_str, dec0)
            logger.info("焦距=%.2fmm, 像素尺寸=%.4fum", focal_length, pixel_size)

            # 3. 调用 solver.solve_from_memory() (内存接口, 无临时文件)
            result = solver.solve_from_memory(
                pixels, w, h, ra0, dec0, focal_length, pixel_size,
            )
            logger.info("solve_from_memory 调用完成")

            if not result.success:
                msg = result.error_msg.decode("utf-8", errors="replace").strip()
                msg = msg or "求解未收敛"
                logger.error("plate_solve 失败: %s", msg)
                _write_err(err_buf, err_cap, msg)
                return -1

            # 5. 注入 WCS 到 header KV 块
            ctype1 = result.ctype1.decode("utf-8", errors="replace").rstrip("\x00")
            ctype2 = result.ctype2.decode("utf-8", errors="replace").rstrip("\x00")
            if not ctype1:
                ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
            if not ctype2:
                ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"

            frame.kv_set("header", "CTYPE1", ctype1)
            frame.kv_set("header", "CTYPE2", ctype2)
            frame.kv_set_double("header", "CRVAL1", result.crval[0])
            frame.kv_set_double("header", "CRVAL2", result.crval[1])
            frame.kv_set_double("header", "CRPIX1", result.crpix[0])
            frame.kv_set_double("header", "CRPIX2", result.crpix[1])
            frame.kv_set_double("header", "CD1_1", result.cd[0])
            frame.kv_set_double("header", "CD1_2", result.cd[1])
            frame.kv_set_double("header", "CD2_1", result.cd[2])
            frame.kv_set_double("header", "CD2_2", result.cd[3])
            frame.kv_set("header", "RADESYS", "ICRS")
            frame.kv_set_double("header", "EQUINOX", 2000.0)

            logger.info("WCS 已注入: CTYPE1=%s, CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f)",
                        ctype1, result.crval[0], result.crval[1],
                        result.crpix[0], result.crpix[1])

            # 6. 注入 SIP 到 header KV 块 (若 sip_order > 0)
            if result.sip_order > 0:
                order = int(result.sip_order)
                frame.kv_set("header", "A_ORDER", str(order))
                frame.kv_set("header", "B_ORDER", str(order))
                for i in range(order + 1):
                    for j in range(order + 1 - i):
                        idx = i * 6 + j
                        if idx < 36:
                            frame.kv_set_double("header", "A_%d_%d" % (i, j),
                                                result.sip_a[idx])
                            frame.kv_set_double("header", "B_%d_%d" % (i, j),
                                                result.sip_b[idx])

                # 逆向 SIP (AP/BP)
                ap_order = int(result.sip_ap_order)
                if ap_order > 0:
                    frame.kv_set("header", "AP_ORDER", str(ap_order))
                    frame.kv_set("header", "BP_ORDER", str(ap_order))
                    for i in range(ap_order + 1):
                        for j in range(ap_order + 1 - i):
                            idx = i * 6 + j
                            if idx < 36:
                                frame.kv_set_double("header", "AP_%d_%d" % (i, j),
                                                    result.sip_ap[idx])
                                frame.kv_set_double("header", "BP_%d_%d" % (i, j),
                                                    result.sip_bp[idx])

                logger.info("SIP 已注入: order=%d, ap_order=%d", order, ap_order)
            else:
                logger.info("无 SIP 系数 (sip_order=0)")

            # 7. 写入 star_det 块 (FLOAT32[N,4]: x, y, flux, mag)
            try:
                pixels_u16 = np.clip(pixels, 0, 65535).astype(np.uint16)
                det_result = sdet.detect_ex(pixels_u16)
                n_stars = det_result.count
                if n_stars > 0:
                    star_det = np.column_stack([
                        np.array(det_result.x, dtype=np.float32),
                        np.array(det_result.y, dtype=np.float32),
                        np.array(det_result.flux, dtype=np.float32),
                        np.array(det_result.mag, dtype=np.float32),
                    ])
                    frame.add_block("star_det", star_det,
                                    description="星点检测结果: x,y,flux,mag")
                    logger.info("star_det 块已写入: %d 颗星", n_stars)
                else:
                    logger.warning("星点检测未找到星点, star_det 块未写入")
            except Exception as e:
                logger.warning("星点检测失败, star_det 块未写入: %s", e, exc_info=True)

            # 8. 写入 gaia_cat 块 (FLOAT64[N,3]: ra, dec, mag)
            try:
                # 使用求解后的 WCS 中心计算 FOV 半径
                cd11, cd12, cd21, cd22 = result.cd
                det_cd = abs(cd11 * cd22 - cd12 * cd21)
                pixel_scale_deg = np.sqrt(det_cd) if det_cd > 0 else 0.0
                # 半对角线 + 20% 余量
                fov_radius_deg = pixel_scale_deg * np.sqrt(w**2 + h**2) / 2.0 * 1.2

                if 0 < fov_radius_deg < 30.0:
                    ra_arr, dec_arr, mag_arr = _gaia_cone_search_for_solver(
                        gaia_client,
                        result.crval[0], result.crval[1],
                        fov_radius_deg, gaia_mag_high,
                    )
                    if len(ra_arr) > 0:
                        gaia_cat = np.column_stack([ra_arr, dec_arr, mag_arr])
                        frame.add_block("gaia_cat", gaia_cat,
                                        description="Gaia 星表: ra,dec,mag")
                        logger.info("gaia_cat 块已写入: %d 颗星, FOV半径=%.2f°",
                                    len(ra_arr), fov_radius_deg)
                    else:
                        logger.warning("Gaia 查询返回空, gaia_cat 块未写入")
                else:
                    logger.warning("FOV 半径异常 (%.4f°), gaia_cat 块未写入", fov_radius_deg)
            except Exception as e:
                logger.warning("Gaia 查询失败, gaia_cat 块未写入: %s", e, exc_info=True)

            logger.info("plate_solve 成功: rms=%.4f\", n_pairs=%d, n_detected=%d, n_catalog=%d",
                        result.rms_arcsec, result.n_pairs,
                        result.n_detected, result.n_catalog)
            return 0

        except Exception as e:
            logger.error("plate_solve 异常: %s", e, exc_info=True)
            _write_err(err_buf, err_cap, str(e))
            return -1

    handler_c = PipelineStageHandlerC(_handler)
    engine.register(STAGE_PLATESOLVE, handler_c)
