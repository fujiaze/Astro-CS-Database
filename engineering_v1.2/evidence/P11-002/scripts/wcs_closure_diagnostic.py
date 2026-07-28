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
# Siril 1.4.3 经验常量 (src/registration/matching/atpmatch.h, src/algos/astrometry_solver.h)
# ============================================================================
# 用于规避"全星等一次性 kd-tree 匹配导致的系统性残差":
#   1. 自适应星等上限 (非固定 18.0)
#   2. 亮星优先匹配 (最亮 N 颗做精匹配)
#   3. 分位数鲁棒剔除 (35 分位做 sigma, 68.3 分位做上报)
SIRIL_AT_MATCH_NBRIGHT = 20             # 三角形粗匹配用最亮星数
SIRIL_AT_MATCH_CATALOG_NBRIGHT = 60     # 星表精匹配用最亮星数 (我们用作 kd-tree 匹配上限)
SIRIL_BRIGHTEST_STARS = 2000            # 视场期望总星数 (用于自适应 mag_high)
SIRIL_AT_MATCH_PERCENTILE = 0.35        # 剔除用分位 (Siril iter_trans)
SIRIL_ONE_STDEV_PERCENTILE = 0.683      # 上报用分位 (1σ, Siril iter_trans 末尾)
SIRIL_AT_MATCH_NSIGMA = 10.0            # 软剔除倍数 (10×35分位)
SIRIL_AT_MATCH_MAXDIST_PX = 50.0        # 硬剔除阈值 (px)
SIRIL_AT_MATCH_MAXITER = 5              # 残差剔除最大迭代


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
# 自适应星等上限 (参考 Siril 1.4.3 src/algos/astrometry_solver.c:174-197)
# ============================================================================
def compute_mag_limit_siril(
    ra0_deg: float,
    dec0_deg: float,
    fov_deg: float,
    n_brightest: int = SIRIL_BRIGHTEST_STARS,
) -> float:
    """按 Siril 1.4.3 自适应计算 Gaia 星等上限

    依据: src/algos/astrometry_solver.c:174-197 compute_mag_limit_from_position_and_fov()

    算法:
        1. 将 (ra, dec) 转换为银道坐标 (l, b)
        2. 计算视场球面积 S = 2*(1-cos(fov/2)) * (180/π)² (平方度)
        3. 模型: limit = m0 + s * (log10(N/S) - 2)
           - m0 = 11.68 + 2.66 * sin(|b|)
           - a = 2.36 + (|l|-90) * 0.0073 * (|l|>=90 ? 1 : -1)
           - b_slope = 0.88 - (|l|-90) * 0.0065 * (|l|>=90 ? 1 : -1)
           - s = a + b_slope * sin(|b|)
        4. clamp(limit, 7.0, +inf)

    参数:
        ra0_deg, dec0_deg: 视场中心 (度, ICRS)
        fov_deg: 视场直径 (度)
        n_brightest: 期望视场内最亮 N 颗星 (Siril 默认 2000)

    返回:
        mag_high (float): 自适应星等上限
    """
    import math

    # 1. ICRS -> 银道坐标 (近似, 用 astropy 若可用, 否则用简化公式)
    try:
        from astropy.coordinates import SkyCoord
        import astropy.units as u
        c = SkyCoord(ra=ra0_deg * u.deg, dec=dec0_deg * u.deg, frame="icrs")
        g = c.galactic
        ml = float(g.l.deg)
        mb = float(g.b.deg)
    except Exception:
        # 简化近似 (精度足够用于星等上限估算)
        # 参考: https://en.wikipedia.org/wiki/Galactic_coordinate_system
        dec_rad = math.radians(dec0_deg)
        ra_rad = math.radians(ra0_deg)
        # 北银极在赤道坐标 (192.85948°, 27.12825°)
        ra_ngp = math.radians(192.85948)
        dec_ngp = math.radians(27.12825)
        sin_b = (math.sin(dec_rad) * math.sin(dec_ngp) +
                 math.cos(dec_rad) * math.cos(dec_ngp) * math.cos(ra_rad - ra_ngp))
        mb = math.degrees(math.asin(max(-1.0, min(1.0, sin_b))))
        # l 简化 (略, 用 0 兜底)
        ml = 0.0

    mb_abs = abs(mb)
    ml_abs = abs(ml)

    # 2. 视场球面积 (平方度)
    fov_rad = math.radians(fov_deg)
    S = 2.0 * (1.0 - math.cos(0.5 * fov_rad)) * (180.0 / math.pi) ** 2

    # 3. 星等模型 (Siril astrometry_solver.c:188-195)
    m0 = 11.68 + 2.66 * math.sin(math.radians(mb_abs))
    sign_l = 1.0 if ml_abs >= 90.0 else -1.0
    a = 2.36 + (ml_abs - 90.0) * 0.0073 * sign_l
    b_slope = 0.88 - (ml_abs - 90.0) * 0.0065 * sign_l
    s = a + b_slope * math.sin(math.radians(mb_abs))

    log_term = math.log10(float(n_brightest) / max(S, 1e-6)) - 2.0
    limit = m0 + s * log_term

    # 4. 下限保护 (Siril: max(limit, 7.0))
    limit = max(limit, 7.0)
    logger.info(
        "Siril 自适应星等上限: (l=%.2f°, b=%.2f°, fov=%.2f°, N=%d) -> mag_high=%.3f",
        ml, mb, fov_deg, n_brightest, limit,
    )
    return limit


