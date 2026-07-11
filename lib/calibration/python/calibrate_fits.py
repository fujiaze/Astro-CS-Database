# -*- coding: utf-8 -*-
"""
FITS图像校准统一接口
功能: 单帧/批量校准Light帧（Bias/Dark/Flat + 坏点修复）
用途: 提供简洁的Python接口，内部调用CalibrationPipeline完成校准
依赖: calibration_pipeline.CalibrationPipeline
调用:
    from calibrate_fits import calibrate_fits, calibrate_batch

    # 单帧校准
    result = calibrate_fits(
        light_path="Light_001.fts",
        output_path="01_calibrated.fits",
        calibration_dir="T4 calibration files/",
    )

    # 批量校准（16线程并行）
    results = calibrate_batch(
        light_paths=["Light_001.fts", "Light_002.fts", ...],
        output_dir="results/",
        calibration_dir="T4 calibration files/",
        max_workers=16,
    )
"""

from __future__ import annotations

import os
import sys
import shutil
import logging
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

# 添加模块路径
_LIB_BASE = os.path.dirname(os.path.abspath(__file__))
if _LIB_BASE not in sys.path:
    sys.path.insert(0, _LIB_BASE)

from calibration_pipeline import CalibrationPipeline


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "calibrate_fits_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("calibrate_fits")
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


# ============================ 单帧校准 ============================

def calibrate_fits(light_path, output_path, calibration_dir,
                   mode="production", cc_method="median",
                   hot_sigma=5.0, cold_sigma=5.0,
                   max_structure_size=4, max_workers=16,
                   dark_optimization=False, enable_cosmetic_correction=True):
    """单帧校准

    内部调用 CalibrationPipeline 完成校准（Bias/Dark/Flat + 坏点修复），
    并将管线默认输出（{base}_calibrated.fits）重命名为指定的 output_path。

    Args:
        light_path: str，Light 帧路径
        output_path: str，输出 FITS 路径（校准后写入此路径）
        calibration_dir: str，校准文件目录（含 master_bias/dark/flat）
        mode: str，"production" 或 "debug"（默认 production，内存直通只写一次）
        cc_method: str，坏点修复方法 ("median" 或 "bilinear")
        hot_sigma: float，Dark 热像素检测 sigma 倍数（默认 5.0）
        cold_sigma: float，Bias 冷像素检测 sigma 倍数（默认 5.0）
        max_structure_size: int，最大结构大小，>=此值的连通区域视为星点（默认 4）
        max_workers: int，管线内部并行线程数（默认 16）
        dark_optimization: bool，是否启用暗场优化（默认关闭）
        enable_cosmetic_correction: bool，是否启用坏点修复（默认 True）

    Returns:
        dict: {success, output_path, stats, error, light_path}
            - success: bool，校准是否成功
            - output_path: str，最终输出文件路径（等于入参 output_path）
            - stats: dict，{calibrated_stats, cc_stats}
            - error: str，错误信息（成功时为空）
            - light_path: str，输入 Light 帧路径
    """
    logger.info("=" * 60)
    logger.info("单帧校准启动")
    logger.info("  Light 帧: %s", light_path)
    logger.info("  输出路径: %s", output_path)
    logger.info("  校准帧目录: %s", calibration_dir)
    logger.info("  模式: %s, cc_method=%s, max_workers=%d", mode, cc_method, max_workers)

    # 校验输入
    if not os.path.isfile(light_path):
        err = f"Light 帧不存在: {light_path}"
        logger.error(err)
        return {"success": False, "output_path": None, "stats": {}, "error": err,
                "light_path": light_path}
    if not os.path.isdir(calibration_dir):
        err = f"校准帧目录不存在: {calibration_dir}"
        logger.error(err)
        return {"success": False, "output_path": None, "stats": {}, "error": err,
                "light_path": light_path}

    output_dir = os.path.dirname(os.path.abspath(output_path))
    os.makedirs(output_dir, exist_ok=True)

    # 创建管线并运行
    logger.info("-" * 40)
    try:
        pipeline = CalibrationPipeline(mode=mode, max_workers=max_workers)
    except Exception as e:
        err = f"CalibrationPipeline 初始化失败: {e}"
        logger.error(err, exc_info=True)
        return {"success": False, "output_path": None, "stats": {}, "error": err,
                "light_path": light_path}

    try:
        result = pipeline.run(
            light_path, output_dir,
            calibration_dir=calibration_dir,
            dark_optimization=dark_optimization,
            cc_method=cc_method,
            hot_sigma=hot_sigma, cold_sigma=cold_sigma,
            max_structure_size=max_structure_size,
            enable_cosmetic_correction=enable_cosmetic_correction,
        )
    except Exception as e:
        err = f"pipeline.run() 异常: {e}"
        logger.error(err, exc_info=True)
        return {"success": False, "output_path": None, "stats": {}, "error": err,
                "light_path": light_path}

    if not result.get("success"):
        err = result.get("error", "未知错误")
        logger.error("校准失败: %s", err)
        return {"success": False, "output_path": None, "error": err,
                "stats": {
                    "calibrated_stats": result.get("calibrated_stats", {}),
                    "cc_stats": result.get("cc_stats", {}),
                },
                "light_path": light_path}

    # 管线默认输出路径（{base}_calibrated.fits 或 {base}_final.fits）
    original_output = result.get("output_path")
    if not original_output or not os.path.isfile(original_output):
        err = f"校准输出文件不存在: {original_output}"
        logger.error(err)
        return {"success": False, "output_path": None, "error": err,
                "stats": {
                    "calibrated_stats": result.get("calibrated_stats", {}),
                    "cc_stats": result.get("cc_stats", {}),
                },
                "light_path": light_path}

    logger.info("校准成功，原始输出: %s", original_output)

    # 重命名为用户指定的 output_path
    final_output = os.path.abspath(output_path)
    if os.path.normpath(original_output) != os.path.normpath(final_output):
        logger.info("-" * 40)
        logger.info("重命名为目标输出: %s", final_output)
        try:
            # 若目标已存在则先删除
            if os.path.isfile(final_output):
                os.remove(final_output)
            shutil.move(original_output, final_output)
            logger.info("重命名完成: %s -> %s", original_output, final_output)
        except Exception as e:
            err = f"重命名输出文件失败: {e}"
            logger.error(err, exc_info=True)
            return {"success": False, "output_path": original_output, "error": err,
                    "stats": {
                        "calibrated_stats": result.get("calibrated_stats", {}),
                        "cc_stats": result.get("cc_stats", {}),
                    },
                    "light_path": light_path}
    else:
        final_output = original_output

    stats = {
        "calibrated_stats": result.get("calibrated_stats", {}),
        "cc_stats": result.get("cc_stats", {}),
    }

    logger.info("-" * 40)
    logger.info("单帧校准完成: %s", final_output)
    logger.info("=" * 60)

    return {
        "success": True,
        "output_path": final_output,
        "stats": stats,
        "error": "",
        "light_path": light_path,
    }


