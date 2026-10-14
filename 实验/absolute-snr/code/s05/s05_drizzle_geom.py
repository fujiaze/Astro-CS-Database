#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05：可分离方形几何下的 Drizzle 线性算子与输出协方差（独立 Oracle）。

为什么用可分离方形模型
----------------------
生产 Drizzle 的权重是「drop 与输出叶的球面交叠面积 / drop 面积」，
输出估计量 y_p = sum_j a_jp x_j / sum_j a_jp（drizzle_engine.cpp:1402/1424 同族）。
在局部切平面上，输出叶是边长 L（arcsec）的方格、drop 是边长 f*s 的方格（s = 源像元
角尺度，f = pixfrac），二维交叠面积 = 两个一维交叠长度的乘积 ⇒
    N_p   = Ux[px]*Uy[py]/(f*s)^2
    C_pq  = v * (Mx[px,qx]/Ux[px]^2) ⊗ (My[py,qy]/Uy[py]^2)
    Mx[a,b] = sum_j Ux[j,a]*Ux[j,b]
这样对任意尺度比 rho = L/s 都能**精确**（在可分离方格模型内）给出输出协方差，
不受「逐 drop 枚举」在 rho≈430 时不可行的限制。该模型相对真实 HEALPix 叶的偏差
由生产探针交叉核对（见 s05_kcorr_units.py 的 crosscheck 一节）。

