# -*- coding: utf-8 -*-
"""
批量光谱积分+梯度估算 (Batch Step3+Step4)
功能: 扫描所有已解析的校准后FITS文件，依次执行 step3 (光谱积分) 和 step4 (梯度估算)
用途: 在批量解析完成后，对所有01_calibrated.fits执行光度定标全链路
调用:
    python batch_step34.py                              # 处理全部
    python batch_step34.py --datasets Galaxy_Center_T4  # 指定数据集
    python batch_step34.py --workers 2                  # 指定并行数 (建议<=2, DLL线程安全)
依赖: step3_integrate, step4_estimate
注意: 串行执行 (workers=1) 最稳定; workers=2 可加速但可能触发 DLL 堆冲突
滤光片自动映射:
    Red -> Baader R (宽带)
    Blue -> Baader B (宽带)
    Green -> Baader G (宽带)
    Lum -> Baader L (宽带)
    H-alpha -> 窄带 656.3nm, 7nm FWHM, 0.9 透过率
    OIII -> 窄带 500.7nm, 7nm FWHM, 0.9 透过率
"""

from __future__ import annotations

import os
import sys
import json
import time
import logging
import argparse
import subprocess
from datetime import datetime

# ============================ 环境初始化 ============================

# 必须在 DLL 加载前设置 OMP 线程数
os.environ.setdefault("OMP_NUM_THREADS", "8")

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

# 日志初始化
_LOG_DIR = os.path.join(PROJECT_ROOT, "lib", "integration_test", "logs", "batch_step34")
os.makedirs(_LOG_DIR, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(
            os.path.join(_LOG_DIR, f"batch_step34_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"),
            encoding="utf-8",
        ),
    ],
)
logger = logging.getLogger(__name__)

# Python 解释器路径
_PYTHON_EXE = sys.executable
_STEP3_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "step3_integrate.py")
_STEP4_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "step4_estimate.py")

# 滤光片映射: 目录名 -> step3 参数
# 宽带: --filter-name "Baader X"
# 窄带: --narrowband-center XXX --narrowband-bw 7 --narrowband-trans 0.9
FILTER_MAP = {
    "Red":      {"type": "broad", "filter_name": "Baader R"},
    "Blue":     {"type": "broad", "filter_name": "Baader B"},
    "Green":    {"type": "broad", "filter_name": "Baader G"},
    "Lum":      {"type": "broad", "filter_name": "Baader UV/IR Cut / L CMOS Optimized"},
    "H-alpha":  {"type": "narrow", "center": 656.3, "bw": 7.0, "trans": 0.9},
    "OIII":     {"type": "narrow", "center": 500.7, "bw": 7.0, "trans": 0.9},
}


def scan_representative_frames(results_dir: str, datasets: list = None) -> list:
    """扫描所有已解析的校准后FITS文件，按 panel/filter 分组选代表帧

    支持两种目录结构:
      - 有 panel: <dataset>/<panel>/<filter>/<frame>/01_calibrated.fits
      - 无 panel: <dataset>/<filter>/<frame>/01_calibrated.fits

    每个 panel/filter 组合选一帧文件最大（通常曝光长、星点足）的代表帧
    执行 step3/4，得到定标系数后可应用到该组其他帧。

    Args:
        results_dir: testdata/results 目录
        datasets: 指定数据集名称列表, None 表示全部

    Returns:
        list: [{fits_path, frame_dir, dataset, filter_dir, panel, n_frames_in_group}, ...]
    """
    from astropy.io import fits

    reps = []
    if not os.path.isdir(results_dir):
        logger.error("结果目录不存在: %s", results_dir)
        return reps

    # 按 (dataset, panel, filter) 分组收集所有已解析帧
    # panel 可为 "n/a" (无 panel 层的数据集)
    groups = {}  # key=(dataset, panel, filter_dir) -> list of (fits_path, frame_dir, size)
    for dataset in sorted(os.listdir(results_dir)):
        dataset_path = os.path.join(results_dir, dataset)
        if not os.path.isdir(dataset_path):
            continue
        if datasets and dataset not in datasets:
            continue

        # 判断结构: 遍历 dataset 下的一级子目录
        for sub in sorted(os.listdir(dataset_path)):
            sub_path = os.path.join(dataset_path, sub)
            if not os.path.isdir(sub_path):
                continue

            if sub in FILTER_MAP:
                # 无 panel 结构: sub 就是 filter 目录
                _collect_group_frames(groups, dataset, "n/a", sub, sub_path, fits)
            else:
                # 有 panel 结构: sub 是 panel 目录，再遍历一层 filter
                for filter_dir in sorted(os.listdir(sub_path)):
                    filter_path = os.path.join(sub_path, filter_dir)
                    if not os.path.isdir(filter_path):
                        continue
                    if filter_dir not in FILTER_MAP:
                        continue
                    _collect_group_frames(groups, dataset, sub, filter_dir,
                                          filter_path, fits)

    # 每组选文件最大的代表帧
    for (dataset, panel, filter_dir), frames in sorted(groups.items()):
        # 文件大小降序，选最大的
        frames.sort(key=lambda x: x[2], reverse=True)
        rep_path, rep_dir, rep_size = frames[0]
        reps.append({
            "fits_path": rep_path,
            "frame_dir": rep_dir,
            "dataset": dataset,
            "panel": panel,
            "filter_dir": filter_dir,
            "n_frames_in_group": len(frames),
        })
        logger.info("代表帧: %s/%s/%s -> %s (组内 %d 帧)",
                    dataset, panel, filter_dir,
                    os.path.basename(os.path.dirname(rep_path)),
                    len(frames))

    return reps


