#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-004 v3.0 — WCS 闭环诊断工具 (权威星对版)

在 P11-002 v2.0 (Siril 1.4.3 升级) 基础上, 新增 `--authoritative-pairs` 模式,
实施 AUTONOMOUS_ENTRY.md §2 双层闭环方案:

  A 层 (Solver Internal): 求解器内部 TRANS 预测 vs detector 坐标
       - 数据来源: solver.get_last_inliers() (C++ SolveInlierCache 导出)
       - 用途: 验证求解器内部 RMS 与最终 inlier 残差一致
       - 不参与硬 Gate, 仅作对照

  B 层 (Serialized WCS Hard Gate): 用最终序列化到 FITS Header 的标准 WCS+SIP
       独立将 Gaia 星回投到像素, 与 detector 坐标比较
       - 数据来源: astropy.wcs.WCS(header).world_to_pixel(gaia_ra, gaia_dec)
       - 残差 = detector_xy - external_pred_xy
       - 硬 Gate: p68<=0.75px, p90<=1.5px, p99<=3.0px, n>=5

  C 层 (Blind Catalog): 全星表 kd-tree 重新匹配, 仅作二级诊断, 不参与硬 Gate
       (本模式禁止启用 C 层)

详见:
  - docs/24_WCS_VALIDATION_V2_SPEC.md
  - docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md
  - docs/26_P11_RECOVERY_RUNBOOK.md

权威星对契约 (25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md):
  pair_id, gaia_ra, gaia_dec, det_x_px, det_y_px,
  internal_pred_x_px, internal_pred_y_px,
  external_pred_x_px, external_pred_y_px,
  internal_residual_x_px, internal_residual_y_px, internal_residual_dist_px,
  external_residual_x_px, external_residual_y_px, external_residual_dist_px,
  abs_delta_pred_x_px, abs_delta_pred_y_px, abs_delta_pred_dist_px

用法:
  # 单帧权威星对模式
  python wcs_closure_diagnostic_v3.py --fits <calibrated.fits> \\
      --output-dir <dir> --project-root <root> --authoritative-pairs

  # 批量权威星对模式
  python wcs_closure_diagnostic_v3.py --batch <frames.json> \\
      --output-dir <dir> --project-root <root> --authoritative-pairs
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

# 复用 P11-002 v2 的工具函数 (避免代码重复)
# 路径: P11-004/scripts -> ../.. -> evidence -> P11-002/scripts
_V2_SCRIPTS_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "P11-002", "scripts")
)
if _V2_SCRIPTS_DIR not in sys.path:
    sys.path.insert(0, _V2_SCRIPTS_DIR)

# 导入 v2 工具函数
from wcs_closure_diagnostic import (  # noqa: E402
    SIRIL_AT_MATCH_CATALOG_NBRIGHT,
    SIRIL_AT_MATCH_PERCENTILE,
    SIRIL_ONE_STDEV_PERCENTILE,
    SIRIL_AT_MATCH_NSIGMA,
    SIRIL_AT_MATCH_MAXDIST_PX,
    SIRIL_AT_MATCH_MAXITER,
    read_fits_image_and_header,
    build_astropy_wcs_from_header,
    init_platesolve_env,
    close_platesolve_env,
    cone_search_gaia,
    project_gaia_to_pixel,
    compute_stats,
    pixel_sky_pixel_closure,
    sky_pixel_sky_closure,
    generate_residual_plot,
    generate_quadrant_plot,
    iterative_outlier_rejection_siril,
)

logger = logging.getLogger("wcs_closure_v3")


# ============================================================================
# 权威星对获取: 调用 solver.get_last_inliers() 拿 C++ SolveInlierCache 数据
# ============================================================================
def fetch_authoritative_inliers(solver) -> np.ndarray:
    """从 IPVSolver 获取最终权威 inlier 数据 (A 层: 内部预测)

    Returns:
        arr: shape=(N, 9) float64
            [0] det_x_px, [1] det_y_px,
            [2] gaia_ra_deg, [3] gaia_dec_deg,
            [4] pred_x_px (internal TRANS 预测),
            [5] pred_y_px,
            [6] residual_x_px = det_x - pred_x,
            [7] residual_y_px = det_y - pred_y,
            [8] residual_dist_px = sqrt(res_x² + res_y²)
    """
    arr = solver.get_last_inliers()
    logger.info(
        "权威 inlier: n=%d (来自 IPVSolver.get_last_inliers)",
        len(arr),
    )
    return arr


