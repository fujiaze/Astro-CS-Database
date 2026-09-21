# -*- coding: utf-8 -*-
"""SCI-A · 完整物理前向仿真（电子域 Poisson → 读出 Gaussian → 增益/饱和/量化 → 平场/天空梯度）

严格按 ASTROCS_DESIGN.md §12.2 / ACCEPTANCE_SPEC.md §3 的三类数据第 1 类：
  源、天光、暗流在电子域做 Poisson；读出噪声在电子域做 Gaussian；
  电子→ADU 经过增益、饱和与量化；加入平场乘性空间响应 m(x,y) 与天空梯度。
禁止用"算术加常数天光"代替散粒噪声物理过程 —— 本模块天光一律进 Poisson。

与生产实现无关：本模块不调用 lib/** 的任何科学代码，独立 numpy 实现。
"""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from scia_common import moffat_profile, rng, render_star


# --------------------------------------------------------------------------
# 仪器配置（"AstroCS 系统"，参数锚定 run/RELEASE-02/parallel/out/ptc2.json 实测）
# --------------------------------------------------------------------------
@dataclass
class Instrument:
    gain: float = 1.333          # e-/ADU（实测 PTC 中位）
    read_noise: float = 8.59     # e- rms
    dark_rate: float = 0.002     # e-/s/px（-20°C 量级）
    saturation_adu: float = 65535.0
    bit_depth: int = 16
    fwhm_px: float = 2.2         # 拟合用 PSF FWHM
    beta_fit: float = 4.0        # 拟合用 Moffat beta（生产族）
    beta_inject: float = 3.5     # 注入用 Moffat beta（与拟合族不同 ⇒ 真实方法系统误差）
    fwhm_inject: float = 2.2
    ellipticity: float = 0.08    # 注入 PSF 椭率（拟合用圆 PSF ⇒ 残余系统误差）


@dataclass
class Truth:
    inject_scale: float = 1.0            # F_instr[ADU] = inject_scale · m(x,y) · F_syn
                                         # 标定因子 k_photo = 1/inject_scale = 10^(-location)
    m_coeffs: np.ndarray = None          # 低阶空间增益多项式系数（二维，deg）
    m_degree: int = 2
    sky_adu: float = 199.6               # 天光中位电平 [ADU/px]
    sky_grad: tuple = (0.0, 0.0)         # 天光梯度 [ADU/px per px]
    exp_time: float = 300.0
    ref_err_mag: float = 0.002           # 参考侧（XP 合成通量）注入误差 [mag]
    flat_pix_sigma: float = 0.0032       # 平场逐像素相对散度
    passband_mismatch_qe: str = None     # 注入用 QE 曲线名（与拟合所用不同 ⇒ 颜色项）
    seed_tag: str = "sim"

    def m_map(self, H, W):
        """低阶空间乘法增益 m(x,y)，中位归一到 1。"""
        yy, xx = np.mgrid[0:H, 0:W]
        xn = (xx - (W - 1) / 2.0) / max(W - 1, 1) * 2.0
        yn = (yy - (H - 1) / 2.0) / max(H - 1, 1) * 2.0
        c = self.m_coeffs
        if c is None:
            return np.ones((H, W))
        d = self.m_degree
        m = np.zeros((H, W))
        k = 0
        for i in range(d + 1):
            for j in range(d + 1 - i):
                m += c[k] * (xn ** i) * (yn ** j)
                k += 1
        return m / np.median(m)


def poly_terms(H, W, degree):
    yy, xx = np.mgrid[0:H, 0:W]
    xn = (xx - (W - 1) / 2.0) / max(W - 1, 1) * 2.0
    yn = (yy - (H - 1) / 2.0) / max(H - 1, 1) * 2.0
    out, names = [], []
    for i in range(degree + 1):
        for j in range(degree + 1 - i):
            out.append((xn ** i) * (yn ** j))
            names.append(f"x{i}y{j}")
    return np.asarray(out), names


# --------------------------------------------------------------------------
# 星场渲染
# --------------------------------------------------------------------------
def render_field(shape, xs, ys, fluxes, inst: Instrument, psf_amp=None):
    """把星场渲染成电子率图 [e-/s]。psf_amp 为可选的逐星幅度扰动（形状失配用）。"""
    img = np.zeros(shape, float)
    H, W = shape
    for k in range(len(xs)):
        if not (0 <= xs[k] < W and 0 <= ys[k] < H):
            continue
        f = fluxes[k]
        if psf_amp is not None:
            f = f * psf_amp[k]
        if inst.ellipticity > 0:
            f = f
        render_star(img, xs[k], ys[k], f, inst.fwhm_inject, inst.beta_inject)
    return img


