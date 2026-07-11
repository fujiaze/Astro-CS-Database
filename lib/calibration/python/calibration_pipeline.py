# -*- coding: utf-8 -*-
"""
Calibration Pipeline - 校准管线入口模块
功能: 统一调度图像校准（Calibrator）与坏点修复（CosmeticCorrector），
      支持 debug 模式（分步输出中间 FITS）与 production 模式（内存直通只输出最终 FITS）。
用途: 天文图像标准校准流程的顶层入口，将校准与坏点修复两步串联为完整管线，
      简化批量处理与命令行调用。
依赖: numpy, logging, argparse,
      astro_image_io (ImageReader / FITSWriter / FITSKeywordPy),
      calibrator (Calibrator / find_matching_master_dark / find_matching_master_flat / _find_master_bias / unify_data_range),
      cosmetic_corrector (CosmeticCorrector / correct_frame)
调用:
      from calibration_pipeline import CalibrationPipeline
      pipe = CalibrationPipeline(mode="production", max_workers=16)
      result = pipe.run("light.fits", "output/",
                        calibration_dir="calibration_files/",
                        dark_optimization=False)
      # 或命令行:
      # python calibration_pipeline.py --light light.fits --output-dir output/ --mode production
数据流:
      debug 模式:      Light -> Calibrator.calibrate_frame -> {name}_calibrated.fits
                      -> correct_frame                    -> {name}_final.fits
      production 模式: Light(内存) -> calibrate_data -> correct_data(内存) -> {name}_calibrated.fits
关键设计:
      - debug 模式分步落盘，便于调试检查中间结果
      - production 模式内存直通，减少 I/O，只写最终 FITS
      - 自动匹配主帧（calibration_dir 提供时调用 calibrator 的匹配函数）
      - 单帧失败不中断批量处理（run 返回 success 标志，CLI 循环跳过失败帧）
"""

from __future__ import annotations

import os
import sys
import logging
import argparse
from datetime import datetime

import numpy as np

# ---- 导入项目统一的 astro_image_io 接口 ----
_lib_base = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "astro_image_io", "python",
)
if _lib_base not in sys.path:
    sys.path.insert(0, _lib_base)
from astro_image_io import ImageReader, FITSWriter, FITSKeywordPy

# ---- 导入同目录的校准与坏点修复模块 ----
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)
from calibrator import (
    Calibrator,
    find_matching_master_dark,
    find_matching_master_flat,
    _find_master_bias,
    unify_data_range,
)
from cosmetic_corrector import CosmeticCorrector, correct_frame


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "calibration_pipeline_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("calibration_pipeline")
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


# ============================ 校准管线 ============================