单位：长度用 arcsec，v 用 ADU^2，面积权重中的 (f*s)^2 因子在归一化中相消。
"""
from __future__ import annotations

import math
from typing import Tuple

import numpy as np


def overlap_1d(n_leaf: int, leaf_len: float, drop_len: float, drop_step: float,
               phase: float = 0.0, margin_drops: int = 2) -> np.ndarray:
    """返回 U[j,k]：第 j 个 drop（长度 drop_len，中心 (j+0.5)*drop_step+phase）与
    第 k 个叶 [k*leaf_len,(k+1)*leaf_len] 的一维交叠长度。"""
    j_lo = int(math.floor((-margin_drops * drop_len) / drop_step))
    j_hi = int(math.ceil((n_leaf * leaf_len + margin_drops * drop_len) / drop_step))
    js = np.arange(j_lo, j_hi + 1, dtype=float)
    centers = (js + 0.5) * drop_step + phase
    lo = centers - 0.5 * drop_len
    hi = centers + 0.5 * drop_len
    k = np.arange(n_leaf, dtype=float)
    klo = k * leaf_len
    khi = klo + leaf_len
    left = np.maximum(lo[:, None], klo[None, :])
    right = np.minimum(hi[:, None], khi[None, :])
    U = np.clip(right - left, 0.0, None)
    return U


def drizzle_covariance_1d_axis(n_leaf: int, leaf_len: float, drop_len: float,
                               drop_step: float, phase: float = 0.0) -> Tuple[np.ndarray, np.ndarray]:
    """返回 (A, keep)：A = Mx/Ux^2（仅保留有覆盖的叶，叶×叶），keep = 被保留的叶下标。

    生产 drizzle 对无覆盖的输出像素给 NaN/无效；控制点估计量只用 **retained**
    的像素 ⇒ 这里把无覆盖叶剔除，N_retained 即剔除后的数量。
    """
    U = overlap_1d(n_leaf, leaf_len, drop_len, drop_step, phase=phase)
    Ux = U.sum(axis=0)
    keep = np.where(Ux > 0)[0]
    if keep.size == 0:
        raise ValueError("窗口内没有任何被覆盖的叶")
    Uk = U[:, keep]
    Uxk = Ux[keep]
    Mx = Uk.T @ Uk
    A = Mx / (Uxk[:, None] * Uxk[None, :])
    return A, keep


def drizzle_covariance_axes(n_leaf_side: int, rho: float, pixfrac: float,
                            phase: Tuple[float, float] = (0.0, 0.0),
                            leaf_len: float = 1.0) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """返回 (Ax, Ay, kx, ky)：可分离模型的两个一维协方差因子与保留叶下标。"""
    s = leaf_len / rho
    drop_len = pixfrac * s
    Ax, kx = drizzle_covariance_1d_axis(n_leaf_side, leaf_len, drop_len, s, phase=phase[0])
    Ay, ky = drizzle_covariance_1d_axis(n_leaf_side, leaf_len, drop_len, s, phase=phase[1])
    return Ax, Ay, kx, ky


def sample_separable_field(Ax: np.ndarray, Ay: np.ndarray, n_mc: int,
                           rng: np.random.Generator) -> np.ndarray:
    """对 C = Ax ⊗ Ay 精确采样：Y = Lx Z Ly^T（Lx Lx^T = Ax, Ly Ly^T = Ay）。

    返回 (n_mc, nx, ny)。
    """
    Lx = np.linalg.cholesky(Ax + 1e-300 * np.eye(Ax.shape[0]))
    Ly = np.linalg.cholesky(Ay + 1e-300 * np.eye(Ay.shape[0]))
    Z = rng.standard_normal((n_mc, Ax.shape[0], Ay.shape[0]))
    T = np.tensordot(Z, Lx, axes=([1], [1]))          # (n_mc, ny, nx)
    Y = np.tensordot(T, Ly, axes=([1], [1]))          # (n_mc, nx, ny)
    return Y


def drizzle_covariance_2d(n_leaf_side: int, rho: float, pixfrac: float,
                          v: float = 1.0, phase: Tuple[float, float] = (0.0, 0.0),
                          leaf_len: float = 1.0) -> Tuple[np.ndarray, np.ndarray]:
    """可分离方格模型的输出协方差。

    返回 (C, idx)：C 为 n_retained × n_retained（仅被覆盖的叶，按行主序），
    idx[p] = (py, px) 原始叶下标 ⇒ N_retained = C.shape[0]。
    """
    s = leaf_len / rho
    drop_len = pixfrac * s
    Ax, kx = drizzle_covariance_1d_axis(n_leaf_side, leaf_len, drop_len, s, phase=phase[0])
    Ay, ky = drizzle_covariance_1d_axis(n_leaf_side, leaf_len, drop_len, s, phase=phase[1])
    A2 = v * np.kron(Ay, Ax)
    A2 = 0.5 * (A2 + A2.T)
    idx = np.array([(int(py), int(px)) for py in ky for px in kx], dtype=int)
    return A2, idx


def drizzle_weights_2d(n_leaf_side: int, rho: float, pixfrac: float,
                       leaf_len: float = 1.0) -> Tuple[np.ndarray, np.ndarray]:
    """显式给出稀疏权重（供小 rho 的 drop 空间模拟用）。

    返回 (W, drop_w)：W[j, p] = a_jp/a_drop，drop_w[j] = a_drop（相对单位）。
    """
    s = leaf_len / rho
    drop_len = pixfrac * s
    Ux = overlap_1d(n_leaf_side, leaf_len, drop_len, s)
    Uy = overlap_1d(n_leaf_side, leaf_len, drop_len, s)
    ny, nx = Ux.shape
    my, mx = Uy.shape
    W = np.zeros((nx * my, n_leaf_side * n_leaf_side), dtype=float)
    area = drop_len * drop_len
    for jx in range(nx):
        for jy in range(my):
            row = Ux[jx, :][None, :] * Uy[jy, :][:, None]   # [leaf_y, leaf_x]
            W[jx * my + jy, :] = (row / area).reshape(-1)
    return W, np.full(nx * my, area)


def psf_vector_on_grid(side: int, fwhm_cells: float, beta: float = 4.0) -> np.ndarray:
    """在 side×side 的叶网格上放一个归一 Moffat（sum = 1），用于线性统计量 a。"""
    yy, xx = np.mgrid[0:side, 0:side].astype(float)
    c = (side - 1) / 2.0
    alpha = fwhm_cells / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
    r2 = (xx - c) ** 2 + (yy - c) ** 2
    p = (1.0 + r2 / (alpha * alpha)) ** (-beta)
    p = p / p.sum()
    return p.reshape(-1)


def median_variance_mc(C: np.ndarray, n_mc: int, rng: np.random.Generator,
                       clip_sigma: float | None = None,
                       bfrac: float = 0.0) -> dict:
    """对协方差 C 的零均值高斯向量，MC 求 patch median 的方差与生产口径 k_corr。

    k_corr = Var(median) / [ (pi/2) * sigma_bg^2 / N_retained ]，
    sigma_bg 分别取 (a) 真实单点散度 sqrt(mean(diag C))，(b) 生产口径的逐实现 MAD。
    bfrac = 亮端污染裁剪比例（去掉最高的 ceil(bfrac*N) 个样本，复现 production
    background_contamination_sigma 裁剪后的非对称保留集）。
    clip_sigma 非空时再对双侧做 |x|<clip_sigma*sigma 的对称裁剪。
    """
    n = C.shape[0]
    L = np.linalg.cholesky(C + 1e-300 * np.eye(n))
    x = rng.standard_normal((n_mc, n)) @ L.T
    if clip_sigma is not None:
        sd = math.sqrt(float(np.mean(np.diag(C))))
        keep = np.abs(x) < clip_sigma * sd
        n_ret = int(np.sum(keep[0]))
        x = np.where(keep, x, np.nan)
        med = np.nanmedian(x, axis=1)
        mad = 1.482602218505602 * np.nanmedian(np.abs(x - med[:, None]), axis=1)
    else:
        n_ret = n
        med = np.median(x, axis=1)
        mad = 1.482602218505602 * np.median(np.abs(x - med[:, None]), axis=1)
    if bfrac > 0.0:
        k_rm = int(math.ceil(bfrac * n))
        order = np.argsort(x, axis=1)
        idx = order[:, : n - k_rm]
        xr = np.take_along_axis(x, idx, axis=1)
        n_ret = xr.shape[1]
        med = np.median(xr, axis=1)
        mad = 1.482602218505602 * np.median(np.abs(xr - med[:, None]), axis=1)
    truth_sd = math.sqrt(float(np.mean(np.diag(C))))
    denom_true = 1.5707963267948966 * truth_sd * truth_sd / n_ret
    denom_mad = 1.5707963267948966 * float(np.mean(mad * mad)) / n_ret
    sd_median = float(np.std(med, ddof=1))
    return {
        "n_retained": int(n_ret),
        "sd_of_median": sd_median,
        "sigma_bg_truth": truth_sd,
        "sigma_bg_mad_mean": float(np.mean(mad)),
        "k_corr_true_sigma": sd_median * sd_median / denom_true,
        "k_corr_production_mad": sd_median * sd_median / denom_mad,
        "mc_se_of_sd_rel": float(1.0 / math.sqrt(2.0 * max(n_mc - 1, 1))),
        "n_mc": int(n_mc),
    }


def corr_length_cells(C: np.ndarray, side: int) -> float:
    """沿一轴的近邻相关系数 ⇒ 相关长度（以叶为单位，取 rho_eff 一阶近似）。"""
    r = C[0, :].copy()
    r /= r[0]
    if r.size < 2:
        return 0.0
    r1 = r[1]
    if r1 <= 0:
        return 0.0
    return r1 / (1.0 - r1) if r1 < 1 else float("inf")
