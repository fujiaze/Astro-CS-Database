# -*- coding: utf-8 -*-
"""
Run_report 可视化报告批量生成器 (7数据集 × 3通道)
功能: 为7套数据集各选1帧×3通道(L/RGB)，调用 generate_report.generate_full_report
      生成全套调试图(6张/帧)。
用途: 梯度校准效果可视化、人工审核、调试分析
调用:
    python run_report.py
注意:
    - 校准后FITS: testdata/results/{数据集}/[panel]/{filter}/{帧名}/01_calibrated.fits
    - 原始Light帧: testdata/{数据集}/lights/[panel]/{帧名}.fts
    - 输出目录: testdata/report/{数据集}/{帧名}/
"""

from __future__ import annotations

import os
import sys
import json
import logging
from datetime import datetime

# ============================ 路径常量 ============================

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", "..", ".."))
_RESULTS_DIR = os.path.join(_PROJECT_ROOT, "testdata", "results")
_TESTDATA_DIR = os.path.join(_PROJECT_ROOT, "testdata")
_REPORT_DIR = os.path.join(_PROJECT_ROOT, "testdata", "report")

# 将 generate_report.py 所目录加入 sys.path
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

_LOG_DIR = os.path.join(_SCRIPT_DIR, "..", "logs", "run_report")
os.makedirs(_LOG_DIR, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(
            os.path.join(_LOG_DIR, "run_report.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger(__name__)


# ============================ 选帧配置 ============================
# 每个数据集指定3个滤镜，脚本自动选第一个帧
# 数据集配置: (实际目录名, [filter1, filter2, filter3])
DATASETS = [
    ("Galaxy_Center_T4", ["Red", "Green", "Blue"]),
    ("Victory_Nebula_T4_Flying_Dutchman", ["Lum", "Red", "Blue"]),
    ("LDN43_T2素材_flying_dutchman", ["Lum", "Red", "Blue"]),
    ("NGC247_T2_flying_dutchman", ["Lum", "Red", "Blue"]),
    ("NGC1727_T2_flying_dutchman", ["Red", "Green", "Blue"]),
    ("NGC55_T3_flying_dutchman", ["Lum", "Red", "Blue"]),
    ("NGC83_cluster_T3_Flying_Dutchman", ["Lum", "Red", "Blue"]),
]


# ============================ 工具函数 ============================

def find_first_frame_dir(dataset_dir: str, filter_name: str) -> str | None:
    """在数据集目录下查找指定滤镜的第一个帧目录

    支持两种目录结构:
        1. {dataset}/{filter}/{frame_name}/01_calibrated.fits
        2. {dataset}/{panel}/{filter}/{frame_name}/01_calibrated.fits

    Args:
        dataset_dir: 数据集结果目录绝对路径
        filter_name: 滤镜名 (Red/Green/Blue/Lum)

    Returns:
        第一个帧目录的绝对路径，或 None
    """
    # 遍历数据集目录下的子目录
    for name in sorted(os.listdir(dataset_dir)):
        sub = os.path.join(dataset_dir, name)
        if not os.path.isdir(sub):
            continue
        # 结构1: {dataset}/{filter}/{frame_name}/
        if name.lower() == filter_name.lower():
            frame_dir = _find_first_frame_in_filter_dir(sub)
            if frame_dir:
                return frame_dir
        # 结构2: {dataset}/{panel}/{filter}/{frame_name}/
        else:
            filt_dir = os.path.join(sub, filter_name)
            if os.path.isdir(filt_dir):
                frame_dir = _find_first_frame_in_filter_dir(filt_dir)
                if frame_dir:
                    return frame_dir
    return None


def _find_first_frame_in_filter_dir(filt_dir: str) -> str | None:
    """在滤镜目录下找第一个含 01_calibrated.fits 的帧子目录"""
    for name in sorted(os.listdir(filt_dir)):
        frame_dir = os.path.join(filt_dir, name)
        if not os.path.isdir(frame_dir):
            continue
        calib = os.path.join(frame_dir, "01_calibrated.fits")
        if os.path.isfile(calib):
            return frame_dir
    return None


def find_original_light(frame_name: str, dataset_name: str) -> str | None:
    """查找原始Light帧文件

    在 testdata/{dataset}/lights/ 下递归查找 {frame_name}.fts

    Args:
        frame_name: 帧名 (目录名，不含扩展名)
        dataset_name: 数据集目录名 (用于定位 lights 目录)

    Returns:
        原始Light帧绝对路径，或 None
    """
    lights_dir = os.path.join(_TESTDATA_DIR, dataset_name, "lights")
    if not os.path.isdir(lights_dir):
        logger.warning("lights目录不存在: %s", lights_dir)
        return None
    target = frame_name + ".fts"
    # 递归查找
    for root, dirs, files in os.walk(lights_dir):
        if target in files:
            return os.path.join(root, target)
    return None


# ============================ 主函数 ============================

def main():
    """批量生成可视化报告"""
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    # 导入 generate_report 模块
    try:
        import generate_report
    except ImportError as e:
        logger.error("无法导入 generate_report 模块: %s", e)
        return 1

    logger.info("=" * 60)
    logger.info("可视化报告批量生成开始: %s", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    logger.info("结果目录: %s", _RESULTS_DIR)
    logger.info("输出目录: %s", _REPORT_DIR)
    logger.info("=" * 60)

    total = 0
    n_success = 0
    n_fail = 0
    details = []

    for dataset_name, filters in DATASETS:
        dataset_results_dir = os.path.join(_RESULTS_DIR, dataset_name)
        if not os.path.isdir(dataset_results_dir):
            logger.error("数据集目录不存在: %s", dataset_results_dir)
            continue

        logger.info("-" * 40)
        logger.info("数据集: %s (filters=%s)", dataset_name, filters)

        for filt in filters:
            # 查找帧目录
            frame_dir = find_first_frame_dir(dataset_results_dir, filt)
            if frame_dir is None:
                logger.error("[%s/%s] 未找到帧目录", dataset_name, filt)
                n_fail += 1
                details.append({
                    "dataset": dataset_name, "filter": filt,
                    "success": False, "error": "未找到帧目录"})
                continue

            frame_name = os.path.basename(frame_dir)
            calibrated = os.path.join(frame_dir, "01_calibrated.fits")

            # 查找原始Light帧
            original = find_original_light(frame_name, dataset_name)
            if original is None:
                logger.warning("[%s/%s] 原始Light帧未找到, 用01_calibrated.fits回退",
                               dataset_name, filt)
                original = calibrated

            # 输出目录: testdata/report/{数据集名}/{帧名}/
            # 用数据集的简短名（去掉后缀）作为输出目录名
            out_dir = os.path.join(_REPORT_DIR, dataset_name, frame_name)

            logger.info("[%s/%s] 帧名=%s", dataset_name, filt, frame_name)
            logger.info("  校准FITS: %s", calibrated)
            logger.info("  原始Light: %s", original)
            logger.info("  输出目录: %s", out_dir)

            total += 1
            # 调用 generate_full_report
            r = generate_report.generate_full_report(
                calibrated_fits_path=calibrated,
                original_light_path=original,
                output_dir=out_dir,
                gradient_result_dir=frame_dir,
            )
            if r["success"]:
                n_success += 1
                logger.info("[%s/%s] 成功: %d张图",
                            dataset_name, filt, len(r["plots"]))
            else:
                n_fail += 1
                logger.error("[%s/%s] 失败: %s",
                             dataset_name, filt, r.get("error", ""))

            details.append({
                "dataset": dataset_name, "filter": filt,
                "frame_name": frame_name,
                "success": r["success"],
                "output_dir": out_dir,
                "n_plots": len(r["plots"]),
                "error": r.get("error", ""),
            })

    # 输出汇总
    summary = {
        "started_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "total_frames": total,
        "success": n_success,
        "failed": n_fail,
        "details": details,
    }
    summary_path = os.path.join(_REPORT_DIR, "report_summary.json")
    os.makedirs(_REPORT_DIR, exist_ok=True)
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    logger.info("=" * 60)
    logger.info("可视化报告生成完成: 共%d帧, 成功%d, 失败%d", total, n_success, n_fail)
    logger.info("汇总报告: %s", summary_path)
    logger.info("=" * 60)

    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
