"""
calibration 模块管线适配器
功能: 将 Calibrator + CosmeticCorrector 包装为 PipelineStageHandler
用途: 在管线引擎中注册 STAGE_CALIBRATE 阶段处理器，通过 PipelineFrame 命名块容器传递数据

本文件从 lib/calibration/python/pipeline_adapter.py 迁移而来。

命名块约定:
  - 输入: "data" (FLOAT32 [H,W]) + "header" (KV, 含 EXPTIME/FILTER)
  - 输出: "data" (替换为校准后像素) + "cal_stats" (KV, 含 DARK_K)
  - 可选: "weight" (FLOAT32 [H,W]) 权重图
"""

from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import Optional

import numpy as np

from astro_image_io import PipelineFramePy, PipelineStageHandlerC, STAGE_CALIBRATE
from calibrator import Calibrator
from cosmetic_corrector import CosmeticCorrector

logger = logging.getLogger(__name__)


@dataclass
class CalibrateParams:
    """校准阶段参数（通过闭包传递给 handler）"""
    master_bias: Optional[np.ndarray] = None
    master_dark: Optional[np.ndarray] = None
    master_flat: Optional[np.ndarray] = None
    dark_exposure: float = 0.0
    dark_optimization: bool = False
    enable_cosmetic_correction: bool = True
    cc_method: str = "median"
    hot_sigma: float = 5.0
    cold_sigma: float = 5.0
    max_structure_size: int = 4


def register_calibrate_handler(engine, params: CalibrateParams):
    """注册校准阶段处理器到管线引擎

    engine: PipelineEngine 实例
    params: CalibrateParams 参数（含 master 帧数据和校准配置）

    用法:
        from astro_image_io import ImageReader
        reader = ImageReader()
        master_dark = reader.read("master_dark.fits").data
        master_flat = reader.read("master_flat.fits").data

        params = CalibrateParams(master_dark=master_dark, master_flat=master_flat, ...)
        register_calibrate_handler(engine, params)
    """
    calibrator = Calibrator()
    cc = CosmeticCorrector() if params.enable_cosmetic_correction else None

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        frame = PipelineFramePy.from_c_ptr(c_frame_ptr)
        try:
            # 1. 读取像素数据（命名块 "data"，零拷贝 numpy view）
            pixels = frame.get_block_data("data")
            if pixels is None:
                raise RuntimeError("frame 中缺少 'data' 块")
            # 确保是 float32 连续数组（避免零拷贝 view 被下游修改影响 C 端内存）
            pixels = np.ascontiguousarray(pixels, dtype=np.float32)

            # 2. 读取曝光时间（header KV 块）
            exptime = frame.kv_get_double("header", "EXPTIME", 0.0)

            # 3. 读取滤镜名（header KV 块）
            filter_name = frame.kv_get("header", "FILTER") or ""

            logger.info(
                "校准开始: shape=%s, EXPTIME=%.2fs, FILTER=%s",
                pixels.shape, exptime, filter_name,
            )

            # 4. 核心校准（内存直通，不读写文件）
            calibrated, cal_stats = calibrator.calibrate_data(
                pixels,
                master_bias=params.master_bias,
                master_dark=params.master_dark,
                master_flat=params.master_flat,
                dark_optimization=params.dark_optimization,
                light_exposure=exptime,
                dark_exposure=params.dark_exposure,
            )

            # 5. 坏点修复（可选，由 enable_cosmetic_correction 控制）
            weight_map = None
            if cc is not None:
                calibrated, cc_info = cc.correct_data(
                    calibrated,
                    hot_sigma=params.hot_sigma,
                    cold_sigma=params.cold_sigma,
                    method=params.cc_method,
                    max_structure_size=params.max_structure_size,
                    dark_data=params.master_dark,
                    bias_data=params.master_bias,
                )

            # 6. 替换 data 块（先删后增，确保新数据被拷贝到 C 端内存）
            frame.remove_block("data")
            frame.add_block("data", calibrated, description="校准后像素")

            # 7. 若有 weight_map，添加 weight 块
            if weight_map is not None:
                frame.add_block("weight", weight_map, description="权重图")

            # 8. 添加校准统计到 cal_stats KV 块
            actual_k = cal_stats.get("dark_scale_factor", 1.0)
            frame.kv_set("cal_stats", "DARK_K", str(actual_k))

            logger.info(
                "校准完成: before mean=%.2f, after mean=%.2f, DARK_K=%.4f",
                cal_stats.get("before", {}).get("mean", 0),
                cal_stats.get("after", {}).get("mean", 0),
                actual_k,
            )
            return 0
        except Exception as e:
            logger.error("校准失败: %s", e, exc_info=True)
            if err_buf and err_cap > 0:
                msg = str(e).encode('utf-8')[:err_cap - 1]
                err_buf[:len(msg)] = msg
            return -1

    handler_c = PipelineStageHandlerC(_handler)
    engine.register(STAGE_CALIBRATE, handler_c)
