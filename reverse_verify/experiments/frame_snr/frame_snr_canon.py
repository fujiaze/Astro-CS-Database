#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FRAME-SNR-CANON —— 帧级 SNR 定案的独立数值内核（纯 Python，不并入主线）。

本文件**不引用**任何主线结论；所有公式从第一性原理 + 公开文献重写一遍，
并与主线生产实现（lib/algorithms/noise_snr/cpp/src/snr_science.cpp）的
**离散归一化 Moffat4 网格规则**逐位对齐（用于对拍，不是照抄）。

单位约定（与主线一致，全部显式）：
  F_s      [e-]      源总通量（**已扣局部背景**；加性天光不进入此量）
  B        [e-/pix]  天光/背景在**单像素**上的期望电子数（加性偏移）
  sigma_R  [e-]      读出噪声（逐像素，与信号无关）
  g        [e-/ADU]  转换增益
  P_i      [1/pix]   离散归一化 PSF：P_i >= 0 且 sum_i P_i = 1
  A_NEA    [pix]     等效噪声面积 = 1 / sum_i P_i^2

核心结论（定案式）：
  sigma_i^2 [e^2] = B + sigma_R^2 + F_s * P_i          (逐像素总方差)
  Var(F_hat)      = 1 / sum_i (P_i^2 / sigma_i^2)      (Horne 1986 最优提取)
  SNR_F           = F_s / sqrt(Var(F_hat))

