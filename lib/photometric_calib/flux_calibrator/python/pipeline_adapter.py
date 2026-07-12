# -*- coding: utf-8 -*-
"""
Photometric Calib 管线适配器 (简化版 C++ DLL)
功能: 将 C++ photometric_calib.dll 包装为 PipelineStageHandler，从 PipelineFrame 命名块
      读取 data/header/gaia_cat/psf，调用 pc_calibrate_simple 全局 scale 校正，
      校正后替换 data 块并输出 photo_stats KV 块
用途: 在管线引擎中注册 STAGE_PHOTOMETRIC 阶段处理器，实现内存管线数据直通
依赖: numpy; astro_image_io (PipelineFramePy); 同目录 fsyn_loader;
      lib/photometric_calib/python/photometric_calib.py (C++ DLL 封装)
调用:
    from pipeline_adapter import PhotometricParams, register_photometric_handler
    params = PhotometricParams(f_syn_path="f_syn.json", log_dir="logs/photometric")
    register_photometric_handler(engine, params)

变更说明 (2026-07-12):
  - 去掉 GradientEstimator 调用 (梯度曲面拟合)
  - 去掉 grad_map 块生成 (M_map 曲面)
  - 改为调用 C++ DLL pc_calibrate_simple 全局 scale 校正
  - 保留 photo_stats KV 块 (N_MATCHED, SCALE_FACTOR)
  - 仍然从 data/gaia_cat/psf 块读取数据
  - 仍然从 header KV 读取 WCS/SIP 参数
"""

from __future__ import annotations

import logging
import os
import sys
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

# ---- 依赖路径配置 ----
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
# astro_image_io: flux_calibrator/python -> photometric_calib -> lib -> astro_image_io/python
_AIO_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "astro_image_io", "python"))
if _AIO_PATH not in sys.path:
    sys.path.insert(0, _AIO_PATH)
# photometric_calib/python (C++ DLL封装): flux_calibrator/python -> photometric_calib/python
_PC_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "python"))
if _PC_PATH not in sys.path:
    sys.path.insert(0, _PC_PATH)
# 同目录 (fsyn_loader)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

from astro_image_io import (  # noqa: E402
    PipelineFramePy, PipelineStageHandlerC, STAGE_PHOTOMETRIC,
)
from photometric_calib import PhotometricCalib  # noqa: E402

# 惰性导入 FSynLoader (同目录, 可能因依赖缺失而失败)
try:
    from fsyn_loader import FSynLoader  # noqa: E402
except ImportError:
    FSynLoader = None

logger = logging.getLogger(__name__)


# ============================================================================
# 参数定义
# ============================================================================

@dataclass
class PhotometricParams:
    """光度定标阶段参数（通过闭包传递给 handler）

    Attributes:
        match_radius_px: 星-图匹配半径 (像素) - C++ DLL固定3.0, 此参数仅用于日志
        outlier_sigma: 离群点清洗 sigma 阈值 - C++ DLL固定3.0, 此参数仅用于日志
        log_dir: 日志目录, None 仅输出到控制台
        f_syn_path: F_syn JSON 文件路径 (光谱积分器输出);
                    存在时从 JSON 加载含 ra/dec/mag_g/f_syn/source_id 的完整星表
        f_syn_loader: 外部注入的 f_syn 加载器对象, 需实现
                      get_f_syn(ra, dec, mag_g) -> float;
                      f_syn_path 为 None 时逐星调用获取 f_syn
        dll_path: photometric_calib.dll 路径, None 时自动查找
    """
    match_radius_px: float = 3.0
    outlier_sigma: float = 3.0
    log_dir: Optional[str] = None
    f_syn_path: Optional[str] = None
    f_syn_loader: Optional[object] = None
    dll_path: Optional[str] = None


# ============================================================================
# 辅助函数: 从 PipelineFrame 命名块读取数据
# ============================================================================

