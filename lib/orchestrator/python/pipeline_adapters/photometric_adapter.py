# -*- coding: utf-8 -*-
"""
Photometric Calib 管线适配器 (简化版 C++ DLL)
功能: 将 C++ photometric_calib.dll 包装为 PipelineStageHandler，从 PipelineFrame 命名块
      读取 data/header/psf，调用 pc_calibrate_simple_with_gaia (优先) 或
      pc_calibrate_simple (fallback) 进行全局 scale 校正，
      校正后替换 data 块并输出 photo_stats KV 块
用途: 在管线引擎中注册 STAGE_PHOTOMETRIC 阶段处理器，实现内存管线数据直通

本文件从 lib/photometric_calib/flux_calibrator/python/pipeline_adapter.py 迁移而来。
依赖路径已更新为从 orchestrator/python/pipeline_adapters/ 出发。

两种校准模式:
  1. 新模式 (gaia_client 不为 None): 调用 pc_calibrate_simple_with_gaia
     - DLL 内部完成: 锥形搜索 Gaia DR3SP -> BP/RP 光谱积分 F_syn ->
       WCS 投影 -> KDTree 匹配 PSF 星 -> MAD 清洗 -> 全局 scale 校正
     - 需要: gaia_client handle + 滤光片曲线 + 光谱波长数组
  2. fallback (gaia_client 为 None): 调用 pc_calibrate_simple
     - 从 gaia_cat 块读取 ra/dec/mag, f_syn=0 (退化路径, n_matched=0, scale=1.0)

依赖: numpy; astro_image_io (PipelineFramePy); photometric_calib/python/photometric_calib.py (C++ DLL 封装);
      photometric_calib/spectrum_integrator/python/curve_loader.py (CurveLoader)
调用:
    from photometric_adapter import PhotometricParams, register_photometric_handler
    params = PhotometricParams(gaia_client=gaia_client, log_dir="logs/photometric")
    register_photometric_handler(engine, params)
"""

from __future__ import annotations

import logging
import os
import sys
from dataclasses import dataclass, field
from typing import Optional, Tuple

import numpy as np

# ---- 依赖路径配置 ----
# 新位置: orchestrator/python/pipeline_adapters/photometric_adapter.py
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
# astro_image_io: pipeline_adapters -> python -> orchestrator -> lib -> astro_image_io/python
_AIO_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "..", "astro_image_io", "python"))
if _AIO_PATH not in sys.path:
    sys.path.insert(0, _AIO_PATH)
# photometric_calib/python (C++ DLL封装): pipeline_adapters -> python -> orchestrator -> lib -> photometric_calib/python
_PC_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "..", "photometric_calib", "python"))
if _PC_PATH not in sys.path:
    sys.path.insert(0, _PC_PATH)
# curve_loader 所在目录 (photometric_calib/spectrum_integrator/python)
_CURVE_LOADER_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "..", "photometric_calib", "spectrum_integrator", "python"))
if _CURVE_LOADER_PATH not in sys.path:
    sys.path.insert(0, _CURVE_LOADER_PATH)
# 同目录 (备用)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

from astro_image_io import (  # noqa: E402
    PipelineFramePy, PipelineStageHandlerC, STAGE_PHOTOMETRIC,
)
from photometric_calib import PhotometricCalib  # noqa: E402
from curve_loader import CurveLoader  # noqa: E402

logger = logging.getLogger(__name__)


# ============================================================================
# 参数定义
# ============================================================================

# 默认 FITS FILTER 字段 -> filters.json 滤光片名映射
_DEFAULT_FILTER_NAME_MAP = {
    "Red": "Baader R",
    "Green": "Baader G",
    "Blue": "Baader B",
    "Lum": "Baader UV/IR Cut / L CMOS Optimized",
    "L": "Baader UV/IR Cut / L CMOS Optimized",
}