# ============================ 批量校准 ============================

def calibrate_batch(light_paths, output_dir, calibration_dir,
                    max_workers=16, mode="production",
                    cc_method="median", hot_sigma=5.0, cold_sigma=5.0,
                    max_structure_size=4, dark_optimization=False,
                    enable_cosmetic_correction=True):
    """批量校准（多线程并行）

    每个 Light 帧在 output_dir 下创建独立子目录，避免文件名冲突。
    每个线程内部使用单线程管线（max_workers=1），避免嵌套线程池。

    Args:
        light_paths: list[str]，Light 帧路径列表
        output_dir: str，输出根目录（每个 Light 帧在其下创建子目录）
        calibration_dir: str，校准文件目录
        max_workers: int，并行线程数（默认 16）
        mode: str，"production" 或 "debug"（默认 production）
        cc_method: str，坏点修复方法 ("median" 或 "bilinear")
        hot_sigma: float，Dark 热像素检测 sigma 倍数
        cold_sigma: float，Bias 冷像素检测 sigma 倍数
        max_structure_size: int，最大结构大小
        dark_optimization: bool，是否启用暗场优化
        enable_cosmetic_correction: bool，是否启用坏点修复

    Returns:
        list[dict]: 每帧的校准结果（顺序与输入一致）
            {success, output_path, stats, error, light_path}
    """
    logger.info("=" * 60)
    logger.info("批量校准启动: %d 帧, max_workers=%d", len(light_paths), max_workers)
    logger.info("  输出根目录: %s", output_dir)
    logger.info("  校准帧目录: %s", calibration_dir)

    os.makedirs(output_dir, exist_ok=True)

    # 预计算每帧的输出路径
    frame_specs = []
    for light_path in light_paths:
        base = os.path.splitext(os.path.basename(light_path))[0]
        frame_output_dir = os.path.join(output_dir, base)
        output_path = os.path.join(frame_output_dir, "01_calibrated.fits")
        frame_specs.append((light_path, output_path))

    results = [None] * len(frame_specs)

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {}
        for i, (light_path, output_path) in enumerate(frame_specs):
            future = executor.submit(
                calibrate_fits, light_path, output_path, calibration_dir,
                mode=mode, cc_method=cc_method,
                hot_sigma=hot_sigma, cold_sigma=cold_sigma,
                max_structure_size=max_structure_size,
                max_workers=1,  # 每个worker内部单线程，避免嵌套线程池
                dark_optimization=dark_optimization,
                enable_cosmetic_correction=enable_cosmetic_correction,
            )
            futures[future] = i

        for future in as_completed(futures):
            i = futures[future]
            light_path = frame_specs[i][0]
            try:
                result = future.result()
                results[i] = result
                status = "成功" if result.get("success") else "失败"
                logger.info("帧 %d/%d %s: %s", i + 1, len(frame_specs), status, light_path)
            except Exception as e:
                logger.error("帧 %d 异常: %s, %s", i + 1, light_path, e, exc_info=True)
                results[i] = {
                    "success": False, "output_path": None, "stats": {},
                    "error": str(e), "light_path": light_path,
                }

    # 汇总
    n_success = sum(1 for r in results if r and r.get("success"))
    logger.info("-" * 40)
    logger.info("批量校准完成: 成功 %d/%d", n_success, len(results))
    logger.info("=" * 60)

    return results


