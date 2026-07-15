# -*- coding: utf-8 -*-
"""
Step1 校准阶段脚本 (Calibration Stage)
功能: 调用 CalibrationPipeline 以 production 模式校准 Light 帧，输出校准后的 FITS 文件。
用途: 全链路整合测试的第一步，将原始 Light 帧经过图像校准 + 坏点修复后输出 01_calibrated.fits，
      供后续 step2_solve.py 进行 plate solving。
接口:
    from calibration_pipeline import CalibrationPipeline
    pipe = CalibrationPipeline(mode="production", max_workers=16)
    result = pipe.run(light_path, output_dir, calibration_dir=..., ...)
    # 返回 {success, output_path, calibrated_stats, cc_stats, error, light_path}
依赖: numpy, shutil, logging, argparse,
      astro_image_io (间接，由 calibration_pipeline 内部使用),
      calibration_pipeline.CalibrationPipeline
调用:
    python step1_calibrate.py --light light.fits --output-dir output/ --calibration-dir calibration/
    # stdout 输出 JSON: {success, output_path, stats, error}
注意:
    CalibrationPipeline.run() 的输出文件名为 {原文件名}_calibrated.fits，
    本脚本在校准完成后将其复制/重命名为固定的 01_calibrated.fits，
    便于下游步骤以固定文件名引用。
"""

from __future__ import annotations

import os
import sys
import json
import shutil
import logging
import argparse
from datetime import datetime

# ---- 将 lib/calibration/python 加入 sys.path ----
_PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
)
_CALIBRATION_PY_DIR = os.path.join(_PROJECT_ROOT, "lib", "calibration", "python")
if _CALIBRATION_PY_DIR not in sys.path:
    sys.path.insert(0, _CALIBRATION_PY_DIR)

from calibration_pipeline import CalibrationPipeline


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
        "step1_calibrate_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("step1_calibrate")
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


# ============================ 固定输出文件名 ============================

_OUTPUT_FILENAME = "01_calibrated.fits"


# ============================ 主入口 ============================

