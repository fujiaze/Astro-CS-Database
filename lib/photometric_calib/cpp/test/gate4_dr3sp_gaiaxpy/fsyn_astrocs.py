# -*- coding: utf-8 -*-
"""
Gate 4: AstroCS DR3SP 积分参考实现 (numpy 移植)

与生产 C++ lib/photometric_calib/cpp/src/spectrum_integrator.cpp 数值等价
(Akima 插值 + Simpson 1/3 复合积分 + lambda 加权 + G 星等归一化):
    F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4·mag_g)

由 fsyn_export.cpp (生产 C++) 交叉验证数值一致 (见 gate4_gaiaxpy_compare.py)。
"""

import numpy as np


def akima_interpolate(x_src, y_src, x_dst, fill=0.0):
    """Akima 子样条插值 (与 C++ akima_interpolate 逐点等价)."""
    x_src = np.asarray(x_src, dtype=float)
    y_src = np.asarray(y_src, dtype=float)
    x_dst = np.asarray(x_dst, dtype=float)
    n = len(x_src)
    y_dst = np.full_like(x_dst, fill, dtype=float)
    if n < 2:
        return y_dst
    slope = np.diff(y_src) / np.diff(x_src)
    ext_m = np.empty(n + 2)
    ext_m[0] = 3.0 * slope[0] - 2.0 * slope[1]
    ext_m[1] = 2.0 * slope[0] - slope[1]
    ext_m[2:n + 1] = slope
    ext_m[n + 1] = 2.0 * slope[n - 2] - slope[n - 3]
    w1 = np.abs(ext_m[3:] - ext_m[2:n + 1])
    w2 = np.abs(ext_m[1:n] - ext_m[0:n - 1])
    t = np.where(w1 + w2 == 0.0,
                 0.5 * (ext_m[1:n] + ext_m[2:n + 1]),
                 (w1 * ext_m[1:n] + w2 * ext_m[2:n + 1]) / (w1 + w2))
    lo = np.searchsorted(x_src, x_dst, side="right") - 1
    lo = np.clip(lo, 0, n - 2)
    x0, x1 = x_src[lo], x_src[lo + 1]
    y0, y1 = y_src[lo], y_src[lo + 1]
    dx = x1 - x0
    s = (x_dst - x0) / dx
    h00 = (2.0 * s - 3.0) * s * s + 1.0
    h10 = ((s - 2.0) * s + 1.0) * s
    h01 = (-2.0 * s + 3.0) * s * s
    h11 = (s - 1.0) * s * s
    inside = (x_dst >= x_src[0]) & (x_dst <= x_src[-1])
    y_dst[inside] = (h00 * y0 + h10 * dx * t[lo] +
                     h01 * y1 + h11 * dx * t[lo + 1])[inside]
    return y_dst


def simpson_integrate(x, y):
    """Simpson 1/3 复合积分 (奇数区间末尾 3 点用 Simpson 3/8, 与 C++ 一致)."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    n_pts = len(x)
    if n_pts < 2:
        return 0.0
    h = (x[-1] - x[0]) / (n_pts - 1)
    n_int = n_pts - 1
    if n_int % 2 == 0:
        sum_ = y[0] + y[-1]
        i = np.arange(1, n_pts - 1)
        sum_ += np.sum(np.where(i % 2 == 1, 4.0, 2.0) * y[i])
        return sum_ * h / 3.0
    if n_int >= 3:
        n_13 = n_int - 3
        sum_ = y[0] + y[n_13]
        i = np.arange(1, n_13)
        sum_ += np.sum(np.where(i % 2 == 1, 4.0, 2.0) * y[i])
        total = sum_ * h / 3.0
        total += (y[n_13] + 3.0 * y[n_13 + 1] + 3.0 * y[n_13 + 2] + y[-1]) * 3.0 * h / 8.0
        return total
    return 0.5 * h * (y[0] + y[1])


def compute_f_syn(spectrum, spectrum_wl, filter_wl, filter_trans,
                  qe_wl=None, qe_trans=None, mag_g=None):
    """F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4·mag_g).
    spectrum: uint8 或 float 数组 [n] (与 spectrum_wl 同网格)
    filter_wl/trans: 滤光片曲线 (可非等间距)
    qe_wl/trans: CCD QE (None -> Q=1.0)
    mag_g: G 星等 (None -> 不做星等归一化, 用于相对比较)
    """
    spectrum = np.asarray(spectrum, dtype=float)
    spectrum_wl = np.asarray(spectrum_wl, dtype=float)
    filter_wl = np.asarray(filter_wl, dtype=float)
    filter_trans = np.asarray(filter_trans, dtype=float)
    # 排序 + 去重 (与 C++ prepare_curve 一致)
    order = np.argsort(filter_wl, kind="stable")
    fwl, ftr = filter_wl[order], filter_trans[order]
    keep = np.concatenate(([True], np.diff(fwl) > 0.0))
    fwl, ftr = fwl[keep], ftr[keep]
    t_resampled = akima_interpolate(fwl, ftr, spectrum_wl, 0.0)
    q_resampled = np.ones_like(spectrum_wl)
    if qe_wl is not None and qe_trans is not None:
        qwl = np.asarray(qe_wl, dtype=float)
        qtr = np.asarray(qe_trans, dtype=float)
        order = np.argsort(qwl, kind="stable")
        qwl, qtr = qwl[order], qtr[order]
        keep = np.concatenate(([True], np.diff(qwl) > 0.0))
        qwl, qtr = qwl[keep], qtr[keep]
        q_resampled = akima_interpolate(qwl, qtr, spectrum_wl, 0.0)
    integrand = spectrum * t_resampled * q_resampled * spectrum_wl
    f_syn = simpson_integrate(spectrum_wl, integrand)
    if mag_g is not None:
        f_syn *= 10.0 ** (-0.4 * mag_g)
    return f_syn


def relative_color_mag(f_syn_ref, f_syn_band):
    """m_band - m_ref = -2.5·log10(F_band/F_ref) (ref 归一化)."""
    with np.errstate(divide="ignore", invalid="ignore"):
        return -2.5 * np.log10(np.where(f_syn_band > 0, f_syn_band / f_syn_ref, np.nan))


def synthetic_band_flux(spectrum, spectrum_wl, filter_wl, filter_trans):
    """光子加权平均通量 F_band = ∫S·T·λ dλ / ∫T·λ dλ (合成测光标准约定,
    与 GaiaXPy synthetic photometry 的可比量; 星等由零点和归一化决定)."""
    spectrum = np.asarray(spectrum, dtype=float)
    spectrum_wl = np.asarray(spectrum_wl, dtype=float)
    filter_wl = np.asarray(filter_wl, dtype=float)
    filter_trans = np.asarray(filter_trans, dtype=float)
    order = np.argsort(filter_wl, kind="stable")
    fwl, ftr = filter_wl[order], filter_trans[order]
    keep = np.concatenate(([True], np.diff(fwl) > 0.0))
    fwl, ftr = fwl[keep], ftr[keep]
    t = akima_interpolate(fwl, ftr, spectrum_wl, 0.0)
    num = simpson_integrate(spectrum_wl, spectrum * t * spectrum_wl)
    den = simpson_integrate(spectrum_wl, t * spectrum_wl)
    return num / den if den > 0 else np.nan
