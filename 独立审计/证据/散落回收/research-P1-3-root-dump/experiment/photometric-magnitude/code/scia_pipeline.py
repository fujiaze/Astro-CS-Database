# -*- coding: utf-8 -*-
"""SCI-A · 流水线级公共函数：仿真帧装载、引导/盲检测、标定、apply photometry。"""
from __future__ import annotations

import os

import numpy as np

import scia_common as sc
import scia_sim as ss
from scia_calib import (aperture_flux, budget_from_frame, calibrate,
                        guided_photometry, psf_vs_aperture_systematics)

SIMDIR = os.path.join(sc.RUN, "sim")


def load_frame(tag):
    p = os.path.join(SIMDIR, f"frame_{tag}.npz")
    z = np.load(p)
    return {k: z[k] for k in z.files}


def frame_wcs(meta):
    return sc.make_wcs(meta["wcs"]["crval"], meta["wcs"]["crpix"], meta["wcs"]["cd"],
                       shape=tuple(meta["shape"]))


def instrument_from(meta):
    return ss.Instrument(**meta["instrument"])


def sample_selection(flux, chi2, sat_mask, mag_inj, fwhm, flux_err=None,
                     chi2_max=3.0, mag_range=(13.0, 19.6)):
    """有效域与质量选择（与 docs/science/PHOTOMETRY.md §4 一致的语义）：
    非饱和、拟合收敛（χ²_red 上限）、有限正值通量、星等在注入范围内。"""
    ok = np.isfinite(flux) & (flux > 0) & (~sat_mask)
    ok &= np.isfinite(chi2) & (chi2 < chi2_max)
    ok &= (mag_inj >= mag_range[0]) & (mag_inj <= mag_range[1])
    if flux_err is not None:
        ok &= np.isfinite(flux_err) & (flux_err > 0)
    return ok


def measure_frame(frame, meta, inst, wcs, m_degree=0, ap_r=10.0):
    """引导测光 + 帧内系统项测量（PSF 域 vs 独立孔径）+ 逐项预算。"""
    img = frame["img"]
    mu = frame["mu"]
    var = (np.clip(mu, 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    g = guided_photometry(img, wcs, frame["ra"], frame["dec"], inst, var_map=var)
    idx = g["idx"]
    sat = np.zeros(idx.size, bool)
    # 饱和判据：拟合 stamp 内出现饱和像素（生产 psf_status/SATURATED 语义）
    H, W = img.shape
    xr = np.clip(np.round(g["x"]).astype(int), 0, W - 1)
    yr = np.clip(np.round(g["y"]).astype(int), 0, H - 1)
    box = int(np.ceil(3 * inst.fwhm_px))
    for k in range(idx.size):
        x0, x1 = max(xr[k] - box, 0), min(xr[k] + box, W - 1)
        y0, y1 = max(yr[k] - box, 0), min(yr[k] + box, H - 1)
        sat[k] = bool(np.any(img[y0:y1 + 1, x0:x1 + 1] >= inst.saturation_adu))
    f_ap = aperture_flux(img, g["x"], g["y"], r_ap=ap_r, r_in=ap_r + 2, r_out=ap_r + 8)
    sysres = psf_vs_aperture_systematics(g["flux"], f_ap, g["flux_err"])
    out = dict(g=g, idx=idx, sat=sat, f_ap=f_ap, sysres=sysres)
    return out


def build_budget(flux_sel, inst, sigma_pix_e, sigma_psfsys, sigma_color, sigma_gaia,
                 sigma_flat, structure_factor, n, tag):
    b = budget_from_frame(flux_sel, inst, sigma_pix_e, sigma_psfsys=sigma_psfsys,
                          sigma_color=sigma_color, sigma_gaia=sigma_gaia,
                          sigma_flat=sigma_flat, structure_factor=structure_factor,
                          tag=tag)
    b.n = int(n)
    b.rho_lo, b.rho_hi = sc.sampling_rho(int(n))
    return b


def apply_photometry(img, k_photo, m_map):
    """apply photometry：I_photo = k_photo · m(x,y) · I_cal（设计 §4.2）。"""
    return k_photo * m_map * img


def fit_low_order_gain(x, y, dmag, degree=2, sigma=None):
    """用星点残差拟合低阶空间乘法增益 m(x,y)（星等域 → 乘性因子）。"""
    A, names = _design(x, y, degree)
    w = None if sigma is None else 1.0 / np.maximum(sigma, 1e-6)
    if w is not None:
        Aw = A * w[:, None]; dw = dmag * w
    else:
        Aw, dw = A, dmag
    coef, *_ = np.linalg.lstsq(Aw, dw, rcond=None)
    return coef, names, A


def eval_low_order_gain(coef, x, y, degree=2):
    A, _ = _design(x, y, degree)
    return A @ coef


def _design(x, y, degree):
    x = np.asarray(x, float); y = np.asarray(y, float)
    x0, x1 = np.min(x), np.max(x); y0, y1 = np.min(y), np.max(y)
    xn = (x - 0.5 * (x0 + x1)) / max(0.5 * (x1 - x0), 1e-9)
    yn = (y - 0.5 * (y0 + y1)) / max(0.5 * (y1 - y0), 1e-9)
    cols, names = [], []
    for i in range(degree + 1):
        for j in range(degree + 1 - i):
            cols.append((xn ** i) * (yn ** j)); names.append(f"x{i}y{j}")
    return np.asarray(cols).T, names