# ============================ 命令行入口 ============================

def main():
    """命令行入口"""
    import argparse
    import json

    parser = argparse.ArgumentParser(
        description="FITS图像校准统一接口: 单帧/批量校准Light帧",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--light", nargs="+", required=True,
                        help="Light 帧路径（支持多个，多个时启用批量模式）")
    parser.add_argument("--output-dir", required=True,
                        help="输出目录（批量模式：每帧在其下创建子目录）")
    parser.add_argument("--calibration-dir", required=True,
                        help="校准文件目录（含 master_bias/dark/flat）")
    parser.add_argument("--mode", default="production", choices=["debug", "production"],
                        help="运行模式 (默认: production)")
    parser.add_argument("--cc-method", default="median", choices=["median", "bilinear"],
                        help="坏点修复方法 (默认: median)")
    parser.add_argument("--hot-sigma", type=float, default=5.0,
                        help="热像素检测 sigma 倍数 (默认: 5.0)")
    parser.add_argument("--cold-sigma", type=float, default=5.0,
                        help="冷像素检测 sigma 倍数 (默认: 5.0)")
    parser.add_argument("--max-structure-size", type=int, default=4,
                        help="最大结构大小 (默认: 4)")
    parser.add_argument("--max-workers", type=int, default=16,
                        help="并行线程数 (默认: 16)")
    parser.add_argument("--dark-optimization", action="store_true",
                        help="启用暗场优化")
    parser.add_argument("--no-cosmetic", action="store_true",
                        help="跳过坏点修复")

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    if len(args.light) == 1:
        # 单帧模式
        light_path = args.light[0]
        base = os.path.splitext(os.path.basename(light_path))[0]
        output_path = os.path.join(args.output_dir, base, "01_calibrated.fits")
        result = calibrate_fits(
            light_path, output_path, args.calibration_dir,
            mode=args.mode, cc_method=args.cc_method,
            hot_sigma=args.hot_sigma, cold_sigma=args.cold_sigma,
            max_structure_size=args.max_structure_size,
            max_workers=args.max_workers,
            dark_optimization=args.dark_optimization,
            enable_cosmetic_correction=not args.no_cosmetic,
        )
        print(json.dumps(result, ensure_ascii=True, default=str))
        return 0 if result.get("success") else 1
    else:
        # 批量模式
        results = calibrate_batch(
            args.light, args.output_dir, args.calibration_dir,
            max_workers=args.max_workers, mode=args.mode,
            cc_method=args.cc_method,
            hot_sigma=args.hot_sigma, cold_sigma=args.cold_sigma,
            max_structure_size=args.max_structure_size,
            dark_optimization=args.dark_optimization,
            enable_cosmetic_correction=not args.no_cosmetic,
        )
        print(json.dumps(results, ensure_ascii=True, default=str))
        n_success = sum(1 for r in results if r.get("success"))
        return 0 if n_success == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
