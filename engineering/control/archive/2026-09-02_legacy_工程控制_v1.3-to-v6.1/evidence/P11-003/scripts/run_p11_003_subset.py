#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-003 子集 driver — 支持指定帧列表并行执行

用法:
    python run_p11_003_subset.py --frames T2_HA_LDN43,T2_OIII_NGC1727,T3_RED_NGC55,T3_GREEN_NGC55
    python run_p11_003_subset.py --group A    # group_a: T2_HA_LDN43,T2_OIII_NGC1727,T3_RED_NGC55,T3_GREEN_NGC55
    python run_p11_003_subset.py --group B    # group_b: T3_BLUE_NGC55,T3_HA_NGC55,T3_OIII_NGC55,T3_LUM_NGC55
    python run_p11_003_subset.py --all        # 全部 16 帧 (等价原 driver)

特性:
    - 每帧独立 init_environment/close_environment (DLL 句柄独立)
    - Gaia 缓存隔离 (in-memory 60s TTL, 跨进程不共享)
    - 输出独立的子集日志: run_p11_003_subset_<group>.log
    - 汇总输出到 reports/<group>_summary.json
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import shutil
import sys
import time
from pathlib import Path
from typing import Dict, List, Tuple

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
logger = logging.getLogger("p11_003_subset")

TASK_DIR = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-003"
WORK_DIR = TASK_DIR / "work"
REPORTS_DIR = TASK_DIR / "reports"
LOG_DIR = TASK_DIR / "raw_logs"
ARCHIVE_JSON = TASK_DIR / "REPRESENTATIVE_FRAMES_ARCHIVE.json"

GROUP_A = ["T2_HA_LDN43", "T2_OIII_NGC1727", "T3_RED_NGC55", "T3_GREEN_NGC55"]
GROUP_B = ["T3_BLUE_NGC55", "T3_HA_NGC55", "T3_OIII_NGC55", "T3_LUM_NGC55"]


def load_archive() -> Dict:
    if not ARCHIVE_JSON.exists():
        raise FileNotFoundError(f"档案 JSON 不存在: {ARCHIVE_JSON}")
    with open(ARCHIVE_JSON, "r", encoding="utf-8") as f:
        return json.load(f)


def get_frame_meta(frame_id: str, archive: Dict) -> Dict:
    for fm in archive["representative_frames"]:
        if fm["frame_id"] == frame_id:
            return fm
    raise ValueError(f"frame_id {frame_id} 不在档案中")