# ============================================================================
# B 层: 用最终序列化的 WCS+SIP (astropy) 回投 Gaia 星到像素
# ============================================================================
def u_to_astropy_pixel(u_xy: np.ndarray, wcs) -> np.ndarray:
    """solver U 坐标 → astropy 0-based pixel 坐标转换

    solver U 坐标系 (ipv_select.cpp L686-687):
        U.x = det_x - cx              (X 中心原点, 方向与 FITS pixel 一致)
        U.y = -(det_y - cy)           (Y 中心原点, 方向与 FITS pixel 相反 — U_y 向上为正)

    astropy 0-based pixel:
        wcs.wcs.crpix 返回 FITS 1-based 原始值 (非 0-based!), 需 -1 转换
        pixel_x = U_x + (CRPIX_fits - 1)     # = U_x + CRPIX_0based
        pixel_y = (CRPIX_fits - 1) - U_y     # 减号: Y 轴方向相反

    P11-004 v3.4 修复: wcs.wcs.crpix 是 FITS 1-based 原始值, 不是 0-based。
        之前误把 1-based 当 0-based, 导致 det_xy_astropy 偏大 +1px,
        与 world_to_pixel (返回 0-based) 比较, 残差系统性偏移 ~1px。
        修复: crpix_0based = wcs.wcs.crpix - 1.0
    P11-004 v3.3 修复: 之前用 `+ CRPIX` 导致 Y 残差 ~2000px (det_y + ext_pred_y ≈ height),
        正确的 Y 转换是 CRPIX - U_y。
    """
    crpix_0based = np.array([float(wcs.wcs.crpix[0]) - 1.0, float(wcs.wcs.crpix[1]) - 1.0])
    out = np.empty_like(u_xy, dtype=np.float64)
    out[:, 0] = u_xy[:, 0] + crpix_0based[0]
    out[:, 1] = crpix_0based[1] - u_xy[:, 1]
    return out


