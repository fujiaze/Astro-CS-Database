#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-002 — 标准 WCS 真实星对闭环诊断工具

工具完全独立于 PlateSolve 内部 transform:
  - WCS 回投仅使用 astropy.wcs.WCS 从 FITS Header 解析
  - 不调用 PlateSolve 的 CD/SIP 转换函数
  - 检测星点仅作为像素坐标来源 (不参与 transform)
  - Gaia 星表通过 gaia_client_cone_search_for_solver 独立查询

闭环验证内容:
  1. 真实匹配对: detector (x,y) ↔ Gaia (ra,dec) 通过标准 WCS 回投后比较
  2. pixel → sky → pixel 双向闭环 (检测星点)
  3. sky → pixel → sky 双向闭环 (Gaia 星表)
  4. 残差统计: median/p90/p99, X/Y, 四象限分布
  5. 有 SIP / 无 SIP 分组对比

输出:
  - matched_pairs.json: 每对 (detector_x, detector_y, gaia_ra, gaia_dec, predicted_x, predicted_y, residual)
  - closure_report.json: 闭环统计报告
  - residual_plot.png: 残差分布图
  - quadrant_plot.png: 四象限分布图

用法:
    python wcs_closure_diagnostic.py --fits <calibrated.fits> --output-dir <dir> --project-root <root>
    python wcs_closure_diagnostic.py --batch <frames.json> --output-dir <dir> --project-root <root>
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# ============================================================================
# 日志配置
# ============================================================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("wcs_closure")


# ============================================================================
# FITS 读取
# ============================================================================
def read_fits_image_and_header(fits_path: str) -> Tuple[np.ndarray, Any]:
    """读取 FITS 图像和 header

    Returns:
        (pixels [H,W] float32, astropy Header)
    """
    from astropy.io import fits

    logger.info("读取 FITS: %s", fits_path)
    with fits.open(fits_path, mode="readonly", memmap=False) as hdul:
        data = hdul[0].data.astype(np.float32)
        header = hdul[0].header.copy()
    logger.info("图像尺寸: %dx%d", data.shape[1], data.shape[0])
    return data, header


# ============================================================================
# 构建 astropy WCS (独立于 PlateSolve transform)
# ============================================================================
def build_astropy_wcs_from_header(header) -> Tuple[Any, Dict]:
    """从 FITS header 构建 astropy WCS

    工具核心: 仅依赖 FITS Header 中的 CRPIX/CRVAL/CD/CTYPE/SIP,
    不读取 PlateSolve 的 IpvWcsResult, 不调用其 transform.

    Returns:
        (astropy.wcs.WCS, header_summary)
    """
    from astropy.wcs import WCS

    wcs = WCS(header)
    has_sip = getattr(wcs, "sip", None) is not None
    sip_order = int(header.get("A_ORDER", 0) or 0)

    summary = {
        "has_sip": bool(has_sip),
        "sip_order": sip_order,
        "crpix": [float(wcs.wcs.crpix[0]), float(wcs.wcs.crpix[1])],
        "crval": [float(wcs.wcs.crval[0]), float(wcs.wcs.crval[1])],
        "cd": [
            [float(wcs.wcs.cd[0][0]), float(wcs.wcs.cd[0][1])],
            [float(wcs.wcs.cd[1][0]), float(wcs.wcs.cd[1][1])],
        ],
        "ctype": [str(wcs.wcs.ctype[0]), str(wcs.wcs.ctype[1])],
    }
    logger.info(
        "WCS 构建: has_sip=%s sip_order=%d ctype=%s crpix=(%.3f,%.3f) crval=(%.6f,%.6f)",
        has_sip, sip_order, summary["ctype"],
        summary["crpix"][0], summary["crpix"][1],
        summary["crval"][0], summary["crval"][1],
    )
    return wcs, summary


# ============================================================================
# PlateSolve 环境 (用于获取检测星点)
# ============================================================================
def init_platesolve_env(project_root: str):
    """初始化 PlateSolve 环境 (GaiaClient + StarDetector + IPVSolver)

    注意: 这里使用 PlateSolve 仅是为了拿到"检测星点"像素坐标,
    工具不使用 PlateSolve 的 WCS 结果做回投.
    """
    sys.path.insert(0, os.path.join(project_root, "lib", "plate_solve", "python"))
    sys.path.insert(0, os.path.join(project_root, "lib", "gaia_xpsd_client", "python"))

    from solve_and_write_wcs import init_environment

    logger.info("初始化 PlateSolve 环境 (project_root=%s)", project_root)
    env = init_environment()
    logger.info("PlateSolve 环境就绪")
    return env


def close_platesolve_env(env):
    """释放 PlateSolve 环境"""
    from solve_and_write_wcs import _close_environment

    gaia_client, sdet, solver = env
    _close_environment(gaia_client, sdet, solver)
    logger.info("PlateSolve 环境已释放")