# ============================================================================
# 亮星优先匹配 (参考 Siril atpmatch.c:1636 sort_star_by_mag + AT_MATCH_CATALOG_NBRIGHT)
# ============================================================================
def match_pairs_bright_first(
    detected_xy: np.ndarray,
    detected_mag: np.ndarray,
    predicted_xy: np.ndarray,
    gaia_mag: np.ndarray,
    max_dist_px: float = 3.0,
    n_brightest: int = SIRIL_AT_MATCH_CATALOG_NBRIGHT,
) -> List[Tuple[int, int, float]]:
    """亮星优先 kd-tree 匹配

    Siril 策略: 先按星等升序排序, 仅用最亮 N 颗做匹配.
    这能规避密集星场中暗星误配导致的系统性残差.

    参数:
        detected_xy: 检测星点 (x, y) [N_det, 2]
        detected_mag: 检测星点星等 [N_det] (来自 star_detector)
        predicted_xy: Gaia 投影像素坐标 [N_pred, 2]
        gaia_mag: Gaia 星等 [N_pred]
        max_dist_px: 最大匹配距离
        n_brightest: 仅用最亮 N 颗 Gaia 星做匹配 (Siril 默认 60)

    返回:
        list of (det_idx, pred_idx, dist_px)
    """
    if len(detected_xy) == 0 or len(predicted_xy) == 0:
        return []

    # 按 Gaia 星等升序 (亮星在前), 取前 n_brightest
    n_use = min(n_brightest, len(predicted_xy))
    bright_order = np.argsort(gaia_mag)[:n_use]
    pred_xy_bright = predicted_xy[bright_order]
    logger.info(
        "亮星优先匹配: 取最亮 %d/%d 颗 Gaia 星 (mag %.2f~%.2f)",
        n_use, len(predicted_xy),
        float(gaia_mag[bright_order[0]]), float(gaia_mag[bright_order[-1]]),
    )

    # 对亮星子集做 kd-tree 匹配
    bright_matches = match_pairs_kdtree(detected_xy, pred_xy_bright, max_dist_px)

    # 将 pred_idx 从 "亮星子集索引" 映射回 "原始 Gaia 索引"
    matches = []
    for det_idx, sub_idx, dist in bright_matches:
        orig_pred_idx = int(bright_order[sub_idx])
        matches.append((det_idx, orig_pred_idx, dist))

    logger.info("亮星优先匹配完成: %d 对", len(matches))
    return matches


