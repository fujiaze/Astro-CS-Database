# -*- coding: utf-8 -*-
"""
SNR Estimator 管线适配器 (Python 调试层)
功能: 将 snr_estimator.dll 包装为 PipelineStageHandler, 从 PipelineFrame 命名块
      读取 data/psf/photo_stats, 调用 snr_estimate 计算每像素 SNR,
      输出 snr 块 (float32[H,W])
用途: 在管线引擎中注册 STAGE_SNR 阶段处理器, 实现 SNR² 加权 drizzle 的前置计算

数据流:
    PipelineFrame
      ├─ data 块 (float32[H,W])         ──┐
      ├─ psf 块 (float64[N,9])          ──┤
      └─ photo_stats KV (SIGMA_RESIDUAL)──┴─→ snr_estimate → snr 块

乘法模型:
    SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))
    SNR_phot = 1 / (ln10 × sigma_residual)   全帧常数
    SNR_psf(pixel) = IDW(PSF星位置, (A-B)/mad)  反距离加权插值

调用示例:
    from snr_adapter import SNRParams, register_snr_handler
    params = SNRParams(log_dir="logs/snr")
    register_snr_handler(engine, params)

注意: Python 调试层定位, 调试完毕后由 C++ pipeline_engine 替代 (spec §6)
"""

from __future__ import annotations

import logging
import os
import sys
from dataclasses import dataclass
from typing import Optional

import numpy as np

# ---- 依赖路径配置 ----
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
# astro_image_io: pipeline_adapters -> python -> orchestrator -> lib -> astro_image_io/python
_AIO_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "..", "astro_image_io", "python"))
if _AIO_PATH not in sys.path:
    sys.path.insert(0, _AIO_PATH)
# snr_estimator/python (C++ DLL封装): pipeline_adapters -> python -> orchestrator -> lib -> snr_estimator/python
_SNR_PATH = os.path.normpath(os.path.join(
    _THIS_DIR, "..", "..", "..", "..", "snr_estimator", "python"))
if _SNR_PATH not in sys.path:
    sys.path.insert(0, _SNR_PATH)

from astro_image_io import PipelineFramePy, PipelineStageHandlerC  # noqa: E402
from snr_estimator import SNREstimator  # noqa: E402

logger = logging.getLogger(__name__)

# STAGE_SNR 阶段常量 (不修改 astro_image_io 现有常量, 独立定义)
# 现有: STAGE_CALIBRATE=0, STAGE_PLATESOLVE=1, STAGE_PHOTOMETRIC=2, STAGE_DRIZZLE=3, STAGE_STACK=4
# SNR 在 PHOTOMETRIC 之后、DRIZZLE 之前执行, 但为避免冲突用 5
STAGE_SNR = 5


# ============================================================================
# 参数定义
# ============================================================================

@dataclass
class SNRParams:
    """SNR 估算阶段参数（通过闭包传递给 handler）

    Attributes:
        log_dir: 日志目录, None 仅输出到控制台
        dll_path: snr_estimator.dll 路径, None 时自动查找
    """
    log_dir: Optional[str] = None
    dll_path: Optional[str] = None


# ============================================================================
# 辅助函数: 从 PipelineFrame 读取 sigma_residual
# ============================================================================

def _read_sigma_residual(frame: PipelineFramePy) -> float:
    """从 photo_stats KV 块读取 SIGMA_RESIDUAL

    Args:
        frame: PipelineFramePy 实例

    Returns:
        sigma_residual 值, 读取失败返回 0.0 (触发退化路径)
    """
    val = frame.kv_get("photo_stats", "SIGMA_RESIDUAL")
    if val is None:
        logger.warning("photo_stats 块无 SIGMA_RESIDUAL, 使用 0.0 (退化路径)")
        return 0.0
    try:
        return float(val)
    except (ValueError, TypeError):
        logger.warning("SIGMA_RESIDUAL 解析失败: %s, 使用 0.0", val)
        return 0.0