def render_field_elliptical(shape, xs, ys, fluxes, inst: Instrument, pa=None):
    """带椭率的注入 PSF（拟合器用圆 PSF ⇒ 制造真实的方法系统误差）。"""
    H, W = shape
    img = np.zeros(shape, float)
    e = inst.ellipticity
    q = 1.0 - e
    sub = 5
    r = int(np.ceil(4.0 * inst.fwhm_inject)) + 1
    off = (np.arange(sub) + 0.5) / sub - 0.5
    for k in range(len(xs)):
        x0, y0 = xs[k], ys[k]
        if not (-r <= x0 < W + r and -r <= y0 < H + r):
            continue
        ix0, ix1 = max(int(np.floor(x0 - r)), 0), min(int(np.ceil(x0 + r)), W - 1)
        iy0, iy1 = max(int(np.floor(y0 - r)), 0), min(int(np.ceil(y0 + r)), H - 1)
        if ix1 < ix0 or iy1 < iy0:
            continue
        gx = np.arange(ix0, ix1 + 1, dtype=float)
        gy = np.arange(iy0, iy1 + 1, dtype=float)
        th = 0.0 if pa is None else pa[k]
        ct, st = np.cos(th), np.sin(th)
        acc = np.zeros((gy.size, gx.size))
        for oy in off:
            for ox in off:
                dx = (gx[None, :] + ox) - x0
                dy = (gy[:, None] + oy) - y0
                u = (dx * ct + dy * st) / q
                v = (-dx * st + dy * ct)
                acc += moffat_profile(u, v, inst.fwhm_inject, inst.beta_inject) / q
        img[iy0:iy1 + 1, ix0:ix1 + 1] += fluxes[k] * acc / (sub * sub)
    return img


# --------------------------------------------------------------------------
# 前向链
# --------------------------------------------------------------------------
def forward(truth: Truth, inst: Instrument, shape, xs, ys, fluxes_e, sky_map_adu,
            tag="frame", saturate=True, quantize=True, add_noise=True):
    """完整前向：返回 (img_adu float, truth_dict)。

    fluxes_e[k] : 第 k 颗星在**整个曝光内**沉积的电子数（总电子，不是速率）
    sky_map_adu : 天光**总电平**图 [ADU/px]（含梯度与真实星云结构；乘增益转电子域后进 Poisson）
    dark_e      : dark_rate · t_exp（总电子）
    平场 m(x,y) 作为**乘性像素响应**作用在光子域（物理正确），并对源与天光同时生效。
    """
    H, W = shape
    r = rng(f"{truth.seed_tag}:{tag}")
    m = truth.m_map(H, W)
    if truth.flat_pix_sigma > 0:
        m = m * (1.0 + r.normal(0.0, truth.flat_pix_sigma, size=(H, W)))
    sky_e = sky_map_adu * inst.gain
    dark_e = inst.dark_rate * truth.exp_time
    src_e = render_field_elliptical(shape, xs, ys, fluxes_e, inst) \
        if inst.ellipticity > 0 else render_field(shape, xs, ys, fluxes_e, inst)
    mu = (src_e + sky_e + dark_e) * m
    if add_noise:
        ne = r.poisson(np.clip(mu, 0.0, None)).astype(float)
        ne += r.normal(0.0, inst.read_noise, size=shape)
    else:
        ne = mu.copy()
    adu = ne / inst.gain
    if saturate:
        adu = np.clip(adu, 0.0, inst.saturation_adu)
    if quantize:
        adu = np.rint(adu)
    truth_info = dict(inject_scale=truth.inject_scale, m=m, sky_map_adu=sky_map_adu,
                      src_e=src_e, mu=mu, exp_time=truth.exp_time,
                      fluxes_e=np.asarray(fluxes_e, float))
    return adu.astype(np.float64), truth_info