# ============================================================================
# Siril 风格迭代残差剔除 (参考 atpmatch.c:2760-3184 iter_trans)
# ============================================================================
def iterative_outlier_rejection_siril(
    residuals: np.ndarray,
    total_dists: np.ndarray,
    matches: List[Tuple[int, int, float]],
    max_iter: int = SIRIL_AT_MATCH_MAXITER,
    hard_max_dist_px: float = SIRIL_AT_MATCH_MAXDIST_PX,
    percentile: float = SIRIL_AT_MATCH_PERCENTILE,
    nsigma: float = SIRIL_AT_MATCH_NSIGMA,
) -> Tuple[np.ndarray, Dict[str, Any]]:
    """Siril 风格迭代残差剔除

    算法 (atpmatch.c:2955-3184 iter_trans):
        1. 硬剔除: dist > hard_max_dist_px 直接丢
        2. 软剔除 sigma = 35 分位 (sort(dist²))
        3. 软剔除: dist > nsigma * sqrt(sigma) 丢弃  (Siril 用 dist², 这里等价)
        4. 迭代 max_iter 次

    返回:
        (keep_mask [N] bool, info dict)
    """
    n = len(matches)
    if n == 0:
        return np.zeros(0, dtype=bool), {"n_iter": 0, "final_sigma_px": 0.0, "n_kept": 0}

    keep = np.ones(n, dtype=bool)
    info = {"n_iter": 0, "final_sigma_px": 0.0, "n_kept": n, "rejected_per_iter": []}

    for it in range(max_iter):
        d_curr = total_dists[keep]
        if len(d_curr) == 0:
            break

        # 1. 硬剔除
        hard_mask = d_curr <= hard_max_dist_px
        n_hard_reject = int(len(d_curr) - np.sum(hard_mask))
        # 同步到 keep
        keep_idx = np.where(keep)[0]
        keep[keep_idx[~hard_mask]] = False

        d_after_hard = total_dists[keep]
        if len(d_after_hard) == 0:
            break

        # 2. 35 分位 sigma (Siril: dist2_sorted, 35 分位)
        sigma = float(np.percentile(d_after_hard, percentile * 100.0))
        if sigma <= 0:
            sigma = float(np.median(d_after_hard)) or 1e-6

        # 3. 软剔除: dist > nsigma * sigma
        soft_thresh = nsigma * sigma
        soft_mask = d_after_hard <= soft_thresh
        n_soft_reject = int(len(d_after_hard) - np.sum(soft_mask))
        keep_idx2 = np.where(keep)[0]
        keep[keep_idx2[~soft_mask]] = False

        info["rejected_per_iter"].append({
            "iter": it + 1,
            "n_before": int(np.sum(keep) + n_hard_reject + n_soft_reject),
            "n_hard_reject": n_hard_reject,
            "n_soft_reject": n_soft_reject,
            "sigma35_px": sigma,
            "soft_thresh_px": soft_thresh,
            "n_kept": int(np.sum(keep)),
        })

        # 4. 收敛检查: 剔除数为 0 即停
        if n_hard_reject == 0 and n_soft_reject == 0:
            break

    # 最终 sigma = 68.3 分位 (Siril ONE_STDEV_PERCENTILE)
    final_d = total_dists[keep]
    final_sigma = float(np.percentile(final_d, SIRIL_ONE_STDEV_PERCENTILE * 100.0)) if len(final_d) > 0 else 0.0
    info["n_iter"] = len(info["rejected_per_iter"])
    info["final_sigma_px"] = final_sigma
    info["n_kept"] = int(np.sum(keep))
    info["final_p35_px"] = float(np.percentile(final_d, SIRIL_AT_MATCH_PERCENTILE * 100.0)) if len(final_d) > 0 else 0.0
    info["final_p68_px"] = final_sigma
    logger.info(
        "Siril 迭代剔除完成: %d 次迭代, 保留 %d/%d, final p68=%.4f px",
        info["n_iter"], info["n_kept"], n, final_sigma,
    )
    return keep, info


