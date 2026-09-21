#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN 公共库: 低阶空间乘法增益 m(x,y) 的基函数 / 稳健加权拟合 / 度量.

只读研究脚本, 不改生产代码。

模型 (本库统一口径):
    I_photo(x,y) = k_photo * m(x,y) * I_cal(x,y)
    m 归一化到 "帧足迹内几何均值 = 1" (log 空间均值为 0), 因此与 k_photo 不重复。

拟合观测量:  r_i = log10(F_instr,i / F_syn,i)   (dex)
    在理想情形 r_i = log10(a_k) + log10(m_k(p_i)) + eps_i
    => 对 r 做低阶多项式曲面拟合, 常数项给 k_photo, 去均值后的空间项给 m。

基函数 (像素坐标, 归一化到 [-1,1] 以避免病态):
    order 0: 1
    order 1: x, y
    order 2: x^2, x*y, y^2
    order 3: x^3, x^2*y, x*y^2, y^3
"""
import math
import numpy as np

# ---------------------------------------------------------------- 基函数

def basis_terms(order):
    """返回 (name, (i,j)) 列表, 不含常数项, 按总阶数递增."""
    out = []
    for d in range(1, order + 1):
        for i in range(d, -1, -1):
            j = d - i
            out.append((f"x{i}y{j}", (i, j)))
    return out


def design(x, y, order, x0, sx, y0, sy):
    """归一化坐标下的设计矩阵 [N, 1+nterm].  order=0 -> 仅常数项."""
    xn = (np.asarray(x, float) - x0) / sx
    yn = (np.asarray(y, float) - y0) / sy
    cols = [np.ones_like(xn)]
    for _, (i, j) in basis_terms(order):
        cols.append((xn ** i) * (yn ** j))
    return np.column_stack(cols)


def norm_params(x, y):
    x = np.asarray(x, float); y = np.asarray(y, float)
    x0 = 0.5 * (x.min() + x.max()); y0 = 0.5 * (y.min() + y.max())
    sx = max(0.5 * (x.max() - x.min()), 1e-9)
    sy = max(0.5 * (y.max() - y.min()), 1e-9)
    return x0, sx, y0, sy


def surf_eval(coef, order, x, y, x0, sx, y0, sy):
    return design(x, y, order, x0, sx, y0, sy) @ np.asarray(coef, float)


def surf_ptp(coef, order, x, y, x0, sx, y0, sy, n=64):
    """曲面在给定足迹上的峰峰值 (dex)."""
    gx = np.linspace(x.min(), x.max(), n)
    gy = np.linspace(y.min(), y.max(), n)
    GX, GY = np.meshgrid(gx, gy)
    Z = surf_eval(coef, order, GX.ravel(), GY.ravel(), x0, sx, y0, sy)
    return float(Z.max() - Z.min()), Z.reshape(GX.shape), GX, GY


# ---------------------------------------------------------------- 稳健加权拟合

MAD_SCALE = 0.6744897501960817
TUKEY_C = 4.685


def mad_sigma(v):
    v = np.asarray(v, float)
    v = v[np.isfinite(v)]
    if v.size < 2:
        return 0.0
    med = np.median(v)
    return float(np.median(np.abs(v - med)) / MAD_SCALE)


def tukey_irls_fit(M, z, w0=None, c=TUKEY_C, max_iter=50, tol=1e-6):
    """Tukey biweight IRLS 加权最小二乘.  返回 (coef, sigma, n_inlier, n_iter).

    M: [N, P] 设计矩阵;  z: [N] 观测;  w0: [N] 先验权重 (例如逆方差), 默认 1.
    sigma = MAD(加权残差)/0.6745 (与生产 star_matcher.cpp:616-627 同口径).
    """
    z = np.asarray(z, float)
    M = np.asarray(M, float)
    n, p = M.shape
    if w0 is None:
        w0 = np.ones(n)
    w0 = np.asarray(w0, float)
    keep = np.isfinite(z) & np.isfinite(w0) & (w0 > 0)
    if keep.sum() < p:
        return None, 0.0, 0, 0
    z = z[keep]; Mk = M[keep]; wk = w0[keep]
    # 初值: 加权最小二乘
    W = np.diag(wk)
    try:
        coef = np.linalg.lstsq(Mk.T @ W @ Mk, Mk.T @ (wk * z), rcond=None)[0]
    except np.linalg.LinAlgError:
        return None, 0.0, 0, 0
    n_iter = 0
    for it in range(max_iter):
        n_iter = it + 1
        r = z - Mk @ coef
        s = mad_sigma(r)
        if s <= 0:
            break
        u = r / (c * s)
        wt = np.where(np.abs(u) >= 1.0, 0.0, (1.0 - u * u) ** 2)
        wtot = wk * wt
        if wtot.sum() <= 0:
            break
        new = np.linalg.lstsq(Mk.T @ (wtot[:, None] * Mk), Mk.T @ (wtot * z), rcond=None)[0]
        d = float(np.max(np.abs(new - coef)))
        coef = new
        if d < tol:
            break
    r = z - Mk @ coef
    s = mad_sigma(r)
    u = r / (c * s) if s > 0 else np.zeros_like(r)
    n_in = int(np.sum(np.abs(u) < 1.0))
    return coef, float(s), n_in, n_iter


# ---------------------------------------------------------------- 模型装配

class SpatialGainFit:
    """单帧 r = log10(F_instr/F_syn) 的低阶空间拟合结果.

    约定 (与生产 k_photo 兼容的关键):
      surf(p)     = 拟合出的 r 曲面 (dex)
      mean_surf   = surf 在**参与拟合的星集合**上的加权均值
      k_photo     = 10^(-mean_surf)              <-- order=0 时严格退化为现有
                                                     标量估计 10^(-location)
      m(p)        = 10^(-(surf(p) - mean_surf))  <-- 星集合上 log 均值 = 0
                                                     (几何均值 1, 与 k_photo 不重复)
      gain(p)     = k_photo * m(p) = 10^(-surf(p)) <-- 实际施加到像素上的总因子
    """

    def __init__(self, order, coef, norm, sigma, n_used, n_inlier, n_iter, x, y, w=None):
        self.order = int(order)
        self.coef = np.asarray(coef, float)
        self.norm = norm
        self.sigma = float(sigma)
        self.n_used = int(n_used)
        self.n_inlier = int(n_inlier)
        self.n_iter = int(n_iter)
        self.x = np.asarray(x, float)
        self.y = np.asarray(y, float)
        sv = surf_eval(self.coef, self.order, self.x, self.y, *norm)
        if w is None:
            self.mean_surf = float(np.mean(sv))
        else:
            w = np.asarray(w, float)
            self.mean_surf = float(np.sum(w * sv) / np.sum(w))
        self.c0 = float(self.coef[0])
        self.k_photo = float(10.0 ** (-self.mean_surf))

    def log_m(self, x, y):
        return -(surf_eval(self.coef, self.order, x, y, *self.norm) - self.mean_surf)

    def m(self, x, y):
        return np.power(10.0, self.log_m(x, y))

    def log_gain(self, x, y):
        return -surf_eval(self.coef, self.order, x, y, *self.norm)

    def gain(self, x, y):
        return np.power(10.0, self.log_gain(x, y))

    def m_ptp_dex(self):
        p, _, _, _ = surf_ptp(self.coef, self.order, self.x, self.y, *self.norm)
        return p

    def m_ptp_pct(self):
        return float(10.0 ** self.m_ptp_dex() - 1.0) * 100.0


def fit_spatial_gain(x, y, r, sigma_r=None, order=1, c=TUKEY_C):
    """对 r_i=log10(F_instr/F_syn) 做低阶曲面 Tukey-IRLS 加权拟合.

    sigma_r: 逐星 r 的 1-sigma 不确定度 (dex); 权重 = 1/sigma_r^2 (逆方差).
             None -> 等权 (仅 Tukey).
    """
    x = np.asarray(x, float); y = np.asarray(y, float); r = np.asarray(r, float)
    ok = np.isfinite(x) & np.isfinite(y) & np.isfinite(r)
    x, y, r = x[ok], y[ok], r[ok]
    if sigma_r is None:
        w0 = np.ones_like(r)
    else:
        sr = np.asarray(sigma_r, float)[ok]
        sr = np.where(np.isfinite(sr) & (sr > 0), sr, np.nan)
        med = np.nanmedian(sr) if np.isfinite(sr).any() else 0.0
        sr = np.where(np.isfinite(sr), sr, (med if med > 0 else 1.0))
        w0 = 1.0 / (sr * sr)
    if len(r) == 0:
        return None
    norm = norm_params(x, y)
    M = design(x, y, order, *norm)
    coef, sigma, n_in, n_iter = tukey_irls_fit(M, r, w0=w0, c=c)
    if coef is None:
        return None
    return SpatialGainFit(order, coef, norm, sigma, len(r), n_in, n_iter, x, y, w=w0)


# ---------------------------------------------------------------- 真值比较

def align_shape(logm_a, logm_b):
    """去掉两者共同的常数 (gauge): 返回各自减去 (a-b) 的加权均值后的值."""
    d = float(np.mean(np.asarray(logm_a) - np.asarray(logm_b)))
    return np.asarray(logm_a) - d, np.asarray(logm_b)


def shape_err_pair(logm_fit, logm_true):
    """同 shape_error_pct, 但输入已是 log10 m 值数组 (在给定点上)."""
    return shape_error_pct(logm_fit, logm_true)


def shape_error_pct(logm_fit, logm_true):
    """形状恢复误差 (去掉 gauge 常数后), 以百分比表示 (dex -> 相对增益 %)."""
    a, b = align_shape(logm_fit, logm_true)
    diff = a - b
    rms = float(np.sqrt(np.mean(diff ** 2)))
    ptp = float(diff.max() - diff.min())
    return dict(rms_dex=rms, ptp_dex=ptp,
                rms_pct=float((10 ** rms - 1) * 100),
                ptp_pct=float((10 ** ptp - 1) * 100),
                bias_dex=float(np.mean(np.asarray(logm_fit) - np.asarray(logm_true))))


def robust_ptp_pct(vals_dex):
    """一组 log10 比值的稳健峰峰值 -> 百分比 (用 2.5/97.5 分位避免单点噪声)."""
    v = np.asarray(vals_dex, float)
    v = v[np.isfinite(v)]
    if v.size < 8:
        return float("nan")
    lo, hi = np.percentile(v, [2.5, 97.5])
    return float((10 ** (hi - lo) - 1) * 100)