def project_authoritative_pairs(
    wcs,
    pairs: np.ndarray,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """B 层: 用 astropy WCS 把权威对的 Gaia (ra,dec) 回投到像素

    输入 pairs: shape=(N, 9) 权威 inlier 数组 (字段见 fetch_authoritative_inliers)
        pairs[:, 0:2] = (det_x_px, det_y_px) — solver U 坐标 (图像中心原点, Y 轴向上)
        pairs[:, 4:6] = (pred_x_px, pred_y_px) — 内部 TRANS 预测 (U 坐标)
    输出:
        external_pred_xy: shape=(N, 2) 外部 WCS 回投的像素坐标 (astropy 0-based, Y 向下)
        external_res: shape=(N, 2) 残差 = det_xy_astropy - external_pred_xy
        external_dist: shape=(N) 残差距离

    坐标系转换 (P11-004 v3.3 修复):
        solver U: 原点=图像中心, Y 轴向上 (ipv_select.cpp: U.y = -(det_y - cy))
        astropy 0-based pixel: 原点=图像角落, Y 轴向下 (与 FITS 一致)
        转换: pixel_x = U_x + CRPIX_x_0based
              pixel_y = CRPIX_y_0based - U_y   (减号, Y 方向相反)
    """
    import astropy.units as u
    from astropy.coordinates import SkyCoord

    if len(pairs) == 0:
        return (
            np.zeros((0, 2)),
            np.zeros((0, 2)),
            np.zeros(0),
        )

    ra = pairs[:, 2]
    dec = pairs[:, 3]
    sky = SkyCoord(ra * u.deg, dec * u.deg)
    pred_x, pred_y = wcs.world_to_pixel(sky)

    external_pred_xy = np.column_stack([pred_x, pred_y])

    # 坐标系转换: solver U → astropy 0-based pixel (Y 轴翻转)
    det_xy_astropy = u_to_astropy_pixel(pairs[:, [0, 1]], wcs)

    external_res = det_xy_astropy - external_pred_xy
    external_dist = np.sqrt(external_res[:, 0] ** 2 + external_res[:, 1] ** 2)
    return external_pred_xy, external_res, external_dist


# ============================================================================
# A vs B 层对比: 内部预测 vs 外部 WCS 回投预测
# ============================================================================
def compute_layer_delta(pairs: np.ndarray, external_pred_xy: np.ndarray, wcs=None) -> Dict[str, Any]:
    """A vs B 预测差 (检验 WCS 序列化是否引入额外误差)

    注意: internal_pred (pairs[:, 4:6]) 和 external_pred (astropy pixel) 在不同坐标系:
        - internal_pred: solver U (中心原点, Y 向上)
        - external_pred: astropy 0-based pixel (角落原点, Y 向下)
    需将 internal_pred 从 U 坐标转为 astropy pixel (使用 u_to_astropy_pixel 统一转换) 后再比较。

    参数:
        pairs: (N, 9) 权威 inlier 数组
        external_pred_xy: (N, 2) astropy WCS 回投的 0-based 像素坐标
        wcs: astropy WCS 对象 (用于获取 CRPIX_0based); None 时尝试从 pairs 长度推断 (向后兼容)
    """
    if len(pairs) == 0:
        return {
            "delta_x_mean_px": 0.0, "delta_y_mean_px": 0.0,
            "delta_x_median_abs_px": 0.0, "delta_y_median_abs_px": 0.0,
            "delta_dist_median_px": 0.0, "delta_dist_p90_px": 0.0,
            "delta_dist_p99_px": 0.0, "delta_dist_max_px": 0.0,
        }

    # 将内部预测从 U 坐标转为 astropy 0-based pixel (使用统一转换函数)
    internal_pred_xy = pairs[:, [4, 5]].copy()
    if wcs is not None:
        internal_pred_astropy = u_to_astropy_pixel(internal_pred_xy, wcs)
    else:
        # 无 WCS 时无法转换, 退回原行为 (会产生大 delta, 仅用于向后兼容)
        internal_pred_astropy = internal_pred_xy

    dx = external_pred_xy[:, 0] - internal_pred_astropy[:, 0]
    dy = external_pred_xy[:, 1] - internal_pred_astropy[:, 1]
    dist = np.sqrt(dx * dx + dy * dy)
    return {
        "delta_x_mean_px": float(np.mean(dx)),
        "delta_y_mean_px": float(np.mean(dy)),
        "delta_x_median_abs_px": float(np.median(np.abs(dx))),
        "delta_y_median_abs_px": float(np.median(np.abs(dy))),
        "delta_dist_median_px": float(np.median(dist)),
        "delta_dist_p90_px": float(np.percentile(dist, 90)),
        "delta_dist_p99_px": float(np.percentile(dist, 99)),
        "delta_dist_max_px": float(np.max(dist)),
    }


# ============================================================================
# 权威星对诊断主流程
# ============================================================================
def diagnose_frame_authoritative(
    fits_path: str,
    output_dir: str,
    project_root: str,
    env=None,
    siril_outlier_rejection: bool = True,
    ra0_override: float = 0.0,
    dec0_override: float = 0.0,
    focal_length_override: float = 0.0,
    pixel_size_override: float = 0.0,
) -> Dict[str, Any]:
    """权威星对双层闭环诊断 (P11-004 v3.0)

    流程:
      1. 读取 FITS (含 WCS header, 若无则先 PlateSolve 求解并写入)
      2. 构建 astropy WCS (独立于 PlateSolve transform)
      3. 运行 PlateSolve (callback 拿检测星点 + 内部状态)
      4. 调用 solver.get_last_inliers() 获取权威 inlier 对
      5. A 层: 用 pairs 的内部预测字段 (TRANS 预测) 计算残差
      6. B 层: 用 astropy WCS.world_to_pixel 把 pairs 的 (ra,dec) 回投到像素
              残差 = detector_xy - external_pred_xy
      7. A vs B 预测差 (delta = external_pred - internal_pred)
      8. 硬 Gate: B 层 p68/p90/p99, n>=5

    参数覆盖 (P11-004 v3.1 新增):
      ra0_override, dec0_override       - 优先于 FITS 头的初始指向 (度)
      focal_length_override             - 优先于 FITS 头的焦距 (mm)
      pixel_size_override               - 优先于 FITS 头的像素尺寸 (um)
      用途: 当 .fts 文件缺失 FOCALLEN/XPIXSZ/OBJCTRA/OBJCTDEC 时,
            从 REPRESENTATIVE_FRAMES_ARCHIVE.json 提供外部参数。

    禁止:
      - kd-tree 重新匹配 (match_pairs_kdtree / match_pairs_bright_first)
      - 全星表 cone_search (权威对已含 Gaia ra/dec)
      - 修改 CD/SIP/CRPIX 以"通过 Gate"

    Returns:
        closure_report dict
    """
    frame_id = Path(fits_path).stem
    logger.info("=" * 70)
    logger.info("P11-004 v3.0 权威星对诊断: %s", frame_id)
    logger.info("FITS: %s", fits_path)
    logger.info("MODE: authoritative_pairs (kd-tree rematch DISABLED)")

    start_time = time.time()

    # 1. 读取 FITS
    pixels, header = read_fits_image_and_header(fits_path)
    height, width = pixels.shape

    # 2. 检查 WCS, 若无则求解
    has_wcs = "CTYPE1" in header and "CRVAL1" in header and "CD1_1" in header
    solve_performed = False
    if not has_wcs:
        logger.info("FITS 无 WCS header, 调用 PlateSolve 求解并写入 WCS...")
        own_env = env is None
        if own_env:
            env = init_platesolve_env(project_root)
        try:
            from solve_and_write_wcs import solve_and_write_wcs
            result = solve_and_write_wcs(
                fits_path,
                ra0=ra0_override, dec0=dec0_override,
                focal_length=focal_length_override,
                pixel_size=pixel_size_override,
                overwrite=True, env=env,
            )
            logger.info(
                "PlateSolve 求解完成: success=%s, n_pairs=%d, rms_px=%.4f",
                result.get("success", False), result.get("n_pairs", 0),
                result.get("rms_px", 0.0),
            )
            solve_performed = True
        finally:
            if own_env:
                close_platesolve_env(env)
                env = None
        _, header = read_fits_image_and_header(fits_path)
        has_wcs = "CTYPE1" in header and "CRVAL1" in header and "CD1_1" in header
        if not has_wcs:
            raise RuntimeError("PlateSolve 求解后 FITS 仍无 WCS, 无法诊断")

    # 3. 构建 astropy WCS
    wcs, wcs_summary = build_astropy_wcs_from_header(header)
    wcs_summary["solve_performed_by_tool"] = bool(solve_performed)

    # 4. 初始化 PlateSolve 环境
    own_env = env is None
    if own_env:
        env = init_platesolve_env(project_root)
    gaia_client, sdet, solver = env

    try:
        # 5. 重新运行 PlateSolve (必须用 callback 路径以触发 cache_last_inliers_)
        # 注意: 即使 FITS 已有 WCS, 也必须重跑一次以填充 last_inliers_ 缓存
        from solve_and_write_wcs import read_fits_header
        header_info = read_fits_header(
            fits_path,
            default_ra0=ra0_override,
            default_dec0=dec0_override,
            default_focal_length=focal_length_override,
            default_pixel_size=pixel_size_override,
        )
        solve_ra0 = header_info["ra0"]
        solve_dec0 = header_info["dec0"]
        focal_length = header_info["focal_length"]
        pixel_size = header_info["pixel_size"]

        if focal_length <= 0 or pixel_size <= 0:
            raise RuntimeError(
                f"FITS header 缺失 FOCALLEN/XPIXSZ: fl={focal_length}, ps={pixel_size}"
            )

        logger.info("重跑 PlateSolve 以填充权威 inlier 缓存...")
        # 用 solve_from_memory_with_callback 触发 cache_last_inliers_
        detected_holder: List[np.ndarray] = []

        def callback(detections: np.ndarray, n: int, user_data) -> None:
            if n > 0:
                detected_holder.append(detections.copy())

        result_obj = solver.solve_from_memory_with_callback(
            pixels, width, height,
            solve_ra0, solve_dec0, focal_length, pixel_size,
            callback, user_data=None,
        )

        if not result_obj.success:
            raise RuntimeError(
                f"PlateSolve 求解失败: success=0, error='{result_obj.error_msg.decode('utf-8', errors='ignore')}'"
            )

        # 6. 获取权威 inlier 对 (A 层: 内部预测字段)
        pairs = fetch_authoritative_inliers(solver)
        n_pairs = len(pairs)

        if n_pairs == 0:
            logger.error("权威 inlier 数为 0, 无法诊断")
            report = {
                "frame_id": frame_id,
                "fits_path": os.path.abspath(fits_path),
                "tool_version": "P11-004 v3.0 (authoritative_pairs)",
                "mode": "authoritative_pairs",
                "elapsed_sec": float(time.time() - start_time),
                "n_authoritative_pairs": 0,
                "gate_passed": False,
                "gate_check": {
                    "n_matched_ge_5": False,
                    "p68_le_0_75_px": False,
                    "p90_le_1_5_px": False,
                    "p99_le_3_0_px": False,
                },
                "error": "no_authoritative_inliers",
            }
            os.makedirs(output_dir, exist_ok=True)
            report_path = os.path.join(output_dir, "closure_report.json")
            with open(report_path, "w", encoding="utf-8") as f:
                json.dump(report, f, indent=2)
            return report

        # 7. A 层统计: 内部 TRANS 预测残差
        # pairs 字段: [0]det_x [1]det_y [2]ra [3]dec [4]pred_x [5]pred_y
        #             [6]res_x [7]res_y [8]res_dist
        internal_res = pairs[:, [6, 7]]
        internal_dist = pairs[:, 8]
        internal_stats = {
            "n": int(n_pairs),
            "residual_dist_median_px": float(np.median(internal_dist)),
            "residual_dist_p35_px": float(np.percentile(internal_dist, SIRIL_AT_MATCH_PERCENTILE * 100.0)),
            "residual_dist_p68_px": float(np.percentile(internal_dist, SIRIL_ONE_STDEV_PERCENTILE * 100.0)),
            "residual_dist_p90_px": float(np.percentile(internal_dist, 90)),
            "residual_dist_p99_px": float(np.percentile(internal_dist, 99)),
            "residual_dist_max_px": float(np.max(internal_dist)),
            "residual_x_mean_px": float(np.mean(internal_res[:, 0])),
            "residual_y_mean_px": float(np.mean(internal_res[:, 1])),
            "residual_x_std_px": float(np.std(internal_res[:, 0])),
            "residual_y_std_px": float(np.std(internal_res[:, 1])),
        }
        logger.info(
            "[A 层 内部 TRANS 预测] n=%d, p68=%.4f px, p90=%.4f px, p99=%.4f px",
            n_pairs, internal_stats["residual_dist_p68_px"],
            internal_stats["residual_dist_p90_px"],
            internal_stats["residual_dist_p99_px"],
        )

        # 8. B 层: astropy WCS 回投 → 外部预测残差
        external_pred_xy, external_res, external_dist = project_authoritative_pairs(wcs, pairs)

        # P11-004 v3.3 诊断: B 层残差预剔除日志 (排查 Siril 全剔除问题)
        if n_pairs > 0:
            det_xy_diag = u_to_astropy_pixel(pairs[:, [0, 1]], wcs)
            logger.info(
                "[B 层 预剔除诊断] n=%d, CRPIX_0based=(%.2f,%.2f), "
                "det_xy range=[%.1f,%.1f]~[%.1f,%.1f], "
                "ext_pred range=[%.1f,%.1f]~[%.1f,%.1f], "
                "ext_dist: median=%.4f p68=%.4f p90=%.4f p99=%.4f max=%.4f",
                n_pairs,
                float(wcs.wcs.crpix[0]), float(wcs.wcs.crpix[1]),
                float(np.min(det_xy_diag[:, 0])), float(np.min(det_xy_diag[:, 1])),
                float(np.max(det_xy_diag[:, 0])), float(np.max(det_xy_diag[:, 1])),
                float(np.min(external_pred_xy[:, 0])), float(np.min(external_pred_xy[:, 1])),
                float(np.max(external_pred_xy[:, 0])), float(np.max(external_pred_xy[:, 1])),
                float(np.median(external_dist)),
                float(np.percentile(external_dist, 68)),
                float(np.percentile(external_dist, 90)),
                float(np.percentile(external_dist, 99)),
                float(np.max(external_dist)),
            )

        # B 层 Siril 风格迭代剔除 (复用 v2 函数, matches 拆解为 (det_idx, pred_idx, dist))
        siril_rejection_info: Dict[str, Any] = {"enabled": bool(siril_outlier_rejection)}
        keep_mask = np.ones(n_pairs, dtype=bool)
        if siril_outlier_rejection and n_pairs > 0:
            # 构造 matches 参数 (要求 list of (det_idx, pred_idx, dist))
            fake_matches = [(i, i, float(external_dist[i])) for i in range(n_pairs)]
            keep_mask, siril_rejection_info = iterative_outlier_rejection_siril(
                external_res, external_dist, fake_matches,
                max_iter=SIRIL_AT_MATCH_MAXITER,
                hard_max_dist_px=SIRIL_AT_MATCH_MAXDIST_PX,
                percentile=SIRIL_AT_MATCH_PERCENTILE,
                nsigma=SIRIL_AT_MATCH_NSIGMA,
            )
            n_kept = int(np.sum(keep_mask))
            logger.info(
                "Siril 剔除后保留: %d/%d 对 (剔除 %d)",
                n_kept, n_pairs, n_pairs - n_kept,
            )

        # 过滤到保留子集
        pairs_kept = pairs[keep_mask]
        external_pred_kept = external_pred_xy[keep_mask]
        external_res_kept = external_res[keep_mask]
        external_dist_kept = external_dist[keep_mask]
        internal_dist_kept = internal_dist[keep_mask]
        n_kept = int(np.sum(keep_mask))

        b_stats = {
            "n_before_rejection": int(n_pairs),
            "n_after_rejection": n_kept,
            "rejection_info": siril_rejection_info,
            "residual_dist_median_px": float(np.median(external_dist_kept)) if n_kept > 0 else 0.0,
            "residual_dist_p35_px": float(np.percentile(external_dist_kept, SIRIL_AT_MATCH_PERCENTILE * 100.0)) if n_kept > 0 else 0.0,
            "residual_dist_p68_px": float(np.percentile(external_dist_kept, SIRIL_ONE_STDEV_PERCENTILE * 100.0)) if n_kept > 0 else 0.0,
            "residual_dist_p90_px": float(np.percentile(external_dist_kept, 90)) if n_kept > 0 else 0.0,
            "residual_dist_p99_px": float(np.percentile(external_dist_kept, 99)) if n_kept > 0 else 0.0,
            "residual_dist_max_px": float(np.max(external_dist_kept)) if n_kept > 0 else 0.0,
            "residual_x_mean_px": float(np.mean(external_res_kept[:, 0])) if n_kept > 0 else 0.0,
            "residual_y_mean_px": float(np.mean(external_res_kept[:, 1])) if n_kept > 0 else 0.0,
            "residual_x_std_px": float(np.std(external_res_kept[:, 0])) if n_kept > 0 else 0.0,
            "residual_y_std_px": float(np.std(external_res_kept[:, 1])) if n_kept > 0 else 0.0,
        }
        logger.info(
            "[B 层 外部 WCS 回投] n=%d, p68=%.4f px, p90=%.4f px, p99=%.4f px",
            n_kept, b_stats["residual_dist_p68_px"],
            b_stats["residual_dist_p90_px"],
            b_stats["residual_dist_p99_px"],
        )

        # 9. A vs B 层预测差 (WCS 序列化引入的额外误差)
        layer_delta = compute_layer_delta(pairs_kept, external_pred_kept, wcs)
        logger.info(
            "[A vs B 预测差] delta_dist: median=%.4f px, p90=%.4f px, max=%.4f px",
            layer_delta["delta_dist_median_px"],
            layer_delta["delta_dist_p90_px"],
            layer_delta["delta_dist_max_px"],
        )

        # 10. 双向闭环 (复用 v2 函数)
        # det_xy 转为 astropy 0-based pixel (使用 u_to_astropy_pixel, Y 轴翻转) 以匹配 WCS 接口
        det_xy_astropy = u_to_astropy_pixel(pairs_kept[:, [0, 1]], wcs)
        gaia_ra_kept = pairs_kept[:, 2]
        gaia_dec_kept = pairs_kept[:, 3]
        psp_closure = pixel_sky_pixel_closure(wcs, det_xy_astropy, n_samples=min(200, n_kept))
        sps_closure = sky_pixel_sky_closure(
            wcs, gaia_ra_kept, gaia_dec_kept, n_samples=min(200, n_kept),
        )

        # 11. 硬 Gate 判定 (B 层)
        gate_check = {
            "n_matched_ge_5": n_kept >= 5,
            "p68_le_0_75_px": b_stats["residual_dist_p68_px"] <= 0.75,
            "p90_le_1_5_px": b_stats["residual_dist_p90_px"] <= 1.5,
            "p99_le_3_0_px": b_stats["residual_dist_p99_px"] <= 3.0,
        }
        gate_passed = all(gate_check.values())

        # 12. 残差分布象限统计 (复用 v2 函数, 需要 detected_xy)
        # 用权威对 (astropy 像素坐标) 作为 detected_xy (象限统计仅看分布)
        full_stats = compute_stats(
            external_res_kept, external_dist_kept,
            int(width), int(height), det_xy_astropy,
        )

        # 13. 输出: JSONL matched_pairs_authoritative.jsonl (按契约)
        # 注意: 所有坐标字段统一为 astropy 0-based pixel (与 WCS 一致, Y 向下);
        # 内部预测需从 U 坐标转为 astropy pixel (使用 u_to_astropy_pixel, Y 轴翻转)
        os.makedirs(output_dir, exist_ok=True)
        pairs_jsonl_path = os.path.join(output_dir, "matched_pairs_authoritative.jsonl")
        with open(pairs_jsonl_path, "w", encoding="utf-8") as f:
            for i in range(n_kept):
                p = pairs_kept[i]
                ep = external_pred_kept[i]
                # 内部预测: U → astropy pixel (使用统一转换, Y 轴翻转)
                in_pred_xy_astropy = u_to_astropy_pixel(
                    np.array([[p[4], p[5]]], dtype=np.float64), wcs
                )[0]
                in_pred_x_astropy = float(in_pred_xy_astropy[0])
                in_pred_y_astropy = float(in_pred_xy_astropy[1])
                # 检测点坐标: U → astropy pixel
                det_xy_astropy_i = u_to_astropy_pixel(
                    np.array([[p[0], p[1]]], dtype=np.float64), wcs
                )[0]
                det_x_astropy = float(det_xy_astropy_i[0])
                det_y_astropy = float(det_xy_astropy_i[1])
                # 内部预测残差 (U 坐标系, 与 solver RMS 一致)
                in_res_x = float(p[6])
                in_res_y = float(p[7])
                in_res_d = float(p[8])
                # 外部 WCS 回投残差
                ex_res_x = float(external_res_kept[i, 0])
                ex_res_y = float(external_res_kept[i, 1])
                ex_res_d = float(external_dist_kept[i])
                # A vs B 预测差 (均在 astropy pixel 坐标系)
                dx = float(ep[0]) - in_pred_x_astropy
                dy = float(ep[1]) - in_pred_y_astropy
                dd = float(np.sqrt(dx * dx + dy * dy))

                pair_record = {
                    "pair_id": i,
                    "gaia_ra_deg": float(p[2]),
                    "gaia_dec_deg": float(p[3]),
                    "det_x_px": det_x_astropy,
                    "det_y_px": det_y_astropy,
                    "internal_pred_x_px": in_pred_x_astropy,
                    "internal_pred_y_px": in_pred_y_astropy,
                    "external_pred_x_px": float(ep[0]),
                    "external_pred_y_px": float(ep[1]),
                    "internal_residual_x_px": in_res_x,
                    "internal_residual_y_px": in_res_y,
                    "internal_residual_dist_px": in_res_d,
                    "external_residual_x_px": ex_res_x,
                    "external_residual_y_px": ex_res_y,
                    "external_residual_dist_px": ex_res_d,
                    "abs_delta_pred_x_px": abs(dx),
                    "abs_delta_pred_y_px": abs(dy),
                    "abs_delta_pred_dist_px": dd,
                }
                f.write(json.dumps(pair_record, ensure_ascii=False) + "\n")
        logger.info("权威星对已保存 (JSONL): %s (n=%d)", pairs_jsonl_path, n_kept)

        # 14. 闭环报告
        elapsed = time.time() - start_time
        report: Dict[str, Any] = {
            "frame_id": frame_id,
            "fits_path": os.path.abspath(fits_path),
            "tool_version": "P11-004 v3.0 (authoritative_pairs)",
            "tool_independent_of_platesolve_transform": True,
            "mode": "authoritative_pairs",
            "kd_tree_rematch_used": False,  # 关键: 禁止 kd-tree
            "elapsed_sec": float(elapsed),
            "image_size": {"width": int(width), "height": int(height)},
            "wcs": wcs_summary,
            "platesolve": {
                "ra0": float(solve_ra0),
                "dec0": float(solve_dec0),
                "focal_length_mm": float(focal_length),
                "pixel_size_um": float(pixel_size),
                "success": bool(result_obj.success),
                "n_pairs": int(result_obj.n_pairs),
                "rms_px": float(result_obj.rms_px),
                "rms_arcsec": float(result_obj.rms_arcsec),
                "trans_order": int(result_obj.trans_order),
                "sip_order": int(result_obj.sip_order),
                "best_inliers": int(result_obj.best_inliers),
            },
            "n_authoritative_pairs": int(n_pairs),
            "n_after_rejection": n_kept,
            "layer_a_internal_stats": internal_stats,
            "layer_b_external_wcs_stats": b_stats,
            "layer_delta_external_minus_internal": layer_delta,
            "residual_stats": full_stats,
            "pixel_sky_pixel_closure": psp_closure,
            "sky_pixel_sky_closure": sps_closure,
            "gate_check": gate_check,
            "gate_passed": bool(gate_passed),
        }

        report_path = os.path.join(output_dir, "closure_report.json")
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        logger.info("闭环报告已保存: %s", report_path)
        logger.info(
            "[Gate] p68=%.4f (<=0.75:%s) p90=%.4f (<=1.5:%s) p99=%.4f (<=3.0:%s) n=%d (>=5:%s) => %s",
            b_stats["residual_dist_p68_px"], gate_check["p68_le_0_75_px"],
            b_stats["residual_dist_p90_px"], gate_check["p90_le_1_5_px"],
            b_stats["residual_dist_p99_px"], gate_check["p99_le_3_0_px"],
            n_kept, gate_check["n_matched_ge_5"],
            "PASS" if gate_passed else "FAIL",
        )

        # 15. 残差图 (复用 v2 函数, 用 B 层外部 WCS 残差)
        plot_path = os.path.join(output_dir, "residual_plot.png")
        generate_residual_plot(external_res_kept, external_dist_kept, plot_path, frame_id)

        # 16. 四象限分布图 (使用 astropy 像素坐标, 与 generate_quadrant_plot 的 width/2, height/2 分界一致)
        matched_mask = np.ones(len(det_xy_astropy), dtype=bool)  # 权威对全部视为 matched
        quad_plot_path = os.path.join(output_dir, "quadrant_plot.png")
        generate_quadrant_plot(det_xy_astropy, matched_mask, width, height, quad_plot_path, frame_id)

        logger.info("权威星对诊断完成: %s (耗时 %.2fs)", frame_id, elapsed)
        return report

    finally:
        if own_env:
            close_platesolve_env(env)


# ============================================================================
# 批量模式
# ============================================================================
def diagnose_batch_authoritative(
    frames_json: str,
    output_dir: str,
    project_root: str,
    siril_outlier_rejection: bool = True,
) -> Dict[str, Any]:
    """批量权威星对诊断

    Args:
        frames_json: JSON 文件, 内容为 [{fits, output_subdir, ra0, dec0,
                                       focal_length_mm, pixel_size_um}, ...]
                                       或 [fits_path, ...]
        output_dir: 输出根目录
        project_root: 项目根目录

    每个 item 字段 (P11-004 v3.1):
      fits (必填), output_subdir (可选, 默认 Path.stem)
      ra0, dec0 (可选, 度) - 优先于 FITS 头
      focal_length_mm (可选, mm) - 优先于 FITS 头
      pixel_size_um (可选, um) - 优先于 FITS 头

    JSON 格式 (P11-004 v3.1):
      支持纯数组 [{"fits":...}, ...]
      也支持对象包装 {"config_version":..., "frames":[...]}
    """
    with open(frames_json, "r", encoding="utf-8") as f:
        raw = json.load(f)
    if isinstance(raw, dict) and "frames" in raw:
        frames = raw["frames"]
    else:
        frames = raw

    logger.info("批量权威星对诊断: %d 帧", len(frames))

    env = init_platesolve_env(project_root)
    summaries: List[Dict[str, Any]] = []

    try:
        for item in frames:
            if isinstance(item, dict):
                fits_path = item["fits"]
                subdir = item.get("output_subdir", Path(fits_path).stem)
                ra0_ov = float(item.get("ra0", 0.0))
                dec0_ov = float(item.get("dec0", 0.0))
                fl_ov = float(item.get("focal_length_mm", 0.0))
                ps_ov = float(item.get("pixel_size_um", 0.0))
            else:
                fits_path = str(item)
                subdir = Path(fits_path).stem
                ra0_ov = dec0_ov = fl_ov = ps_ov = 0.0

            frame_out = os.path.join(output_dir, subdir)
            try:
                report = diagnose_frame_authoritative(
                    fits_path, frame_out, project_root,
                    env=env,
                    siril_outlier_rejection=siril_outlier_rejection,
                    ra0_override=ra0_ov,
                    dec0_override=dec0_ov,
                    focal_length_override=fl_ov,
                    pixel_size_override=ps_ov,
                )
                summaries.append({
                    "frame_id": report["frame_id"],
                    "fits_path": report["fits_path"],
                    "output_dir": frame_out,
                    "has_sip": report["wcs"]["has_sip"],
                    "sip_order": report["wcs"]["sip_order"],
                    "n_authoritative_pairs": report["n_authoritative_pairs"],
                    "n_after_rejection": report["n_after_rejection"],
                    "layer_a_internal_p68_px": report["layer_a_internal_stats"]["residual_dist_p68_px"],
                    "layer_b_external_p68_px": report["layer_b_external_wcs_stats"]["residual_dist_p68_px"],
                    "layer_b_external_p90_px": report["layer_b_external_wcs_stats"]["residual_dist_p90_px"],
                    "layer_b_external_p99_px": report["layer_b_external_wcs_stats"]["residual_dist_p99_px"],
                    "delta_pred_dist_median_px": report["layer_delta_external_minus_internal"]["delta_dist_median_px"],
                    "platesolve_rms_px": report["platesolve"]["rms_px"],
                    "gate_passed": report["gate_passed"],
                    "elapsed_sec": report["elapsed_sec"],
                })
            except Exception as e:
                logger.error("帧 %s 诊断失败: %s", fits_path, e, exc_info=True)
                summaries.append({
                    "frame_id": Path(fits_path).stem,
                    "fits_path": fits_path,
                    "output_dir": frame_out,
                    "error": str(e),
                    "gate_passed": False,
                })
    finally:
        close_platesolve_env(env)

    n_total = len(summaries)
    n_ok = sum(1 for s in summaries if s.get("gate_passed", False))
    n_err = sum(1 for s in summaries if "error" in s)

    summary = {
        "tool_version": "P11-004 v3.0 (authoritative_pairs)",
        "mode": "authoritative_pairs",
        "n_total": n_total,
        "n_passed": n_ok,
        "n_failed": n_total - n_ok - n_err,
        "n_errors": n_err,
        "pass_rate": float(n_ok / n_total) if n_total > 0 else 0.0,
        "frames": summaries,
    }
    summary_path = os.path.join(output_dir, "batch_summary.json")
    os.makedirs(output_dir, exist_ok=True)
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    logger.info("批量汇总: %s (pass=%d/%d, err=%d)",
                summary_path, n_ok, n_total, n_err)
    return summary


# ============================================================================
# CLI 入口
# ============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description="P11-004 v3.0 WCS 闭环诊断工具 (权威星对版)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--fits", help="校准后 FITS 文件路径 (单帧模式)")
    parser.add_argument("--batch", help="批量模式: JSON 文件, 含帧列表")
    parser.add_argument("--output-dir", required=True, help="输出目录")
    parser.add_argument("--project-root", default=".", help="项目根目录")
    parser.add_argument("--authoritative-pairs", action="store_true",
                        help="启用权威星对模式 (P11-004 v3.0 双层闭环, 禁止 kd-tree)")
    parser.add_argument("--no-siril-outlier-rejection", action="store_true",
                        help="禁用 Siril 风格迭代残差剔除 (默认启用)")
    parser.add_argument("--log", help="日志文件路径 (可选)")
    args = parser.parse_args()

    if args.log:
        fh = logging.FileHandler(args.log, encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
        logging.getLogger().addHandler(fh)

    siril_outlier_rejection = not args.no_siril_outlier_rejection

    if args.authoritative_pairs:
        # 权威星对模式 (P11-004 v3.0)
        if args.batch:
            diagnose_batch_authoritative(
                args.batch, args.output_dir, args.project_root,
                siril_outlier_rejection=siril_outlier_rejection,
            )
        elif args.fits:
            diagnose_frame_authoritative(
                args.fits, args.output_dir, args.project_root,
                env=None,
                siril_outlier_rejection=siril_outlier_rejection,
            )
        else:
            parser.error("必须指定 --fits 或 --batch")
    else:
        # 默认: 回退到 v2 模式 (kd-tree 重新匹配)
        logger.info("未启用 --authoritative-pairs, 回退到 v2 模式 (kd-tree)")
        from wcs_closure_diagnostic import main as v2_main
        # 替换 sys.argv 以让 v2 解析
        v2_args = ["wcs_closure_diagnostic.py"]
        if args.fits:
            v2_args.extend(["--fits", args.fits])
        if args.batch:
            v2_args.extend(["--batch", args.batch])
        v2_args.extend(["--output-dir", args.output_dir])
        v2_args.extend(["--project-root", args.project_root])
        if args.no_siril_outlier_rejection:
            v2_args.append("--no-siril-outlier-rejection")
        if args.log:
            v2_args.extend(["--log", args.log])
        sys.argv = v2_args
        return v2_main()
    return 0


if __name__ == "__main__":
    sys.exit(main())