def _read_sip_coeffs(frame: PipelineFramePy,
                     order_key: str, coeff_prefix: str):
    """从 header KV 块读取 SIP 多项式系数

    Args:
        frame: PipelineFramePy 实例
        order_key: 阶数 KV key (如 "A_ORDER")
        coeff_prefix: 系数前缀 (如 "A", "B", "AP", "BP")

    Returns:
        (order, coeffs): order=0 且 coeffs=None 表示无该组 SIP 系数;
        coeffs 为长度 36 的扁平数组 (按 i*6+j 索引, 与 C++ DLL 一致)
    """
    order_str = frame.kv_get("header", order_key)
    if order_str is None:
        return 0, None
    try:
        order = int(order_str)
    except (ValueError, TypeError):
        return 0, None
    if order <= 0:
        return 0, None

    coeffs = np.zeros(36, dtype=np.float64)
    found = 0
    for i in range(order + 1):
        for j in range(order + 1 - i):
            key = f"{coeff_prefix}_{i}_{j}"
            val = frame.kv_get("header", key)
            if val is not None:
                try:
                    coeffs[i * 6 + j] = float(val)
                    found += 1
                except (ValueError, TypeError):
                    pass
    if found == 0:
        return 0, None
    return order, coeffs


def _read_wcs_params(frame: PipelineFramePy) -> dict:
    """从 header KV 块读取 WCS/SIP 原始参数 (供 C++ DLL 使用)

    Args:
        frame: PipelineFramePy 实例

    Returns:
        dict 含 crval1/2, crpix1/2, cd11/12/21/22,
        sip_order, sip_a/b/ap/bp (None 表示无)

    Raises:
        ValueError: 缺少必要 WCS 关键字 (CRVAL/CRPIX 全为 0)
    """
    crval1 = frame.kv_get_double("header", "CRVAL1", 0.0)
    crval2 = frame.kv_get_double("header", "CRVAL2", 0.0)
    crpix1 = frame.kv_get_double("header", "CRPIX1", 0.0)
    crpix2 = frame.kv_get_double("header", "CRPIX2", 0.0)
    cd11 = frame.kv_get_double("header", "CD1_1", 0.0)
    cd12 = frame.kv_get_double("header", "CD1_2", 0.0)
    cd21 = frame.kv_get_double("header", "CD2_1", 0.0)
    cd22 = frame.kv_get_double("header", "CD2_2", 0.0)

    if crval1 == 0.0 and crval2 == 0.0 and crpix1 == 0.0 and crpix2 == 0.0:
        raise ValueError(
            "header KV 块缺少 WCS 关键字 (CRVAL/CRPIX 全为 0), 无法进行测光校准")

    # SIP 系数
    a_order, sip_a = _read_sip_coeffs(frame, "A_ORDER", "A")
    b_order, sip_b = _read_sip_coeffs(frame, "B_ORDER", "B")
    sip_order = max(a_order, b_order)
    ap_order, sip_ap = _read_sip_coeffs(frame, "AP_ORDER", "AP")
    bp_order, sip_bp = _read_sip_coeffs(frame, "BP_ORDER", "BP")
    # AP/BP 都有时才传, 否则 C++ 端用迭代法
    has_ap = (ap_order > 0 and bp_order > 0)

    logger.info(
        "WCS 参数: CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f), "
        "CD=[[%.6e, %.6e], [%.6e, %.6e]], SIP order=%d, AP=%s",
        crval1, crval2, crpix1, crpix2,
        cd11, cd12, cd21, cd22, sip_order, has_ap)

    return {
        "crval1": crval1, "crval2": crval2,
        "crpix1": crpix1, "crpix2": crpix2,
        "cd11": cd11, "cd12": cd12, "cd21": cd21, "cd22": cd22,
        "sip_order": sip_order,
        "sip_a": sip_a if sip_order > 0 else None,
        "sip_b": sip_b if sip_order > 0 else None,
        "sip_ap": sip_ap if has_ap else None,
        "sip_bp": sip_bp if has_ap else None,
    }