@dataclass
class PhotometricParams:
    """光度定标阶段参数（通过闭包传递给 handler）

    Attributes:
        match_radius_px: 星-图匹配半径 (像素) - C++ DLL固定3.0, 此参数仅用于日志
        outlier_sigma: 离群点清洗 sigma 阈值 - C++ DLL固定3.0, 此参数仅用于日志
        log_dir: 日志目录, None 仅输出到控制台
        dll_path: photometric_calib.dll 路径, None 时自动查找
        gaia_client: GaiaClientPy 实例 (带 _handle 和 get_spectrum_params 方法);
                     不为 None 时使用 calibrate_with_gaia 新模式;
                     为 None 时 fallback 到 calibrate_simple (退化路径)
        filter_name_map: FITS FILTER 字段 -> filters.json 滤光片名映射;
                         None 时使用默认映射 (Red->Baader R 等)
        mag_min: Gaia 星等下限 (新模式锥形搜索用)
        mag_max: Gaia 星等上限 (新模式锥形搜索用)
    """
    match_radius_px: float = 3.0
    outlier_sigma: float = 3.0
    log_dir: Optional[str] = None
    dll_path: Optional[str] = None
    gaia_client: Optional[object] = None
    filter_name_map: Optional[dict] = None
    mag_min: float = 8.0
    mag_max: float = 16.0


# ============================================================================
# 辅助函数: RA/DEC 解析
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