天光 B **只**出现在 sigma_i^2（分母的方差），**从不**出现在分子 F_s 里
—— 这是"不被天光抬高的假信噪比"的充要结构条件。
"""

from __future__ import annotations

import math

import numpy as np

# ---------------------------------------------------------------------------
# 1. 轮廓：离散归一化 Moffat4(beta=4)（与生产 snr_science.cpp:41-84 同规则）
# ---------------------------------------------------------------------------

MOFFAT4_FWHM_FACTOR = 1.230310  # FWHM = 1.230310 * sigma (SCI-PSF-001 §5)


def moffat4_sigma_from_fwhm(fwhm_px: float) -> float:
    return fwhm_px / MOFFAT4_FWHM_FACTOR


def moffat4_auto_half(fwhm_px: float) -> int:
    """生产规则: half = clamp(ceil(12*fwhm), 30, 256)。"""
    h = int(math.ceil(12.0 * fwhm_px))
    h = max(h, 30)
    h = min(h, 256)
    return h


def moffat4_discrete_profile(fwhm_px: float, half_px: int | None = None) -> np.ndarray:
    """返回离散归一化轮廓 P（2*half+1 方阵，sum=1），与生产网格规则一致。"""
    sigma = moffat4_sigma_from_fwhm(fwhm_px)
    half = moffat4_auto_half(fwhm_px) if half_px is None else int(half_px)
    j, i = np.mgrid[-half : half + 1, -half : half + 1]
    r2 = (i.astype(float) ** 2) + (j.astype(float) ** 2)
    alpha2 = 2.0 * sigma * sigma
    v = (1.0 + r2 / alpha2) ** -4.0
    return v / v.sum()


def a_nea(profile: np.ndarray) -> float:
    """等效噪声面积 A_NEA = 1 / sum_i P_i^2  [pix]。"""
    return 1.0 / float(np.sum(profile**2))


# ---------------------------------------------------------------------------
# 2. 定案式（CANON）：天光只进方差，不进信号
# ---------------------------------------------------------------------------


def pixel_variance_e2(F_s_e: float, B_e: float, sigma_R_e: float, P: np.ndarray) -> np.ndarray:
    """逐像素总方差 [e^2]：天光散粒 + 读出 + 源散粒。"""
    return B_e + sigma_R_e**2 + F_s_e * P


def var_flux_optimal_e2(F_s_e: float, B_e: float, sigma_R_e: float, P: np.ndarray) -> float:
    """Horne 1986 最优提取的通量方差 [e^2]：1 / sum_i P_i^2/sigma_i^2。"""
    s2 = pixel_variance_e2(F_s_e, B_e, sigma_R_e, P)
    return 1.0 / float(np.sum(P**2 / s2))


def sigma_flux_optimal_e(F_s_e: float, B_e: float, sigma_R_e: float, P: np.ndarray) -> float:
    return math.sqrt(var_flux_optimal_e2(F_s_e, B_e, sigma_R_e, P))


def snr_canon(F_s_e: float, B_e: float, sigma_R_e: float, P: np.ndarray) -> float:
    """定案帧级 SNR（点源、PSF 加权最优提取、天光已扣）。

    SNR = F_s / sigma_F，sigma_F 含天光散粒 + 读出 + 源散粒。
    """
    return F_s_e / sigma_flux_optimal_e(F_s_e, B_e, sigma_R_e, P)


def snr_canon_sky_limited(F_s_e: float, B_e: float, P: np.ndarray) -> float:
    """天空受限闭式（gain -> inf，源泊松与读噪可忽略）：
        Var(F) = B * A_NEA ,  SNR = F_s / sqrt(B * A_NEA)
    """
    return F_s_e / math.sqrt(B_e * a_nea(P))


# ---------------------------------------------------------------------------
# 3. 红例（必须被否决的定义）
# ---------------------------------------------------------------------------


def snr_red_unsubtracted(F_s_e: float, B_e: float, sigma_R_e: float, n_pix: float) -> float:
    """红例 A：**未扣背景**的通量型 SNR = (F_s + n_pix*B) / sqrt(n_pix*(B+sigma_R^2))。

    分子含天光基座 -> 随 B 上升。
    """
    num = F_s_e + n_pix * B_e
    den = math.sqrt(n_pix * (B_e + sigma_R_e**2))
    return num / den


def snr_red_power_ratio(F_s_e: float, B_e: float, sigma_R_e: float, n_pix: float) -> float:
    """红例 B：功率比型 SNR^2 = (sum_i I_i)^2 / sum_i sigma_i^2（I 未扣背景）。

    即 PixInsight 文档所称 "standard SNR" 的同构形式（全局尺度估计/噪声方差）。
    """
    s = F_s_e + n_pix * B_e
    v = n_pix * (B_e + sigma_R_e**2)
    return s / math.sqrt(v)


def snr_red_window_unsubtracted(F_s_e: float, B_e: float, sigma_R_e: float, P: np.ndarray) -> float:
    """红例 C：高斯窗口口径 SNR，窗口和**未扣局部背景**：
        SNR = sum_i P_i I_i / sqrt(sum_i P_i^2 sigma_i^2)
    """
    I = F_s_e * P + B_e
    s2 = B_e + sigma_R_e**2 + F_s_e * P
    num = float(np.sum(P * I))
    den = math.sqrt(float(np.sum(P**2 * s2)))
    return num / den


# ---------------------------------------------------------------------------
# 4. 估计量（用于加性天光不变性测试）
# ---------------------------------------------------------------------------


def aperture_sum_bgsub(image: np.ndarray, ap_mask: np.ndarray, ann_mask: np.ndarray) -> float:
    """孔径和，局部背景取天空环**中位数**（中位数对加性常数等变 => 差不变）。"""
    bkg = float(np.median(image[ann_mask]))
    return float(np.sum(image[ap_mask] - bkg))


def psf_weighted_flux_bgsub(
    image: np.ndarray, P: np.ndarray, ann_mask: np.ndarray
) -> float:
    """PSF 加权通量，局部背景取天空环中位数：
        F_hat = sum_i P_i (I_i - b_hat) / sum_i P_i^2
    """
    bkg = float(np.median(image[ann_mask]))
    d = image - bkg
    return float(np.sum(P * d) / np.sum(P**2))


def synth_frame(
    F_s_e: float,
    B_e: float,
    sigma_R_e: float,
    shape: tuple[int, int],
    center: tuple[int, int],
    fwhm_px: float,
    rng: np.random.Generator,
    poisson: bool = True,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """合成一帧：源 + 天光 + 读出噪声。返回 (image_e, P_full, ap_mask)。"""
    half = int(math.ceil(6.0 * fwhm_px))
    prof = moffat4_discrete_profile(fwhm_px, half_px=max(half, 8))
    ph, pw = prof.shape
    P_full = np.zeros(shape, dtype=float)
    y0 = center[0] - ph // 2
    x0 = center[1] - pw // 2
    P_full[y0 : y0 + ph, x0 : x0 + pw] = prof
    lam = B_e + F_s_e * P_full
    if poisson:
        img = rng.poisson(lam).astype(float)
    else:
        img = lam + rng.normal(0.0, 1.0, size=shape) * np.sqrt(lam)
    img = img + rng.normal(0.0, sigma_R_e, size=shape)
    yy, xx = np.mgrid[0 : shape[0], 0 : shape[1]]
    rr = np.hypot(yy - center[0], xx - center[1])
    ap_mask = rr <= 1.5 * fwhm_px
    ann_mask = (rr > 2.5 * fwhm_px) & (rr <= 4.0 * fwhm_px)
    return img, P_full, ap_mask, ann_mask