# ============================================================================
# 按星等分 bin 残差统计 (诊断增强)
# ============================================================================
def compute_residuals_by_mag_bin(
    residuals: np.ndarray,
    total_dists: np.ndarray,
    gaia_mag: np.ndarray,
    keep_mask: np.ndarray,
    bin_width: float = 1.0,
    mag_low: float = 6.0,
    mag_high: float = 18.0,
) -> List[Dict[str, Any]]:
    """按 Gaia 星等分 bin 报告残差

    参数:
        residuals: [N, 2] 残差
        total_dists: [N] 残差距离
        gaia_mag: [N] Gaia 星等
        keep_mask: [N] bool, Siril 剔除后保留的
        bin_width: bin 宽度 (默认 1 mag)
        mag_low, mag_high: bin 范围

    返回:
        list of {mag_bin, n, median_px, p68_px, p90_px, x_mean, y_mean, kept}
    """
    bins = np.arange(mag_low, mag_high + bin_width, bin_width)
    result = []
    for i in range(len(bins) - 1):
        lo, hi = bins[i], bins[i + 1]
        in_bin = (gaia_mag >= lo) & (gaia_mag < hi) & keep_mask
        n = int(np.sum(in_bin))
        if n == 0:
            result.append({
                "mag_bin": f"{lo:.0f}-{hi:.0f}",
                "n": 0, "median_px": 0.0, "p68_px": 0.0,
                "p90_px": 0.0, "x_mean_px": 0.0, "y_mean_px": 0.0,
            })
            continue
        d_bin = total_dists[in_bin]
        r_bin = residuals[in_bin]
        result.append({
            "mag_bin": f"{lo:.0f}-{hi:.0f}",
            "n": n,
            "median_px": float(np.median(d_bin)),
            "p68_px": float(np.percentile(d_bin, SIRIL_ONE_STDEV_PERCENTILE * 100.0)),
            "p90_px": float(np.percentile(d_bin, 90.0)),
            "x_mean_px": float(np.mean(r_bin[:, 0])),
            "y_mean_px": float(np.mean(r_bin[:, 1])),
        })
    return result


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
    gaia_mag_high: float = 0.0,
    gaia_search_radius_factor: float = 0.7,
    solve_if_no_wcs: bool = True,
    siril_bright_first: bool = True,
    siril_n_brightest: int = SIRIL_AT_MATCH_CATALOG_NBRIGHT,
    siril_outlier_rejection: bool = True,
) -> Dict[str, Any]:
    """诊断单帧 WCS 闭环 (P11-004 Siril 1.4.3 策略升级版)

    相比原版 (固定 mag_high=18.0 + 全星等 kd-tree 匹配) 的改进:
        1. 自适应星等上限 (参考 Siril compute_mag_limit_from_position_and_fov)
        2. 亮星优先匹配 (参考 Siril sort_star_by_mag + AT_MATCH_CATALOG_NBRIGHT=60)
        3. Siril 风格迭代残差剔除 (35 分位 sigma + 10×sigma 软剔除, 5 次迭代)
        4. 残差按星等分 bin 报告 (诊断增强)
        5. gate 阈值改用 68.3 分位 (Siril ONE_STDEV_PERCENTILE) 替代 median

    Args:
        fits_path: FITS 文件路径 (含或不含 WCS header)
        output_dir: 输出目录
        project_root: 项目根目录 (用于加载 PlateSolve lib)
        env: 可选 PlateSolve 环境 (None 则自动初始化)
        max_match_dist_px: 最大匹配距离 (像素)
        gaia_mag_high: Gaia 星等上限; 0=自适应 (Siril 公式), >0=固定值
        gaia_search_radius_factor: Gaia 查询半径 = FOV_diag * factor
        solve_if_no_wcs: 若 FITS 无 WCS, 自动调用 PlateSolve 求解并写入
        siril_bright_first: True=仅用最亮 N 颗 Gaia 星做匹配 (Siril 策略)
        siril_n_brightest: 亮星优先匹配的 N (Siril 默认 60)
        siril_outlier_rejection: True=启用 Siril 风格迭代剔除

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

        # P11-004: 自适应星等上限 (Siril compute_mag_limit_from_position_and_fov)
        # gaia_mag_high=0 触发自适应; >0 使用固定值 (兼容旧调用)
        mag_high_effective = gaia_mag_high
        mag_high_source = "fixed"
        if gaia_mag_high <= 0:
            mag_high_effective = compute_mag_limit_siril(
                ra0, dec0, fov_diag_deg, n_brightest=SIRIL_BRIGHTEST_STARS,
            )
            mag_high_source = "siril_adaptive"

        gaia_ra, gaia_dec, gaia_mag = cone_search_gaia(
            gaia_client, ra0, dec0, search_radius, mag_high=mag_high_effective,
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

        # 8. 匹配 (P11-004: 亮星优先, 参考 Siril sort_star_by_mag)
        if siril_bright_first:
            matches = match_pairs_bright_first(
                det_xy, detections[:, 3] if len(detections) > 0 else np.zeros(0),
                pred_xy, gaia_mag_v,
                max_dist_px=max_match_dist_px,
                n_brightest=siril_n_brightest,
            )
            match_method = f"siril_bright_first(n={siril_n_brightest})"
        else:
            matches = match_pairs_kdtree(det_xy, pred_xy, max_match_dist_px)
            match_method = "scipy.spatial.cKDTree bidirectional nearest-neighbor (all magnitudes)"

        # 9. 残差计算
        residuals, match_dists, total_dists = compute_residuals(det_xy, pred_xy, matches)

        # 9.5 P11-004: Siril 风格迭代残差剔除 (atpmatch.c iter_trans)
        siril_rejection_info: Dict[str, Any] = {"enabled": bool(siril_outlier_rejection)}
        keep_mask = np.ones(len(matches), dtype=bool)
        if siril_outlier_rejection and len(matches) > 0:
            keep_mask, siril_rejection_info_inner = iterative_outlier_rejection_siril(
                residuals, total_dists, matches,
                max_iter=SIRIL_AT_MATCH_MAXITER,
                hard_max_dist_px=SIRIL_AT_MATCH_MAXDIST_PX,
                percentile=SIRIL_AT_MATCH_PERCENTILE,
                nsigma=SIRIL_AT_MATCH_NSIGMA,
            )
            siril_rejection_info = siril_rejection_info_inner
            siril_rejection_info["enabled"] = True
            n_kept = int(np.sum(keep_mask))
            logger.info(
                "Siril 剔除后保留: %d/%d 对 (剔除 %d)",
                n_kept, len(matches), len(matches) - n_kept,
            )
            # 过滤 matches/residuals/dists 到保留子集
            matches = [m for i, m in enumerate(matches) if keep_mask[i]]
            residuals = residuals[keep_mask]
            total_dists = total_dists[keep_mask]
            match_dists = match_dists[keep_mask]
            # keep_mask 也需相应"重置"为全 True (后续代码用 len(matches) 索引)
            keep_mask_after = np.ones(len(matches), dtype=bool)
        else:
            keep_mask_after = keep_mask

        # 10. 统计 (原版 median/p90/p99)
        stats = compute_stats(residuals, total_dists, width, height, det_xy)

        # 10.5 P11-004: Siril 分位数统计 + 按星等分 bin (诊断增强)
        siril_stats: Dict[str, Any] = {}
        mag_bin_stats: List[Dict[str, Any]] = []
        if len(matches) > 0:
            # 重新构造 gaia_mag_matched (按 matches 顺序, 已被 Siril 剔除过滤)
            gaia_mag_matched = np.array([
                float(gaia_mag_v[m[1]]) if len(gaia_mag_v) > m[1] else 0.0
                for m in matches
            ], dtype=np.float64)
            siril_stats = {
                "p35_px": float(np.percentile(total_dists, SIRIL_AT_MATCH_PERCENTILE * 100.0)),
                "p68_px": float(np.percentile(total_dists, SIRIL_ONE_STDEV_PERCENTILE * 100.0)),
                "n_kept": int(len(matches)),
                "rejection_info": siril_rejection_info,
            }
            mag_bin_stats = compute_residuals_by_mag_bin(
                residuals, total_dists, gaia_mag_matched, keep_mask_after,
                bin_width=1.0, mag_low=6.0, mag_high=mag_high_effective,
            )
        else:
            siril_stats = {
                "p35_px": 0.0, "p68_px": 0.0, "n_kept": 0,
                "rejection_info": siril_rejection_info,
            }

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
            "tool_version": "P11-002 v2.0 (P11-004 Siril 1.4.3 升级)",
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
                "mag_high_requested": float(gaia_mag_high),
                "mag_high_effective": float(mag_high_effective),
                "mag_high_source": mag_high_source,
                "n_catalog": int(len(gaia_ra)),
                "n_valid_predicted": int(len(pred_xy)),
            },
            "matching": {
                "method": match_method,
                "max_dist_px": float(max_match_dist_px),
                "n_detected": int(len(det_xy)),
                "n_matched": int(len(matches)),
                "siril_bright_first": bool(siril_bright_first),
                "siril_n_brightest": int(siril_n_brightest) if siril_bright_first else 0,
            },
            "residual_stats": stats,
            "siril_stats": siril_stats,
            "residuals_by_mag_bin": mag_bin_stats,
            "pixel_sky_pixel_closure": psp_closure,
            "sky_pixel_sky_closure": sps_closure,
            "fov_diag_deg": fov_diag_deg,
            # P11-004: gate 改用 Siril 68.3 分位 (1σ) 替代 median, 更鲁棒
            # 原版 gate (median≤0.75, p90≤1.5, p99≤3.0) 仍保留为 legacy_gate_check
            "legacy_gate_check": {
                "median_le_0_75_px": stats["dist_median_px"] <= 0.75,
                "p90_le_1_5_px": stats["dist_p90_px"] <= 1.5,
                "p99_le_3_0_px": stats["dist_p99_px"] <= 3.0,
            },
            "gate_check": {
                # 新版 gate (P11-004): 用 Siril 1σ 分位 + 亮星优先 + 剔除后
                "p68_le_0_75_px": siril_stats.get("p68_px", 0.0) <= 0.75,
                "p90_le_1_5_px": stats["dist_p90_px"] <= 1.5,
                "p99_le_3_0_px": stats["dist_p99_px"] <= 3.0,
                "n_matched_ge_5": int(len(matches)) >= 5,
            },
        }
        report["gate_passed"] = bool(
            report["gate_check"]["p68_le_0_75_px"]
            and report["gate_check"]["p90_le_1_5_px"]
            and report["gate_check"]["p99_le_3_0_px"]
            and report["gate_check"]["n_matched_ge_5"]
        )
        report["legacy_gate_passed"] = bool(
            report["legacy_gate_check"]["median_le_0_75_px"]
            and report["legacy_gate_check"]["p90_le_1_5_px"]
            and report["legacy_gate_check"]["p99_le_3_0_px"]
        )

        report_path = os.path.join(output_dir, "closure_report.json")
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        logger.info("闭环报告已保存: %s", report_path)
        logger.info(
            "[新版 gate] p68=%.3f px (<=0.75: %s), p90=%.3f px (<=1.5: %s), p99=%.3f px (<=3.0: %s), n=%d (>=5: %s) => %s",
            siril_stats.get("p68_px", 0.0), report["gate_check"]["p68_le_0_75_px"],
            stats["dist_p90_px"], report["gate_check"]["p90_le_1_5_px"],
            stats["dist_p99_px"], report["gate_check"]["p99_le_3_0_px"],
            len(matches), report["gate_check"]["n_matched_ge_5"],
            "PASS" if report["gate_passed"] else "FAIL",
        )
        logger.info(
            "[Legacy gate] median=%.3f px (<=0.75: %s) => %s",
            stats["dist_median_px"], report["legacy_gate_check"]["median_le_0_75_px"],
            "PASS" if report["legacy_gate_passed"] else "FAIL",
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
    gaia_mag_high: float = 0.0,
    siril_bright_first: bool = True,
    siril_n_brightest: int = SIRIL_AT_MATCH_CATALOG_NBRIGHT,
    siril_outlier_rejection: bool = True,
) -> Dict[str, Any]:
    """批量诊断多帧

    Args:
        frames_json: JSON 文件, 内容为 [{fits, output_subdir}, ...] 或 [fits_path, ...]
        output_dir: 输出根目录 (每帧会建子目录)
        project_root: 项目根目录
        gaia_mag_high: 0=自适应 (Siril 公式), >0=固定值
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
                    siril_bright_first=siril_bright_first,
                    siril_n_brightest=siril_n_brightest,
                    siril_outlier_rejection=siril_outlier_rejection,
                )
                summaries.append({
                    "frame_id": report["frame_id"],
                    "fits_path": report["fits_path"],
                    "output_dir": frame_out,
                    "has_sip": report["wcs"]["has_sip"],
                    "sip_order": report["wcs"]["sip_order"],
                    "n_matched": report["matching"]["n_matched"],
                    "mag_high_effective": report["gaia"]["mag_high_effective"],
                    "mag_high_source": report["gaia"]["mag_high_source"],
                    "dist_median_px": report["residual_stats"]["dist_median_px"],
                    "dist_p90_px": report["residual_stats"]["dist_p90_px"],
                    "dist_p99_px": report["residual_stats"]["dist_p99_px"],
                    "siril_p68_px": report["siril_stats"].get("p68_px", 0.0),
                    "gate_passed": report["gate_passed"],
                    "legacy_gate_passed": report.get("legacy_gate_passed", False),
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
    n_legacy_ok = sum(1 for s in summaries if s.get("legacy_gate_passed", False))
    n_err = sum(1 for s in summaries if "error" in s)

    summary = {
        "tool_version": "P11-002 v2.0 (P11-004 Siril 1.4.3 升级)",
        "n_total": n_total,
        "n_passed": n_ok,
        "n_legacy_passed": n_legacy_ok,
        "n_failed": n_total - n_ok - n_err,
        "n_errors": n_err,
        "pass_rate": float(n_ok / n_total) if n_total > 0 else 0.0,
        "legacy_pass_rate": float(n_legacy_ok / n_total) if n_total > 0 else 0.0,
        "frames": summaries,
    }
    summary_path = os.path.join(output_dir, "batch_summary.json")
    os.makedirs(output_dir, exist_ok=True)
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    logger.info("批量汇总: %s (新 gate pass=%d/%d, legacy gate pass=%d/%d)",
                summary_path, n_ok, n_total, n_legacy_ok, n_total)
    return summary


# ============================================================================
# CLI 入口
# ============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description="P11-002 标准 WCS 真实星对闭环诊断工具 (P11-004 Siril 1.4.3 升级版)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--fits", help="校准后 FITS 文件路径 (单帧模式)")
    parser.add_argument("--batch", help="批量模式: JSON 文件, 含帧列表")
    parser.add_argument("--output-dir", required=True, help="输出目录")
    parser.add_argument("--project-root", default=".", help="项目根目录")
    parser.add_argument("--max-match-dist-px", type=float, default=3.0,
                        help="最大匹配距离 (像素), 默认 3.0")
    parser.add_argument("--gaia-mag-high", type=float, default=0.0,
                        help="Gaia 星等上限; 0=自适应 (Siril 公式, 默认), >0=固定值")
    parser.add_argument("--no-siril-bright-first", action="store_true",
                        help="禁用亮星优先匹配 (默认启用, Siril AT_MATCH_CATALOG_NBRIGHT=60)")
    parser.add_argument("--siril-n-brightest", type=int, default=SIRIL_AT_MATCH_CATALOG_NBRIGHT,
                        help=f"亮星优先匹配的 N (默认 {SIRIL_AT_MATCH_CATALOG_NBRIGHT})")
    parser.add_argument("--no-siril-outlier-rejection", action="store_true",
                        help="禁用 Siril 风格迭代残差剔除 (默认启用)")
    parser.add_argument("--log", help="日志文件路径 (可选)")
    args = parser.parse_args()

    if args.log:
        fh = logging.FileHandler(args.log, encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
        logging.getLogger().addHandler(fh)

    siril_bright_first = not args.no_siril_bright_first
    siril_outlier_rejection = not args.no_siril_outlier_rejection

    if args.batch:
        diagnose_batch(
            args.batch, args.output_dir, args.project_root,
            max_match_dist_px=args.max_match_dist_px,
            gaia_mag_high=args.gaia_mag_high,
            siril_bright_first=siril_bright_first,
            siril_n_brightest=args.siril_n_brightest,
            siril_outlier_rejection=siril_outlier_rejection,
        )
    elif args.fits:
        diagnose_frame(
            args.fits, args.output_dir, args.project_root,
            env=None,
            max_match_dist_px=args.max_match_dist_px,
            gaia_mag_high=args.gaia_mag_high,
            siril_bright_first=siril_bright_first,
            siril_n_brightest=args.siril_n_brightest,
            siril_outlier_rejection=siril_outlier_rejection,
        )
    else:
        parser.error("必须指定 --fits 或 --batch")
    return 0


if __name__ == "__main__":
    sys.exit(main())