def _build_psf_arrays(frame: PipelineFramePy) -> Tuple[
        np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """从 psf 块读取 PSF 拟合结果并转为 numpy 数组

    psf 块格式: FLOAT64[N, 9], 每行 [status, B, flux, cx, cy, fwhm, A, mad, eccentricity]
    (前6列向后兼容, 后3列供 SNR 模块 §14 使用)

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
        logger.warning("psf 块格式异常: shape=%s, 期望 (N, 9)",
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


def _build_gaia_arrays_fallback(frame: PipelineFramePy) -> Tuple[
        np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """从 gaia_cat 块构造 Gaia 星 numpy 数组 (fallback 模式, f_syn=0)

    fallback 模式下无 f_syn 来源, f_syn 全部设为 0,
    C++ 端会跳过 f_syn<=0 的星, 导致 n_matched=0, scale=1.0 (退化路径)。

    Args:
        frame: PipelineFramePy 实例

    Returns:
        (gaia_ra, gaia_dec, gaia_mag, gaia_fsyn) float64 数组, 可能为空
    """
    gaia_cat = frame.get_block_data("gaia_cat")
    if gaia_cat is None or gaia_cat.size == 0:
        logger.warning("gaia_cat 块不存在或为空 (fallback 模式)")
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
    fsyn = np.zeros(n_stars, dtype=np.float64)  # fallback: f_syn=0 (退化路径)
    logger.info("fallback 模式: 从 gaia_cat 块读取 %d 颗星 (f_syn=0, 退化路径)",
                n_stars)
    return ra, dec, mag, fsyn


def _compute_cone_radius(width: int, height: int,
                         cd11: float, cd12: float,
                         cd21: float, cd22: float) -> float:
    """计算锥形搜索半径 = 0.5 * FOV 对角线 (度) + 20% 余量

    FOV 从 CD 矩阵 + 图像尺寸计算:
        pixel_scale = sqrt(|det(CD)|) 度/像素
        fov_diag = pixel_scale * sqrt(width^2 + height^2) 度
        radius = 0.5 * fov_diag * 1.2

    Args:
        width, height: 图像尺寸 (像素)
        cd11, cd12, cd21, cd22: CD 矩阵元素

    Returns:
        锥形搜索半径 (度), 限制在 [0.1, 30.0] 范围; CD 异常时返回 1.0
    """
    det_cd = abs(cd11 * cd22 - cd12 * cd21)
    if det_cd <= 0:
        logger.warning("CD 矩阵行列式=%.6e <=0, 使用默认半径 1.0°", det_cd)
        return 1.0
    pixel_scale_deg = float(np.sqrt(det_cd))
    fov_diag_deg = pixel_scale_deg * float(np.sqrt(width ** 2 + height ** 2))
    radius = 0.5 * fov_diag_deg * 1.2  # 20% 余量
    # 限制在合理范围
    radius = min(max(radius, 0.1), 30.0)
    logger.info("FOV 半径计算: pixel_scale=%.6e°/px, fov_diag=%.4f°, radius=%.4f°",
                pixel_scale_deg, fov_diag_deg, radius)
    return radius


def _get_gaia_client_handle(gaia_client) -> Optional[int]:
    """从 gaia_client 对象提取 C 端 handle 值

    支持 GaiaClientPy (vector_match_v2.py, _handle 属性) 和
    GaiaSpectrumClient (gaia_spectrum_client.py, _client 属性)。

    Args:
        gaia_client: GaiaClientPy 或 GaiaSpectrumClient 实例

    Returns:
        handle 值 (int), 失败返回 None
    """
    handle = getattr(gaia_client, "_handle", None)
    if handle is None:
        handle = getattr(gaia_client, "_client", None)
    if handle is None:
        return None
    # ctypes c_void_p 返回值可能是 int 或 c_void_p 对象
    import ctypes
    if isinstance(handle, ctypes.c_void_p):
        return handle.value
    return int(handle)


def _get_spectrum_wavelength(gaia_client) -> np.ndarray:
    """从 gaia_client 获取光谱波长数组 [336, 338, ..., 1020] nm

    调用 gaia_client.get_spectrum_params() -> (start_nm, step_nm, count)
    构造波长数组: wl[j] = start_nm + j * step_nm

    Args:
        gaia_client: GaiaClientPy 或 GaiaSpectrumClient 实例

    Returns:
        波长数组 float64 [count] (通常 343 个点)

    Raises:
        RuntimeError: 获取光谱参数失败 (当前数据库无光谱)
    """
    start_nm, step_nm, count = gaia_client.get_spectrum_params()
    wl = start_nm + np.arange(count, dtype=np.float64) * step_nm
    logger.info("光谱波长数组: start=%d nm, step=%d nm, count=%d, 范围=[%.0f, %.0f] nm",
                start_nm, step_nm, count, wl[0], wl[-1])
    return wl


# ============================================================================
# 管线适配器注册
# ============================================================================

def register_photometric_handler(engine, params: PhotometricParams):
    """注册光度定标阶段处理器到管线引擎

    Args:
        engine: PipelineEngine 实例
        params: PhotometricParams 参数 (含 gaia_client/dll_path/滤光片映射等)
    """
    # 加载 C++ DLL
    pc_dll = PhotometricCalib(params.dll_path)
    logger.info("C++ photometric_calib.dll 已加载")

    # 初始化 CurveLoader (用于加载滤光片曲线)
    curve_loader = CurveLoader()
    logger.info("CurveLoader 已初始化")

    # 滤光片名映射
    filter_name_map = params.filter_name_map or _DEFAULT_FILTER_NAME_MAP

    # 提取 gaia_client handle (新模式)
    gaia_handle = None
    if params.gaia_client is not None:
        gaia_handle = _get_gaia_client_handle(params.gaia_client)
        if gaia_handle is None:
            logger.warning("gaia_client 对象无 _handle/_client 属性, 回退到 fallback 模式")
        else:
            logger.info("gaia_client handle 已提取: %s", gaia_handle)

    use_gaia_mode = (gaia_handle is not None)

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        """STAGE_PHOTOMETRIC 处理回调

        流程:
          1. 从命名块读取 data/psf
          2. 从 header KV 读取 WCS/SIP 参数
          3. 构造 psf numpy 数组
          4. 选择校准模式:
             - 新模式 (gaia_client handle 有效): calibrate_with_gaia
               a. 从 header 读 OBJCTRA/OBJCTDEC (或 CRVAL1/2) 作为锥形搜索中心
               b. 从 header 读 FILTER, 映射滤光片名, 用 CurveLoader 加载曲线
               c. 从 gaia_client 获取光谱波长数组
               d. 计算 FOV 半径
               e. 调用 pc_calibrate_simple_with_gaia
             - fallback (无 gaia_client): calibrate_simple
               a. 从 gaia_cat 块读取 ra/dec/mag, f_syn=0 (退化路径)
               b. 调用 pc_calibrate_simple
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
            pixels = np.ascontiguousarray(pixels, dtype=np.float32)
            if pixels.ndim != 2:
                raise ValueError(f"data 块必须为 2D, 实际为 {pixels.ndim}D")
            height, width = pixels.shape
            logger.info("读取 data 块: shape=%s, dtype=%s",
                        pixels.shape, pixels.dtype)

            # 2. 读取 WCS/SIP 参数
            wcs_params = _read_wcs_params(frame)

            # 3. 构造 psf 数组
            psf_cx, psf_cy, psf_flux, psf_status = _build_psf_arrays(frame)
            logger.info("psf 星: %d 颗", psf_cx.size)

            # 4. 选择校准模式
            if use_gaia_mode:
                # ---- 新模式: calibrate_with_gaia ----
                logger.info("-" * 40)
                logger.info("校准模式: calibrate_with_gaia (DLL 内部锥形搜索+光谱积分)")
                logger.info("-" * 40)

                # 4a. 锥形搜索中心: 优先 OBJCTRA/OBJCTDEC, 否则用 CRVAL1/CRVAL2
                ra_str = frame.kv_get("header", "OBJCTRA")
                dec_str = frame.kv_get("header", "OBJCTDEC")
                if ra_str and dec_str:
                    ra_center = _parse_ra(ra_str)
                    dec_center = _parse_dec(dec_str)
                    logger.info("锥形搜索中心 (OBJCTRA/OBJCTDEC): ra=%.6f°, dec=%.6f°",
                                ra_center, dec_center)
                else:
                    ra_center = wcs_params["crval1"]
                    dec_center = wcs_params["crval2"]
                    logger.info("锥形搜索中心 (CRVAL1/CRVAL2): ra=%.6f°, dec=%.6f°",
                                ra_center, dec_center)

                # 4b. 滤光片曲线
                filter_field = frame.kv_get("header", "FILTER") or ""
                filter_field = filter_field.strip()
                filter_name = filter_name_map.get(filter_field, None)
                if filter_name is None:
                    # 未知 FILTER 字段, 尝试直接用 FILTER 值作为滤光片名
                    if filter_field:
                        logger.warning("FILTER '%s' 未在 filter_name_map 中找到, 尝试直接使用",
                                       filter_field)
                        filter_name = filter_field
                    else:
                        raise ValueError(
                            "header KV 块缺少 FILTER 字段, 无法加载滤光片曲线")
                logger.info("滤光片映射: FILTER='%s' -> '%s'", filter_field, filter_name)
                filter_wl, filter_trans = curve_loader.load_filter(filter_name)
                logger.info("滤光片曲线加载完成: %d 个点, 波长范围 [%.0f, %.0f] nm",
                            filter_wl.size, filter_wl[0], filter_wl[-1])

                # 4c. 光谱波长数组
                spectrum_wl = _get_spectrum_wavelength(params.gaia_client)

                # 4d. 计算锥形搜索半径
                radius_deg = _compute_cone_radius(
                    width, height,
                    wcs_params["cd11"], wcs_params["cd12"],
                    wcs_params["cd21"], wcs_params["cd22"])

                # 4e. 调用 pc_calibrate_simple_with_gaia
                out_pixels, n_matched, scale_factor, sigma_residual = pc_dll.calibrate_with_gaia(
                    gaia_handle,
                    ra_center, dec_center, radius_deg,
                    params.mag_min, params.mag_max,
                    filter_wl, filter_trans,
                    spectrum_wl,
                    pixels,
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
            else:
                # ---- fallback: calibrate_simple (从 gaia_cat 块读取) ----
                logger.info("-" * 40)
                logger.info("校准模式: calibrate_simple (fallback, gaia_client 为 None)")
                logger.info("-" * 40)

                gaia_ra, gaia_dec, gaia_mag, gaia_fsyn = _build_gaia_arrays_fallback(
                    frame)
                logger.info("gaia 星 (fallback): %d 颗", gaia_ra.size)

                out_pixels, n_matched, scale_factor, sigma_residual = pc_dll.calibrate_simple(
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

            logger.info("C++ 测光校准完成: n_matched=%d, scale=%.6e",
                        n_matched, scale_factor)

            # 5. 替换 data 块为校正后图像
            frame.remove_block("data")
            frame.add_block("data", out_pixels,
                            description="光度定标后图像 (float32, Gaia 参考系统, 全局scale校正)")
            logger.info("data 块已替换为校正后图像: shape=%s, dtype=%s",
                        out_pixels.shape, out_pixels.dtype)

            # 6. 添加 photo_stats KV 块
            frame.kv_set("photo_stats", "N_MATCHED", str(n_matched))
            frame.kv_set("photo_stats", "SCALE_FACTOR",
                         f"{scale_factor:.6e}")
            frame.kv_set("photo_stats", "SIGMA_RESIDUAL",
                         f"{sigma_residual:.6e}")
            logger.info("photo_stats KV 块已添加: N_MATCHED=%d, SCALE=%.6e, SIGMA_RESIDUAL=%.6e",
                        n_matched, scale_factor, sigma_residual)

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
                "match_radius=%.2f px, outlier_sigma=%.2f, gaia_mode=%s, mag=[%.1f, %.1f]",
                params.match_radius_px, params.outlier_sigma,
                use_gaia_mode, params.mag_min, params.mag_max)
