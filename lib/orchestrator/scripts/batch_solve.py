# -*- coding: utf-8 -*-
"""
批量解析脚本 (Batch Plate Solving)
功能: 扫描所有校准后FITS文件，调用solve_and_write_wcs批量解析并写入WCS
用途: 在批量校准完成后，对所有01_calibrated.fits执行plate solving
调用:
    python batch_solve.py                          # 解析全部
    python batch_solve.py --max-workers 16         # 指定线程数
    python batch_solve.py --datasets Galaxy_Center_T4  # 指定数据集
依赖: solve_and_write_wcs (lib/plate_solve/python/)
"""

from __future__ import annotations

import os
import sys
import json
import time
import logging
import argparse
import glob
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed
from astropy.io import fits as fits

# ============================ 环境初始化 ============================

PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
)
MINGW_BIN = r"C:\msys64\mingw64\bin"

if MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

# astro_image_io.dll 所在目录
_ASTRO_IO_DIR = os.path.join(PROJECT_ROOT, "lib", "astro_image_io")
if _ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

# sys.path
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python"))
sys.path.insert(
    0,
    os.path.join(
        PROJECT_ROOT, "lib", "plate_solve", "archive", "vector_method", "python", "python"
    ),
)
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "star_detector", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "photometric_calib", "flux_calibrator", "python"))


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "logs", "batch_solve"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] [%(threadName)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "batch_solve_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("batch_solve")
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


# ============================ 主逻辑 ============================

def scan_calibrated_fits(results_dir: str, datasets: list = None) -> list:
    """扫描所有校准后FITS文件

    Args:
        results_dir: 结果根目录 (testdata/results)
        datasets: 可选，指定数据集列表过滤

    Returns:
        list: FITS文件路径列表
    """
    pattern = os.path.join(results_dir, "**", "01_calibrated.fits")
    all_files = glob.glob(pattern, recursive=True)

    if datasets:
        filtered = []
        for f in all_files:
            # 从路径中提取数据集名
            rel = os.path.relpath(f, results_dir)
            parts = rel.split(os.sep)
            if parts and parts[0] in datasets:
                filtered.append(f)
        all_files = filtered

    all_files.sort()
    return all_files


def solve_single(fits_path: str) -> dict:
    """解析单个FITS文件（线程安全，每线程独立实例）

    使用 solve_and_write_wcs._batch_worker 实现线程本地环境复用，
    避免每帧重新初始化 GaiaClient/StarDetector/IPVSolver（省3-5秒/帧）。

    跳过已解析的帧（FITS头中已含CTYPE1关键字）以支持断点续跑。

    Args:
        fits_path: FITS文件路径

    Returns:
        dict: {success, fits_path, rms_arcsec, n_pairs, error}
    """
    # 跳过已解析的帧（断点续跑）
    try:
        with fits.open(fits_path, mode="readonly") as hdul:
            if "CTYPE1" in hdul[0].header:
                logger.info("跳过（已解析）: %s", os.path.basename(fits_path))
                return {
                    "success": True,
                    "fits_path": fits_path,
                    "rms_arcsec": 0.0,
                    "n_pairs": 0,
                    "error": "skipped (already solved)",
                }
    except Exception:
        pass  # 读取失败时继续尝试解析

    try:
        from solve_and_write_wcs import _batch_worker

        result = _batch_worker(
            fits_path, ra0=0.0, dec0=0.0,
            focal_length=0.0, pixel_size=0.0, overwrite=True,
        )
        return {
            "success": result.get("success", False),
            "fits_path": fits_path,
            "rms_arcsec": result.get("rms_arcsec", 0.0),
            "n_pairs": result.get("n_pairs", 0),
            "error": result.get("error", ""),
        }
    except Exception as e:
        return {
            "success": False,
            "fits_path": fits_path,
            "rms_arcsec": 0.0,
            "n_pairs": 0,
            "error": str(e),
        }


def run_batch_solve(fits_files: list, max_workers: int = 16) -> list:
    """批量解析

    Args:
        fits_files: FITS文件路径列表
        max_workers: 最大并行线程数

    Returns:
        list: 每个文件的结果字典列表
    """
    total = len(fits_files)
    logger.info("=" * 60)
    logger.info("批量解析启动")
    logger.info("  总帧数: %d", total)
    logger.info("  并行线程: %d", max_workers)
    logger.info("=" * 60)

    results = []
    success_count = 0
    fail_count = 0
    start_time = time.time()

    with ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="Solve") as executor:
        future_to_path = {
            executor.submit(solve_single, path): path for path in fits_files
        }

        for i, future in enumerate(as_completed(future_to_path), 1):
            path = future_to_path[future]
            try:
                result = future.result()
                results.append(result)

                if result["success"]:
                    success_count += 1
                    logger.info(
                        "[%d/%d] 成功: %s (RMS=%.3f\", n=%d)",
                        i, total,
                        os.path.basename(os.path.dirname(path)),
                        result["rms_arcsec"],
                        result["n_pairs"],
                    )
                else:
                    fail_count += 1
                    logger.warning(
                        "[%d/%d] 失败: %s (%s)",
                        i, total,
                        os.path.basename(os.path.dirname(path)),
                        result["error"][:100],
                    )
            except Exception as e:
                fail_count += 1
                results.append({
                    "success": False,
                    "fits_path": path,
                    "rms_arcsec": 0.0,
                    "n_pairs": 0,
                    "error": str(e),
                })
                logger.error("[%d/%d] 异常: %s (%s)", i, total, path, str(e)[:100])

    elapsed = time.time() - start_time
    logger.info("=" * 60)
    logger.info("批量解析完成")
    logger.info("  成功: %d / %d (%.1f%%)", success_count, total, 100.0 * success_count / max(total, 1))
    logger.info("  失败: %d", fail_count)
    logger.info("  耗时: %.1f 秒 (%.2f 秒/帧)", elapsed, elapsed / max(total, 1))
    logger.info("=" * 60)

    # 保存结果摘要
    summary_path = os.path.join(_LOG_DIR, "batch_solve_summary_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump({
            "total": total,
            "success": success_count,
            "fail": fail_count,
            "elapsed_sec": elapsed,
            "results": results,
        }, f, ensure_ascii=False, indent=2)
    logger.info("结果摘要已保存: %s", summary_path)

    return results


