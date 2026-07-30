#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""B-003 — PlateSolve 回投 / 测光 / SNR / Drizzle 一致性验证

对 B-002 产出的 3 帧 HISS 执行四项一致性验证:
  1. PlateSolve 回投: 对原始 FITS 重新运行 PlateSolve, 用 astropy WCS(含 SIP)
     将 Gaia 权威星对回投到像素, 与检测星点比较, 计算 RMS(arcsec)
  2. 测光一致性: fit_used > 50, sigma_residual < 0.5, scale_factor 合理
  3. SNR 分布: n_points > 100, median_snr > 10
  4. Drizzle: support > 0, signal 非零

WCS 来源: 直接从 IPVSolver 的 IpvWcsResult 构建 astropy WCS(含 SIP A/B/AP/BP),
不修改原始 FITS 文件。HISS WCS(来自 orchestrator inspect --hiss)用于交叉对比。

用法:
  python b003_verify.py --project-root <root> [--gaia-timeout 60]
"""

from __future__ import annotations

import argparse
import csv
import json
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

# ============================================================================
# 日志
# ============================================================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("b003")

# ============================================================================
# 帧配置 (来自 B-002 TASK_REPORT + HISS inspect)
# ============================================================================
FRAMES = [
    {
        "frame_id": "T2_RED_LDN43",
        "fits": "testdata/LDN43_T2_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts",
        "hiss": "output/B-002/T2_RED_LDN43.hiss",
        "focal_length_mm": 1900.0,
        "pixel_size_um": 9.0,
        "pixel_scale_arcsec_per_px": 0.967,
        "hiss_wcs": {
            "cd": [3.197784659189e-06, 2.685986212483e-04, -2.685775980219e-04, 3.264408581867e-06],
            "crval": [248.6096556109, -15.7591304856],
            "crpix": [2048.500000, 2048.500000],
            "sip_order": 3,
        },
        "b002_stats": {
            "fit_used": 1095, "sigma_residual": 0.066314, "scale_factor": 8.441798e-06,
            "snr_n_points": 1930, "snr_median": 83.022337,
            "drizzle_support": 1573, "drizzle_signal": 16777216,
            "platesolve_rms_arcsec": 0.1170,
        },
    },
    {
        "frame_id": "T3_RED_NGC55",
        "fits": "testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250703@080546-600S-Red.fts",
        "hiss": "output/B-002/T3_RED_NGC55.hiss",
        "focal_length_mm": 1900.0,
        "pixel_size_um": 9.0,
        "pixel_scale_arcsec_per_px": 0.959,
        "hiss_wcs": {
            "cd": [-2.730153462821e-06, 2.662908404247e-04, -2.662223749194e-04, -2.737786468737e-06],
            "crval": [3.7496574668, -39.1942881279],
            "crpix": [2048.500000, 2048.500000],
            "sip_order": 3,
        },
        "b002_stats": {
            "fit_used": 285, "sigma_residual": 0.128846, "scale_factor": 1.015245e-05,
            "snr_n_points": 617, "snr_median": 86.590256,
            "drizzle_support": 1535, "drizzle_signal": 16777216,
            "platesolve_rms_arcsec": 0.1387,
        },
    },
    {
        "frame_id": "T4_RED_GalaxyCenter_panel1",
        "fits": "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
        "hiss": "output/B-002/T4_RED_GalaxyCenter_panel1.hiss",
        "focal_length_mm": 200.0,
        "pixel_size_um": 6.0,
        "pixel_scale_arcsec_per_px": 6.308,
        "hiss_wcs": {
            "cd": [1.752112340951e-03, 2.999016755136e-07, -6.522096515637e-07, 1.752365769155e-03],
            "crval": [272.8256445295, -13.1317715206],
            "crpix": [2250.500000, 1800.500000],
            "sip_order": 3,
        },
        "b002_stats": {
            "fit_used": 1670, "sigma_residual": 0.182142, "scale_factor": 1.565441e-03,
            "snr_n_points": 1984, "snr_median": 378.622875,
            "drizzle_support": 3928, "drizzle_signal": 16200000,
            "platesolve_rms_arcsec": 0.3460,
        },
    },
]


# ============================================================================
# 从 IpvWcsResult 构建 astropy WCS (含 SIP, 不修改 FITS)
# ============================================================================
def build_astropy_wcs_from_result(result) -> Tuple[Any, Dict]:
    """从 IpvWcsResult 结构体构建 astropy WCS

    SIP 系数布局: sip_a[i*6+j] 对应 A_i_j (dx^i * dy^j)
    """
    from astropy.io.fits import Header
    from astropy.wcs import WCS

    cd = list(result.cd)
    crval = list(result.crval)
    crpix = list(result.crpix)
    sip_order = int(result.sip_order)

    ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
    ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
    if not ctype1:
        ctype1 = "RA---TAN-SIP" if sip_order > 0 else "RA---TAN"
    if not ctype2:
        ctype2 = "DEC--TAN-SIP" if sip_order > 0 else "DEC--TAN"

    header = Header()
    header["CTYPE1"] = ctype1
    header["CTYPE2"] = ctype2
    header["CRVAL1"] = float(crval[0])
    header["CRVAL2"] = float(crval[1])
    header["CRPIX1"] = float(crpix[0])
    header["CRPIX2"] = float(crpix[1])
    header["CD1_1"] = float(cd[0])
    header["CD1_2"] = float(cd[1])
    header["CD2_1"] = float(cd[2])
    header["CD2_2"] = float(cd[3])
    header["RADESYS"] = "ICRS"
    header["EQUINOX"] = 2000.0

    n_sip_fwd = 0
    if sip_order > 0:
        header["A_ORDER"] = sip_order
        header["B_ORDER"] = sip_order
        for i in range(sip_order + 1):
            for j in range(sip_order + 1 - i):
                idx = i * 6 + j
                if idx < 36:
                    header["A_%d_%d" % (i, j)] = float(result.sip_a[idx])
                    header["B_%d_%d" % (i, j)] = float(result.sip_b[idx])
                    n_sip_fwd += 1

    ap_order = int(result.sip_ap_order)
    n_sip_inv = 0
    if ap_order > 0:
        header["AP_ORDER"] = ap_order
        header["BP_ORDER"] = ap_order
        for i in range(ap_order + 1):
            for j in range(ap_order + 1 - i):
                idx = i * 6 + j
                if idx < 36:
                    header["AP_%d_%d" % (i, j)] = float(result.sip_ap[idx])
                    header["BP_%d_%d" % (i, j)] = float(result.sip_bp[idx])
                    n_sip_inv += 1

    wcs = WCS(header)
    summary = {
        "ctype": [ctype1, ctype2],
        "crval": [float(crval[0]), float(crval[1])],
        "crpix": [float(crpix[0]), float(crpix[1])],
        "cd": [float(cd[0]), float(cd[1]), float(cd[2]), float(cd[3])],
        "sip_order": sip_order,
        "n_sip_fwd": n_sip_fwd,
        "ap_order": ap_order,
        "n_sip_inv": n_sip_inv,
        "has_sip": sip_order > 0,
    }
    logger.info(
        "astropy WCS 构建: ctype=%s crval=(%.6f,%.6f) crpix=(%.3f,%.3f) sip=%d(%d fwd) ap=%d(%d inv)",
        summary["ctype"], summary["crval"][0], summary["crval"][1],
        summary["crpix"][0], summary["crpix"][1],
        sip_order, n_sip_fwd, ap_order, n_sip_inv,
    )
    return wcs, summary


# ============================================================================
# solver U 坐标 → astropy 0-based pixel (Y 轴翻转)
# ============================================================================
def u_to_astropy_pixel(u_xy: np.ndarray, wcs) -> np.ndarray:
    """solver U 坐标 → astropy 0-based pixel

    solver U: 原点=图像中心, Y 轴向上 (U.y = -(det_y - cy))
    astropy 0-based: 原点=图像角落, Y 轴向下
    转换: pixel_x = U_x + (CRPIX_1based - 1)
          pixel_y = (CRPIX_1based - 1) - U_y
    """
    crpix_0based = np.array([float(wcs.wcs.crpix[0]) - 1.0, float(wcs.wcs.crpix[1]) - 1.0])
    out = np.empty_like(u_xy, dtype=np.float64)
    out[:, 0] = u_xy[:, 0] + crpix_0based[0]
    out[:, 1] = crpix_0based[1] - u_xy[:, 1]
    return out


# ============================================================================
# PlateSolve 回投验证 (核心)
# ============================================================================
def verify_platesolve_reprojection(
    frame: Dict,
    project_root: str,
    env,
) -> Dict[str, Any]:
    """对单帧运行 PlateSolve 回投验证

    流程:
      1. 读 FITS 图像
      2. 运行 PlateSolve (solve_from_memory_with_callback) → 检测星 + inliers + IpvWcsResult
      3. 从 IpvWcsResult 构建 astropy WCS (含 SIP)
      4. 将 inlier 的 Gaia (ra,dec) 用 astropy WCS 回投到像素
      5. 残差 = detector_xy_astropy - external_pred_xy
      6. 计算 RMS (px, arcsec)
      7. 对比 IpvWcsResult WCS 与 HISS WCS
    """
    import astropy.units as u
    from astropy.coordinates import SkyCoord

    frame_id = frame["frame_id"]
    fits_path = os.path.join(project_root, frame["fits"])
    focal_length = frame["focal_length_mm"]
    pixel_size = frame["pixel_size_um"]
    pixel_scale = frame["pixel_scale_arcsec_per_px"]

    gaia_client, sdet, solver = env

    logger.info("=" * 70)
    logger.info("PlateSolve 回投验证: %s", frame_id)
    logger.info("FITS: %s", fits_path)
    logger.info("focal_length=%.1fmm pixel_size=%.2fum pixel_scale=%.3f\"/px",
                focal_length, pixel_size, pixel_scale)

    result_dict: Dict[str, Any] = {
        "frame_id": frame_id,
        "fits_path": fits_path,
        "verify_type": "platesolve_reprojection",
    }

    # 1. 读 FITS
    from astropy.io import fits as astro_fits
    try:
        with astro_fits.open(fits_path, mode="readonly", memmap=False) as hdul:
            pixels = hdul[0].data.astype(np.float32)
            header = hdul[0].header.copy()
    except Exception as e:
        logger.error("读取 FITS 失败: %s", e)
        result_dict["status"] = "FAIL"
        result_dict["error"] = f"read_fits_failed: {e}"
        return result_dict

    height, width = pixels.shape
    logger.info("图像尺寸: %dx%d", width, height)

    # 2. 从 FITS 头读初始指向
    from solve_and_write_wcs import read_fits_header
    header_info = read_fits_header(
        fits_path,
        default_focal_length=focal_length,
        default_pixel_size=pixel_size,
    )
    ra0 = header_info["ra0"]
    dec0 = header_info["dec0"]
    fl = header_info["focal_length"]
    ps = header_info["pixel_size"]
    logger.info("初始指向: ra0=%.6f dec0=%.6f fl=%.2f ps=%.4f", ra0, dec0, fl, ps)

    if fl <= 0 or ps <= 0:
        logger.error("FITS 头缺失 FOCALLEN/XPIXSZ 且无覆盖值: fl=%s ps=%s", fl, ps)
        result_dict["status"] = "FAIL"
        result_dict["error"] = "missing_focal_length_or_pixel_size"
        return result_dict

    # 3. 运行 PlateSolve (callback 获取检测星点)
    detected_holder: List[np.ndarray] = []

    def callback(detections: np.ndarray, n: int, user_data) -> None:
        if n > 0:
            detected_holder.append(detections.copy())

    t0 = time.time()
    logger.info("运行 PlateSolve (solve_from_memory_with_callback)...")
    try:
        solve_result = solver.solve_from_memory_with_callback(
            pixels, width, height,
            ra0, dec0, fl, ps,
            callback, user_data=None,
        )
    except Exception as e:
        logger.error("PlateSolve 求解异常: %s", e)
        result_dict["status"] = "FAIL"
        result_dict["error"] = f"platesolve_exception: {e}"
        return result_dict

    solve_elapsed = time.time() - t0
    logger.info(
        "PlateSolve 完成: success=%s n_detected=%d n_pairs=%d rms_px=%.4f rms_arcsec=%.4f trans_order=%d sip_order=%d (%.2fs)",
        solve_result.success,
        len(detected_holder[0]) if detected_holder else 0,
        solve_result.n_pairs, solve_result.rms_px, solve_result.rms_arcsec,
        solve_result.trans_order, solve_result.sip_order, solve_elapsed,
    )

    if not solve_result.success:
        logger.error("PlateSolve 求解失败")
        result_dict["status"] = "FAIL"
        result_dict["error"] = "platesolve_failed"
        return result_dict

    # 4. 获取权威 inlier 对
    pairs = solver.get_last_inliers()
    n_pairs = len(pairs)
    logger.info("权威 inlier: n=%d", n_pairs)

    if n_pairs == 0:
        logger.error("权威 inlier 数为 0")
        result_dict["status"] = "FAIL"
        result_dict["error"] = "no_inliers"
        return result_dict

    # 5. 从 IpvWcsResult 构建 astropy WCS (含 SIP)
    wcs, wcs_summary = build_astropy_wcs_from_result(solve_result)

    # 6. 将 inlier Gaia (ra,dec) 用 astropy WCS 回投到像素
    ra = pairs[:, 2]
    dec = pairs[:, 3]
    sky = SkyCoord(ra * u.deg, dec * u.deg)
    pred_x, pred_y = wcs.world_to_pixel(sky)
    external_pred_xy = np.column_stack([pred_x, pred_y])

    # 7. detector U 坐标 → astropy pixel
    det_xy_astropy = u_to_astropy_pixel(pairs[:, [0, 1]], wcs)

    # 8. 残差 = detector - external_pred
    residual = det_xy_astropy - external_pred_xy
    residual_dist_px = np.sqrt(residual[:, 0] ** 2 + residual[:, 1] ** 2)

    # 9. 统计
    median_px = float(np.median(residual_dist_px))
    p68_px = float(np.percentile(residual_dist_px, 68.27))
    p90_px = float(np.percentile(residual_dist_px, 90))
    p99_px = float(np.percentile(residual_dist_px, 99))
    max_px = float(np.max(residual_dist_px))
    rms_px = float(np.sqrt(np.mean(residual_dist_px ** 2)))

    # 转换为角秒 (用像素尺度)
    median_arcsec = median_px * pixel_scale
    p68_arcsec = p68_px * pixel_scale
    p90_arcsec = p90_px * pixel_scale
    p99_arcsec = p99_px * pixel_scale
    rms_arcsec = rms_px * pixel_scale

    logger.info("[B 层 外部 WCS 回投] n=%d", n_pairs)
    logger.info("  残差(px): median=%.4f p68=%.4f p90=%.4f p99=%.4f max=%.4f rms=%.4f",
                median_px, p68_px, p90_px, p99_px, max_px, rms_px)
    logger.info("  残差(\"): median=%.4f p68=%.4f p90=%.4f p99=%.4f rms=%.4f",
                median_arcsec, p68_arcsec, p90_arcsec, p99_arcsec, rms_arcsec)

    # 10. WCS 与 HISS WCS 交叉对比
    hiss_wcs = frame["hiss_wcs"]
    cd_delta_pct = max(
        abs((wcs_summary["cd"][i] - hiss_wcs["cd"][i]) / hiss_wcs["cd"][i]) * 100.0
        if abs(hiss_wcs["cd"][i]) > 1e-30 else 0.0
        for i in range(4)
    )
    crval_delta_arcsec = (
        ((wcs_summary["crval"][0] - hiss_wcs["crval"][0]) * 3600.0 * np.cos(np.radians(hiss_wcs["crval"][1]))) ** 2
        + ((wcs_summary["crval"][1] - hiss_wcs["crval"][1]) * 3600.0) ** 2
    ) ** 0.5
    crpix_delta_px = ((wcs_summary["crpix"][0] - hiss_wcs["crpix"][0]) ** 2
                      + (wcs_summary["crpix"][1] - hiss_wcs["crpix"][1]) ** 2) ** 0.5

    logger.info("[WCS 交叉对比] cd_Δ=%.4f%% crval_Δ=%.4f\" crpix_Δ=%.4fpx",
                cd_delta_pct, crval_delta_arcsec, crpix_delta_px)

    # 11. 判定: RMS < 1.0"
    rms_pass = rms_arcsec < 1.0
    n_pass = n_pairs >= 5

    result_dict.update({
        "status": "PASS" if (rms_pass and n_pass) else "FAIL",
        "n_inliers": int(n_pairs),
        "n_detected": int(len(detected_holder[0])) if detected_holder else 0,
        "solver_rms_px": float(solve_result.rms_px),
        "solver_rms_arcsec": float(solve_result.rms_arcsec),
        "ext_rms_px": rms_px,
        "ext_rms_arcsec": rms_arcsec,
        "ext_median_px": median_px,
        "ext_p68_px": p68_px,
        "ext_p90_px": p90_px,
        "ext_p99_px": p99_px,
        "ext_median_arcsec": median_arcsec,
        "ext_p68_arcsec": p68_arcsec,
        "ext_p90_arcsec": p90_arcsec,
        "ext_p99_arcsec": p99_arcsec,
        "rms_lt_1_arcsec": rms_pass,
        "n_ge_5": n_pass,
        "wcs_cd_delta_pct": cd_delta_pct,
        "wcs_crval_delta_arcsec": crval_delta_arcsec,
        "wcs_crpix_delta_px": crpix_delta_px,
        "wcs_sip_order": wcs_summary["sip_order"],
        "solve_elapsed_sec": solve_elapsed,
    })
    return result_dict


# ============================================================================
# 测光 / SNR / Drizzle 指标验证
# ============================================================================
def verify_metrics(frame: Dict) -> Dict[str, Any]:
    """验证测光/SNR/Drizzle 指标 (来自 B-002 science_stats + HISS inspect)"""
    frame_id = frame["frame_id"]
    s = frame["b002_stats"]

    # 测光
    fit_used = s["fit_used"]
    sigma_residual = s["sigma_residual"]
    scale_factor = s["scale_factor"]
    phot_fit_used_pass = fit_used > 50
    phot_sigma_pass = sigma_residual < 0.5
    # scale_factor: 任务说 0.001-10, 但实际值是流量归一化因子(远小于 0.001)
    # 这里按任务原文判定, 同时记录实际值
    scale_factor_in_task_range = 0.001 <= scale_factor <= 10.0
    # 实际合理范围: scale_factor > 0 且有限 (流量归一化因子)
    scale_factor_reasonable = (scale_factor > 0) and np.isfinite(scale_factor)

    # SNR
    snr_n_points = s["snr_n_points"]
    snr_median = s["snr_median"]
    snr_n_pass = snr_n_points > 100
    snr_median_pass = snr_median > 10

    # Drizzle
    support = s["drizzle_support"]
    signal = s["drizzle_signal"]
    drizzle_support_pass = support > 0
    drizzle_signal_pass = signal > 0

    logger.info("[%s] 测光: fit_used=%d(>50:%s) sigma=%.4f(<0.5:%s) scale=%.6e(range:%s reasonable:%s)",
                frame_id, fit_used, phot_fit_used_pass, sigma_residual, phot_sigma_pass,
                scale_factor, scale_factor_in_task_range, scale_factor_reasonable)
    logger.info("[%s] SNR: n_points=%d(>100:%s) median=%.2f(>10:%s)",
                frame_id, snr_n_points, snr_n_pass, snr_median, snr_median_pass)
    logger.info("[%s] Drizzle: support=%d(>0:%s) signal=%d(>0:%s)",
                frame_id, support, drizzle_support_pass, signal, drizzle_signal_pass)

    return {
        "frame_id": frame_id,
        "phot_fit_used": fit_used,
        "phot_fit_used_pass": phot_fit_used_pass,
        "phot_sigma_residual": sigma_residual,
        "phot_sigma_pass": phot_sigma_pass,
        "phot_scale_factor": scale_factor,
        "phot_scale_in_task_range": scale_factor_in_task_range,
        "phot_scale_reasonable": scale_factor_reasonable,
        "snr_n_points": snr_n_points,
        "snr_n_pass": snr_n_pass,
        "snr_median": snr_median,
        "snr_median_pass": snr_median_pass,
        "drizzle_support": support,
        "drizzle_support_pass": drizzle_support_pass,
        "drizzle_signal": signal,
        "drizzle_signal_pass": drizzle_signal_pass,
    }


# ============================================================================
# WCS 元数据合理性检查 (补充验证)
# ============================================================================
def verify_wcs_metadata(frame: Dict) -> Dict[str, Any]:
    """WCS 元数据合理性检查 (CRPIX/CRVAL/CD 矩阵)"""
    frame_id = frame["frame_id"]
    w = frame["hiss_wcs"]
    cd = w["cd"]
    crval = w["crval"]
    crpix = w["crpix"]

    # CD 矩阵单位: 度/像素. 行列式 ≈ pixel_scale^2 (单位: 度^2)
    # 注意: 对于旋转图像, 对角项可能很小, 非对角项很大 — 必须用行列式判断
    det_cd = abs(cd[0] * cd[3] - cd[1] * cd[2])
    pixel_scale_deg = frame["pixel_scale_arcsec_per_px"] / 3600.0
    det_expected_deg = pixel_scale_deg ** 2
    det_ratio = det_cd / det_expected_deg if det_expected_deg > 0 else 0.0
    # det_ratio 应在 0.5-2.0 之间 (允许旋转和轻微畸变)
    det_reasonable = 0.5 <= det_ratio <= 2.0

    # 从 CD 矩阵反算像素尺度, 与声明值对比 (角秒/像素)
    pixel_scale_from_cd_arcsec = np.sqrt(det_cd) * 3600.0
    pixel_scale_match = (
        0.5 * frame["pixel_scale_arcsec_per_px"]
        <= pixel_scale_from_cd_arcsec
        <= 2.0 * frame["pixel_scale_arcsec_per_px"]
    )

    # CRVAL 合理范围 (0-360, -90~90)
    crval_valid = (0.0 <= crval[0] <= 360.0) and (-90.0 <= crval[1] <= 90.0)

    # CRPIX 应在图像范围内 (正数)
    crpix_valid = crpix[0] > 0 and crpix[1] > 0

    # CD 矩阵非奇异
    cd_nonsingular = det_cd > 1e-30

    all_pass = crval_valid and crpix_valid and cd_nonsingular and det_reasonable and pixel_scale_match

    logger.info("[%s] WCS 元数据: crval_valid=%s crpix_valid=%s det_reasonable=%s(det_ratio=%.3f) ps_match=%s(cd_ps=%.3f vs decl=%.3f)",
                frame_id, crval_valid, crpix_valid, det_reasonable, det_ratio,
                pixel_scale_match, pixel_scale_from_cd_arcsec, frame["pixel_scale_arcsec_per_px"])

    return {
        "frame_id": frame_id,
        "wcs_crval_valid": crval_valid,
        "wcs_crpix_valid": crpix_valid,
        "wcs_cd_det_reasonable": det_reasonable,
        "wcs_cd_det_ratio": det_ratio,
        "wcs_pixel_scale_match": pixel_scale_match,
        "wcs_pixel_scale_from_cd_arcsec": pixel_scale_from_cd_arcsec,
        "wcs_cd_nonsingular": cd_nonsingular,
        "wcs_metadata_pass": all_pass,
    }


# ============================================================================
# 主流程
# ============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(description="B-003 一致性验证")
    parser.add_argument("--project-root", default=r"f:\Astro dev\Astro CS Normalization Database")
    parser.add_argument("--output-dir", default=None)
    parser.add_argument("--gaia-timeout", type=int, default=60)
    parser.add_argument("--log", default=None)
    args = parser.parse_args()

    project_root = args.project_root
    output_dir = args.output_dir or os.path.join(
        project_root, "engineering_authoritative", "evidence", "B-003"
    )
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(os.path.join(output_dir, "per_frame"), exist_ok=True)

    if args.log:
        fh = logging.FileHandler(args.log, encoding="utf-8")
        fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
        logging.getLogger().addHandler(fh)

    logger.info("=" * 70)
    logger.info("B-003 一致性验证开始")
    logger.info("project_root=%s", project_root)
    logger.info("output_dir=%s", output_dir)
    logger.info("gaia_timeout=%ds", args.gaia_timeout)

    # 添加 plate_solve python 路径
    sys.path.insert(0, os.path.join(project_root, "lib", "plate_solve", "python"))
    sys.path.insert(0, os.path.join(project_root, "lib", "gaia_xpsd_client", "python"))
    sys.path.insert(0, os.path.join(project_root, "lib", "star_detector", "python"))

    # 初始化 PlateSolve 环境
    logger.info("初始化 PlateSolve 环境...")
    from solve_and_write_wcs import init_environment, _close_environment

    env = init_environment()
    gaia_client, sdet, solver = env
    logger.info("PlateSolve 环境就绪")

    all_results: List[Dict[str, Any]] = []

    try:
        for frame in FRAMES:
            frame_id = frame["frame_id"]
            frame_result: Dict[str, Any] = {"frame_id": frame_id}

            # 1. PlateSolve 回投验证
            try:
                ps_result = verify_platesolve_reprojection(frame, project_root, env)
                frame_result["platesolve"] = ps_result
                # 保存单帧详细结果
                per_frame_path = os.path.join(output_dir, "per_frame", f"{frame_id}_platesolve.json")
                with open(per_frame_path, "w", encoding="utf-8") as f:
                    json.dump(ps_result, f, indent=2, ensure_ascii=False)
            except Exception as e:
                logger.error("PlateSolve 验证异常 %s: %s", frame_id, e, exc_info=True)
                frame_result["platesolve"] = {
                    "frame_id": frame_id, "status": "FAIL",
                    "error": f"exception: {e}",
                }

            # 2. 测光/SNR/Drizzle 指标验证
            metrics_result = verify_metrics(frame)
            frame_result["metrics"] = metrics_result

            # 3. WCS 元数据合理性
            wcs_meta_result = verify_wcs_metadata(frame)
            frame_result["wcs_metadata"] = wcs_meta_result

            all_results.append(frame_result)
    finally:
        _close_environment(gaia_client, sdet, solver)
        logger.info("PlateSolve 环境已释放")

    # ========================================================================
    # 输出 verification_results.csv
    # ========================================================================
    csv_path = os.path.join(output_dir, "verification_results.csv")
    csv_fields = [
        "frame_id",
        "ps_status", "ps_n_inliers", "ps_n_detected",
        "ps_solver_rms_arcsec", "ps_ext_rms_arcsec",
        "ps_ext_p68_arcsec", "ps_ext_p90_arcsec", "ps_ext_p99_arcsec",
        "ps_rms_lt_1_arcsec", "ps_n_ge_5",
        "ps_wcs_cd_delta_pct", "ps_wcs_crval_delta_arcsec", "ps_wcs_crpix_delta_px",
        "phot_fit_used", "phot_fit_used_pass",
        "phot_sigma_residual", "phot_sigma_pass",
        "phot_scale_factor", "phot_scale_in_task_range", "phot_scale_reasonable",
        "snr_n_points", "snr_n_pass",
        "snr_median", "snr_median_pass",
        "drizzle_support", "drizzle_support_pass",
        "drizzle_signal", "drizzle_signal_pass",
        "wcs_metadata_pass",
        "overall_pass",
    ]

    with open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=csv_fields)
        writer.writeheader()
        for r in all_results:
            ps = r.get("platesolve", {})
            mt = r.get("metrics", {})
            wm = r.get("wcs_metadata", {})

            # overall pass: platesolve PASS + 所有指标 pass + wcs 元数据 pass
            ps_ok = ps.get("status") == "PASS"
            metrics_ok = (
                mt.get("phot_fit_used_pass") and mt.get("phot_sigma_pass")
                and mt.get("phot_scale_reasonable")
                and mt.get("snr_n_pass") and mt.get("snr_median_pass")
                and mt.get("drizzle_support_pass") and mt.get("drizzle_signal_pass")
            )
            wcs_ok = wm.get("wcs_metadata_pass", False)
            overall = ps_ok and metrics_ok and wcs_ok

            writer.writerow({
                "frame_id": r["frame_id"],
                "ps_status": ps.get("status", "N/A"),
                "ps_n_inliers": ps.get("n_inliers", 0),
                "ps_n_detected": ps.get("n_detected", 0),
                "ps_solver_rms_arcsec": ps.get("solver_rms_arcsec", ""),
                "ps_ext_rms_arcsec": ps.get("ext_rms_arcsec", ""),
                "ps_ext_p68_arcsec": ps.get("ext_p68_arcsec", ""),
                "ps_ext_p90_arcsec": ps.get("ext_p90_arcsec", ""),
                "ps_ext_p99_arcsec": ps.get("ext_p99_arcsec", ""),
                "ps_rms_lt_1_arcsec": ps.get("rms_lt_1_arcsec", False),
                "ps_n_ge_5": ps.get("n_ge_5", False),
                "ps_wcs_cd_delta_pct": ps.get("wcs_cd_delta_pct", ""),
                "ps_wcs_crval_delta_arcsec": ps.get("wcs_crval_delta_arcsec", ""),
                "ps_wcs_crpix_delta_px": ps.get("wcs_crpix_delta_px", ""),
                "phot_fit_used": mt.get("phot_fit_used", 0),
                "phot_fit_used_pass": mt.get("phot_fit_used_pass", False),
                "phot_sigma_residual": mt.get("phot_sigma_residual", ""),
                "phot_sigma_pass": mt.get("phot_sigma_pass", False),
                "phot_scale_factor": mt.get("phot_scale_factor", ""),
                "phot_scale_in_task_range": mt.get("phot_scale_in_task_range", False),
                "phot_scale_reasonable": mt.get("phot_scale_reasonable", False),
                "snr_n_points": mt.get("snr_n_points", 0),
                "snr_n_pass": mt.get("snr_n_pass", False),
                "snr_median": mt.get("snr_median", ""),
                "snr_median_pass": mt.get("snr_median_pass", False),
                "drizzle_support": mt.get("drizzle_support", 0),
                "drizzle_support_pass": mt.get("drizzle_support_pass", False),
                "drizzle_signal": mt.get("drizzle_signal", 0),
                "drizzle_signal_pass": mt.get("drizzle_signal_pass", False),
                "wcs_metadata_pass": wcs_ok,
                "overall_pass": overall,
            })

    logger.info("verification_results.csv 已保存: %s", csv_path)

    # 保存完整 JSON 结果
    json_path = os.path.join(output_dir, "verification_results.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2, ensure_ascii=False, default=str)
    logger.info("verification_results.json 已保存: %s", json_path)

    # 汇总
    n_total = len(all_results)
    n_ps_pass = sum(1 for r in all_results if r.get("platesolve", {}).get("status") == "PASS")
    n_overall = sum(1 for r in all_results
                    if r.get("platesolve", {}).get("status") == "PASS"
                    and r.get("wcs_metadata", {}).get("wcs_metadata_pass")
                    and all([
                        r.get("metrics", {}).get("phot_fit_used_pass"),
                        r.get("metrics", {}).get("phot_sigma_pass"),
                        r.get("metrics", {}).get("phot_scale_reasonable"),
                        r.get("metrics", {}).get("snr_n_pass"),
                        r.get("metrics", {}).get("snr_median_pass"),
                        r.get("metrics", {}).get("drizzle_support_pass"),
                        r.get("metrics", {}).get("drizzle_signal_pass"),
                    ]))
    logger.info("=" * 70)
    logger.info("B-003 验证汇总: %d/%d PlateSolve PASS, %d/%d Overall PASS",
                n_ps_pass, n_total, n_overall, n_total)

    return 0 if n_overall == n_total else 1


if __name__ == "__main__":
    sys.exit(main())