def run_frame(frame_meta: Dict, env) -> Dict:
    frame_id = frame_meta["frame_id"]
    src_fits = str(PROJECT_ROOT / frame_meta["light_path"])
    logger.info("=" * 70)
    logger.info("处理帧: %s (%s/%s/%s)",
                frame_id, frame_meta["device"], frame_meta["filter"], frame_meta["target"])
    logger.info("源 FITS: %s", src_fits)

    if not os.path.exists(src_fits):
        logger.error("源 FITS 不存在: %s", src_fits)
        return {"frame_id": frame_id, "src_fits": src_fits, "error": "源文件不存在", **frame_meta}

    # 清理已有残缺证据 (全部重跑策略)
    work_fits = WORK_DIR / f"{frame_id}_solved.fits"
    frame_reports_dir = REPORTS_DIR / frame_id
    if work_fits.exists():
        try:
            work_fits.unlink()
            logger.info("清理已有 work_fits: %s", work_fits.name)
        except Exception as e:
            logger.warning("清理 work_fits 失败: %s", e)
    if frame_reports_dir.exists():
        try:
            shutil.rmtree(frame_reports_dir)
            logger.info("清理已有 reports 目录: %s", frame_reports_dir.name)
        except Exception as e:
            logger.warning("清理 reports 失败: %s", e)

    # 复制到 work
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    logger.info("复制到: %s", work_fits)
    shutil.copy2(src_fits, work_fits)

    # PlateSolve 求解 + 写 WCS
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
        return {"frame_id": frame_id, "src_fits": src_fits, "error": f"求解异常: {e}", **frame_meta}
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
            "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits), **frame_meta,
            "solve_result": {
                "success": False, "rms_px": solve_result.get("rms_px", 0.0),
                "n_pairs": solve_result.get("n_pairs", 0),
            },
            "diagnose_result": None, "error": "PlateSolve 求解失败",
            "solve_elapsed_sec": solve_elapsed,
        }

    # WCS 闭环诊断
    logger.info("-" * 40)
    logger.info("WCS 闭环诊断 (astropy WCS, 独立于 PlateSolve transform)...")
    diag_start = time.time()
    try:
        report = diagnose_frame(
            str(work_fits), str(frame_reports_dir), str(PROJECT_ROOT),
            env=env,
            max_match_dist_px=3.0,
            gaia_mag_high=18.0,
            solve_if_no_wcs=False,
        )
    except Exception as e:
        logger.error("诊断异常: %s", e, exc_info=True)
        return {
            "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits), **frame_meta,
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
        "frame_id": frame_id, "src_fits": src_fits, "work_fits": str(work_fits), **frame_meta,
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=str, default="",
                        help="逗号分隔的 frame_id 列表, e.g. T2_HA_LDN43,T2_OIII_NGC1727")
    parser.add_argument("--group", type=str, choices=["A", "B"], default="",
                        help="预定义组: A=4帧, B=4帧")
    parser.add_argument("--all", action="store_true", help="全部 16 帧")
    parser.add_argument("--label", type=str, default="subset",
                        help="日志/汇总标签 (e.g. group_a)")
    args = parser.parse_args()

    if args.all:
        archive = load_archive()
        frame_ids = [fm["frame_id"] for fm in archive["representative_frames"]]
    elif args.group:
        frame_ids = GROUP_A if args.group.upper() == "A" else GROUP_B
        args.label = f"group_{args.group.lower()}"
    elif args.frames:
        frame_ids = [s.strip() for s in args.frames.split(",") if s.strip()]
    else:
        parser.error("必须指定 --frames / --group / --all 之一")

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    log_file = LOG_DIR / f"run_p11_003_{args.label}.log"
    fh = logging.FileHandler(log_file, encoding="utf-8")
    fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
    logging.getLogger().addHandler(fh)

    logger.info("=" * 70)
    logger.info("P11-003 子集 driver (标签: %s)", args.label)
    logger.info("PROJECT_ROOT: %s", PROJECT_ROOT)
    logger.info("WORK_DIR: %s", WORK_DIR)
    logger.info("REPORTS_DIR: %s", REPORTS_DIR)
    logger.info("待处理帧数: %d", len(frame_ids))
    for fid in frame_ids:
        logger.info("  - %s", fid)

    archive = load_archive()

    # 初始化独立环境 (DLL 句柄独立)
    logger.info("-" * 70)
    logger.info("初始化 PlateSolve 环境 (独立)...")
    env = init_environment()
    logger.info("环境就绪")

    summaries: List[Dict] = []
    try:
        for i, fid in enumerate(frame_ids, 1):
            logger.info("#%d/%d: %s", i, len(frame_ids), fid)
            try:
                fm = get_frame_meta(fid, archive)
            except ValueError as e:
                logger.error("%s", e)
                summaries.append({"frame_id": fid, "error": str(e)})
                continue
            try:
                summary = run_frame(fm, env)
                summaries.append(summary)
            except Exception as e:
                logger.error("帧 %s 处理失败: %s", fid, e, exc_info=True)
                summaries.append({"frame_id": fid, **fm, "error": str(e)})
    finally:
        logger.info("-" * 70)
        logger.info("释放 PlateSolve 环境...")
        _close_environment(*env)
        logger.info("环境已释放")

    # 汇总
    summary_path = REPORTS_DIR / f"{args.label}_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(
            {
                "tool_version": "P11-003 v1.0 (subset)",
                "tool_independent_of_platesolve_transform": True,
                "label": args.label,
                "frames": summaries,
                "n_total": len(summaries),
                "n_solved": sum(1 for s in summaries if s.get("solve_result", {}).get("success", False)),
                "n_diagnosed": sum(1 for s in summaries if s.get("diagnose_result") is not None),
                "n_gate_passed": sum(1 for s in summaries if s.get("diagnose_result", {}) and s["diagnose_result"].get("gate_passed", False)),
                "n_errors": sum(1 for s in summaries if "error" in s),
            },
            f, indent=2, ensure_ascii=False,
        )
    logger.info("=" * 70)
    logger.info("汇总报告: %s", summary_path)
    logger.info("帧数: %d", len(summaries))
    logger.info("求解成功: %d", sum(1 for s in summaries if s.get("solve_result", {}).get("success", False)))
    logger.info("诊断成功: %d", sum(1 for s in summaries if s.get("diagnose_result") is not None))
    logger.info("通过门限: %d", sum(1 for s in summaries if s.get("diagnose_result", {}) and s["diagnose_result"].get("gate_passed", False)))
    logger.info("错误: %d", sum(1 for s in summaries if "error" in s))
    logger.info("=" * 70)


if __name__ == "__main__":
    main()