def run(light_path: str, output_dir: str, calibration_dir: str) -> dict:
    """运行校准阶段

    Args:
        light_path: Light 帧路径
        output_dir: 输出目录
        calibration_dir: 主校准帧目录（自动匹配 Bias/Dark/Flat）

    Returns:
        dict: {success, output_path, stats, error}
            - success: bool，校准是否成功
            - output_path: str，最终输出文件路径（01_calibrated.fits）
            - stats: dict，校准统计信息（calibrated_stats + cc_stats）
            - error: str，错误信息（成功时为空）
    """
    logger.info("=" * 60)
    logger.info("Step1 校准阶段启动")
    logger.info("  Light 帧: %s", light_path)
    logger.info("  输出目录: %s", output_dir)
    logger.info("  校准帧目录: %s", calibration_dir)
    logger.info("  固定输出文件名: %s", _OUTPUT_FILENAME)

    # 校验输入
    if not os.path.isfile(light_path):
        err = f"Light 帧不存在: {light_path}"
        logger.error(err)
        return {
            "success": False,
            "output_path": None,
            "stats": {},
            "error": err,
        }

    if not os.path.isdir(calibration_dir):
        err = f"校准帧目录不存在: {calibration_dir}"
        logger.error(err)
        return {
            "success": False,
            "output_path": None,
            "stats": {},
            "error": err,
        }

    os.makedirs(output_dir, exist_ok=True)

    # 创建管线（production 模式，16 线程）
    logger.info("-" * 40)
    logger.info("创建 CalibrationPipeline: mode=production, max_workers=16")
    try:
        pipe = CalibrationPipeline(mode="production", max_workers=16)
    except Exception as e:
        err = f"CalibrationPipeline 初始化失败: {e}"
        logger.error(err, exc_info=True)
        return {
            "success": False,
            "output_path": None,
            "stats": {},
            "error": err,
        }

    # 调用 pipe.run()
    logger.info("-" * 40)
    logger.info("调用 pipe.run() 进行校准")
    try:
        result = pipe.run(
            light_path,
            output_dir,
            calibration_dir=calibration_dir,
        )
    except Exception as e:
        err = f"pipe.run() 异常: {e}"
        logger.error(err, exc_info=True)
        return {
            "success": False,
            "output_path": None,
            "stats": {},
            "error": err,
        }

    # 检查校准结果
    if not result.get("success"):
        err = result.get("error", "未知错误")
        logger.error("校准失败: %s", err)
        return {
            "success": False,
            "output_path": None,
            "stats": {
                "calibrated_stats": result.get("calibrated_stats", {}),
                "cc_stats": result.get("cc_stats", {}),
            },
            "error": err,
        }

    # 获取原始输出路径（{原文件名}_calibrated.fits）
    original_output = result.get("output_path")
    if not original_output or not os.path.isfile(original_output):
        err = f"校准输出文件不存在: {original_output}"
        logger.error(err)
        return {
            "success": False,
            "output_path": None,
            "stats": {
                "calibrated_stats": result.get("calibrated_stats", {}),
                "cc_stats": result.get("cc_stats", {}),
            },
            "error": err,
        }

    logger.info("校准成功，原始输出: %s", original_output)

    # 复制/重命名为固定文件名 01_calibrated.fits
    final_output = os.path.join(output_dir, _OUTPUT_FILENAME)
    logger.info("-" * 40)
    logger.info("重命名/复制为固定输出文件名: %s", final_output)

    try:
        if os.path.normpath(original_output) == os.path.normpath(final_output):
            # 路径相同，无需操作
            logger.info("原始输出路径与目标路径相同，无需复制")
        else:
            # 使用 shutil.copy2 复制文件（保留元数据）
            shutil.copy2(original_output, final_output)
            logger.info("文件复制完成: %s -> %s", original_output, final_output)
            # 删除原始文件（如果路径不同）
            try:
                os.remove(original_output)
                logger.info("已删除原始输出文件: %s", original_output)
            except OSError as e:
                logger.warning("删除原始输出文件失败（不影响结果）: %s, %s", original_output, e)
    except Exception as e:
        err = f"复制/重命名输出文件失败: {e}"
        logger.error(err, exc_info=True)
        return {
            "success": False,
            "output_path": None,
            "stats": {
                "calibrated_stats": result.get("calibrated_stats", {}),
                "cc_stats": result.get("cc_stats", {}),
            },
            "error": err,
        }

    # 汇总统计信息
    stats = {
        "calibrated_stats": result.get("calibrated_stats", {}),
        "cc_stats": result.get("cc_stats", {}),
    }

    logger.info("-" * 40)
    logger.info("Step1 校准阶段完成")
    logger.info("  最终输出: %s", final_output)
    logger.info("=" * 60)

    return {
        "success": True,
        "output_path": final_output,
        "stats": stats,
        "error": "",
    }


# ============================ 命令行入口 ============================

def main():
    """
    命令行入口，支持以下参数：
    --light: Light 帧路径（必需）
    --output-dir: 输出目录（必需）
    --calibration-dir: 主校准帧目录（必需，自动匹配 Bias/Dark/Flat）
    """
    parser = argparse.ArgumentParser(
        description="Step1 校准阶段: 调用 CalibrationPipeline 校准 Light 帧，输出 01_calibrated.fits",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--light", required=True,
                        help="Light 帧路径")
    parser.add_argument("--output-dir", required=True,
                        help="输出目录")
    parser.add_argument("--calibration-dir", required=True,
                        help="主校准帧目录（自动匹配 Bias/Dark/Flat）")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    result = run(
        light_path=args.light,
        output_dir=args.output_dir,
        calibration_dir=args.calibration_dir,
    )

    # 输出 JSON 到 stdout
    print(json.dumps(result, ensure_ascii=True, default=str))
    return 0 if result.get("success") else 1


if __name__ == "__main__":
    sys.exit(main())