def stamp_photometry(xs, ys, fluxes_adu, truth: Truth, inst: Instrument, tag="stamp",
                     half=16, fit=True):
    """逐星 stamp 级完整前向 + PSF 拟合（解析合成的"多噪声组"）。

    每颗星在 (x,y) 处生成 half×2+1 的 stamp：
      源 PSF 电子 + 天光率 + 暗流率 → 乘平场 m(x,y) → 电子域 Poisson
      → 读出噪声 Gaussian → 增益/饱和/量化（ADU）
    再做 (x,y,amp) 三参数 PSF 最小二乘 → 返回实测通量 [ADU]。
    全部噪声为 Poisson/Gaussian 物理过程；天光进 Poisson，不用算术常数代替。
    """
    from scia_common import fit_psf
    r = rng(f"{truth.seed_tag}:{tag}")
    n = len(xs)
    F_out = np.full(n, np.nan); ok = np.zeros(n, bool)
    dark_e = inst.dark_rate * truth.exp_time
    for k in range(n):
        x0, y0 = float(xs[k]), float(ys[k])
        ix0, iy0 = int(np.floor(x0)) - half, int(np.floor(y0)) - half
        gx = np.arange(ix0, ix0 + 2 * half + 1, dtype=float)
        gy = np.arange(iy0, iy0 + 2 * half + 1, dtype=float)
        # 平场 m(x,y)（用真值图，越界处用最近邻）
        H, W = int(np.ceil(ys.max())) + half + 2, int(np.ceil(xs.max())) + half + 2
        mm = truth.m_map(max(H, 2 * half + 2), max(W, 2 * half + 2))
        mx = np.clip((gx + 0.5).astype(int), 0, mm.shape[1] - 1)
        my = np.clip((gy + 0.5).astype(int), 0, mm.shape[0] - 1)
        flat = mm[np.ix_(my, mx)]
        dx = gx[None, :] - x0
        dy = gy[:, None] - y0
        psf = moffat_profile(dx, dy, inst.fwhm_inject, inst.beta_inject)
        # fluxes_adu 是**积分后**的仪器通量 [ADU]（不是速率），电子数 = F_adu·g·PSF
        src_e = fluxes_adu[k] * inst.gain * psf
        sky_e = truth.sky_adu * inst.gain
        mu = (src_e + sky_e + dark_e) * flat
        ne = r.poisson(np.clip(mu, 0.0, None)).astype(float)
        ne += r.normal(0.0, inst.read_noise, size=ne.shape)
        adu = np.clip(ne / inst.gain, 0.0, inst.saturation_adu)
        adu = np.rint(adu)
        var_adu = (np.clip(mu, 0, None) + inst.read_noise ** 2) / inst.gain ** 2
        f = fit_psf(adu, x0 - ix0, y0 - iy0, inst.fwhm_px, inst.beta_fit, var_map=var_adu)
        if f["ok"] and np.isfinite(f["flux"]) and f["flux"] > 0:
            F_out[k] = f["flux"]; ok[k] = True
    return F_out, ok


def variance_map(adu_img, truth_info, inst: Instrument, sky_sigma_adu=None):
    """逐像素方差 [ADU²]：泊松（源+天光+暗流）+ 读出。用于 PSF 拟合加权。

    sky_sigma_adu: 若给出，用"含天光面结构"的逐像素噪声替代纯泊松天光项（robust 口径）。
    """
    mu_e = truth_info["mu"]
    var_e = np.clip(mu_e, 0.0, None) + inst.read_noise ** 2
    return var_e / (inst.gain ** 2)


# --------------------------------------------------------------------------
# 解析代数合成（第二类数据）
# --------------------------------------------------------------------------
def analytic_frame(shape, xs, ys, f_syn, truth: Truth, inst: Instrument,
                   noise=True, sky_adu=None, tag="analytic", n_eff=None):
    """纯解析：F_instr[ADU] = inject_scale·m(x,y)·F_syn 直接给出（noise=False 时无噪声）。

    noise=True 时用 PSF 加权积分的等效电子域模型（全部为 Poisson 过程，不用算术常数天光）：
        Ne_src ~ Poisson(F_instr·g)
        Ne_sky ~ Poisson(sky_adu·g·N_eff)        # 天光散粒噪声
        Ne_dark~ Poisson(dark_rate·t·N_eff)
        Ne += N(0, RN²·N_eff)                    # 读出噪声（按有效像素数合成）
        ADU = clip(Ne/g, 0, SAT) → rint          # 增益/饱和/量化
    """
    H, W = shape
    m = truth.m_map(H, W)
    k = truth.inject_scale
    mi = m[np.clip(np.round(ys).astype(int), 0, H - 1), np.clip(np.round(xs).astype(int), 0, W - 1)]
    f_instr_adu = k * mi * np.asarray(f_syn, float)
    if not noise:
        return f_instr_adu, m
    if n_eff is None:
        from scia_common import _sum_psf_sq
        n_eff = 1.0 / _sum_psf_sq(inst.fwhm_px, inst.beta_fit)
    r = rng(f"{truth.seed_tag}:{tag}")
    sky = (truth.sky_adu if sky_adu is None else sky_adu)
    # 源：全部电子进 Poisson
    ne = r.poisson(np.clip(f_instr_adu * inst.gain, 0.0, None)).astype(float)
    # 天光/暗流：在 N_eff 个有效像素上做 Poisson，**只保留零均值涨落**（背景已扣除，不引入偏置）
    mu_sky = np.clip(sky * inst.gain * n_eff, 0.0, None)
    ne += (r.poisson(mu_sky, size=xs.size) - mu_sky).astype(float)
    mu_dark = np.clip(inst.dark_rate * truth.exp_time * n_eff, 0.0, None)
    ne += (r.poisson(mu_dark, size=xs.size) - mu_dark).astype(float)
    # 读出噪声（N_eff 像素合成）
    ne += r.normal(0.0, inst.read_noise * np.sqrt(n_eff), size=xs.size)
    adu = ne / inst.gain
    # 增益/饱和/量化
    adu = np.clip(adu, 0.0, inst.saturation_adu)
    return np.rint(adu), m
