#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-003 — T1-T4 代表帧 WCS 闭环缺陷复现 driver

扩展 P11-002 的诊断工具到全部 16 个代表帧:
  - 自动从 P10-006 CSV 读取代表帧列表
  - 对每帧: 复制 → PlateSolve 求解 → astropy WCS 独立诊断
  - 汇总: X/Y 偏差、象限、SIP 阶数、设备/滤镜/目标模式

用法:
    python run_p11_003_diagnostic.py
"""

from __future__ import annotations

import csv
import json
import logging
import os
import shutil
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# 添加 lib 路径
PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
sys.path.insert(0, str(PROJECT_ROOT / "lib" / "plate_solve" / "python"))
sys.path.insert(0, str(PROJECT_ROOT / "lib" / "gaia_xpsd_client" / "python"))
sys.path.insert(0, str(PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-002" / "scripts"))

from solve_and_write_wcs import init_environment, _close_environment, solve_and_write_wcs
from wcs_closure_diagnostic import diagnose_frame

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("p11_003_driver")

# ============================================================================
# 配置
# ============================================================================
TASK_DIR = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-003"
WORK_DIR = TASK_DIR / "work"
REPORTS_DIR = TASK_DIR / "reports"
LOG_DIR = TASK_DIR / "raw_logs"

# P10-006 代表帧 CSV
REPRESENTATIVE_CSV = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P10-006" / "REPRESENTATIVE_CALIBRATION_REPORT.csv"


def load_representative_frames() -> List[Dict[str, str]]:
    """从 P10-006 CSV 读取代表帧列表

    Returns:
        List of dict, 每项:
          - frame_id: e.g. "T3_LUM_NGC55"
          - device: T2/T3/T4
          - filter: RED/GREEN/BLUE/HA/OIII/LUM
          - target: NGC55/LDN43/NGC1727/Galaxy_Center
          - light_path: 相对路径
    """
    frames: List[Dict[str, str]] = []
    if not REPRESENTATIVE_CSV.exists():
        raise FileNotFoundError(f"代表帧 CSV 不存在: {REPRESENTATIVE_CSV}")

    with open(REPRESENTATIVE_CSV, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            device = row["device_id"].strip()
            filt = row["filter_canonical"].strip()
            target = row["target"].strip()
            light_path = row["light_path"].strip().replace("\\", "/")
            frame_id = f"{device}_{filt}_{target}"
            frames.append({
                "frame_id": frame_id,
                "device": device,
                "filter": filt,
                "target": target,
                "light_path": light_path,
            })
    return frames


def run_frame(
    frame_meta: Dict[str, str],
    work_dir: Path,
    reports_dir: Path,
    env,
) -> Dict:
    """处理单帧: 复制 → 求解 → 诊断"""
    frame_id = frame_meta["frame_id"]
    src_fits = str(PROJECT_ROOT / frame_meta["light_path"])
    logger.info("=" * 70)
    logger.info("处理帧: %s (%s/%s/%s)",
                frame_id, frame_meta["device"], frame_meta["filter"], frame_meta["target"])
    logger.info("源 FITS: %s", src_fits)

    if not os.path.exists(src_fits):
        logger.error("源 FITS 不存在: %s", src_fits)
        return {"frame_id": frame_id, "src_fits": src_fits, "error": "源文件不存在",
                **frame_meta}

    # 1. 复制到 work
    work_dir.mkdir(parents=True, exist_ok=True)
    work_fits = work_dir / f"{frame_id}_solved.fits"
    logger.info("复制到: %s", work_fits)
    shutil.copy2(src_fits, work_fits)

    # 2. PlateSolve 求解 + 写 WCS
    logger.info("-" * 40)
    logger.info("PlateSolve 求解...")
    solve_start = time.time()
    try:
        solve_result = solve_and_write_wcs(
            str(work_fits), ra0=0, dec0=0, focal_length=0, pixel_size=0,
            overwrite=True, env=env,
        )
    except Exception as e:
        logger.error("PlateSolve 求解异常: %s", e, exc_info=True)
        return {"frame_id": frame_id, "src_fits": src_fits, "error": f"求解异常: {e}",
                **frame_meta}
    solve_elapsed = time.time() - solve_start
    logger.info(
        "求解完成: success=%s, n_pairs=%d, rms_px=%.4f, 耗时=%.2fs",
        solve_result.get("success", False),
        solve_result.get("n_pairs", 0),
        solve_result.get("rms_px", 0.0),
        solve_elapsed,
    )

    if not solve_result.get("success", False):
        logger.error("PlateSolve 求解失败, 跳过诊断")
        return {
            "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits),
            **frame_meta,
            "solve_result": {
                "success": False, "rms_px": solve_result.get("rms_px", 0.0),
                "n_pairs": solve_result.get("n_pairs", 0),
            },
            "diagnose_result": None, "error": "PlateSolve 求解失败",
            "solve_elapsed_sec": solve_elapsed,
        }

    # 3. WCS 闭环诊断 (工具独立于 PlateSolve transform)
    logger.info("-" * 40)
    logger.info("WCS 闭环诊断 (astropy WCS, 独立于 PlateSolve transform)...")
    frame_reports = reports_dir / frame_id
    diag_start = time.time()
    try:
        report = diagnose_frame(
            str(work_fits), str(frame_reports), str(PROJECT_ROOT),
            env=env,
            max_match_dist_px=3.0,
            gaia_mag_high=18.0,
            solve_if_no_wcs=False,
        )
    except Exception as e:
        logger.error("诊断异常: %s", e, exc_info=True)
        return {
            "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits),
            **frame_meta,
            "solve_result": {
                "success": True, "rms_px": solve_result.get("rms_px", 0.0),
                "n_pairs": solve_result.get("n_pairs", 0),
            },
            "diagnose_result": None, "error": f"诊断异常: {e}",
            "solve_elapsed_sec": solve_elapsed,
        }
    diag_elapsed = time.time() - diag_start
    logger.info(
        "诊断完成: n_matched=%d, median=%.3f px, p90=%.3f px, p99=%.3f px, gate=%s, 耗时=%.2fs",
        report["matching"]["n_matched"],
        report["residual_stats"]["dist_median_px"],
        report["residual_stats"]["dist_p90_px"],
        report["residual_stats"]["dist_p99_px"],
        report["gate_passed"],
        diag_elapsed,
    )

    return {
        "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits),
        **frame_meta,
        "solve_result": {
            "success": True,
            "rms_px": solve_result.get("rms_px", 0.0),
            "n_pairs": solve_result.get("n_pairs", 0),
            "trans_order": solve_result.get("trans_order", 0),
        },
        "diagnose_result": {
            "n_matched": report["matching"]["n_matched"],
            "dist_median_px": report["residual_stats"]["dist_median_px"],
            "dist_p90_px": report["residual_stats"]["dist_p90_px"],
            "dist_p99_px": report["residual_stats"]["dist_p99_px"],
            "dist_mean_px": report["residual_stats"]["dist_mean_px"],
            "dist_std_px": report["residual_stats"]["dist_std_px"],
            "x_median_abs_px": report["residual_stats"]["x_median_abs_px"],
            "y_median_abs_px": report["residual_stats"]["y_median_abs_px"],
            "x_mean_px": report["residual_stats"]["x_mean_px"],
            "y_mean_px": report["residual_stats"]["y_mean_px"],
            "x_std_px": report["residual_stats"]["x_std_px"],
            "y_std_px": report["residual_stats"]["y_std_px"],
            "x_p90_abs_px": report["residual_stats"]["x_p90_abs_px"],
            "y_p90_abs_px": report["residual_stats"]["y_p90_abs_px"],
            "quadrant_counts": report["residual_stats"]["quadrant_counts"],
            "edge_counts": report["residual_stats"]["edge_counts"],
            "gate_passed": report["gate_passed"],
            "has_sip": report["wcs"]["has_sip"],
            "sip_order": report["wcs"]["sip_order"],
            "fov_diag_deg": report["fov_diag_deg"],
            "ps_to_sky_closure_err_median_px": report["pixel_sky_pixel_closure"]["closure_err_median_px"],
            "sky_ps_sky_closure_err_median_deg": report["sky_pixel_sky_closure"]["closure_err_median_deg"],
        },
        "solve_elapsed_sec": solve_elapsed,
        "diag_elapsed_sec": diag_elapsed,
    }


def compute_summary_stats(frames: List[Dict]) -> Dict:
    """计算跨帧汇总统计: 设备/滤镜/目标模式"""
    summary = {
        "total": len(frames),
        "n_solved": sum(1 for f in frames if f.get("solve_result", {}).get("success", False)),
        "n_diagnosed": sum(1 for f in frames if f.get("diagnose_result") is not None),
        "n_gate_passed": sum(1 for f in frames if f.get("diagnose_result", {}) and f["diagnose_result"].get("gate_passed", False)),
        "n_errors": sum(1 for f in frames if "error" in f),
        "by_device": {},
        "by_filter": {},
        "by_target": {},
        "sip_orders": {},
        "quadrant_totals": {"Q1": 0, "Q2": 0, "Q3": 0, "Q4": 0},
        "median_residual_range_px": None,
        "x_y_ratio_frames": [],  # X/Y 比率分类
    }

    medians = []
    for f in frames:
        diag = f.get("diagnose_result")
        if not diag:
            continue
        medians.append(diag["dist_median_px"])

        # 累加象限
        for q, n in diag["quadrant_counts"].items():
            summary["quadrant_totals"][q] += n

        # SIP 阶数统计
        sip = diag.get("sip_order", 0)
        summary["sip_orders"][str(sip)] = summary["sip_orders"].get(str(sip), 0) + 1

        # X/Y 比率分类
        x_med = diag["x_median_abs_px"]
        y_med = diag["y_median_abs_px"]
        if x_med + y_med > 0:
            ratio = y_med / (x_med + y_med)
            if ratio > 0.65:
                category = "Y_dominant"
            elif ratio < 0.35:
                category = "X_dominant"
            else:
                category = "balanced"
            summary["x_y_ratio_frames"].append({
                "frame_id": f["frame_id"],
                "device": f.get("device", ""),
                "filter": f.get("filter", ""),
                "target": f.get("target", ""),
                "x_median_abs_px": x_med,
                "y_median_abs_px": y_med,
                "y_ratio": round(ratio, 3),
                "category": category,
            })

        # 按设备/滤镜/目标分组
        for key in ("device", "filter", "target"):
            val = f.get(key, "")
            if not val:
                continue
            grp = summary[f"by_{key}"].setdefault(val, {
                "n": 0, "median_px_sum": 0.0, "median_px_avg": 0.0,
                "x_med_sum": 0.0, "y_med_sum": 0.0,
            })
            grp["n"] += 1
            grp["median_px_sum"] += diag["dist_median_px"]
            grp["x_med_sum"] += diag["x_median_abs_px"]
            grp["y_med_sum"] += diag["y_median_abs_px"]

    # 完成分组平均
    for key in ("device", "filter", "target"):
        for val, grp in summary[f"by_{key}"].items():
            if grp["n"] > 0:
                grp["median_px_avg"] = round(grp["median_px_sum"] / grp["n"], 4)
                grp["x_med_avg"] = round(grp["x_med_sum"] / grp["n"], 4)
                grp["y_med_avg"] = round(grp["y_med_sum"] / grp["n"], 4)
            del grp["median_px_sum"]
            del grp["x_med_sum"]
            del grp["y_med_sum"]

    if medians:
        summary["median_residual_range_px"] = {
            "min": round(min(medians), 4),
            "max": round(max(medians), 4),
            "avg": round(sum(medians) / len(medians), 4),
        }

    # 象限总分布
    qtot = summary["quadrant_totals"]
    q_sum = sum(qtot.values())
    if q_sum > 0:
        for q in qtot:
            qtot[q] = {
                "count": qtot[q],
                "ratio": round(qtot[q] / q_sum, 4),
            }

    return summary


def main():
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    log_file = LOG_DIR / "run_p11_003.log"
    fh = logging.FileHandler(log_file, encoding="utf-8")
    fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
    logging.getLogger().addHandler(fh)

    logger.info("=" * 70)
    logger.info("P11-003 T1-T4 代表帧 WCS 闭环缺陷复现 driver")
    logger.info("PROJECT_ROOT: %s", PROJECT_ROOT)
    logger.info("WORK_DIR: %s", WORK_DIR)
    logger.info("REPORTS_DIR: %s", REPORTS_DIR)

    # 加载代表帧
    frames_meta = load_representative_frames()
    logger.info("代表帧数: %d", len(frames_meta))
    for fm in frames_meta:
        logger.info("  - %s (%s/%s/%s)", fm["frame_id"], fm["device"], fm["filter"], fm["target"])

    # 初始化共享环境
    logger.info("-" * 70)
    logger.info("初始化 PlateSolve 环境...")
    env = init_environment()
    logger.info("环境就绪")

    summaries: List[Dict] = []
    try:
        for i, fm in enumerate(frames_meta, 1):
            logger.info("#%d/%d: %s", i, len(frames_meta), fm["frame_id"])
            try:
                summary = run_frame(fm, WORK_DIR, REPORTS_DIR, env)
                summaries.append(summary)
            except Exception as e:
                logger.error("帧 %s 处理失败: %s", fm["frame_id"], e, exc_info=True)
                summaries.append({
                    **fm,
                    "error": str(e),
                })
    finally:
        logger.info("-" * 70)
        logger.info("释放 PlateSolve 环境...")
        _close_environment(*env)
        logger.info("环境已释放")

    # 跨帧汇总
    summary_stats = compute_summary_stats(summaries)

    # 汇总报告
    summary_path = REPORTS_DIR / "p11_003_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(
            {
                "tool_version": "P11-003 v1.0",
                "tool_independent_of_platesolve_transform": True,
                "frames": summaries,
                "summary_stats": summary_stats,
            },
            f, indent=2, ensure_ascii=False,
        )
    logger.info("=" * 70)
    logger.info("汇总报告: %s", summary_path)
    logger.info("帧数: %d", summary_stats["total"])
    logger.info("求解成功: %d", summary_stats["n_solved"])
    logger.info("诊断成功: %d", summary_stats["n_diagnosed"])
    logger.info("通过门限: %d", summary_stats["n_gate_passed"])
    logger.info("错误: %d", summary_stats["n_errors"])
    logger.info("SIP 阶数分布: %s", summary_stats["sip_orders"])
    logger.info("象限总分布: %s", summary_stats["quadrant_totals"])
    if summary_stats["median_residual_range_px"]:
        r = summary_stats["median_residual_range_px"]
        logger.info("median 残差范围: min=%.4f max=%.4f avg=%.4f", r["min"], r["max"], r["avg"])
    logger.info("=" * 70)


if __name__ == "__main__":
    main()