class CalibrationPipeline:
    """校准管线：统一调度 Calibrator 与 CosmeticCorrector"""

    def __init__(self, mode="debug", max_workers=4):
        """
        Args:
            mode: "debug" 或 "production"
                  debug 模式: 校准和坏点修复分步运行，各自输出中间 FITS
                  production 模式: 校准和坏点修复内存直通，只输出最终 FITS
            max_workers: int，并行线程数（传递给 Calibrator / CosmeticCorrector）
        """
        if mode not in ("debug", "production"):
            raise ValueError(f"不支持的 mode: {mode}（可选: debug / production）")
        self.mode = mode
        self.max_workers = max_workers
        self.calibrator = Calibrator(max_workers=max_workers)
        self.cosmetic_corrector = CosmeticCorrector(max_workers=max_workers)
        self._reader = ImageReader()
        self._writer = FITSWriter()
        logger.info("CalibrationPipeline 初始化: mode=%s, max_workers=%d", mode, max_workers)

    # ---------- 主帧路径解析 ----------

    def _resolve_master_paths(self, light_exposure, light_filter,
                              master_bias, master_dark, master_flat, calibration_dir):
        """
        解析主帧路径：优先使用显式指定的路径，未指定时从 calibration_dir 自动匹配。

        Returns:
            (bias_path, dark_path, flat_path)，未匹配到为 None
        """
        bias_path = master_bias
        dark_path = master_dark
        flat_path = master_flat

        if calibration_dir is not None:
            if bias_path is None:
                bias_path = _find_master_bias(calibration_dir)
            if dark_path is None and light_exposure > 0:
                dark_path = find_matching_master_dark(calibration_dir, light_exposure)
            if flat_path is None and light_filter:
                flat_path = find_matching_master_flat(calibration_dir, light_filter)

        logger.info(
            "主帧路径解析: bias=%s, dark=%s, flat=%s",
            bias_path or "无", dark_path or "无", flat_path or "无",
        )
        return bias_path, dark_path, flat_path

    def _read_light_meta(self, light_path):
        """读取 Light 帧的 EXPTIME / FILTER / BITPIX（仅元数据，读取后立即关闭）"""
        img = self._reader.read(light_path)
        try:
            exptime = img.get_keyword_float("EXPTIME", 0.0)
            filt = img.get_keyword("FILTER", "") or ""
            bitpix = img.get_keyword_int("BITPIX", 16)
        finally:
            img.close()
        logger.info(
            "Light 元数据: EXPTIME=%.2fs, FILTER=%s, BITPIX=%d",
            exptime, filt, bitpix,
        )
        return exptime, filt, bitpix

    @staticmethod
    def _log_stats(label, stats):
        """记录校准前后的统计信息（min/max/mean/std）"""
        if not stats:
            return
        before = stats.get("before", {})
        after = stats.get("after", {})
        if before:
            logger.info(
                "%s 校准前: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
                label, before.get("min", 0.0), before.get("max", 0.0),
                before.get("mean", 0.0), before.get("std", 0.0),
            )
        if after:
            logger.info(
                "%s 校准后: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
                label, after.get("min", 0.0), after.get("max", 0.0),
                after.get("mean", 0.0), after.get("std", 0.0),
            )

    # ---------- 主入口 ----------

    def run(self, light_path, output_dir,
            master_bias=None, master_dark=None, master_flat=None,
            calibration_dir=None,
            dark_optimization=False,
            cc_method="median",
            hot_sigma=5.0, cold_sigma=5.0,
            max_structure_size=4,
            enable_cosmetic_correction=True):
        """
        运行完整校准管线。

        校准流程:
            无暗场优化: (Light - Dark) / Flat  （Dark 已含 Bias）
            有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat

        坏点修复:
            Dark全局统计检测热像素 + Bias全局统计检测冷像素。
            连通区域大小过滤排除星点（< max_structure_size 保留）。
            不检测 cosmic ray（留给叠加时 3sigma 处理）。

        Returns:
            dict: success, output_path, calibrated_stats, cc_stats
        """
        logger.info("=" * 60)
        logger.info("校准管线启动: %s -> %s (mode=%s)", light_path, output_dir, self.mode)

        if not os.path.isfile(light_path):
            err = f"Light 帧不存在: {light_path}"
            logger.error(err)
            return {
                "success": False, "output_path": None,
                "calibrated_stats": {}, "cc_stats": {},
                "error": err, "light_path": light_path,
            }

        os.makedirs(output_dir, exist_ok=True)

        try:
            if self.mode == "debug":
                return self._run_debug(
                    light_path, output_dir,
                    master_bias, master_dark, master_flat,
                    calibration_dir, dark_optimization,
                    cc_method, hot_sigma, cold_sigma,
                    max_structure_size, enable_cosmetic_correction,
                )
            else:
                return self._run_production(
                    light_path, output_dir,
                    master_bias, master_dark, master_flat,
                    calibration_dir, dark_optimization,
                    cc_method, hot_sigma, cold_sigma,
                    max_structure_size, enable_cosmetic_correction,
                )
        except Exception as e:
            logger.error("管线运行失败: %s", e, exc_info=True)
            return {
                "success": False, "output_path": None,
                "calibrated_stats": {}, "cc_stats": {},
                "error": str(e), "light_path": light_path,
            }

    # ---------- debug 模式 ----------

    def _run_debug(self, light_path, output_dir,
                   master_bias, master_dark, master_flat,
                   calibration_dir, dark_optimization,
                   cc_method, hot_sigma, cold_sigma,
                   max_structure_size, enable_cosmetic_correction):
        """debug 模式：分步落盘，校准输出 _calibrated.fits，坏点修复输出 _final.fits"""
        base = os.path.splitext(os.path.basename(light_path))[0]
        calibrated_path = os.path.join(output_dir, base + "_calibrated.fits")
        final_path = os.path.join(output_dir, base + "_final.fits")

        # 读取 Light 元数据用于主帧匹配
        exptime, filt, _ = self._read_light_meta(light_path)
        bias_path, dark_path, flat_path = self._resolve_master_paths(
            exptime, filt, master_bias, master_dark, master_flat, calibration_dir,
        )

        # 1. 校准（文件模式，显式传入匹配后的主帧路径，不再让 calibrate_frame 二次匹配）
        logger.info("-" * 40)
        logger.info("[debug] 步骤1: 图像校准 -> %s", calibrated_path)
        cal_result = self.calibrator.calibrate_frame(
            light_path, calibrated_path,
            master_bias=bias_path, master_dark=dark_path, master_flat=flat_path,
            dark_optimization=dark_optimization, calibration_dir=None,
        )
        calibrated_stats = cal_result.get("stats", {})
        self._log_stats("Light", calibrated_stats)

        if not cal_result.get("success"):
            logger.error("[debug] 校准失败，跳过坏点修复: %s", cal_result.get("error"))
            return {
                "success": False, "output_path": calibrated_path,
                "calibrated_stats": calibrated_stats, "cc_stats": {},
                "error": cal_result.get("error"), "light_path": light_path,
            }

        # 2. 坏点修复（两种检测：Dark热像素 + Bias冷像素）
        if not enable_cosmetic_correction:
            logger.info("[debug] 坏点修复已禁用，管线完成")
            return {
                "success": True, "output_path": calibrated_path,
                "calibrated_stats": calibrated_stats, "cc_stats": {},
            }

        logger.info("-" * 40)
        logger.info("[debug] 步骤2: 坏点修复 -> %s", final_path)
        cc_result = correct_frame(
            calibrated_path, final_path,
            hot_sigma=hot_sigma, cold_sigma=cold_sigma,
            method=cc_method, max_structure_size=max_structure_size,
            master_dark=dark_path, master_bias=bias_path,
            reader=self._reader, writer=self._writer,
        )
        cc_stats = cc_result

        if cc_result.get("success"):
            logger.info(
                "[debug] 坏点修复完成: 热坏点=%d, 冷坏点=%d, 总修复=%d",
                cc_result.get("hot_pixels", 0), cc_result.get("cold_pixels", 0),
                cc_result.get("total_bad", 0),
            )
            output_path = final_path
            success = True
        else:
            logger.error("[debug] 坏点修复失败: %s", cc_result.get("error"))
            output_path = calibrated_path
            success = False

        return {
            "success": success, "output_path": output_path,
            "calibrated_stats": calibrated_stats, "cc_stats": cc_stats,
            "calibrated_path": calibrated_path,
        }

    # ---------- production 模式 ----------

    def _run_production(self, light_path, output_dir,
                        master_bias, master_dark, master_flat,
                        calibration_dir, dark_optimization,
                        cc_method, hot_sigma, cold_sigma,
                        max_structure_size, enable_cosmetic_correction):
        """production 模式：内存直通，只写一次最终 FITS"""
        base = os.path.splitext(os.path.basename(light_path))[0]
        output_path = os.path.join(output_dir, base + "_calibrated.fits")

        # 1. 读取 Light 帧
        logger.info("-" * 40)
        logger.info("[production] 读取 Light 帧: %s", light_path)
        img = self._reader.read(light_path)
        light_data = img.data.astype(np.float32, copy=True)
        light_keywords = img.keywords
        light_exposure = img.get_keyword_float("EXPTIME", 0.0)
        light_filter = img.get_keyword("FILTER", "") or ""
        light_bitpix = img.get_keyword_int("BITPIX", 16)
        img.close()
        logger.info(
            "Light 加载: shape=%s, EXPTIME=%.2fs, FILTER=%s, BITPIX=%d",
            str(light_data.shape), light_exposure, light_filter, light_bitpix,
        )

        # 2. 匹配主帧路径并加载数据
        bias_path, dark_path, flat_path = self._resolve_master_paths(
            light_exposure, light_filter, master_bias, master_dark, master_flat, calibration_dir,
        )

        bias_data = None
        bias_bitpix = None
        dark_data = None
        dark_exposure = 0.0
        dark_bitpix = None
        flat_data = None
        flat_bitpix = None

        if bias_path is not None:
            bimg = self._reader.read(bias_path)
            bias_data = bimg.data.astype(np.float32, copy=True)
            bias_bitpix = bimg.get_keyword_int("BITPIX", -32)
            bimg.close()
            logger.info("Master Bias 加载: shape=%s, BITPIX=%d", str(bias_data.shape), bias_bitpix)
        if dark_path is not None:
            dimg = self._reader.read(dark_path)
            dark_data = dimg.data.astype(np.float32, copy=True)
            dark_exposure = dimg.get_keyword_float("EXPTIME", 0.0)
            dark_bitpix = dimg.get_keyword_int("BITPIX", -32)
            dimg.close()
            logger.info(
                "Master Dark 加载: shape=%s, EXPTIME=%.2fs, BITPIX=%d",
                str(dark_data.shape), dark_exposure, dark_bitpix,
            )
        if flat_path is not None:
            fimg = self._reader.read(flat_path)
            flat_data = fimg.data.astype(np.float32, copy=True)
            flat_bitpix = fimg.get_keyword_int("BITPIX", -32)
            fimg.close()
            logger.info("Master Flat 加载: shape=%s, BITPIX=%d", str(flat_data.shape), flat_bitpix)

        # 3. 统一数据范围（与 calibrate_file 模式一致，calibrate_data 假设上游已统一）
        if bias_data is not None:
            light_data, bias_data = unify_data_range(light_data, light_bitpix, bias_data, bias_bitpix)
        if dark_data is not None:
            light_data, dark_data = unify_data_range(light_data, light_bitpix, dark_data, dark_bitpix)
        if flat_data is not None:
            light_data, flat_data = unify_data_range(light_data, light_bitpix, flat_data, flat_bitpix)

        # 4. 内存校准
        logger.info("-" * 40)
        logger.info("[production] 步骤1: 内存校准")
        calibrated, cal_stats = self.calibrator.calibrate_data(
            light_data,
            master_bias=bias_data, master_dark=dark_data, master_flat=flat_data,
            dark_optimization=dark_optimization,
            light_exposure=light_exposure, dark_exposure=dark_exposure,
        )
        self._log_stats("Light", cal_stats)
        calibrated_stats = cal_stats

        # 5. 内存坏点修复（两种检测：Dark热像素 + Bias冷像素）
        final_data = calibrated
        cc_stats = {}
        if enable_cosmetic_correction:
            logger.info("-" * 40)
            logger.info("[production] 步骤2: 内存坏点修复")
            corrected, cc_info = self.cosmetic_corrector.correct_data(
                calibrated,
                hot_sigma=hot_sigma, cold_sigma=cold_sigma,
                method=cc_method, max_structure_size=max_structure_size,
                dark_data=dark_data, bias_data=bias_data,
            )
            final_data = corrected
            cc_stats = cc_info
            if cc_info.get("success"):
                logger.info(
                    "[production] 坏点修复完成: 热像素=%d, 冷像素=%d, 总修复=%d",
                    cc_info.get("hot_pixels", 0), cc_info.get("cold_pixels", 0),
                    cc_info.get("total_bad", 0),
                )
            else:
                logger.warning(
                    "[production] 坏点修复失败: %s（仍输出校准结果）", cc_info.get("error"),
                )

        # 6. 写最终 FITS（只写一次）
        # 过滤 BZERO/BSCALE：校准后为 float32 数据，不应携带原始 16 位帧的 BZERO/BSCALE
        out_keywords = [
            kw for kw in (light_keywords or [])
            if kw.name.upper() not in ("BZERO", "BSCALE")
        ]
        out_keywords.append(FITSKeywordPy(name="CALIBRAT", value="T", comment="Calibrated"))
        out_keywords.append(FITSKeywordPy(name="PIPEMODE", value=self.mode, comment="Pipeline mode"))
        if cc_stats:
            out_keywords.append(FITSKeywordPy(
                name="CCHOT", value=str(cc_stats.get("hot_pixels", 0)),
                comment="Hot pixels detected",
            ))
            out_keywords.append(FITSKeywordPy(
                name="CCCOLD", value=str(cc_stats.get("cold_pixels", 0)),
                comment="Cold pixels detected",
            ))

        self._writer.write(final_data, output_path, keywords=out_keywords, float_sample=True)
        logger.info("[production] 最终 FITS 写入完成: %s", output_path)

        return {
            "success": True, "output_path": output_path,
            "calibrated_stats": calibrated_stats, "cc_stats": cc_stats,
        }


