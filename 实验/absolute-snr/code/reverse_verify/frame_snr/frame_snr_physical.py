#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FRAME-SNR-CANON —— **正向物理仿真**（合成数据噪声物理模型，论文方法节用）。

设计原则（负责人 2026-09-19 纠正）：**天光对 SNR 的影响只能通过散粒噪声体现**，
禁止用"算术加常数"冒充"加天光"。因此本模块实现完整的**正向物理链**：

    m(x,y)   = [ B + D*t + F_s*P(x-x0,y-y0) ] * flat(x,y) + grad(x,y)      [e-]
    I_e(x,y) = Poisson( m(x,y) ) + N(0, sigma_R)                            [e-]
    I_adu    = round( I_e / g )                                             [ADU]

符号与单位：
    B        [e-/pix]     天光水平（**泊松均值 = 泊松方差**，散粒噪声由此产生）
    D        [e-/pix/s]   暗电流；t [s] 曝光时间
    F_s      [e-]         源总通量（**真值**，与天光无关）
    P        [1/pix]      离散归一化 Moffat4 轮廓（sum P = 1）
    flat     [1]          乘性平场响应（PRNU 逐像素 + 低阶空间项）
    grad     [e-/pix]     天空/背景梯度（加性结构）
    sigma_R  [e-]         读出噪声（高斯，电子域）
    g        [e-/ADU]     转换增益（**含 ADU 量化**：round(I_e/g)）

被检验的估计量（"ACSD 等价管线"，**全部从数据估计，不用真值**）：
    1) 局部背景 b_hat = 天空环像素**中位数**                       [ADU]
    2) F_hat = sum_i P_i (I_i - b_hat)/sigma_i^2 / sum_i P_i^2/sigma_i^2
    3) sigma_i^2 = sig_sky_hat^2 + (sigma_R/g)^2 + max(F_hat,0)*P_i/g   [ADU^2]
       （sig_sky_hat 由天空环 MAD*1.4826 稳健估计）
    4) sigma_F_hat = 1/sqrt(sum_i P_i^2/sigma_i^2) ;  SNR_hat = F_hat/sigma_F_hat

