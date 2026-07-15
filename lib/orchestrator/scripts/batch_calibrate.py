# -*- coding: utf-8 -*-
"""
批量校准编排脚本 (Batch Calibration Orchestrator)
功能: 自动扫描 testdata 下所有 7 套数据集，按 数据集→panel→滤镜 分组，
      批量调用 CalibrationPipeline 对每个 Light 帧进行校准，输出 01_calibrated.fits。
用途: 全链路整合测试的批量入口，替代手动逐个调用 step1_calibrate.py，
      自动匹配 T2/T3/T4 校准文件目录，支持串行/有限并行执行。
接口:
    from calibration_pipeline import CalibrationPipeline
    pipe = CalibrationPipeline(mode="production", max_workers=16)
    result = pipe.run(light_path, output_dir, calibration_dir=...)
    # 单帧接口：light_path 为单个文件路径
依赖: logging, argparse, shutil, concurrent.futures, re,
      calibration_pipeline.CalibrationPipeline
调用:
    # 校准全部数据集（串行）
    python batch_calibrate.py
    # 指定数据集（空格分隔）
    python batch_calibrate.py --datasets Galaxy_Center_T4 NGC55_T3_flying_dutchman
    # 有限并行（同时 2 组）
    python batch_calibrate.py --max-groups 2
    # 仅打印计划不执行
    python batch_calibrate.py --dry-run
输出结构:
    testdata/results/{数据集名}/{panel}/{滤镜}/{light_basename}/01_calibrated.fits
    若无 panel（单 panel 数据集且文件名无 mosaic 标识）:
    testdata/results/{数据集名}/{滤镜}/{light_basename}/01_calibrated.fits
注意:
    CalibrationPipeline.run() 为单帧接口（内部校验 os.path.isfile），
    故每个 Light 帧独立调用一次 run()，输出到独立子目录避免文件名冲突。
    每个子目录最终包含一个 01_calibrated.fits，符合 step1 规范供下游 plate solving 使用。
"""

from __future__ import annotations

import os
import sys
import json
import shutil
import logging
import argparse
import re
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

# ---- 将 lib/calibration/python 加入 sys.path（参考 step1_calibrate.py）----
_PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
)
_CALIBRATION_PY_DIR = os.path.join(_PROJECT_ROOT, "lib", "calibration", "python")
if _CALIBRATION_PY_DIR not in sys.path:
    sys.path.insert(0, _CALIBRATION_PY_DIR)

from calibration_pipeline import CalibrationPipeline


# ============================ 常量定义 ============================

# 日志目录：lib/integration_test/logs/batch_calibrate/
_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "logs", "batch_calibrate"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"

# 固定输出文件名（与 step1 保持一致）
_OUTPUT_FILENAME = "01_calibrated.fits"

# Light 帧支持的扩展名
_LIGHT_EXTENSIONS = (".fts", ".fit")

# 望远镜标识 -> 校准文件目录名映射
_CALIBRATION_DIR_MAP = {
    "T2": "T2 calibration files",
    "T3": "T3 calibration files",
    "T4": "T4 calibration files",
}

# 内部线程池上限（开发环境 16 线程 CPU）
_MAX_INNER_WORKERS = 16


# ============================ 日志配置 ============================