def _build_gaia_arrays(frame: PipelineFramePy,
                       params: PhotometricParams) -> Tuple[
                           np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """构造 Gaia 星 numpy 数组 (含 f_syn 合成流量)

    f_syn 获取优先级:
        1. f_syn_path: 从 JSON 加载 (含 ra/dec/mag_g/f_syn/source_id 完整字段)
        2. f_syn_loader: 从 gaia_cat 块读取 ra/dec/mag_g, 逐星调用 loader 获取 f_syn
        3. 无 f_syn 来源: 返回空数组

    Args:
        frame: PipelineFramePy 实例
        params: PhotometricParams 参数

    Returns:
        (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn) float64 数组, 可能为空
    """
    # ---- 优先级 1: f_syn_path JSON ----
    if params.f_syn_path and FSynLoader is not None:
        try:
            stars = FSynLoader.load(params.f_syn_path)
            logger.info("从 JSON 加载 f_syn: %s, %d 颗星",
                        params.f_syn_path, len(stars))
            if len(stars) == 0:
                return (np.array([], dtype=np.float64),
                        np.array([], dtype=np.float64),
                        np.array([], dtype=np.float64),
                        np.array([], dtype=np.float64))
            ra = np.array([s["ra"] for s in stars], dtype=np.float64)
            dec = np.array([s["dec"] for s in stars], dtype=np.float64)
            mag = np.array([s["mag_g"] for s in stars], dtype=np.float64)
            fsyn = np.array([s["f_syn"] for s in stars], dtype=np.float64)
            return ra, dec, mag, fsyn
        except (FileNotFoundError, ValueError) as e:
            logger.warning("加载 f_syn JSON 失败: %s (%s), 回退到 gaia_cat 块",
                           params.f_syn_path, e)

    # ---- 从 gaia_cat 块读取 ----
    gaia_cat = frame.get_block_data("gaia_cat")
    if gaia_cat is None or gaia_cat.size == 0:
        logger.warning("gaia_cat 块不存在或为空, 且无可用 f_syn 来源")
        return (np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64))

    if gaia_cat.ndim != 2 or gaia_cat.shape[1] < 3:
        logger.warning("gaia_cat 块格式异常: shape=%s, 期望 (N, 3) [ra,dec,mag_g]",
                        gaia_cat.shape)
        return (np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64))

    n_stars = gaia_cat.shape[0]
    ra = np.ascontiguousarray(gaia_cat[:, 0], dtype=np.float64)
    dec = np.ascontiguousarray(gaia_cat[:, 1], dtype=np.float64)
    mag = np.ascontiguousarray(gaia_cat[:, 2], dtype=np.float64)
    logger.info("从 gaia_cat 块读取 %d 颗星 (ra/dec/mag_g)", n_stars)

    # ---- 优先级 2: f_syn_loader 逐星获取 ----
    if params.f_syn_loader is not None:
        fsyn = np.zeros(n_stars, dtype=np.float64)
        n_valid = 0
        for i in range(n_stars):
            try:
                fsyn[i] = float(params.f_syn_loader.get_f_syn(
                    float(ra[i]), float(dec[i]), float(mag[i])))
            except Exception as e:
                logger.warning("f_syn_loader 获取 f_syn 失败 (星 %d): %s", i, e)
            if fsyn[i] > 0.0:
                n_valid += 1
        logger.info("f_syn_loader 获取完成: %d/%d 颗有效", n_valid, n_stars)
        return ra, dec, mag, fsyn

    # ---- 优先级 3: 无 f_syn 来源 ----
    logger.warning("无 f_syn 来源 (f_syn_path/f_syn_loader 均未提供), 返回空")
    return (np.array([], dtype=np.float64),
            np.array([], dtype=np.float64),
            np.array([], dtype=np.float64),
            np.array([], dtype=np.float64))


def _build_psf_arrays(frame: PipelineFramePy) -> Tuple[
        np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """从 psf 块读取 PSF 拟合结果并转为 numpy 数组

    psf 块格式: FLOAT64[N, 6], 每行 [status, B, flux, cx, cy, fwhm]

    Args:
        frame: PipelineFramePy 实例

    Returns:
        (psf_cx, psf_cy, psf_flux, psf_status) 数组, 可能为空
        psf_status 为 int32, 其余为 float64
    """
    psf_data = frame.get_block_data("psf")
    if psf_data is None or psf_data.size == 0:
        logger.warning("psf 块不存在或为空")
        return (np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.int32))

    if psf_data.ndim != 2 or psf_data.shape[1] < 5:
        logger.warning("psf 块格式异常: shape=%s, 期望 (N, 6)",
                        psf_data.shape)
        return (np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.int32))

    n = psf_data.shape[0]
    psf_status = np.ascontiguousarray(psf_data[:, 0], dtype=np.int32)
    psf_cx = np.ascontiguousarray(psf_data[:, 3], dtype=np.float64)
    psf_cy = np.ascontiguousarray(psf_data[:, 4], dtype=np.float64)
    psf_flux = np.ascontiguousarray(psf_data[:, 2], dtype=np.float64)
    n_ok = int(np.sum(psf_status == 0))
    logger.info("psf 块读取完成: %d 颗, 成功(status=0) %d 颗", n, n_ok)
    return psf_cx, psf_cy, psf_flux, psf_status