# ============================ 命令行入口 ============================

def main():
    parser = argparse.ArgumentParser(
        description="批量解析: 扫描校准后FITS文件，调用IPVSolver解析并写入WCS",
    )
    parser.add_argument(
        "--results-dir",
        default=os.path.join(PROJECT_ROOT, "testdata", "results"),
        help="校准结果根目录 (默认: testdata/results)",
    )
    parser.add_argument(
        "--datasets",
        nargs="*",
        default=None,
        help="指定数据集列表（空格分隔，默认全部）",
    )
    parser.add_argument(
        "--max-workers",
        type=int,
        default=16,
        help="并行线程数 (默认: 16)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="仅扫描不执行解析",
    )

    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    # 扫描校准后FITS文件
    fits_files = scan_calibrated_fits(args.results_dir, args.datasets)

    logger.info("扫描完成: 找到 %d 个校准后FITS文件", len(fits_files))
    if not fits_files:
        logger.warning("未找到任何校准后FITS文件，请先执行批量校准")
        return 1

    if args.dry_run:
        for f in fits_files[:20]:
            logger.info("  %s", f)
        if len(fits_files) > 20:
            logger.info("  ... (共 %d 个)", len(fits_files))
        return 0

    # 执行批量解析
    results = run_batch_solve(fits_files, max_workers=args.max_workers)

    # 输出摘要
    success_count = sum(1 for r in results if r["success"])
    print(json.dumps({
        "total": len(results),
        "success": success_count,
        "fail": len(results) - success_count,
        "success_rate": 100.0 * success_count / max(len(results), 1),
    }, ensure_ascii=False))

    return 0 if success_count > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
