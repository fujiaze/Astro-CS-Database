"""
dynamic_psf PSF 拟合管线适配器
功能: 将 DynamicPSF 包装为 PipelineStageHandlerC
用途: 在 plate_solve 后插入 PSF 拟合步骤，读取 star_det 块（星点坐标）和 data 块（像素），
      调用 DynamicPSF 拟合，结果写入 psf 块供 photometric_calib 复用

本文件从 lib/astro_image_io/python/orchestrator.py 的 make_psf_fit_handler 函数提取而来。

命名块约定:
  - 输入: "star_det" (FLOAT32[N,4]: x,y,flux,mag) + "data" (FLOAT32[H,W])
  - 输出: "psf" (FLOAT64[N,9]: status,B,flux,cx,cy,fwhm,A,mad,eccentricity)
    后3列(A/mad/eccentricity)由 Moffat4 拟合计算,供 SNR 模块(§14)
    计算 SNR_psf(pixel) = IDW(PSF星位置 (A-B)/mad) 使用
"""

from __future__ import annotations

import logging
import os
import sys

import numpy as np

# 添加模块路径（dynamic_psf）
_HERE = os.path.dirname(os.path.abspath(__file__))
_LIB_DIR = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
for module_dir in ["dynamic_psf/python"]:
    p = os.path.join(_LIB_DIR, module_dir)
    if p not in sys.path:
        sys.path.insert(0, p)

from astro_image_io import PipelineFramePy, PipelineStageHandlerC

logger = logging.getLogger(__name__)


def _write_error(err_buf, err_cap, msg):
    """写入错误信息到缓冲区"""
    if err_buf and err_cap > 0:
        b = msg.encode('utf-8')[:err_cap - 1]
        err_buf[:len(b)] = b


def make_psf_fit_handler(dll_path=None):
    """创建PSF拟合handler
    返回: PipelineStageHandlerC 包装的handler

    用法:
        handler = make_psf_fit_handler()
        engine.register(STAGE_PHOTOMETRIC, handler)  # 或自定义阶段
    """
    def handler(c_frame_ptr, _params_ptr, err_buf, err_cap):
        try:
            frame = PipelineFramePy.from_c_ptr(c_frame_ptr)

            # 读取星点坐标
            star_det = frame.get_block_data("star_det")
            if star_det is None:
                # 如果没有star_det块，跳过PSF拟合
                logger.info("star_det 块不存在，跳过 PSF 拟合")
                return 0

            # 读取像素数据
            pixels = frame.get_block_data("data")
            if pixels is None:
                _write_error(err_buf, err_cap, "data块不存在")
                return -1

            logger.info("PSF 拟合开始: %d 颗星, 图像 shape=%s",
                        star_det.shape[0], pixels.shape)

            # float32 → uint16 转换（DynamicPSF要求uint16输入）
            image_u16 = np.clip(pixels, 0, 65535).astype(np.uint16)

            # 提取坐标
            cx_list = star_det[:, 0].tolist()
            cy_list = star_det[:, 1].tolist()

            # 调用PSF拟合
            from dynamic_psf import DynamicPSF
            results = DynamicPSF.fit_batch(image_u16, cx_list, cy_list, dll_path=dll_path)

            # 构造psf数组 FLOAT64[N, 9]
            # 前6列: status,B,flux,cx,cy,fwhm (向后兼容, 索引不变)
            # 后3列: A,mad,eccentricity (新增, 供 SNR 模块 §14 使用)
            n = len(results)
            psf_array = np.zeros((n, 9), dtype=np.float64)
            for i, r in enumerate(results):
                psf_array[i, 0] = float(r.status)
                psf_array[i, 1] = float(r.B)
                psf_array[i, 2] = float(r.flux)
                psf_array[i, 3] = float(r.cx)
                psf_array[i, 4] = float(r.cy)
                # fwhm = (fwhm_x + fwhm_y) / 2
                psf_array[i, 5] = float((r.fwhm_x + r.fwhm_y) / 2.0)
                # 新增3列: Moffat振幅/残差MAD/椭率 (C++ DPSFFitResult 已计算)
                psf_array[i, 6] = float(r.A)
                psf_array[i, 7] = float(r.mad)
                psf_array[i, 8] = float(r.eccentricity)

            # 写入psf块
            frame.add_block("psf", psf_array, description="PSF拟合结果")

            n_ok = int(np.sum(psf_array[:, 0] == 0))
            logger.info("PSF 拟合完成: %d/%d 成功", n_ok, n)
            return 0
        except Exception as e:
            logger.error("PSF 拟合异常: %s", e, exc_info=True)
            _write_error(err_buf, err_cap, str(e))
            return -1

    return PipelineStageHandlerC(handler)