# ============================================================================
# 管线适配器注册
# ============================================================================

def register_photometric_handler(engine, params: PhotometricParams):
    """注册光度定标阶段处理器到管线引擎

    Args:
        engine: PipelineEngine 实例
        params: PhotometricParams 参数 (含匹配参数/日志目录/f_syn 来源)
    """
    # 加载 C++ DLL
    pc_dll = PhotometricCalib(params.dll_path)
    logger.info("C++ photometric_calib.dll 已加载")

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        """STAGE_PHOTOMETRIC 处理回调

        流程:
          1. 从命名块读取 data/gaia_cat/psf
          2. 从 header KV 读取 WCS/SIP 参数
          3. 构造 gaia/psf numpy 数组
          4. 调用 C++ pc_calibrate_simple 全局 scale 校正
          5. 替换 data 块为校正后图像
          6. 添加 photo_stats KV 块 (N_MATCHED, SCALE_FACTOR)
        """
        frame = PipelineFramePy.from_c_ptr(c_frame_ptr)
        try:
            logger.info("=" * 60)
            logger.info("光度定标阶段开始 (STAGE_PHOTOMETRIC, C++ DLL)")
            logger.info("=" * 60)

            # 1. 读取像素数据
            pixels = frame.get_block_data("data")
            if pixels is None:
                raise ValueError("data 块不存在, 无法进行光度定标")
            logger.info("读取 data 块: shape=%s, dtype=%s",
                        pixels.shape, pixels.dtype)

            # 2. 读取 WCS/SIP 参数
            wcs_params = _read_wcs_params(frame)

            # 3. 构造 gaia 数组 (含 f_syn)
            gaia_ra, gaia_dec, gaia_mag, gaia_fsyn = _build_gaia_arrays(
                frame, params)
            logger.info("gaia 星: %d 颗", gaia_ra.size)

            # 4. 构造 psf 数组
            psf_cx, psf_cy, psf_flux, psf_status = _build_psf_arrays(frame)
            logger.info("psf 星: %d 颗", psf_cx.size)

            # 5. 调用 C++ pc_calibrate_simple
            out_pixels, n_matched, scale_factor = pc_dll.calibrate_simple(
                pixels,
                gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
                psf_cx, psf_cy, psf_flux, psf_status,
                wcs_params["crval1"], wcs_params["crval2"],
                wcs_params["crpix1"], wcs_params["crpix2"],
                wcs_params["cd11"], wcs_params["cd12"],
                wcs_params["cd21"], wcs_params["cd22"],
                sip_order=wcs_params["sip_order"],
                sip_a=wcs_params["sip_a"],
                sip_b=wcs_params["sip_b"],
                sip_ap=wcs_params["sip_ap"],
                sip_bp=wcs_params["sip_bp"],
            )
            logger.info(
                "C++ 测光校准完成: n_matched=%d, scale=%.6e",
                n_matched, scale_factor)

            # 6. 替换 data 块为校正后图像
            frame.remove_block("data")
            frame.add_block("data", out_pixels,
                            description="光度定标后图像 (float32, Gaia 参考系统, 全局scale校正)")
            logger.info("data 块已替换为校正后图像: shape=%s, dtype=%s",
                        out_pixels.shape, out_pixels.dtype)

            # 7. 添加 photo_stats KV 块
            frame.kv_set("photo_stats", "N_MATCHED", str(n_matched))
            frame.kv_set("photo_stats", "SCALE_FACTOR",
                         f"{scale_factor:.6e}")
            logger.info("photo_stats KV 块已添加: N_MATCHED=%d, SCALE=%.6e",
                        n_matched, scale_factor)

            logger.info("=" * 60)
            logger.info("光度定标阶段完成 (返回 0)")
            logger.info("=" * 60)
            return 0
        except Exception as e:
            logger.error("光度定标失败: %s", e, exc_info=True)
            if err_buf and err_cap > 0:
                msg = str(e).encode("utf-8")[:err_cap - 1]
                err_buf[:len(msg)] = msg
            return -1

    handler_c = PipelineStageHandlerC(_handler)
    engine.register(STAGE_PHOTOMETRIC, handler_c)
    logger.info("STAGE_PHOTOMETRIC handler 已注册 (C++ DLL): "
                "match_radius=%.2f px, outlier_sigma=%.2f, f_syn_path=%s",
                params.match_radius_px, params.outlier_sigma,
                params.f_syn_path or "(none)")