def _collect_group_frames(groups: dict, dataset: str, panel: str,
                          filter_dir: str, filter_path: str, fits_module) -> None:
    """收集一个 filter 目录下所有已解析的校准帧

    Args:
        groups: 分组字典 (会被修改)
        dataset: 数据集名
        panel: panel 名 (无 panel 时为 "n/a")
        filter_dir: 滤光片目录名
        filter_path: 滤光片目录路径
        fits_module: astropy.io.fits 模块
    """
    for frame in sorted(os.listdir(filter_path)):
        frame_path = os.path.join(filter_path, frame)
        if not os.path.isdir(frame_path):
            continue

        fits_path = os.path.join(frame_path, "01_calibrated.fits")
        if not os.path.isfile(fits_path):
            continue

        # 检查是否已解析 (CTYPE1 关键字存在)
        try:
            with fits_module.open(fits_path, mode='readonly') as hdul:
                header = hdul[0].header
                if 'CTYPE1' not in header:
                    continue
        except Exception as e:
            logger.warning("读取FITS头失败: %s, %s", fits_path, e)
            continue

        key = (dataset, panel, filter_dir)
        size = os.path.getsize(fits_path)
        groups.setdefault(key, []).append((fits_path, frame_path, size))


def run_step3(fits_path: str, filter_config: dict, output_path: str) -> dict:
    """运行 step3 光谱积分

    Returns:
        dict: {success, n_stars, error}
    """
    cmd = [_PYTHON_EXE, _STEP3_SCRIPT,
           "--image", fits_path,
           "--output", output_path]

    if filter_config["type"] == "broad":
        cmd.extend(["--filter-name", filter_config["filter_name"]])
    else:
        cmd.extend(["--narrowband-center", str(filter_config["center"]),
                    "--narrowband-bw", str(filter_config["bw"]),
                    "--narrowband-trans", str(filter_config["trans"])])

    logger.info("Step3 启动: %s", fits_path)
    logger.debug("Step3 命令: %s", " ".join(cmd))

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, encoding="utf-8",
            timeout=300,  # 5分钟超时
        )
        if result.returncode != 0:
            error_msg = result.stderr[-500:] if result.stderr else "未知错误"
            logger.error("Step3 失败 (exit=%d): %s", result.returncode, error_msg)
            return {"success": False, "n_stars": 0, "error": f"exit={result.returncode}: {error_msg}"}

        # 解析 stdout JSON
        stdout = result.stdout.strip()
        if stdout:
            # 取最后一行 JSON
            for line in reversed(stdout.splitlines()):
                line = line.strip()
                if line.startswith("{"):
                    step3_result = json.loads(line)
                    logger.info("Step3 完成: n_stars=%d", step3_result.get("n_stars", 0))
                    return step3_result

        logger.error("Step3 无有效 JSON 输出: %s", stdout[-200:])
        return {"success": False, "n_stars": 0, "error": "无有效 JSON 输出"}

    except subprocess.TimeoutExpired:
        logger.error("Step3 超时 (5分钟)")
        return {"success": False, "n_stars": 0, "error": "超时"}
    except Exception as e:
        logger.error("Step3 异常: %s", e, exc_info=True)
        return {"success": False, "n_stars": 0, "error": str(e)}


