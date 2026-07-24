"""
healpix_drizzle 管线适配器（命名块直通版）
功能: 将 hp_drizzle_run 命名块直通 API 包装为 PipelineStageHandler
用途: 在管线引擎中注册 STAGE_DRIZZLE 阶段处理器，从 PipelineFrame 的 "data" 块
      和 "header" KV 块直接调用 DrizzleEngine，输出 .hiss 文件，不经临时 FITS

本文件从 lib/healpix_db/healpix_drizzle/pipeline_adapter.py 迁移而来。

数据流:
    PipelineFrame
      ├─ data 块 (float32[H,W])  ──┐
      └─ header KV 块 (WCS+SIP)  ──┴─→ hp_drizzle_run → .hiss

使用示例:
    from astro_image_io import PipelineEngine, STAGE_DRIZZLE
    from drizzle_adapter import DrizzleParams, register_drizzle_handler

    engine = PipelineEngine()
    params = DrizzleParams(nside=32768, nested=True, pixfrac=0.8, output_dir="./hiss_output")
    register_drizzle_handler(engine, params)
    engine.run_single(frame, STAGE_DRIZZLE, STAGE_DRIZZLE)
"""

from __future__ import annotations

import logging
import os
from dataclasses import dataclass
from typing import Optional

from astro_image_io import PipelineFramePy, PipelineStageHandlerC, STAGE_DRIZZLE
from healpix_drizzle import hp_drizzle_run

logger = logging.getLogger(__name__)


@dataclass
class DrizzleParams:
    """Drizzle 阶段参数（通过闭包传递给 handler）"""
    nside: int = 32768
    nested: bool = True
    pixfrac: float = 0.8
    output_dir: str = "."  # .hiss 文件输出目录


def _build_output_path(frame: PipelineFramePy, output_dir: str) -> str:
    """从 header 的 SOURCE_PATH 或 OBJECT 构造输出 .hiss 路径

    优先用 SOURCE_PATH 的文件名主干；其次用 OBJECT（清理非法字符）；
    都没有时用默认名 "drizzle_output"。
    """
    basename = None
    source_path = frame.kv_get("header", "SOURCE_PATH")
    if source_path:
        basename = os.path.splitext(os.path.basename(source_path))[0]
    if not basename:
        obj = frame.kv_get("header", "OBJECT")
        if obj:
            basename = "".join(c if c.isalnum() or c in "-_" else "_" for c in obj)
    if not basename:
        basename = "drizzle_output"
    return os.path.join(output_dir, basename + ".hiss")


def register_drizzle_handler(engine, params: DrizzleParams):
    """注册 Drizzle 阶段处理器到管线引擎

    engine: PipelineEngine 实例
    params: DrizzleParams 参数（含 nside/nested/pixfrac/output_dir）

    用法:
        params = DrizzleParams(nside=32768, output_dir="./hiss_output")
        register_drizzle_handler(engine, params)
    """

    def _handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        frame = PipelineFramePy.from_c_ptr(c_frame_ptr)
        try:
            # 检查 data 块存在
            if not frame.has_block("data"):
                raise RuntimeError("frame 中缺少 'data' 块")

            # 读取像素尺寸用于日志
            pixels = frame.get_block_data("data")
            if pixels is None:
                raise RuntimeError("frame 中缺少 'data' 块")
            shape = pixels.shape if hasattr(pixels, "shape") else ("?", "?")

            logger.info("Drizzle 开始: shape=%s, nside=%d, pixfrac=%.2f",
                        shape, params.nside, params.pixfrac)

            # 构造输出路径并确保目录存在
            output_hiss = _build_output_path(frame, params.output_dir)
            os.makedirs(params.output_dir, exist_ok=True)

            # 直接调用命名块直通 API (不经临时 FITS)
            result = hp_drizzle_run(
                frame=frame,
                nside=params.nside,
                nested=params.nested,
                pixfrac=params.pixfrac,
                output_path=output_hiss,
            )
            logger.info("Drizzle 完成: %s (源像素=%d, HEALPix 像素=%d, 耗时=%.3fs)",
                        output_hiss, result.n_source_pixels,
                        result.n_healpix_pixels, result.elapsed_sec)
            return 0
        except Exception as e:
            logger.error("Drizzle 失败: %s", e)
            try:
                if err_buf and err_cap > 0:
                    msg = str(e).encode('utf-8')[:err_cap - 1]
                    err_buf[:len(msg)] = msg
            except Exception:
                pass
            return -1

    handler_c = PipelineStageHandlerC(_handler)
    engine.register(STAGE_DRIZZLE, handler_c)