def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台。

    Returns:
        logging.Logger: 配置完成的日志器实例。
    """
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "batch_calibrate_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("batch_calibrate")
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


# ============================ 工具函数 ============================

def extract_telescope(dataset_name: str) -> str | None:
    """从数据集名提取望远镜标识（T2/T3/T4）。

    匹配规则：数据集名中包含 _T2/_T3/_T4 且后面非数字（避免匹配 _T40 等）。

    Args:
        dataset_name: 数据集目录名，如 "Galaxy_Center_T4"、"LDN43_T2素材_flying_dutchman"。

    Returns:
        str | None: 望远镜标识（"T2"/"T3"/"T4"），无法识别时返回 None。
    """
    m = re.search(r"_T([234])(?!\d)", dataset_name)
    if m:
        return f"T{m.group(1)}"
    return None


def extract_filter(filename: str) -> str | None:
    """从 Light 帧文件名提取滤镜名。

    文件名格式约定：...-{曝光时间}S-{滤镜}.fts/.fit
    示例：
        Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts -> Red
        LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts -> Lum
        NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts -> OIII
        NGC55_T3_flying_dutchman-20250701@083458-1200S-Oiii.fts -> Oiii
        NGC90_2025wwk_T3_flying_dutchman-20251011@020846-600S-H-alpha.fts -> H-alpha

    Args:
        filename: Light 帧文件名（仅 basename）。

    Returns:
        str | None: 滤镜名（保留原始大小写，如 Red/Green/Blue/Lum/H-alpha/OIII/Oiii），
                   无法识别时返回 None。
    """
    # 匹配 -{数字}S-{滤镜}.fts/.fit 末尾
    m = re.search(r"-(\d+)S-([^.]+)\.(?:fts|fit)$", filename, re.IGNORECASE)
    if m:
        return m.group(2)
    return None


def extract_panel_from_filename(filename: str) -> str | None:
    """从文件名提取 mosaic 编号并规范化为 panel 标识。

    文件名中包含 mosaic1/mosaic2/mosaic3 等标识时，转换为 panel1/panel2/panel3。
    示例：
        Victory_Nebula_mosaic1_flying_dutchman-... -> panel1
        Galaxy_Center_mosaic2_T4_flying_dutchman-... -> panel2

    Args:
        filename: Light 帧文件名（仅 basename）。

    Returns:
        str | None: panel 标识（如 "panel1"），无 mosaic 标识时返回 None。
    """
    m = re.search(r"mosaic(\d+)", filename, re.IGNORECASE)
    if m:
        return f"panel{m.group(1)}"
    return None


def match_calibration_dir(telescope: str, testdata_dir: str) -> str | None:
    """根据望远镜标识匹配校准文件目录。

    Args:
        telescope: 望远镜标识（"T2"/"T3"/"T4"）。
        testdata_dir: testdata 根目录绝对路径。

    Returns:
        str | None: 校准文件目录绝对路径，不存在时返回 None。
    """
    dir_name = _CALIBRATION_DIR_MAP.get(telescope)
    if not dir_name:
        return None
    calib_path = os.path.join(testdata_dir, dir_name)
    if os.path.isdir(calib_path):
        return calib_path
    return None


def scan_dataset(dataset_path: str, dataset_name: str, testdata_dir: str) -> list[dict]:
    """扫描单个数据集的 lights 目录，返回 light 帧分组列表。

    目录结构处理：
      1. lights/panel1/*.fts, lights/panel2/*.fts ... -> panel=目录名
      2. lights/*.fts（无 panel 子目录）-> 从文件名提取 mosaic 作为 panel，无 mosaic 则 panel=None

    Args:
        dataset_path: 数据集根目录绝对路径。
        dataset_name: 数据集目录名。
        testdata_dir: testdata 根目录绝对路径（用于定位校准文件目录）。

    Returns:
        list[dict]: 分组列表，每个元素格式：
            {
                "dataset": 数据集名,
                "telescope": 望远镜标识,
                "panel": panel 标识或 None,
                "filter": 滤镜名,
                "calibration_dir": 校准文件目录,
                "light_path": light 帧绝对路径,
                "light_basename": light 帧去扩展名 basename,
            }
    """
    groups: list[dict] = []

    # 提取望远镜标识
    telescope = extract_telescope(dataset_name)
    if not telescope:
        logger.warning("数据集 '%s' 无法提取望远镜标识（T2/T3/T4），跳过", dataset_name)
        return groups

    # 匹配校准文件目录
    calibration_dir = match_calibration_dir(telescope, testdata_dir)
    if not calibration_dir:
        logger.error(
            "数据集 '%s'（%s）匹配校准文件目录失败，跳过", dataset_name, telescope
        )
        return groups

    lights_dir = os.path.join(dataset_path, "lights")
    if not os.path.isdir(lights_dir):
        logger.warning("数据集 '%s' 不存在 lights 目录: %s，跳过", dataset_name, lights_dir)
        return groups

    logger.info(
        "扫描数据集: %s（%s），校准目录: %s", dataset_name, telescope, calibration_dir
    )

    # 检查 lights 下是否有 panel 子目录
    entries = os.listdir(lights_dir)
    panel_dirs = [
        e
        for e in entries
        if os.path.isdir(os.path.join(lights_dir, e))
        and re.match(r"panel\d+", e, re.IGNORECASE)
    ]

    if panel_dirs:
        # 结构1：lights/panel{n}/*.fts
        logger.info(
            "  发现 %d 个 panel 子目录: %s", len(panel_dirs), ", ".join(sorted(panel_dirs))
        )
        for panel_dir in sorted(panel_dirs):
            panel_path = os.path.join(lights_dir, panel_dir)
            for fname in sorted(os.listdir(panel_path)):
                if not fname.lower().endswith(_LIGHT_EXTENSIONS):
                    continue
                fpath = os.path.join(panel_path, fname)
                if not os.path.isfile(fpath):
                    continue
                filt = extract_filter(fname)
                if not filt:
                    logger.warning(
                        "  无法提取滤镜，跳过: %s/%s/%s", dataset_name, panel_dir, fname
                    )
                    continue
                groups.append({
                    "dataset": dataset_name,
                    "telescope": telescope,
                    "panel": panel_dir,
                    "filter": filt,
                    "calibration_dir": calibration_dir,
                    "light_path": fpath,
                    "light_basename": os.path.splitext(fname)[0],
                })
    else:
        # 结构2：lights/*.fts（无 panel 子目录）
        logger.info("  无 panel 子目录，从文件名提取 mosaic 作为 panel 标识")
        for fname in sorted(entries):
            fpath = os.path.join(lights_dir, fname)
            if not os.path.isfile(fpath):
                continue
            if not fname.lower().endswith(_LIGHT_EXTENSIONS):
                continue
            filt = extract_filter(fname)
            if not filt:
                logger.warning(
                    "  无法提取滤镜，跳过: %s/%s", dataset_name, fname
                )
                continue
            panel = extract_panel_from_filename(fname)
            groups.append({
                "dataset": dataset_name,
                "telescope": telescope,
                "panel": panel,
                "filter": filt,
                "calibration_dir": calibration_dir,
                "light_path": fpath,
                "light_basename": os.path.splitext(fname)[0],
            })

    logger.info("  数据集 '%s' 共扫描到 %d 个 light 帧", dataset_name, len(groups))
    return groups


def scan_all_datasets(testdata_dir: str, dataset_filter: list[str] | None = None) -> list[dict]:
    """扫描 testdata 下所有数据集，汇总 light 帧分组列表。

    自动跳过：
      - 校准文件目录（名称含 "calibration files"）
      - results 输出目录
      - 非目录文件

    Args:
        testdata_dir: testdata 根目录绝对路径。
        dataset_filter: 指定数据集名列表（仅扫描这些数据集），None 表示全部。

    Returns:
        list[dict]: 所有数据集的 light 帧分组列表（格式同 scan_dataset 返回值）。
    """
    logger.info("=" * 60)
    logger.info("开始扫描 testdata 目录: %s", testdata_dir)
    if dataset_filter:
        logger.info("仅扫描指定数据集: %s", ", ".join(dataset_filter))

    all_groups: list[dict] = []
    if not os.path.isdir(testdata_dir):
        logger.error("testdata 目录不存在: %s", testdata_dir)
        return all_groups

    for name in sorted(os.listdir(testdata_dir)):
        path = os.path.join(testdata_dir, name)
        if not os.path.isdir(path):
            continue
        # 跳过校准文件目录
        if "calibration files" in name.lower():
            continue
        # 跳过 results 输出目录
        if name.lower() == "results":
            continue
        # 按数据集过滤
        if dataset_filter and name not in dataset_filter:
            logger.debug("数据集 '%s' 不在指定列表中，跳过", name)
            continue
        all_groups.extend(scan_dataset(path, name, testdata_dir))

    # 统计汇总
    datasets = sorted({g["dataset"] for g in all_groups})
    panels = sorted({g["panel"] for g in all_groups if g["panel"]})
    filters = sorted({g["filter"] for g in all_groups})
    logger.info("-" * 40)
    logger.info("扫描完成汇总:")
    logger.info("  数据集数: %d", len(datasets))
    logger.info("  panel 数: %d (%s)", len(panels), ", ".join(panels) if panels else "无")
    logger.info("  滤镜种类: %d (%s)", len(filters), ", ".join(filters))
    logger.info("  light 帧总数: %d", len(all_groups))
    logger.info("=" * 60)
    return all_groups


def build_output_dir(result_root: str, group: dict) -> str:
    """构建单个 light 帧的输出目录路径。

    规则：
      - 有 panel: {result_root}/{数据集名}/{panel}/{滤镜}/{light_basename}/
      - 无 panel: {result_root}/{数据集名}/{滤镜}/{light_basename}/

    Args:
        result_root: 结果根目录（testdata/results）。
        group: 分组字典（含 dataset/panel/filter/light_basename）。

    Returns:
        str: 输出目录绝对路径。
    """
    parts = [result_root, group["dataset"]]
    if group["panel"]:
        parts.append(group["panel"])
    parts.append(group["filter"])
    parts.append(group["light_basename"])
    return os.path.join(*parts)


def calibrate_single(
    light_path: str,
    output_dir: str,
    calibration_dir: str,
    pipe: CalibrationPipeline,
    index: int,
    total: int,
) -> dict:
    """校准单个 light 帧并输出 01_calibrated.fits。

    流程：
      1. 调用 CalibrationPipeline.run() 校准，输出 {原文件名}_calibrated.fits
      2. 复制/重命名为固定文件名 01_calibrated.fits
      3. 删除原始输出文件（路径不同时）

    Args:
        light_path: Light 帧绝对路径。
        output_dir: 输出目录绝对路径。
        calibration_dir: 校准文件目录绝对路径。
        pipe: CalibrationPipeline 实例。
        index: 当前帧序号（从 1 开始，用于日志）。
        total: 总帧数（用于日志）。

    Returns:
        dict: {
            "success": bool,
            "light_path": str,
            "output_path": str | None,  # 最终 01_calibrated.fits 路径
            "error": str,
        }
    """
    tag = f"[{index}/{total}]"
    logger.info("%s 开始校准: %s", tag, light_path)
    logger.info("%s 输出目录: %s", tag, output_dir)
    logger.info("%s 校准目录: %s", tag, calibration_dir)

    # 校验输入
    if not os.path.isfile(light_path):
        err = f"Light 帧不存在: {light_path}"
        logger.error("%s %s", tag, err)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    if not os.path.isdir(calibration_dir):
        err = f"校准文件目录不存在: {calibration_dir}"
        logger.error("%s %s", tag, err)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    os.makedirs(output_dir, exist_ok=True)

    # 跳过已校准的帧（断点续跑）
    final_output = os.path.join(output_dir, _OUTPUT_FILENAME)
    if os.path.isfile(final_output):
        logger.info("%s 跳过（已存在）: %s", tag, final_output)
        return {"success": True, "light_path": light_path, "output_path": final_output, "error": ""}

    # 调用 pipe.run()
    try:
        result = pipe.run(
            light_path,
            output_dir,
            calibration_dir=calibration_dir,
        )
    except Exception as e:
        err = f"pipe.run() 异常: {e}"
        logger.error("%s %s", tag, err, exc_info=True)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    if not result.get("success"):
        err = result.get("error", "未知错误")
        logger.error("%s 校准失败: %s", tag, err)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    # 获取原始输出路径（{原文件名}_calibrated.fits）
    original_output = result.get("output_path")
    if not original_output or not os.path.isfile(original_output):
        err = f"校准输出文件不存在: {original_output}"
        logger.error("%s %s", tag, err)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    logger.info("%s 校准成功，原始输出: %s", tag, original_output)

    # 复制/重命名为固定文件名 01_calibrated.fits
    final_output = os.path.join(output_dir, _OUTPUT_FILENAME)
    try:
        if os.path.normpath(original_output) == os.path.normpath(final_output):
            logger.info("%s 原始输出路径与目标路径相同，无需复制", tag)
        else:
            shutil.copy2(original_output, final_output)
            logger.info("%s 复制为固定输出: %s -> %s", tag, original_output, final_output)
            # 删除原始文件（路径不同时）
            try:
                os.remove(original_output)
                logger.info("%s 已删除原始输出文件: %s", tag, original_output)
            except OSError as e:
                logger.warning(
                    "%s 删除原始输出文件失败（不影响结果）: %s, %s", tag, original_output, e
                )
    except Exception as e:
        err = f"复制/重命名输出文件失败: {e}"
        logger.error("%s %s", tag, err, exc_info=True)
        return {"success": False, "light_path": light_path, "output_path": None, "error": err}

    logger.info("%s 完成，最终输出: %s", tag, final_output)
    return {"success": True, "light_path": light_path, "output_path": final_output, "error": ""}


# ============================ 批量编排 ============================

def _format_group(group: dict) -> str:
    """格式化分组信息为可读字符串（用于 dry-run 日志）。"""
    panel_str = f"/{group['panel']}" if group["panel"] else ""
    return (
        f"{group['dataset']}{panel_str}/{group['filter']} | "
        f"{group['telescope']} | {os.path.basename(group['light_path'])}"
    )


def run_batch(
    groups: list[dict],
    result_root: str,
    max_groups: int = 1,
    dry_run: bool = False,
) -> dict:
    """批量执行校准编排。

    串行模式（max_groups=1）：创建单个 CalibrationPipeline 实例，复用处理所有帧。
    并行模式（max_groups>1）：每个 worker 线程创建独立实例，内部线程数自适应
      （inner_workers = max(1, 16 // max_groups)），总线程数约 16。

    Args:
        groups: 分组列表（scan_all_datasets 返回值）。
        result_root: 结果根目录（testdata/results）。
        max_groups: 同时处理的组数（默认 1，串行）。
        dry_run: True 时仅打印计划不执行。

    Returns:
        dict: {
            "total": int,          # 总帧数
            "success": int,        # 成功数
            "failed": int,         # 失败数
            "results": list[dict], # 每帧结果
        }
    """
    total = len(groups)
    summary = {"total": total, "success": 0, "failed": 0, "results": []}

    if total == 0:
        logger.warning("无可处理的 light 帧")
        return summary

    logger.info("=" * 60)
    logger.info("批量校准编排启动")
    logger.info("  总帧数: %d", total)
    logger.info("  并行组数: %d (%s)", max_groups, "串行" if max_groups <= 1 else "并行")
    logger.info("  结果根目录: %s", result_root)
    logger.info("  模式: %s", "DRY-RUN（仅打印计划）" if dry_run else "执行校准")
    logger.info("=" * 60)

    # Dry-run 模式：仅打印计划
    if dry_run:
        for i, g in enumerate(groups, 1):
            output_dir = build_output_dir(result_root, g)
            final_output = os.path.join(output_dir, _OUTPUT_FILENAME)
            logger.info("[DRY-RUN %d/%d] %s", i, total, _format_group(g))
            logger.info("           -> %s", final_output)
        logger.info("=" * 60)
        logger.info("[DRY-RUN] 计划打印完成，共 %d 帧，未执行校准", total)
        return summary

    os.makedirs(result_root, exist_ok=True)

    # 串行模式
    if max_groups <= 1:
        logger.info("创建 CalibrationPipeline: mode=production, max_workers=%d", _MAX_INNER_WORKERS)
        try:
            pipe = CalibrationPipeline(mode="production", max_workers=_MAX_INNER_WORKERS)
        except Exception as e:
            logger.error("CalibrationPipeline 初始化失败: %s", e, exc_info=True)
            for g in groups:
                summary["failed"] += 1
                summary["results"].append({
                    "success": False, "light_path": g["light_path"],
                    "output_path": None, "error": f"pipeline 初始化失败: {e}",
                })
            return summary

        for i, g in enumerate(groups, 1):
            output_dir = build_output_dir(result_root, g)
            res = calibrate_single(
                g["light_path"], output_dir, g["calibration_dir"], pipe, i, total
            )
            summary["results"].append(res)
            if res["success"]:
                summary["success"] += 1
            else:
                summary["failed"] += 1
            logger.info("进度: %d/%d（成功 %d，失败 %d）",
                        i, total, summary["success"], summary["failed"])
        return summary

    # 并行模式
    inner_workers = max(1, _MAX_INNER_WORKERS // max_groups)
    logger.info(
        "并行模式: max_groups=%d, 每个 worker 内部线程数=%d（总线程约 %d）",
        max_groups, inner_workers, inner_workers * max_groups,
    )

    def worker(idx: int, group: dict) -> dict:
        """并行 worker：创建独立 pipeline 实例处理单个帧。"""
        tag = f"[{idx}/{total}]"
        logger.info("%s 启动并行 worker", tag)
        try:
            pipe = CalibrationPipeline(mode="production", max_workers=inner_workers)
        except Exception as e:
            logger.error("%s pipeline 初始化失败: %s", tag, e, exc_info=True)
            return {
                "success": False, "light_path": group["light_path"],
                "output_path": None, "error": f"pipeline 初始化失败: {e}",
            }
        output_dir = build_output_dir(result_root, group)
        return calibrate_single(
            group["light_path"], output_dir, group["calibration_dir"], pipe, idx, total
        )

    with ThreadPoolExecutor(max_workers=max_groups) as executor:
        futures = {
            executor.submit(worker, i, g): i
            for i, g in enumerate(groups, 1)
        }
        for future in as_completed(futures):
            idx = futures[future]
            try:
                res = future.result()
            except Exception as e:
                err = f"worker 异常: {e}"
                logger.error("[并行结果 %d] %s", idx, err, exc_info=True)
                res = {"success": False, "light_path": "", "output_path": None, "error": err}
            summary["results"].append(res)
            if res["success"]:
                summary["success"] += 1
            else:
                summary["failed"] += 1
            logger.info("并行进度: 已完成 %d/%d（成功 %d，失败 %d）",
                        len(summary["results"]), total,
                        summary["success"], summary["failed"])

    return summary


# ============================ 命令行入口 ============================

def main():
    """命令行入口。

    支持参数：
      --datasets: 指定数据集列表（空格分隔，默认全部）
      --dry-run: 仅打印计划不执行
      --max-groups: 同时处理的组数（默认 1，串行）
    """
    parser = argparse.ArgumentParser(
        description="批量校准编排: 自动扫描 testdata 数据集，批量调用 CalibrationPipeline 校准",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--datasets", nargs="*", default=None,
        help="指定数据集列表（空格分隔，默认全部）",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="仅打印校准计划不执行",
    )
    parser.add_argument(
        "--max-groups", type=int, default=1,
        help="同时处理的组数（默认 1，串行；>1 启用有限并行，总线程约 16）",
    )

    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    testdata_dir = os.path.join(_PROJECT_ROOT, "testdata")
    result_root = os.path.join(testdata_dir, "results")

    # 扫描数据集
    groups = scan_all_datasets(testdata_dir, dataset_filter=args.datasets)
    if not groups:
        logger.error("未扫描到任何 light 帧，退出")
        print(json.dumps({"success": False, "error": "未扫描到任何 light 帧"},
                         ensure_ascii=True))
        return 1

    # 批量校准
    summary = run_batch(
        groups,
        result_root=result_root,
        max_groups=max(1, args.max_groups),
        dry_run=args.dry_run,
    )

    # 输出汇总
    logger.info("=" * 60)
    logger.info("批量校准完成汇总:")
    logger.info("  总帧数: %d", summary["total"])
    logger.info("  成功: %d", summary["success"])
    logger.info("  失败: %d", summary["failed"])
    logger.info("=" * 60)

    # 输出 JSON 到 stdout
    print(json.dumps({
        "success": summary["failed"] == 0,
        "total": summary["total"],
        "success_count": summary["success"],
        "failed_count": summary["failed"],
        "results": summary["results"],
    }, ensure_ascii=True, default=str))

    return 0 if summary["failed"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