def run_step4(fits_path: str, fsyn_path: str, output_fits: str, report_path: str) -> dict:
    """运行 step4 梯度估算

    Returns:
        dict: {success, n_matched, scale_factor, error}
    """
    cmd = [_PYTHON_EXE, _STEP4_SCRIPT,
           "--image", fits_path,
           "--fsyn", fsyn_path,
           "--output", output_fits,
           "--report", report_path]

    logger.info("Step4 启动: %s", fits_path)
    logger.debug("Step4 命令: %s", " ".join(cmd))

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, encoding="utf-8",
            timeout=300,  # 5分钟超时
        )
        if result.returncode != 0:
            error_msg = result.stderr[-500:] if result.stderr else "未知错误"
            logger.error("Step4 失败 (exit=%d): %s", result.returncode, error_msg)
            return {"success": False, "n_matched": 0, "scale_factor": 0.0,
                    "error": f"exit={result.returncode}: {error_msg}"}

        stdout = result.stdout.strip()
        if stdout:
            for line in reversed(stdout.splitlines()):
                line = line.strip()
                if line.startswith("{"):
                    step4_result = json.loads(line)
                    logger.info("Step4 完成: n_matched=%d, scale=%.6e",
                                step4_result.get("n_matched", 0),
                                step4_result.get("scale_factor", 0.0))
                    return step4_result

        logger.error("Step4 无有效 JSON 输出: %s", stdout[-200:])
        return {"success": False, "n_matched": 0, "scale_factor": 0.0,
                "error": "无有效 JSON 输出"}

    except subprocess.TimeoutExpired:
        logger.error("Step4 超时 (5分钟)")
        return {"success": False, "n_matched": 0, "scale_factor": 0.0, "error": "超时"}
    except Exception as e:
        logger.error("Step4 异常: %s", e, exc_info=True)
        return {"success": False, "n_matched": 0, "scale_factor": 0.0, "error": str(e)}


def process_frame(frame_info: dict) -> dict:
    """处理单个帧: step3 + step4

    Args:
        frame_info: {fits_path, frame_dir, dataset, filter_dir}

    Returns:
        dict: 处理结果
    """
    fits_path = frame_info["fits_path"]
    frame_dir = frame_info["frame_dir"]
    filter_config = FILTER_MAP[frame_info["filter_dir"]]

    # 输出文件路径
    fsyn_path = os.path.join(frame_dir, "03_fsyn.json")
    output_fits = os.path.join(frame_dir, "04_calibrated_final.fits")
    report_path = os.path.join(frame_dir, "04_quality_report.json")

    # 断点续跑: 如果 04_calibrated_final.fits 已存在则跳过
    if os.path.isfile(output_fits):
        logger.info("跳过已完成帧: %s", fits_path)
        return {"success": True, "skipped": True, "fits_path": fits_path}

    result = {
        "fits_path": fits_path,
        "dataset": frame_info["dataset"],
        "filter": frame_info["filter_dir"],
        "step3": None,
        "step4": None,
    }

    # Step3: 光谱积分
    step3_result = run_step3(fits_path, filter_config, fsyn_path)
    result["step3"] = step3_result

    if not step3_result.get("success"):
        result["success"] = False
        result["error"] = f"step3: {step3_result.get('error', '未知')}"
        return result

    # Step3 无匹配星则跳过 step4
    if step3_result.get("n_stars", 0) == 0:
        logger.warning("Step3 无匹配星，跳过 step4: %s", fits_path)
        result["success"] = True
        result["step4"] = {"success": True, "skipped": True, "reason": "no_stars"}
        return result

    # Step4: 梯度估算
    step4_result = run_step4(fits_path, fsyn_path, output_fits, report_path)
    result["step4"] = step4_result

    result["success"] = step4_result.get("success", False)
    if not result["success"]:
        result["error"] = f"step4: {step4_result.get('error', '未知')}"

    return result


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="批量光谱积分+梯度估算 (Step3+Step4)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--results-dir", type=str,
                        default=os.path.join(PROJECT_ROOT, "testdata", "results"),
                        help="结果目录，默认 testdata/results")
    parser.add_argument("--datasets", type=str, nargs="*", default=None,
                        help="指定数据集名称列表，默认全部")
    parser.add_argument("--workers", type=int, default=2,
                        help="并行数 (子进程级别, DLL 安全), 默认 2")
    parser.add_argument("--max-frames", type=int, default=0,
                        help="最大处理帧数 (0=全部), 默认 0")
    return parser.parse_args()