# ============================================================================
# Gaia 星表查询 (独立调用 gaia_client_cone_search_for_solver)
# ============================================================================
def cone_search_gaia(
    gaia_client,
    ra_deg: float,
    dec_deg: float,
    radius_deg: float,
    mag_high: float = 18.0,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """通过 ctypes 直接调用 gaia_client_cone_search_for_solver

    与 PlateSolve 内部使用同一 C API, 但工具侧独立查询,
    不依赖 PlateSolve 内部状态.

    Returns:
        (ra[N] float64, dec[N] float64, mag[N] float32)
    """
    import ctypes

    dll = gaia_client._dll
    handle = gaia_client._handle
    if isinstance(handle, ctypes.c_void_p):
        handle = handle.value

    out_ra = ctypes.POINTER(ctypes.c_double)()
    out_dec = ctypes.POINTER(ctypes.c_double)()
    out_mag = ctypes.POINTER(ctypes.c_float)()
    out_count = ctypes.c_int(0)

    logger.info(
        "Gaia cone_search: center=(%.6f, %.6f) radius=%.4f deg mag_high=%.1f",
        ra_deg, dec_deg, radius_deg, mag_high,
    )

    ret = dll.gaia_client_cone_search_for_solver(
        handle,
        ctypes.c_double(ra_deg),
        ctypes.c_double(dec_deg),
        ctypes.c_double(radius_deg),
        ctypes.c_double(mag_high),
        ctypes.byref(out_ra),
        ctypes.byref(out_dec),
        ctypes.byref(out_mag),
        ctypes.byref(out_count),
    )
    if ret != 0:
        raise RuntimeError(f"gaia_client_cone_search_for_solver 失败: ret={ret}")

    n = out_count.value
    logger.info("Gaia 查询返回: n=%d", n)
    if n == 0:
        return np.zeros(0), np.zeros(0), np.zeros(0)

    ra_arr = np.ctypeslib.as_array(out_ra, shape=(n,)).copy()
    dec_arr = np.ctypeslib.as_array(out_dec, shape=(n,)).copy()
    mag_arr = np.ctypeslib.as_array(out_mag, shape=(n,)).copy()

    # 释放 C 端内存
    gaia_client._msvcrt.free(out_ra)
    gaia_client._msvcrt.free(out_dec)
    gaia_client._msvcrt.free(out_mag)
    return ra_arr, dec_arr, mag_arr


# ============================================================================
# 运行 PlateSolve 求解 (callback 拿检测星点)
# ============================================================================
def solve_with_callback(
    solver,
    pixels: np.ndarray,
    width: int,
    height: int,
    ra0: float,
    dec0: float,
    focal_length: float,
    pixel_size: float,
    params=None,
) -> Tuple[np.ndarray, Any]:
    """运行 PlateSolve 并通过 callback 拿检测星点

    Returns:
        (detections [N,6] float64, IpvWcsResult)
        detections 列: [x_px, y_px, flux, mag, saturated, has_saturated]
        注意: 工具不使用 IpvWcsResult 的 WCS 字段做回投.
    """
    detected_holder: List[np.ndarray] = []

    def callback(detections: np.ndarray, n: int, user_data) -> None:
        if n > 0:
            # 已经在 trampoline 中复制过, 这里再保一份
            detected_holder.append(detections.copy())
            logger.info("callback 收到 %d 个检测星点", n)

    logger.info(
        "PlateSolve 求解: ra0=%.6f dec0=%.6f fl=%.2f ps=%.4f",
        ra0, dec0, focal_length, pixel_size,
    )
    result = solver.solve_from_memory_with_callback(
        pixels, width, height,
        ra0, dec0, focal_length, pixel_size,
        callback, user_data=None, params=params,
    )
    logger.info(
        "PlateSolve 完成: success=%s n_detected=%d n_pairs=%d rms_px=%.4f trans_order=%d",
        result.success, result.n_detected, result.n_pairs,
        result.rms_px, result.trans_order,
    )

    if not detected_holder:
        return np.zeros((0, 6)), result
    return detected_holder[0], result


# ============================================================================
# astropy WCS 回投 (核心: 独立于 PlateSolve transform)
# ============================================================================
def project_gaia_to_pixel(wcs, ra: np.ndarray, dec: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """用 astropy WCS 将 Gaia (ra,dec) 投影到像素坐标

    Returns:
        (pred_xy [N,2], valid_mask [N] bool)
        调用方按 valid_mask 过滤原始 ra/dec/mag
    """
    import astropy.units as u
    from astropy.coordinates import SkyCoord

    if len(ra) == 0:
        return np.zeros((0, 2)), np.zeros(0, dtype=bool)

    sky = SkyCoord(ra * u.deg, dec * u.deg)
    pred_x, pred_y = wcs.world_to_pixel(sky)

    # 过滤 NaN/Inf
    valid = np.isfinite(pred_x) & np.isfinite(pred_y)
    n_valid = int(np.sum(valid))
    logger.info("world_to_pixel 有效: %d/%d", n_valid, len(ra))

    pred_xy = np.column_stack([pred_x[valid], pred_y[valid]])
    return pred_xy, valid


# ============================================================================
# kd-tree 匹配 (检测星点 ↔ 预测像素)
# ============================================================================
def match_pairs_kdtree(
    detected_xy: np.ndarray,
    predicted_xy: np.ndarray,
    max_dist_px: float = 3.0,
) -> List[Tuple[int, int, float]]:
    """用 scipy.spatial.cKDTree 双向最近邻匹配

    Returns:
        list of (det_idx, pred_idx, dist_px)
    """
    if len(detected_xy) == 0 or len(predicted_xy) == 0:
        return []

    try:
        from scipy.spatial import cKDTree
    except ImportError:
        logger.warning("scipy 不可用, 回退到 numpy 暴力匹配")
        return _match_pairs_numpy(detected_xy, predicted_xy, max_dist_px)

    tree_det = cKDTree(detected_xy)
    tree_pred = cKDTree(predicted_xy)

    matches: List[Tuple[int, int, float]] = []
    # 对每个预测像素, 找最近的检测星点
    dists, idxs = tree_det.query(predicted_xy, k=1, distance_upper_bound=max_dist_px)
    for i, (d, det_idx) in enumerate(zip(dists, idxs)):
        if not np.isfinite(d) or d > max_dist_px:
            continue
        # 反向校验: 检测星点的最近预测像素是否为 i
        d_back, idx_back = tree_pred.query(detected_xy[det_idx], k=1, distance_upper_bound=max_dist_px)
        if idx_back == i and d_back <= max_dist_px:
            matches.append((int(det_idx), i, float(d)))
    logger.info("kd-tree 匹配: %d 对 (max_dist=%.2f px)", len(matches), max_dist_px)
    return matches


def _match_pairs_numpy(
    detected_xy: np.ndarray,
    predicted_xy: np.ndarray,
    max_dist_px: float,
) -> List[Tuple[int, int, float]]:
    """numpy 暴力最近邻匹配 (scipy 不可用时的回退)"""
    matches: List[Tuple[int, int, float]] = []
    for i in range(len(predicted_xy)):
        d = np.sqrt(((detected_xy - predicted_xy[i]) ** 2).sum(axis=1))
        j = int(np.argmin(d))
        if d[j] <= max_dist_px:
            # 反向校验
            d_back = np.sqrt(((predicted_xy - detected_xy[j]) ** 2).sum(axis=1))
            k = int(np.argmin(d_back))
            if k == i and d_back[k] <= max_dist_px:
                matches.append((j, i, float(d[j])))
    return matches


# ============================================================================
# 残差统计
# ============================================================================
def compute_residuals(
    detected_xy: np.ndarray,
    predicted_xy: np.ndarray,
    matches: List[Tuple[int, int, float]],
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """计算残差 (detector - predicted)

    Returns:
        (residuals [N,2], match_dists [N], total_dists [N])
    """
    if not matches:
        return np.zeros((0, 2)), np.zeros(0), np.zeros(0)
    residuals = np.zeros((len(matches), 2))
    match_dists = np.zeros(len(matches))
    for i, (det_idx, pred_idx, dist) in enumerate(matches):
        residuals[i, 0] = detected_xy[det_idx, 0] - predicted_xy[pred_idx, 0]
        residuals[i, 1] = detected_xy[det_idx, 1] - predicted_xy[pred_idx, 1]
        match_dists[i] = dist
    # 欧氏距离
    total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
    return residuals, match_dists, total_dists


def compute_stats(
    residuals: np.ndarray,
    total_dists: np.ndarray,
    image_width: int,
    image_height: int,
    detected_xy: np.ndarray,
) -> Dict[str, Any]:
    """计算 median/p90/p99, X/Y, 四象限, 边缘分布"""
    if len(residuals) == 0:
        return {
            "n": 0,
            "dist_median_px": 0.0, "dist_p90_px": 0.0, "dist_p99_px": 0.0,
            "dist_mean_px": 0.0, "dist_max_px": 0.0, "dist_std_px": 0.0,
            "x_median_abs_px": 0.0, "y_median_abs_px": 0.0,
            "x_mean_px": 0.0, "y_mean_px": 0.0,
            "x_std_px": 0.0, "y_std_px": 0.0,
            "quadrant_counts": {"Q1": 0, "Q2": 0, "Q3": 0, "Q4": 0},
            "edge_counts": {"top": 0, "bottom": 0, "left": 0, "right": 0, "center": 0},
        }

    abs_x = np.abs(residuals[:, 0])
    abs_y = np.abs(residuals[:, 1])

    stats: Dict[str, Any] = {
        "n": int(len(residuals)),
        "dist_median_px": float(np.median(total_dists)),
        "dist_p90_px": float(np.percentile(total_dists, 90)),
        "dist_p99_px": float(np.percentile(total_dists, 99)),
        "dist_mean_px": float(np.mean(total_dists)),
        "dist_max_px": float(np.max(total_dists)),
        "dist_std_px": float(np.std(total_dists)),
        "x_median_abs_px": float(np.median(abs_x)),
        "y_median_abs_px": float(np.median(abs_y)),
        "x_mean_px": float(np.mean(residuals[:, 0])),
        "y_mean_px": float(np.mean(residuals[:, 1])),
        "x_std_px": float(np.std(residuals[:, 0])),
        "y_std_px": float(np.std(residuals[:, 1])),
        "x_p90_abs_px": float(np.percentile(abs_x, 90)),
        "y_p90_abs_px": float(np.percentile(abs_y, 90)),
    }

    # 四象限分布 (基于检测星点在图像中的位置)
    cx = image_width / 2.0
    cy = image_height / 2.0
    quads = {"Q1": 0, "Q2": 0, "Q3": 0, "Q4": 0}  # Q1=右上, Q2=左上, Q3=左下, Q4=右下
    # 边缘分布 (按 1/3 划分)
    edge_w = image_width / 3.0
    edge_h = image_height / 3.0
    edges = {"top": 0, "bottom": 0, "left": 0, "right": 0, "center": 0}

    # 注意: 四象限统计对所有检测星点 (不只是匹配的), 用于评估检测分布均匀性
    for i in range(len(detected_xy)):
        x, y = detected_xy[i]
        if x >= cx and y < cy:
            quads["Q1"] += 1
        elif x < cx and y < cy:
            quads["Q2"] += 1
        elif x < cx and y >= cy:
            quads["Q3"] += 1
        else:
            quads["Q4"] += 1
        # 边缘: 左 1/3 / 中 1/3 / 右 1/3, 上 1/3 / 中 / 下 1/3
        if x < edge_w:
            edges["left"] += 1
        elif x >= 2 * edge_w:
            edges["right"] += 1
        if y < edge_h:
            edges["top"] += 1
        elif y >= 2 * edge_h:
            edges["bottom"] += 1
        if edge_w <= x < 2 * edge_w and edge_h <= y < 2 * edge_h:
            edges["center"] += 1

    stats["quadrant_counts"] = quads
    stats["edge_counts"] = edges
    return stats


# ============================================================================
# 双向闭环: pixel → sky → pixel
# ============================================================================
def pixel_sky_pixel_closure(wcs, xy: np.ndarray, n_samples: int = 100) -> Dict[str, Any]:
    """pixel → sky → pixel 闭环

    随机采样检测星点, 通过 astropy WCS 转 sky 再转回 pixel, 比较误差.
    """
    if len(xy) == 0:
        return {"n_samples": 0, "closure_err_median_px": 0.0}
    n = min(n_samples, len(xy))
    rng = np.random.default_rng(42)
    idx = rng.choice(len(xy), n, replace=False)
    samples = xy[idx].astype(np.float64)

    # pixel → sky
    sky = wcs.pixel_to_world(samples[:, 0], samples[:, 1])
    # sky → pixel
    back_x, back_y = wcs.world_to_pixel(sky)

    err_x = samples[:, 0] - back_x
    err_y = samples[:, 1] - back_y
    err = np.sqrt(err_x ** 2 + err_y ** 2)

    return {
        "n_samples": int(n),
        "closure_err_median_px": float(np.median(err)),
        "closure_err_p90_px": float(np.percentile(err, 90)),
        "closure_err_p99_px": float(np.percentile(err, 99)),
        "closure_err_max_px": float(np.max(err)),
        "x_err_median_px": float(np.median(np.abs(err_x))),
        "y_err_median_px": float(np.median(np.abs(err_y))),
    }


# ============================================================================
# 双向闭环: sky → pixel → sky
# ============================================================================
def sky_pixel_sky_closure(
    wcs,
    ra: np.ndarray,
    dec: np.ndarray,
    n_samples: int = 100,
) -> Dict[str, Any]:
    """sky → pixel → sky 闭环

    随机采样 Gaia 星, 通过 astropy WCS 转 pixel 再转回 sky, 比较误差.
    """
    import astropy.units as u
    from astropy.coordinates import SkyCoord

    if len(ra) == 0:
        return {"n_samples": 0, "closure_err_median_deg": 0.0}
    n = min(n_samples, len(ra))
    rng = np.random.default_rng(42)
    idx = rng.choice(len(ra), n, replace=False)
    ra_s = ra[idx].astype(np.float64)
    dec_s = dec[idx].astype(np.float64)

    sky_in = SkyCoord(ra_s * u.deg, dec_s * u.deg)
    # sky → pixel
    px, py = wcs.world_to_pixel(sky_in)
    # pixel → sky
    sky_out = wcs.pixel_to_world(px, py)

    err_ra = sky_in.ra.deg - sky_out.ra.deg
    err_dec = sky_in.dec.deg - sky_out.dec.deg
    err = np.sqrt(err_ra ** 2 + err_dec ** 2)

    return {
        "n_samples": int(n),
        "closure_err_median_deg": float(np.median(err)),
        "closure_err_p90_deg": float(np.percentile(err, 90)),
        "closure_err_p99_deg": float(np.percentile(err, 99)),
        "closure_err_max_deg": float(np.max(err)),
        "ra_err_median_deg": float(np.median(np.abs(err_ra))),
        "dec_err_median_deg": float(np.median(np.abs(err_dec))),
    }


# ============================================================================
# 图生成
# ============================================================================
def generate_residual_plot(
    residuals: np.ndarray,
    total_dists: np.ndarray,
    output_path: str,
    frame_id: str,
) -> None:
    """生成残差分布图 (4 子图)"""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    if len(residuals) == 0:
        fig, ax = plt.subplots(figsize=(8, 6))
        ax.text(0.5, 0.5, "No matches", ha="center", va="center", fontsize=20)
        ax.set_title(f"{frame_id} - No matches")
        plt.savefig(output_path, dpi=100, bbox_inches="tight")
        plt.close()
        return

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # 1. 残差散点图
    ax = axes[0, 0]
    sc = ax.scatter(residuals[:, 0], residuals[:, 1], c=total_dists, s=15, alpha=0.7, cmap="viridis")
    ax.axhline(0, color="r", linestyle="--", alpha=0.3)
    ax.axvline(0, color="r", linestyle="--", alpha=0.3)
    ax.set_xlabel("X residual (px)")
    ax.set_ylabel("Y residual (px)")
    ax.set_title("Residual scatter (detector - predicted)")
    ax.set_aspect("equal")
    plt.colorbar(sc, ax=ax, label="dist (px)")

    # 2. 残差距离直方图
    ax = axes[0, 1]
    ax.hist(total_dists, bins=30, alpha=0.7, color="steelblue", edgecolor="black")
    med = np.median(total_dists)
    p90 = np.percentile(total_dists, 90)
    p99 = np.percentile(total_dists, 99)
    ax.axvline(med, color="r", linestyle="--", label=f"median={med:.3f}")
    ax.axvline(p90, color="g", linestyle="--", label=f"p90={p90:.3f}")
    ax.axvline(p99, color="orange", linestyle="--", label=f"p99={p99:.3f}")
    ax.set_xlabel("Residual distance (px)")
    ax.set_ylabel("Count")
    ax.set_title("Residual distance histogram")
    ax.legend()

    # 3. X 残差直方图
    ax = axes[1, 0]
    ax.hist(residuals[:, 0], bins=30, alpha=0.7, color="steelblue", edgecolor="black")
    ax.axvline(np.median(residuals[:, 0]), color="r", linestyle="--",
               label=f"median={np.median(residuals[:, 0]):.3f}")
    ax.set_xlabel("X residual (px)")
    ax.set_ylabel("Count")
    ax.set_title("X residual histogram")
    ax.legend()

    # 4. Y 残差直方图
    ax = axes[1, 1]
    ax.hist(residuals[:, 1], bins=30, alpha=0.7, color="steelblue", edgecolor="black")
    ax.axvline(np.median(residuals[:, 1]), color="r", linestyle="--",
               label=f"median={np.median(residuals[:, 1]):.3f}")
    ax.set_xlabel("Y residual (px)")
    ax.set_ylabel("Count")
    ax.set_title("Y residual histogram")
    ax.legend()

    plt.suptitle(f"{frame_id} — WCS Closure Residuals (n={len(residuals)})", fontsize=14)
    plt.tight_layout()
    plt.savefig(output_path, dpi=100, bbox_inches="tight")
    plt.close()
    logger.info("残差图已保存: %s", output_path)


def generate_quadrant_plot(
    detected_xy: np.ndarray,
    matched_mask: np.ndarray,
    image_width: int,
    image_height: int,
    output_path: str,
    frame_id: str,
) -> None:
    """生成四象限+匹配分布图"""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(10, 10))
    cx = image_width / 2.0
    cy = image_height / 2.0

    # 全部检测星点 (灰)
    ax.scatter(detected_xy[:, 0], detected_xy[:, 1], s=5, c="lightgray", alpha=0.5, label="all detected")
    # 匹配的 (红)
    if np.any(matched_mask):
        ax.scatter(
            detected_xy[matched_mask, 0], detected_xy[matched_mask, 1],
            s=15, c="red", alpha=0.7, label="matched",
        )

    # 中心线 + 边缘线
    ax.axhline(cy, color="b", linestyle="--", alpha=0.3)
    ax.axvline(cx, color="b", linestyle="--", alpha=0.3)
    ax.axhline(image_height / 3.0, color="g", linestyle=":", alpha=0.2)
    ax.axhline(2 * image_height / 3.0, color="g", linestyle=":", alpha=0.2)
    ax.axvline(image_width / 3.0, color="g", linestyle=":", alpha=0.2)
    ax.axvline(2 * image_width / 3.0, color="g", linestyle=":", alpha=0.2)

    ax.set_xlim(0, image_width)
    ax.set_ylim(image_height, 0)  # Y 翻转以匹配图像显示
    ax.set_xlabel("X (px)")
    ax.set_ylabel("Y (px)")
    ax.set_title(f"{frame_id} — Detected & Matched star distribution")
    ax.legend(loc="upper right")
    ax.set_aspect("equal")
    plt.tight_layout()
    plt.savefig(output_path, dpi=100, bbox_inches="tight")
    plt.close()
    logger.info("四象限分布图已保存: %s", output_path)


# ============================================================================
# 主诊断流程
# ============================================================================
def diagnose_frame(
    fits_path: str,
    output_dir: str,
    project_root: str,
    env=None,
    max_match_dist_px: float = 3.0,
    gaia_mag_high: float = 18.0,
    gaia_search_radius_factor: float = 0.7,
    solve_if_no_wcs: bool = True,
) -> Dict[str, Any]:
    """诊断单帧 WCS 闭环

    Args:
        fits_path: FITS 文件路径 (含或不含 WCS header)
        output_dir: 输出目录
        project_root: 项目根目录 (用于加载 PlateSolve lib)
        env: 可选 PlateSolve 环境 (None 则自动初始化)
        max_match_dist_px: 最大匹配距离 (像素)
        gaia_mag_high: Gaia 星等上限
        gaia_search_radius_factor: Gaia 查询半径 = FOV_diag * factor
        solve_if_no_wcs: 若 FITS 无 WCS, 自动调用 PlateSolve 求解并写入

    Returns:
        closure_report dict
    """
    frame_id = Path(fits_path).stem
    logger.info("=" * 70)
    logger.info("诊断帧: %s", frame_id)
    logger.info("FITS: %s", fits_path)

    start_time = time.time()

    # 1. 读取 FITS
    pixels, header = read_fits_image_and_header(fits_path)
    height, width = pixels.shape

    # 2. 检查 WCS 是否存在, 若无则求解
    has_wcs = "CTYPE1" in header and "CRVAL1" in header and "CD1_1" in header
    solve_performed = False
    if not has_wcs and solve_if_no_wcs:
        logger.info("FITS 无 WCS header, 调用 PlateSolve 求解并写入 WCS...")
        # 初始化环境 (如未提供)
        own_env = env is None
        if own_env:
            env = init_platesolve_env(project_root)
        try:
            from solve_and_write_wcs import solve_and_write_wcs
            result = solve_and_write_wcs(
                fits_path, ra0=0, dec0=0, focal_length=0, pixel_size=0,
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
        # 重新读 header
        _, header = read_fits_image_and_header(fits_path)
        has_wcs = "CTYPE1" in header and "CRVAL1" in header and "CD1_1" in header
        if not has_wcs:
            raise RuntimeError("PlateSolve 求解后 FITS 仍无 WCS, 无法诊断")

    # 3. 构建 astropy WCS (独立于 PlateSolve transform)
    wcs, wcs_summary = build_astropy_wcs_from_header(header)
    wcs_summary["solve_performed_by_tool"] = bool(solve_performed)

    # 4. 初始化 PlateSolve 环境
    own_env = env is None
    if own_env:
        env = init_platesolve_env(project_root)
    gaia_client, sdet, solver = env

    try:
        # 4. 从 FITS header 读初始指向 (PlateSolve 需要)
        from solve_and_write_wcs import read_fits_header

        header_info = read_fits_header(fits_path)
        solve_ra0 = header_info["ra0"]
        solve_dec0 = header_info["dec0"]
        focal_length = header_info["focal_length"]
        pixel_size = header_info["pixel_size"]

        if focal_length <= 0 or pixel_size <= 0:
            raise RuntimeError(
                f"FITS header 缺失 FOCALLEN/XPIXSZ: fl={focal_length}, ps={pixel_size}"
            )

        # 5. Gaia 星表查询 (用 WCS CRVAL 作为中心)
        ra0 = wcs.wcs.crval[0]
        dec0 = wcs.wcs.crval[1]
        cd = wcs.wcs.cd
        fov_x_deg = abs(cd[0][0]) * width + abs(cd[0][1]) * height
        fov_y_deg = abs(cd[1][0]) * width + abs(cd[1][1]) * height
        fov_diag_deg = float(np.sqrt(fov_x_deg ** 2 + fov_y_deg ** 2))
        search_radius = max(fov_diag_deg * gaia_search_radius_factor, 0.5)

        gaia_ra, gaia_dec, gaia_mag = cone_search_gaia(
            gaia_client, ra0, dec0, search_radius, mag_high=gaia_mag_high,
        )

        # 6. 运行 PlateSolve (callback 拿检测星点)
        detections, wcs_result = solve_with_callback(
            solver, pixels, width, height,
            solve_ra0, solve_dec0, focal_length, pixel_size,
        )
        if len(detections) == 0:
            logger.warning("PlateSolve 未检测到星点")
            det_xy = np.zeros((0, 2))
        else:
            det_xy = detections[:, :2].astype(np.float64)

        # 7. astropy WCS.world_to_pixel 投影 Gaia (核心: 独立于 PlateSolve transform)
        pred_xy, valid_mask = project_gaia_to_pixel(wcs, gaia_ra, gaia_dec)
        gaia_ra_v = gaia_ra[valid_mask] if len(gaia_ra) > 0 else gaia_ra
        gaia_dec_v = gaia_dec[valid_mask] if len(gaia_dec) > 0 else gaia_dec
        gaia_mag_v = gaia_mag[valid_mask] if len(gaia_mag) > 0 else gaia_mag
        logger.info(
            "Gaia 过滤后: ra=%d, dec=%d, mag=%d, pred_xy=%d",
            len(gaia_ra_v), len(gaia_dec_v), len(gaia_mag_v), len(pred_xy),
        )

        # 8. kd-tree 匹配
        matches = match_pairs_kdtree(det_xy, pred_xy, max_match_dist_px)

        # 9. 残差计算
        residuals, match_dists, total_dists = compute_residuals(det_xy, pred_xy, matches)

        # 10. 统计
        stats = compute_stats(residuals, total_dists, width, height, det_xy)

        # 11. 双向闭环
        psp_closure = pixel_sky_pixel_closure(wcs, det_xy, n_samples=min(200, len(det_xy)))
        sps_closure = sky_pixel_sky_closure(
            wcs, gaia_ra_v, gaia_dec_v, n_samples=min(200, len(gaia_ra_v)),
        )

        # 12. 输出
        os.makedirs(output_dir, exist_ok=True)

        # 匹配对 JSON
        matched_pairs: List[Dict] = []
        for i, (det_idx, pred_idx, dist) in enumerate(matches):
            pair = {
                "pair_id": i,
                "detector_x": float(det_xy[det_idx, 0]),
                "detector_y": float(det_xy[det_idx, 1]),
                "detector_flux": float(detections[det_idx, 2]) if len(detections) > det_idx else 0.0,
                "detector_mag": float(detections[det_idx, 3]) if len(detections) > det_idx else 0.0,
                "gaia_ra": float(gaia_ra_v[pred_idx]),
                "gaia_dec": float(gaia_dec_v[pred_idx]),
                "gaia_mag": float(gaia_mag_v[pred_idx]) if len(gaia_mag_v) > pred_idx else 0.0,
                "predicted_x": float(pred_xy[pred_idx, 0]),
                "predicted_y": float(pred_xy[pred_idx, 1]),
                "residual_x_px": float(residuals[i, 0]),
                "residual_y_px": float(residuals[i, 1]),
                "residual_dist_px": float(dist),
            }
            matched_pairs.append(pair)

        pairs_path = os.path.join(output_dir, "matched_pairs.json")
        with open(pairs_path, "w", encoding="utf-8") as f:
            json.dump(matched_pairs, f, indent=2)
        logger.info("匹配对已保存: %s (n=%d)", pairs_path, len(matched_pairs))

        # 闭环报告
        elapsed = time.time() - start_time
        report: Dict[str, Any] = {
            "frame_id": frame_id,
            "fits_path": os.path.abspath(fits_path),
            "tool_version": "P11-002 v1.0",
            "tool_independent_of_platesolve_transform": True,
            "elapsed_sec": float(elapsed),
            "image_size": {"width": int(width), "height": int(height)},
            "wcs": wcs_summary,
            "platesolve": {
                "ra0": float(solve_ra0),
                "dec0": float(solve_dec0),
                "focal_length_mm": float(focal_length),
                "pixel_size_um": float(pixel_size),
                "success": bool(wcs_result.success),
                "n_detected": int(wcs_result.n_detected),
                "n_pairs": int(wcs_result.n_pairs),
                "rms_px": float(wcs_result.rms_px),
                "trans_order": int(wcs_result.trans_order),
                "best_inliers": int(wcs_result.best_inliers),
                "sip_order": int(wcs_result.sip_order),
            },
            "gaia": {
                "search_center_ra": float(ra0),
                "search_center_dec": float(dec0),
                "search_radius_deg": float(search_radius),
                "mag_high": float(gaia_mag_high),
                "n_catalog": int(len(gaia_ra)),
                "n_valid_predicted": int(len(pred_xy)),
            },
            "matching": {
                "method": "scipy.spatial.cKDTree bidirectional nearest-neighbor",
                "max_dist_px": float(max_match_dist_px),
                "n_detected": int(len(det_xy)),
                "n_matched": int(len(matches)),
            },
            "residual_stats": stats,
            "pixel_sky_pixel_closure": psp_closure,
            "sky_pixel_sky_closure": sps_closure,
            "fov_diag_deg": fov_diag_deg,
            "gate_check": {
                "median_le_0_75_px": stats["dist_median_px"] <= 0.75,
                "p90_le_1_5_px": stats["dist_p90_px"] <= 1.5,
                "p99_le_3_0_px": stats["dist_p99_px"] <= 3.0,
            },
        }
        report["gate_passed"] = bool(
            report["gate_check"]["median_le_0_75_px"]
            and report["gate_check"]["p90_le_1_5_px"]
            and report["gate_check"]["p99_le_3_0_px"]
        )

        report_path = os.path.join(output_dir, "closure_report.json")
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        logger.info("闭环报告已保存: %s", report_path)
        logger.info(
            "门限: median=%.3f px (<=0.75: %s), p90=%.3f px (<=1.5: %s), p99=%.3f px (<=3.0: %s)",
            stats["dist_median_px"], report["gate_check"]["median_le_0_75_px"],
            stats["dist_p90_px"], report["gate_check"]["p90_le_1_5_px"],
            stats["dist_p99_px"], report["gate_check"]["p99_le_3_0_px"],
        )

        # 残差图
        plot_path = os.path.join(output_dir, "residual_plot.png")
        generate_residual_plot(residuals, total_dists, plot_path, frame_id)

        # 四象限分布图
        matched_mask = np.zeros(len(det_xy), dtype=bool)
        for det_idx, _, _ in matches:
            matched_mask[det_idx] = True
        quad_plot_path = os.path.join(output_dir, "quadrant_plot.png")
        generate_quadrant_plot(det_xy, matched_mask, width, height, quad_plot_path, frame_id)

        logger.info("诊断完成: %s (耗时 %.2fs)", frame_id, elapsed)
        return report

    finally:
        if own_env:
            close_platesolve_env(env)


def diagnose_batch(
    frames_json: str,
    output_dir: str,
    project_root: str,
    max_match_dist_px: float = 3.0,
    gaia_mag_high: float = 18.0,
) -> Dict[str, Any]:
    """批量诊断多帧

    Args:
        frames_json: JSON 文件, 内容为 [{fits, output_subdir}, ...] 或 [fits_path, ...]
        output_dir: 输出根目录 (每帧会建子目录)
        project_root: 项目根目录
    """
    with open(frames_json, "r", encoding="utf-8") as f:
        frames = json.load(f)

    logger.info("批量诊断: %d 帧", len(frames))

    # 共享 PlateSolve 环境
    env = init_platesolve_env(project_root)
    summaries: List[Dict[str, Any]] = []

    try:
        for i, item in enumerate(frames):
            if isinstance(item, dict):
                fits_path = item["fits"]
                subdir = item.get("output_subdir", Path(fits_path).stem)
            else:
                fits_path = str(item)
                subdir = Path(fits_path).stem

            frame_out = os.path.join(output_dir, subdir)
            try:
                report = diagnose_frame(
                    fits_path, frame_out, project_root,
                    env=env, max_match_dist_px=max_match_dist_px,
                    gaia_mag_high=gaia_mag_high,
                )
                summaries.append({
                    "frame_id": report["frame_id"],
                    "fits_path": report["fits_path"],
                    "output_dir": frame_out,
                    "has_sip": report["wcs"]["has_sip"],
                    "sip_order": report["wcs"]["sip_order"],
                    "n_matched": report["matching"]["n_matched"],
                    "dist_median_px": report["residual_stats"]["dist_median_px"],
                    "dist_p90_px": report["residual_stats"]["dist_p90_px"],
                    "dist_p99_px": report["residual_stats"]["dist_p99_px"],
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

    # 汇总报告
    n_total = len(summaries)
    n_ok = sum(1 for s in summaries if s.get("gate_passed", False))
    n_err = sum(1 for s in summaries if "error" in s)

    summary = {
        "tool_version": "P11-002 v1.0",
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
    logger.info("批量汇总: %s (pass=%d/%d)", summary_path, n_ok, n_total)
    return summary


# ============================================================================
# CLI 入口
# ============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description="P11-002 标准 WCS 真实星对闭环诊断工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--fits", help="校准后 FITS 文件路径 (单帧模式)")
    parser.add_argument("--batch", help="批量模式: JSON 文件, 含帧列表")
    parser.add_argument("--output-dir", required=True, help="输出目录")
    parser.add_argument("--project-root", default=".", help="项目根目录")
    parser.add_argument("--max-match-dist-px", type=float, default=3.0,
                        help="最大匹配距离 (像素), 默认 3.0")
    parser.add_argument("--gaia-mag-high", type=float, default=18.0,
                        help="Gaia 星等上限, 默认 18.0")
    parser.add_argument("--log", help="日志文件路径 (可选)")
    args = parser.parse_args()

    if args.log:
        fh = logging.FileHandler(args.log, encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
        logging.getLogger().addHandler(fh)

    if args.batch:
        diagnose_batch(
            args.batch, args.output_dir, args.project_root,
            max_match_dist_px=args.max_match_dist_px,
            gaia_mag_high=args.gaia_mag_high,
        )
    elif args.fits:
        diagnose_frame(
            args.fits, args.output_dir, args.project_root,
            env=None,
            max_match_dist_px=args.max_match_dist_px,
            gaia_mag_high=args.gaia_mag_high,
        )
    else:
        parser.error("必须指定 --fits 或 --batch")
    return 0


if __name__ == "__main__":
    sys.exit(main())
