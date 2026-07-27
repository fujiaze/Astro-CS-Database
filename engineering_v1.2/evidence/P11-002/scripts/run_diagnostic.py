#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-002 — WCS 闭环诊断 driver 脚本

流程:
  1. 复制原始 Light FITS 到 work/ 目录 (避免污染原始)
  2. 调用 solve_and_write_wcs 求解 + 写入 WCS (PlateSolve)
  3. 调用 wcs_closure_diagnostic.diagnose_frame 诊断 (astropy WCS, 独立)
  4. 输出报告到 reports/<frame_id>/

用法:
    python run_diagnostic.py
"""

from __future__ import annotations

import json
import logging
import os
import shutil
import sys
import time
from pathlib import Path
from typing import Dict, List, Tuple

# 添加 lib 路径
PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
sys.path.insert(0, str(PROJECT_ROOT / "lib" / "plate_solve" / "python"))
sys.path.insert(0, str(PROJECT_ROOT / "lib" / "gaia_xpsd_client" / "python"))
sys.path.insert(0, str(Path(__file__).parent.resolve()))  # 添加本脚本目录

from solve_and_write_wcs import init_environment, _close_environment, solve_and_write_wcs
from wcs_closure_diagnostic import diagnose_frame

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("p11_002_driver")

# ============================================================================
# 配置
# ============================================================================
WORK_DIR = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-002" / "work"
REPORTS_DIR = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-002" / "reports"
LOG_DIR = PROJECT_ROOT / "engineering_v1.2" / "evidence" / "P11-002" / "raw_logs"

# 验证帧 (从 P10-006 代表帧选择, 覆盖不同设备/目标/滤镜)
VALIDATION_FRAMES: List[Tuple[str, str]] = [
    # (frame_id, 相对路径)
    (
        "T3_LUM_NGC55",
        r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts",
    ),
    (
        "T2_HA_LDN43",
        r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts",
    ),
]


def run_frame(
    frame_id: str,
    src_fits: str,
    work_dir: Path,
    reports_dir: Path,
    env,
) -> Dict:
    """处理单帧: 复制 -> 求解 -> 诊断"""
    logger.info("=" * 70)
    logger.info("处理帧: %s", frame_id)
    logger.info("源 FITS: %s", src_fits)

    # 1. 复制到 work
    work_dir.mkdir(parents=True, exist_ok=True)
    work_fits = work_dir / f"{frame_id}_solved.fits"
    logger.info("复制到: %s", work_fits)
    shutil.copy2(src_fits, work_fits)

    # 2. PlateSolve 求解 + 写 WCS
    logger.info("-" * 40)
    logger.info("PlateSolve 求解...")
    solve_start = time.time()
    solve_result = solve_and_write_wcs(
        str(work_fits), ra0=0, dec0=0, focal_length=0, pixel_size=0,
        overwrite=True, env=env,
    )
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
            "frame_id": frame_id,
            "src_fits": src_fits,
            "work_fits": str(work_fits),
            "solve_result": solve_result,
            "diagnose_result": None,
            "error": "PlateSolve 求解失败",
        }

    # 3. WCS 闭环诊断 (工具独立于 PlateSolve transform)
    logger.info("-" * 40)
    logger.info("WCS 闭环诊断 (astropy WCS, 独立于 PlateSolve transform)...")
    frame_reports = reports_dir / frame_id
    diag_start = time.time()
    report = diagnose_frame(
        str(work_fits), str(frame_reports), str(PROJECT_ROOT),
        env=env,
        max_match_dist_px=3.0,
        gaia_mag_high=18.0,
        solve_if_no_wcs=False,  # 已经求解过, 不再重复
    )
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
        "frame_id": frame_id,
        "src_fits": src_fits,
        "work_fits": str(work_fits),
        "solve_result": {
            "success": solve_result.get("success", False),
            "rms_px": solve_result.get("rms_px", 0.0),
            "n_pairs": solve_result.get("n_pairs", 0),
        },
        "diagnose_result": {
            "n_matched": report["matching"]["n_matched"],
            "dist_median_px": report["residual_stats"]["dist_median_px"],
            "dist_p90_px": report["residual_stats"]["dist_p90_px"],
            "dist_p99_px": report["residual_stats"]["dist_p99_px"],
            "gate_passed": report["gate_passed"],
            "has_sip": report["wcs"]["has_sip"],
            "sip_order": report["wcs"]["sip_order"],
        },
        "solve_elapsed_sec": solve_elapsed,
        "diag_elapsed_sec": diag_elapsed,
    }


def main():
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    log_file = LOG_DIR / "run_diagnostic.log"
    fh = logging.FileHandler(log_file, encoding="utf-8")
    fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
    logging.getLogger().addHandler(fh)

    logger.info("=" * 70)
    logger.info("P11-002 WCS 闭环诊断 driver")
    logger.info("PROJECT_ROOT: %s", PROJECT_ROOT)
    logger.info("WORK_DIR: %s", WORK_DIR)
    logger.info("REPORTS_DIR: %s", REPORTS_DIR)
    logger.info("验证帧数: %d", len(VALIDATION_FRAMES))

    # 初始化共享环境
    logger.info("-" * 70)
    logger.info("初始化 PlateSolve 环境...")
    env = init_environment()
    logger.info("环境就绪")

    summaries: List[Dict] = []
    try:
        for frame_id, rel_path in VALIDATION_FRAMES:
            src_fits = str(PROJECT_ROOT / rel_path)
            if not os.path.exists(src_fits):
                logger.error("源 FITS 不存在: %s", src_fits)
                summaries.append({
                    "frame_id": frame_id,
                    "src_fits": src_fits,
                    "error": "源文件不存在",
                })
                continue

            try:
                summary = run_frame(frame_id, src_fits, WORK_DIR, REPORTS_DIR, env)
                summaries.append(summary)
            except Exception as e:
                logger.error("帧 %s 处理失败: %s", frame_id, e, exc_info=True)
                summaries.append({
                    "frame_id": frame_id,
                    "src_fits": src_fits,
                    "error": str(e),
                })
    finally:
        logger.info("-" * 70)
        logger.info("释放 PlateSolve 环境...")
        _close_environment(*env)
        logger.info("环境已释放")

    # 汇总报告
    summary_path = REPORTS_DIR / "driver_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(
            {
                "tool_version": "P11-002 v1.0",
                "frames": summaries,
                "n_total": len(summaries),
                "n_passed": sum(1 for s in summaries if s.get("diagnose_result", {}).get("gate_passed", False)),
                "n_errors": sum(1 for s in summaries if "error" in s),
            },
            f, indent=2,
        )
    logger.info("=" * 70)
    logger.info("汇总报告: %s", summary_path)
    logger.info("帧数: %d", len(summaries))
    logger.info("通过: %d", sum(1 for s in summaries if s.get("diagnose_result", {}).get("gate_passed", False)))
    logger.info("错误: %d", sum(1 for s in summaries if "error" in s))
    logger.info("=" * 70)


if __name__ == "__main__":
    main()