def main():
    """主入口"""
    args = parse_args()

    # 扫描代表帧: 每 panel/filter 选 1 帧
    frames = scan_representative_frames(args.results_dir, args.datasets)

    if args.max_frames > 0:
        frames = frames[:args.max_frames]

    total = len(frames)
    logger.info("=" * 60)
    logger.info("批量 Step3+Step4 启动")
    logger.info("  总帧数: %d", total)
    logger.info("  并行数: %d", args.workers)
    logger.info("  Python: %s", _PYTHON_EXE)
    logger.info("=" * 60)

    if total == 0:
        logger.warning("无待处理帧")
        return 0

    results = []
    success_count = 0
    fail_count = 0
    skip_count = 0
    start_time = time.time()

    if args.workers <= 1:
        # 串行执行 (最稳定)
        for i, frame in enumerate(frames, 1):
            logger.info("-" * 40)
            logger.info("[%d/%d] %s", i, total, frame["fits_path"])
            result = process_frame(frame)
            results.append(result)
            if result.get("success"):
                if result.get("skipped"):
                    skip_count += 1
                else:
                    success_count += 1
            else:
                fail_count += 1

            elapsed = time.time() - start_time
            avg = elapsed / i
            remaining = avg * (total - i)
            logger.info("进度: %d/%d, 成功=%d, 失败=%d, 跳过=%d, "
                        "已用=%.0fs, 预计剩余=%.0fs",
                        i, total, success_count, fail_count, skip_count,
                        elapsed, remaining)
    else:
        # 并行执行 (子进程级别, 避免DLL线程安全问题)
        from concurrent.futures import ThreadPoolExecutor, as_completed
        with ThreadPoolExecutor(max_workers=args.workers, thread_name_prefix="Step34") as executor:
            future_to_frame = {
                executor.submit(process_frame, frame): frame for frame in frames
            }
            for i, future in enumerate(as_completed(future_to_frame), 1):
                frame = future_to_frame[future]
                try:
                    result = future.result()
                except Exception as e:
                    logger.error("帧处理异常: %s, %s", frame["fits_path"], e)
                    result = {"success": False, "error": str(e),
                              "fits_path": frame["fits_path"]}
                results.append(result)
                if result.get("success"):
                    if result.get("skipped"):
                        skip_count += 1
                    else:
                        success_count += 1
                else:
                    fail_count += 1
                logger.info("进度: %d/%d, 成功=%d, 失败=%d, 跳过=%d",
                            i, total, success_count, fail_count, skip_count)

    elapsed = time.time() - start_time
    logger.info("=" * 60)
    logger.info("批量 Step3+Step4 完成")
    logger.info("  总帧数: %d", total)
    logger.info("  成功: %d", success_count)
    logger.info("  失败: %d", fail_count)
    logger.info("  跳过: %d", skip_count)
    logger.info("  耗时: %.0f 秒 (%.1f 分钟)", elapsed, elapsed / 60.0)
    logger.info("=" * 60)

    # 保存摘要
    summary_path = os.path.join(
        _LOG_DIR, f"batch_step34_summary_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json")
    summary = {
        "total": total,
        "success": success_count,
        "fail": fail_count,
        "skip": skip_count,
        "elapsed_sec": elapsed,
        "results": results,
    }
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    logger.info("摘要已保存: %s", summary_path)

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