# ============================ 命令行入口 ============================

def main():
    """
    命令行入口，支持以下参数：
    --light: Light 帧路径（必需，支持多个）
    --output-dir: 输出目录（必需）
    --mode: debug/production（默认 debug）
    --calibration-dir: 主校准帧目录（自动匹配）
    --master-bias: Master Bias 文件路径
    --master-dark: Master Dark 文件路径
    --master-flat: Master Flat 文件路径
    --dark-optimization: 启用暗场优化（默认关闭）
    --dark-threshold: Dark 热像素检测 sigma 倍数（默认 5.0）
    --bias-threshold: Bias 冷像素检测 sigma 倍数（默认 5.0）
    --max-neighbors-above: 孤立性检查邻居阈值（默认 2）
    --cc-method: 坏点修复方法 median/bilinear（默认 median）
    --no-cosmetic: 跳过坏点修复
    --max-workers: 并行线程数（默认 16）
    """
    parser = argparse.ArgumentParser(
        description="校准管线: 统一调度图像校准（Calibrator）与坏点修复（CosmeticCorrector）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--light", nargs="+", required=True,
                        help="Light 帧路径（支持多个）")
    parser.add_argument("--output-dir", required=True,
                        help="输出目录")
    parser.add_argument("--mode", default="debug", choices=["debug", "production"],
                        help="运行模式: debug=分步输出中间FITS, production=内存直通只输出最终FITS (默认: debug)")
    parser.add_argument("--calibration-dir", default=None,
                        help="主校准帧目录（自动匹配 Bias/Dark/Flat）")
    parser.add_argument("--master-bias", default=None,
                        help="Master Bias 文件路径")
    parser.add_argument("--master-dark", default=None,
                        help="Master Dark 文件路径")
    parser.add_argument("--master-flat", default=None,
                        help="Master Flat 文件路径")
    parser.add_argument("--dark-optimization", action="store_true",
                        help="启用暗场优化（默认关闭）")
    parser.add_argument("--hot-sigma", type=float, default=5.0,
                        help="热坏点检测 sigma 倍数（默认: 5.0）")
    parser.add_argument("--cold-sigma", type=float, default=5.0,
                        help="冷坏点检测 sigma 倍数（默认: 5.0）")
    parser.add_argument("--max-structure-size", type=int, default=4,
                        help="最大结构大小，>=此值的连通区域视为星点（默认: 4）")
    parser.add_argument("--cc-method", default="median", choices=["median", "bilinear"],
                        help="坏点修复方法: median/bilinear (默认: median)")
    parser.add_argument("--no-cosmetic", action="store_true",
                        help="跳过坏点修复")
    parser.add_argument("--max-workers", type=int, default=16,
                        help="并行线程数（默认: 16）")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    pipeline = CalibrationPipeline(mode=args.mode, max_workers=args.max_workers)

    results = []
    n_total = len(args.light)
    n_success = 0
    for i, light_path in enumerate(args.light):
        logger.info("#" * 60)
        logger.info("处理帧 %d/%d: %s", i + 1, n_total, light_path)
        result = pipeline.run(
            light_path, args.output_dir,
            master_bias=args.master_bias, master_dark=args.master_dark,
            master_flat=args.master_flat, calibration_dir=args.calibration_dir,
            dark_optimization=args.dark_optimization,
            cc_method=args.cc_method,
            hot_sigma=args.hot_sigma, cold_sigma=args.cold_sigma,
            max_structure_size=args.max_structure_size,
            enable_cosmetic_correction=not args.no_cosmetic,
        )
        results.append(result)
        if result.get("success"):
            n_success += 1
        else:
            # 单帧失败不中断批量处理
            logger.warning("帧 %d 失败，继续处理后续帧: %s", i + 1, result.get("error"))

    logger.info("#" * 60)
    logger.info("批量处理完成: 成功 %d/%d", n_success, n_total)
    for i, r in enumerate(results):
        status = "成功" if r.get("success") else "失败"
        logger.info("  帧 %d: %s -> %s", i + 1, status, r.get("output_path"))

    return 0 if n_success == n_total else 1


if __name__ == "__main__":
    sys.exit(main())
