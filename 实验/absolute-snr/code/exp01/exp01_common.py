#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01 公共库：生产算法的**逐字镜像** + 估计量/方差的解析与蒙特卡洛工具。

设计原则（AGENTS §5「不以当前程序输出生成唯一 expected」）：
  * 本模块的函数是 lib/algorithms/noise_snr/cpp/src/snr_science.cpp 与
    lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp 的**独立重写**，
    用于解析/蒙特卡洛对照；**不 import 生产代码、不运行任何 ACSD 可执行文件**。
  * 所有常数与网格规则给出代码锚（按符号名），供落地时核对。

单位约定：ADU（图像值）、e-（电子）；gain [e-/ADU]；RN [e-]。
"""

from __future__ import annotations

import math
from typing import Dict, Tuple

import numpy as np

# ── 生产常数（逐字取自 snr_science.cpp 匿名命名空间） ───────────────────────
GAUSS_FWHM_FACTOR = 2.3548200450309493   # kGaussFwhmFactor（检测块：FWHM=2.35482σ）
MOFFAT4_FWHM_FACTOR = 1.230310           # kMoffat4FwhmFactor（PSF 块：FWHM=1.230310σ）
MAD_TO_SIGMA = 1.482602218505602         # NOISE_MODEL.md §5 冻结的稳健尺度常数
SQRT_2PI = math.sqrt(2.0 * math.pi)
KAPPA_MEDIAN = math.pi / 2.0             # 中位数渐近常数 κ = π/2

# 生产 σ_sky 语义枚举（snr_estimator.h:291-293）
SRC_UNSPECIFIED = 0
SRC_SHOT_ONLY = 1
SRC_EMPIRICAL_TOTAL_RMS = 2

# ── 生产估计量的形状参数（star_detector.cpp::detect 的 5×5 盒） ─────────────
BOX_HALF = 2
BOX_N = (2 * BOX_HALF + 1) ** 2          # 25


# ===========================================================================
# 1. 轮廓与网格（snr_science.cpp::moffat4Discrete / autoHalf / detectionSigmaFromFwhm）
# ===========================================================================
def detection_sigma_from_fwhm(fwhm_px: float) -> float:
    """检测块高斯 FWHM -> sigma（snr_science.cpp::detectionSigmaFromFwhm）。"""
    return fwhm_px / GAUSS_FWHM_FACTOR


def auto_half(fwhm_eff_px: float) -> int:
    """网格半宽（snr_science.cpp::autoHalf）：clip(ceil(12*fwhm_eff), 30, 256)。"""
    h = int(math.ceil(12.0 * fwhm_eff_px))
    return min(max(h, 30), 256)


def moffat4_profile(sigma: float, half: int) -> Tuple[np.ndarray, float, float]:
    """离散归一化 Moffat4(beta=4) 轮廓（**逐字镜像** moffat4Discrete）。

    返回 (P[2h+1,2h+1] 且 sum(P)=1, sum_p2 = sum_i P_i^2, p_center = P[中心])。
    """
    j, i = np.mgrid[-half:half + 1, -half:half + 1]
    r2 = (i.astype(float) ** 2) + (j.astype(float) ** 2)
    v = (1.0 + r2 / (2.0 * sigma * sigma)) ** (-4)
    s = float(v.sum())
    P = v / s
    return P, float((P * P).sum()), float(P[half, half])


def profile_for_fwhm(fwhm_px: float, half_px: int = 0) -> Tuple[np.ndarray, float, int, float]:
    """由 fwhm_px（检测块列）导出生产网格与轮廓。返回 (P, sum_p2, half, sigma)。"""
    sigma = detection_sigma_from_fwhm(fwhm_px)
    fwhm_eff = sigma * MOFFAT4_FWHM_FACTOR
    half = half_px if half_px > 0 else auto_half(fwhm_eff)
    P, sum_p2, _ = moffat4_profile(sigma, half)
    return P, sum_p2, half, sigma


# ===========================================================================
# 2. 逐像素方差（snr_science.cpp::snr_source_snr_f64 的 sigma_i^2 分支）
# ===========================================================================
def pixel_variance(P: np.ndarray, F_adu: float, sigma_sky_adu: float, gain: float,
                   rn_e: float, sigma_sky_source: int) -> np.ndarray:
    """sigma_i^2 = sigma_sky^2 + [语义] (RN/g)^2 + F*P_i/g  [ADU^2]（gain<=0 时退化为 sigma_sky^2）。

    与生产逐字一致：rn_in_sky = (source == EMPIRICAL_TOTAL_RMS)；
    只有 E 不加 (RN/g)^2，其余（含 UNSPECIFIED）一律加。
    """
    v = np.full(P.shape, float(sigma_sky_adu) ** 2)
    if gain > 0.0:
        rn_in_sky = (int(sigma_sky_source) == SRC_EMPIRICAL_TOTAL_RMS)
        if (not rn_in_sky) and rn_e > 0.0:
            v = v + (rn_e / gain) ** 2
        si = F_adu * P
        v = v + np.where(si > 0.0, si / gain, 0.0)
    return v


def var_optimal(P: np.ndarray, V: np.ndarray) -> float:
    """Horne 最优提取方差 Var(F_opt) = 1 / sum_i P_i^2/sigma_i^2  [ADU^2]。"""
    a = float((P * P / V).sum())
    return (1.0 / a) if a > 0.0 else float("inf")


def sigma_optimal(F_adu: float, fwhm_px: float, sigma_sky_adu: float, gain: float,
                  rn_e: float, sigma_sky_source: int,
                  half_px: int = 0) -> Dict[str, object]:
    """生产 snr_source_snr_f64 的 sigma_F / SNR 两支（逐字镜像，返回全部中间量）。"""
    P, sum_p2, half, sigma = profile_for_fwhm(fwhm_px, half_px)
    if gain > 0.0:
        V = pixel_variance(P, F_adu, sigma_sky_adu, gain, rn_e, sigma_sky_source)
        var_f = var_optimal(P, V)
    else:
        var_f = (sigma_sky_adu ** 2) / sum_p2
        V = np.full(P.shape, sigma_sky_adu ** 2)
    sig_f = math.sqrt(var_f)
    return {
        "half": half, "sigma_px": sigma, "sum_p2": sum_p2, "var_f": var_f,
        "sigma_f_adu": sig_f,
        "snr_optimal": (F_adu / sig_f) if sig_f > 0 else float("nan"),
        "P": P, "V": V,
    }


# ===========================================================================
# 3. 盒和估计量（star_detector.cpp::detect 的 5×5 正性截断盒和）
# ===========================================================================
def box_slice(P: np.ndarray, half: int = BOX_HALF) -> np.ndarray:
    """取轮廓中心 5×5 窗口（生产盒以**峰值像素**为中心；本函数取网格中心）。"""
    c = P.shape[0] // 2
    return P[c - half:c + half + 1, c - half:c + half + 1]


def energy_fraction_box(P: np.ndarray, half: int = BOX_HALF) -> float:
    """E_B = 5×5 窗口捕获的 PSF 能量份额（sum_all P_i = 1）。"""
    return float(box_slice(P, half).sum())


def truncated_normal_moments(mu: np.ndarray, sigma: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """E[y*1{y>0}] 与 E[y^2*1{y>0}]，y ~ N(mu, sigma^2)（标准截断正态矩）。"""
    from scipy.special import erf
    z = mu / sigma
    Phi = 0.5 * (1.0 + erf(z / math.sqrt(2.0)))
    phi = np.exp(-0.5 * z * z) / SQRT_2PI
    m1 = mu * Phi + sigma * phi
    m2 = (mu * mu + sigma * sigma) * Phi + mu * sigma * phi
    return m1, m2


def box_estimator_moments(F_true: float, P: np.ndarray, sigma_sky_adu: float,
                          gain: float, rn_e: float, sigma_sky_source: int,
                          half: int = BOX_HALF) -> Dict[str, float]:
    """5×5 盒和估计量（含 v<=0 正性截断）的**精确**均值与方差。

    模型：y_i = d_i - b_true，y_i ~ N(F_true*P_i, V_i)，像素独立；
    生产估计量 F_hat = sum_{i in B} max(0, y_i - (b_hat - b_true))，本函数取 b_hat = b_true
    （背景电平项的贡献单独给出，量级 <= 2.5e-6 相对）。
    """
    Pb = box_slice(P, half)
    Vb = pixel_variance(Pb, F_true, sigma_sky_adu, gain, rn_e, sigma_sky_source)
    mu = F_true * Pb
    sig = np.sqrt(Vb)
    m1, m2 = truncated_normal_moments(mu, sig)
    E = float(m1.sum())
    Var = float((m2 - m1 * m1).sum())
    E_unt = float(mu.sum())
    Var_unt = float(Vb.sum())
    return {
        "E_trunc": E, "Var_trunc": Var,
        "E_untrunc": E_unt, "Var_untrunc": Var_unt,
        "E_B": float(Pb.sum()),
        "bias_trunc": E - E_unt,
        "bias_trunc_over_Fbox": (E / E_unt - 1.0) if E_unt > 0 else float("nan"),
        "var_ratio_trunc_over_untrunc": Var / Var_unt if Var_unt > 0 else float("nan"),
        "n_box": float(Pb.size),
    }


def var_box_with_background(P: np.ndarray, F_true: float, sigma_sky_adu: float,
                            gain: float, rn_e: float, sigma_sky_source: int,
                            n_sky: float, half: int = BOX_HALF) -> Dict[str, float]:
    """盒和方差（未截断）＋背景电平估计项的**精确**合成。

    Var(F_box) = sum_{i in B} V_i + n_B^2*Var(b_hat) - 2*n_B*sum_i Cov(d_i, b_hat)

    生产 b_hat = **帧全局** 2 轮 median±3*1.4826*MAD 裁剪后的**中位数**，n_sky ~ 全帧像素数。

    中位数的**影响函数** IF(x) = (1/2 - 1{x < b}) / f_b(b)，b_hat - b ~ (1/n) sum_j IF(d_j)，故
      Var(b_hat) = 1/(4 n f_b(b)^2) = kappa*sigma_b^2/n ，kappa = pi/2（高斯）
      Cov(d_i, b_hat) = (1/n) E[(d_i-b) IF(d_i)] = (1/n) * sigma_b^2 / 1 = V_i / n
    注意 Cov 里**没有**额外的 sqrt(2pi)*sigma_b/2 因子（那是"把 IF 当成与 x 成正比"的常见笔误：
    IF 的幅度被 1/(2 f_b(0)) 截断，而 (d_i-b) 的期望恰好把它抵消掉）。
    """
    # 自审（EXP-01）：本式在 EXP-01 之前写作 V_i*sqrt(2pi)*sigma_b/(2 n_sky)，量纲为 ADU^3/n，
    # 与 Cov 应有的 ADU^2 不符（偏差因子 1.2533*sigma_b）。因该项整体 ~1e-6 相对量级，
    # 修正不改变任何结论，但公式本身必须正确。
    Pb = box_slice(P, half)
    Vb = pixel_variance(Pb, F_true, sigma_sky_adu, gain, rn_e, sigma_sky_source)
    sum_V = float(Vb.sum())
    sig_b = float(sigma_sky_adu)
    var_bhat = KAPPA_MEDIAN * sig_b * sig_b / n_sky
    cov_coef = 1.0 / n_sky                    # Cov(d_i, b_hat) = V_i / n_sky（中位数 IF 的精确结果）
    cov_sum = float((Vb * cov_coef).sum())
    var_tot = sum_V + BOX_N ** 2 * var_bhat - 2.0 * BOX_N * cov_sum
    return {
        "sum_V": sum_V, "var_bhat": var_bhat,
        "term_bg": BOX_N ** 2 * var_bhat,
        "term_cov": -2.0 * BOX_N * cov_sum,
        "var_box_bg": var_tot,
        "bg_rel": (BOX_N ** 2 * var_bhat - 2.0 * BOX_N * cov_sum) / sum_V,
    }


# ===========================================================================
# 4. PSF 解析通量（module_adapters.cpp::p1_psf_analytic_flux）
# ===========================================================================
def psf_analytic_flux(A: float, sx: float, sy: float) -> float:
    """flux = 2*pi*A*sx*sy/3（Moffat4 beta=4 整平面解析积分；ADU）。"""
    return 2.0 * math.pi * A * sx * sy / 3.0


# ===========================================================================
# 5. 估计量对照（现行 / 方案 A / 方案 B）
# ===========================================================================
def estimator_table(F_true: float, fwhm_det_px: float, sigma_sky_adu: float, gain: float,
                    rn_e: float, sigma_sky_source: int, n_sky: float,
                    half_px: int = 0) -> Dict[str, object]:
    """三个候选口径的解析表（现行 / A=PSF 解析通量+最优方差 / B=盒和+盒和方差）。"""
    P, sum_p2, half, sigma = profile_for_fwhm(fwhm_det_px, half_px)
    V = pixel_variance(P, F_true, sigma_sky_adu, gain, rn_e, sigma_sky_source)
    v_opt = var_optimal(P, V)
    bm = box_estimator_moments(F_true, P, sigma_sky_adu, gain, rn_e, sigma_sky_source)
    bg = var_box_with_background(P, F_true, sigma_sky_adu, gain, rn_e,
                                 sigma_sky_source, n_sky)
    E_B = bm["E_B"]
    return {
        "F_true": F_true, "fwhm_det_px": fwhm_det_px, "half": half, "sigma_px": sigma,
        "sum_p2": sum_p2, "E_B": E_B, "var_opt": v_opt, "sigma_opt": math.sqrt(v_opt),
        "snr_truth_total_flux": F_true / math.sqrt(v_opt),
        # 现行：盒和通量 / 最优提取方差
        "current_flux": F_true * E_B,
        "current_sigma": math.sqrt(v_opt),
        "current_snr": F_true * E_B / math.sqrt(v_opt),
        # 方案 A：PSF 解析通量（= 总通量，无偏）/ 最优提取方差
        "A_flux": F_true, "A_sigma": math.sqrt(v_opt),
        "A_snr": F_true / math.sqrt(v_opt),
        # 方案 B：盒和（未截断）/ 盒和自身方差
        "B_flux": F_true * E_B,
        "B_sigma": math.sqrt(bg["var_box_bg"]),
        "B_snr": F_true * E_B / math.sqrt(bg["var_box_bg"]),
        "box_moments": bm, "box_bg": bg,
        # 正性截断口径（B 若不做截断改正）
        "B_trunc_flux": bm["E_trunc"],
        "B_trunc_sigma": math.sqrt(bm["Var_trunc"]),
        "B_trunc_snr": bm["E_trunc"] / math.sqrt(bm["Var_trunc"]),
    }


# ===========================================================================
# 6. 通用工具
# ===========================================================================
def production_clip_sigma(img: np.ndarray, n_rounds: int = 2, k: float = 3.0) -> Dict[str, float]:
    """**逐字镜像** star_detector.cpp::estimate_background 的 sigma 生产者。

    生产实现：全帧像素 -> 每轮用 (median, 1.4826*MAD) 做 median ± k*scale 裁剪
    -> 最终 noise_sigma = sqrt(sum((keep-median)^2)/kn)（中心取中位数）。
    """
    v = np.asarray(img, dtype=np.float64).ravel()
    v = v[np.isfinite(v)]
    n_in = int(v.size)
    for _ in range(max(int(n_rounds), 0)):
        med = float(np.median(v))
        mad = float(np.median(np.abs(v - med)))
        scale = MAD_TO_SIGMA * mad
        if not (scale > 0.0):
            break
        keep = np.abs(v - med) <= k * scale
        if keep.all():
            break
        v = v[keep]
    med = float(np.median(v))
    sig = float(np.sqrt(np.mean((v - med) ** 2)))
    return {"background": med, "sigma": sig, "n_keep": int(v.size), "n_in": n_in}


def analytic_clip_bias(k: float) -> float:
    """±k*sigma 单轮截断高斯 RMS 相对低偏的解析值：sqrt(1 - 2k*phi(k)/(2*Phi(k)-1)) - 1。"""
    from scipy.special import erf
    Phi = 0.5 * (1.0 + erf(k / math.sqrt(2.0)))
    phi = math.exp(-0.5 * k * k) / SQRT_2PI
    return math.sqrt(max(1.0 - 2.0 * k * phi / (2.0 * Phi - 1.0), 0.0)) - 1.0


def rss(*vals: float) -> float:
    return math.sqrt(sum(float(v) ** 2 for v in vals))


def rel(a: float, b: float) -> float:
    """(a/b - 1)（b 为 0 时返回 nan）。"""
    return (a / b - 1.0) if b != 0 else float("nan")