# ============================================================================
# handler 注册
# ============================================================================

def register_snr_handler(engine, params: SNRParams):
    """注册 SNR 阶段处理器到管线引擎

    Args:
        engine: PipelineEngine 实例
        params: SNRParams 参数

    用法:
        params = SNRParams(log_dir="logs/snr")
        register_snr_handler(engine, params)
    """
    # 初始化 SNR DLL 封装 (闭包内复用)
    snr_est = SNREstimator(dll_path=params.dll_path)
    logger.info("SNREstimator DLL 已加载: %s", params.dll_path or "自动查找")

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        """STAGE_SNR 处理回调

        流程:
          1. 从 data 块读取像素 (float32[H,W])
          2. 从 psf 块读取 PSF 星数据 (float64[N,9])
          3. 从 photo_stats KV 读取 SIGMA_RESIDUAL
          4. 调用 snr_estimate 计算 SNR
          5. 将 SNR 结果写入 snr 块 (float32[H,W])
        """
        frame = PipelineFramePy.from_c_ptr(c_frame_ptr)
        try:
            logger.info("=" * 60)
            logger.info("SNR 估算阶段开始 (STAGE_SNR, C++ DLL)")
            logger.info("=" * 60)

            # 1. 读取像素数据
            pixels = frame.get_block_data("data")
            if pixels is None:
                raise ValueError("data 块不存在, 无法进行 SNR 估算")
            pixels = np.ascontiguousarray(pixels, dtype=np.float32)
            if pixels.ndim != 2:
                raise ValueError(f"data 块必须为 2D, 实际为 {pixels.ndim}D")
            height, width = pixels.shape
            logger.info("读取 data 块: shape=%s", pixels.shape)

            # 2. 读取 psf 块 (float64[N,9])
            psf_data = frame.get_block_data("psf")
            if psf_data is None or psf_data.size == 0:
                logger.warning("psf 块不存在或为空, 使用退化路径 (n_stars=0)")
                psf_arr = np.zeros((0, 9), dtype=np.float64)
            else:
                psf_arr = np.ascontiguousarray(psf_data, dtype=np.float64)
                if psf_arr.ndim != 2 or psf_arr.shape[1] < 9:
                    logger.warning("psf 块格式异常: shape=%s, 期望 (N, 9)", psf_arr.shape)
                    psf_arr = np.zeros((0, 9), dtype=np.float64)
                else:
                    psf_arr = psf_arr[:, :9]  # 取前 9 列
            logger.info("读取 psf 块: %d 颗星", psf_arr.shape[0])

            # 3. 读取 sigma_residual
            sigma_residual = _read_sigma_residual(frame)
            logger.info("sigma_residual = %.6e", sigma_residual)

            # 4. 调用 SNR 估算
            snr_array, ret_code = snr_est.estimate(
                pixels, psf_arr, sigma_residual)

            code_msg = {
                0: "成功",
                1: "n_stars<=0 退化 (全填 SNR_phot)",
                2: "sigma_residual<=0 退化 (全填 1.0)",
            }.get(ret_code, f"未知码 {ret_code}")
            logger.info("SNR 估算完成: ret=%d (%s)", ret_code, code_msg)

            # 5. 写入 snr 块 (float32[H,W])
            if frame.has_block("snr"):
                frame.remove_block("snr")
            frame.add_block("snr", snr_array,
                            description="SNR 估算结果 (float32, 乘法模型)")
            logger.info("snr 块已写入: shape=%s, dtype=%s",
                        snr_array.shape, snr_array.dtype)

            # 打印统计
            snr_flat = snr_array.ravel()
            logger.info("SNR 统计: min=%.4f, max=%.4f, median=%.4f, mean=%.4f",
                        float(np.min(snr_flat)), float(np.max(snr_flat)),
                        float(np.median(snr_flat)), float(np.mean(snr_flat)))

            logger.info("=" * 60)
            logger.info("SNR 估算阶段完成 (返回 0)")
            logger.info("=" * 60)
            return 0
        except Exception as e:
            logger.error("SNR 估算失败: %s", e)
            try:
                if err_buf and err_cap > 0:
                    msg = str(e).encode('utf-8')[:err_cap - 1]
                    err_buf[:len(msg)] = msg
            except Exception:
                pass
            return -1

    handler_c = PipelineStageHandlerC(_handler)
    engine.register(STAGE_SNR, handler_c)
    logger.info("STAGE_SNR handler 已注册 (C++ DLL): snr_estimator.dll")
    return snr_est  # 返回 SNREstimator 实例供调试用