注意 1) 与 3)：**天光均值只被减掉、从不进分子；天光散粒只进 sigma_i^2**。
"""

from __future__ import annotations

import math

import numpy as np

import frame_snr_canon as C

MAD_TO_SIGMA = 1.4826


# ---------------------------------------------------------------------------
# 1. 正向物理仿真
# ---------------------------------------------------------------------------


def make_flat(shape, rng, prnu_rms=0.01, low_order=0.02):
    """乘性平场响应：低阶梯度 + 逐像素 PRNU（相对 rms）。"""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    gx = (xx / max(nx - 1, 1)) - 0.5
    gy = (yy / max(ny - 1, 1)) - 0.5
    low = 1.0 + low_order * (gx + 0.5 * gy)
    prnu = 1.0 + prnu_rms * rng.normal(0.0, 1.0, size=shape)
    return low * prnu


def simulate_physical(
    *,
    F_s_e: float,
    B_e: float,
    rng: np.random.Generator,
    shape=(121, 121),
    center=(60, 60),
    fwhm_px: float = 4.0,
    gain: float = 1.5,
    read_noise_e: float = 5.0,
    dark_e: float = 0.0,
    flat: np.ndarray | None = None,
    sky_grad_e: float = 0.0,
    base_e: np.ndarray | None = None,
    quantize: bool = True,
    profile: np.ndarray | None = None,
    variance_override_e2: float | None = None,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """生成一帧 ADU 图像。返回 (image_adu, P_full, masks)。

    base_e != None 时用真实帧的**结构**（源+背景）作底（真实星场/PSF/背景结构），
    再叠加受控的泊松天光与读出噪声 —— "真实数据作底 + 模拟噪声过程"。

    variance_override_e2 != None 时：天光**均值**仍为 B_e，但逐像素天光方差被强制为
    该值（**非物理**对照臂，用于把"均值"与"方差"两种效应分离）。
    """
    half = int(math.ceil(6.0 * fwhm_px))
    prof = C.moffat4_discrete_profile(fwhm_px, half_px=max(half, 8)) if profile is None else profile
    ph, pw = prof.shape
    P_full = np.zeros(shape, dtype=float)
    y0 = center[0] - ph // 2
    x0 = center[1] - pw // 2
    P_full[y0 : y0 + ph, x0 : x0 + pw] = prof

    if base_e is None:
        mean_e = B_e + dark_e + F_s_e * P_full
    else:
        mean_e = base_e * (F_s_e / max(F_s_e, 1e-300)) if False else base_e + B_e + dark_e
        mean_e = mean_e + F_s_e * P_full  # 注入已知点源
    if sky_grad_e:
        ny, nx = shape
        yy, xx = np.mgrid[0:ny, 0:nx]
        mean_e = mean_e + sky_grad_e * ((xx / max(nx - 1, 1)) - 0.5)
    if flat is not None:
        mean_e = mean_e * flat
    mean_e = np.maximum(mean_e, 0.0)

    if variance_override_e2 is None:
        img_e = rng.poisson(mean_e).astype(float)
    else:
        # 非物理对照臂：均值固定、方差被人为设定（不随均值走）
        img_e = mean_e + rng.normal(0.0, 1.0, size=shape) * math.sqrt(variance_override_e2)
    img_e = img_e + rng.normal(0.0, read_noise_e, size=shape)
    img_adu = img_e / gain
    if quantize:
        img_adu = np.round(img_adu)

    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    rr = np.hypot(yy - center[0], xx - center[1])
    ap_mask = rr < 1.5 * fwhm_px
    ann_mask = (rr > 2.5 * fwhm_px) & (rr <= 4.0 * fwhm_px)
    return img_adu, P_full, ap_mask, ann_mask


# ---------------------------------------------------------------------------
# 2. 被检验的估计量（只用数据，不用真值）
# ---------------------------------------------------------------------------


def estimate_snr(
    image_adu: np.ndarray,
    P_full: np.ndarray,
    ann_mask: np.ndarray,
    *,
    gain: float,
    read_noise_e: float,
    n_iter: int = 3,
    mode: str = "canon_full",
) -> dict:
    """ACSD 等价管线：局部背景中位数 + PSF 加权最优提取 + 逐像素方差模型。

    mode:
      "canon_full"           : sigma_i^2 = sig_sky^2 + (RN/gain)^2 + F_hat*P_i/gain
                               （需要 gain/RN；**FITS 头拿不到 gain 时不可用**）
      "production_gain_free" : sigma_i^2 = sig_sky^2  （gain<=0 分支）
                               => sigma_F = sig_sky/sqrt(sum P^2) = sig_sky*sqrt(A_NEA)
                               **不需要 gain/口径/曝光时间**，且对信号单位线性缩放不变
                               （实测：全部真实生产 run 都走这一支）
    """
    bkg_adu = float(np.median(image_adu[ann_mask]))
    dev = np.abs(image_adu[ann_mask] - bkg_adu)
    sig_sky_adu = MAD_TO_SIGMA * float(np.median(dev))
    sig_sky2 = sig_sky_adu**2
    rn2 = (read_noise_e / gain) ** 2 if (gain > 0 and mode == "canon_full") else 0.0

    P = P_full.ravel()
    d = (image_adu - bkg_adu).ravel()
    F_hat = 0.0
    var_F = float("nan")
    for _ in range(max(n_iter, 1)):
        s2 = sig_sky2 + rn2
        if mode == "canon_full" and gain > 0 and F_hat > 0:
            s2 = s2 + F_hat * P / gain
        s2 = np.maximum(s2, 1e-300)
        w = P / s2
        denom = float(np.sum(P * w))
        F_hat = float(np.sum(w * d) / denom) if denom > 0 else float("nan")
        var_F = 1.0 / denom if denom > 0 else float("nan")
    sigma_F = math.sqrt(var_F) if var_F > 0 else float("nan")
    return {
        "bkg_adu": bkg_adu,
        "sig_sky_adu": sig_sky_adu,
        "F_hat_adu": F_hat,
        "sigma_F_adu": sigma_F,
        "snr": F_hat / sigma_F if sigma_F > 0 else float("nan"),
        "mode": mode,
    }


def estimate_red_definitions(
    image_adu: np.ndarray,
    P_full: np.ndarray,
    ap_mask: np.ndarray,
    ann_mask: np.ndarray,
    *,
    gain: float,
    read_noise_e: float,
) -> dict:
    """在同一**物理**数据上算三个红例定义（必须被否决）。"""
    bkg_adu = float(np.median(image_adu[ann_mask]))
    dev = np.abs(image_adu[ann_mask] - bkg_adu)
    sig_sky_adu = MAD_TO_SIGMA * float(np.median(dev))
    rn2 = (read_noise_e / gain) ** 2 if gain > 0 else 0.0
    n_pix = float(np.sum(ap_mask))
    raw_sum = float(np.sum(image_adu[ap_mask]))            # 未扣背景
    sub_sum = float(np.sum(image_adu[ap_mask] - bkg_adu))  # 已扣背景
    # 逐像素方差（ADU^2）：天光方差 + 读出 + 源散粒（用已扣背景的源估计）
    s2_ap = sig_sky_adu**2 + rn2 + np.maximum(sub_sum, 0.0) * P_full[ap_mask] / gain
    P = P_full.ravel()
    d_raw = image_adu.ravel()
    d_sub = (image_adu - bkg_adu).ravel()
    s2_all = sig_sky_adu**2 + rn2 + np.maximum(
        float(np.sum(P * np.maximum(d_sub, 0.0))), 0.0) * P / gain
    red_a = (sub_sum + n_pix * bkg_adu) / math.sqrt(float(np.sum(s2_ap)))
    red_b = raw_sum / math.sqrt(float(np.sum(s2_ap)))
    red_c = float(np.sum(P * d_raw)) / math.sqrt(float(np.sum(P**2 * s2_all)))
    return {"RED_A_unsub_aperture": red_a, "RED_B_power_ratio_raw": red_b,
            "RED_C_window_unsub": red_c,
            "GREEN_canon_optimal": None}