# ============================================================================
# 独立调用函数 (不依赖 engine.register, 供 run_pipeline_debug.py 使用)
# ============================================================================

def run_snr_stage(frame: PipelineFramePy,
                  snr_est: Optional[SNREstimator] = None,
                  log_dir: Optional[str] = None) -> int:
    """独立执行 SNR 估算阶段 (不依赖 PipelineEngine)

    供 run_pipeline_debug.py 在 4_photometric 之后、5_drizzle 之前调用。

    Args:
        frame: PipelineFramePy 实例
        snr_est: SNREstimator 实例 (None 时自动创建)
        log_dir: 日志目录 (未使用, 保留兼容)

    Returns:
        0=成功, <0=失败
    """
    if snr_est is None:
        snr_est = SNREstimator()

    try:
        logger.info("=" * 60)
        logger.info("SNR 估算阶段开始 (独立调用, C++ DLL)")
        logger.info("=" * 60)

        # 1. 读取像素数据
        pixels = frame.get_block_data("data")
        if pixels is None:
            raise ValueError("data 块不存在")
        pixels = np.ascontiguousarray(pixels, dtype=np.float32)
        if pixels.ndim != 2:
            raise ValueError(f"data 块必须为 2D, 实际为 {pixels.ndim}D")
        logger.info("读取 data 块: shape=%s", pixels.shape)

        # 2. 读取 psf 块
        psf_data = frame.get_block_data("psf")
        if psf_data is None or psf_data.size == 0:
            logger.warning("psf 块不存在或为空, 退化路径")
            psf_arr = np.zeros((0, 9), dtype=np.float64)
        else:
            psf_arr = np.ascontiguousarray(psf_data, dtype=np.float64)
            if psf_arr.ndim != 2 or psf_arr.shape[1] < 9:
                psf_arr = np.zeros((0, 9), dtype=np.float64)
            else:
                psf_arr = psf_arr[:, :9]
        logger.info("读取 psf 块: %d 颗星", psf_arr.shape[0])

        # 3. 读取 sigma_residual
        sigma_residual = _read_sigma_residual(frame)
        logger.info("sigma_residual = %.6e", sigma_residual)

        # 4. 调用 SNR 估算
        snr_array, ret_code = snr_est.estimate(pixels, psf_arr, sigma_residual)

        code_msg = {0: "成功", 1: "n_stars<=0 退化",
                    2: "sigma<=0 退化"}.get(ret_code, f"码 {ret_code}")
        logger.info("SNR 估算完成: ret=%d (%s)", ret_code, code_msg)

        # 5. 写入 snr 块
        if frame.has_block("snr"):
            frame.remove_block("snr")
        frame.add_block("snr", snr_array,
                        description="SNR 估算结果 (float32, 乘法模型)")
        logger.info("snr 块已写入: shape=%s", snr_array.shape)

        snr_flat = snr_array.ravel()
        logger.info("SNR 统计: min=%.4f, max=%.4f, median=%.4f, mean=%.4f",
                    float(np.min(snr_flat)), float(np.max(snr_flat)),
                    float(np.median(snr_flat)), float(np.mean(snr_flat)))

        logger.info("SNR 估算阶段完成 (返回 0)")
        return 0
    except Exception as e:
        logger.error("SNR 估算失败: %s", e)
        return -1
