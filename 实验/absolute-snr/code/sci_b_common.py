#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 公共库：物理前向仿真、Horne 最优提取、生产口径镜像、稳健统计。

实验单元：实验/SCI-B/（跨帧绝对 SNR 传递链）
权威依据：ASTROCS_DESIGN.md §2.2/§4.4/§5.3/§12.3；ACCEPTANCE_SPEC.md §2.2；
          docs/science/PSF_SIGNAL_WEIGHT.md；docs/plugins/algorithms_phase1/07_noise_snr.md §4.1/§4.2。
生产口径镜像对象：lib/algorithms/noise_snr/cpp/src/snr_science.cpp（只读引用，不改）。
固定 seed：SEED_BASE；所有子实验用 SEED_BASE + 固定偏移，禁止时间/环境相关 seed。
"""
from __future__ import annotations

import json
import os
import time
import warnings

import numpy as np

SEED_BASE = 20260921

_HERE = os.path.dirname(os.path.abspath(__file__))
UNIT = os.path.dirname(_HERE)
ROOT = os.path.dirname(os.path.dirname(UNIT))          # 仓库根
RESULTS = os.path.join(UNIT, "results")
DATA = os.path.join(UNIT, "data")
FIGS = os.path.join(RESULTS, "figs")
TESTDATA = os.path.join(ROOT, "testdata")
HST_M16 = os.path.join(ROOT, "testdata", "HST_M16")

# 冻结常数（与生产源逐位一致，见 snr_science.cpp:50-54）
K_GAUSS_FWHM = 2.3548200450309493   # 检测块椭圆高斯 FWHM = k*sigma
K_MOFFAT4_FWHM = 1.230310           # 本块 Moffat4 beta=4 FWHM = k*sigma
K_MAD_TO_SIGMA = 1.482602218505602  # 1.4826*MAD（noise_model.cpp robust_sigma）
K_TRIM_MEAN_TO_SIGMA = 0.7316727929211932
M_REF = 6.0                         # F_ref 锚定参考星等（07_noise_snr.md §5）


# ---------------------------------------------------------------------------
# 基础工具
# ---------------------------------------------------------------------------
def rng(offset: int = 0) -> np.random.Generator:
    return np.random.default_rng(SEED_BASE + int(offset))


def save_json(path: str, obj) -> str:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=1, ensure_ascii=False, sort_keys=False)
    return path


def robust_median(x):
    return float(np.median(x))


def robust_sigma(x) -> float:
    """1.482602218505602*MAD —— 生产 noise_model.cpp robust_sigma 的逐位镜像。"""
    x = np.asarray(x, dtype=np.float64).ravel()
    if x.size == 0:
        return 0.0
    med = np.median(x)
    return float(K_MAD_TO_SIGMA * np.median(np.abs(x - med)))


def sig_clip(x, n_iter: int = 2, k: float = 5.0):
    """5σ×2 轮裁剪（生产 collect_patch_sky 同口径），返回保留样本。"""
    x = np.asarray(x, dtype=np.float64).ravel()
    keep = x
    for _ in range(n_iter):
        med = np.median(keep)
        s = K_MAD_TO_SIGMA * np.median(np.abs(keep - med))
        if not np.isfinite(s) or s <= 0:
            break
        m = np.abs(keep - med) <= k * s
        if m.all() or m.sum() < 8:
            break
        keep = keep[m]
    return keep


def loglog_slope(x, y):
    x = np.asarray(x, float); y = np.asarray(y, float)
    m = np.isfinite(x) & np.isfinite(y) & (x > 0) & (y > 0)
    if m.sum() < 2:
        return float("nan"), float("nan")
    A = np.vstack([np.log10(x[m]), np.ones(m.sum())]).T
    slope, intercept = np.linalg.lstsq(A, np.log10(y[m]), rcond=None)[0]
    return float(slope), float(intercept)


def block_bootstrap_ci(values, block: int, B: int = 2000, seed: int = 0, stat=np.median):
    """块自助法 CI（百分位 2.5–97.5%）。values 为一维样本。"""
    v = np.asarray(values, float).ravel()
    n = v.size
    if n < 4:
        return float("nan"), float("nan"), float("nan")
    block = max(1, min(int(block), n))
    r = np.random.default_rng(SEED_BASE + 900000 + int(seed))
    nblk = int(np.ceil(n / block))
    starts = np.arange(0, n, block)
    out = np.empty(B, float)
    for b in range(B):
        idx = r.choice(starts, size=nblk, replace=True)
        idx = (idx[:, None] + np.arange(block)[None, :]).ravel()[:n] % n
        out[b] = stat(v[idx])
    return float(np.percentile(out, 2.5)), float(np.percentile(out, 97.5)), float(np.std(out, ddof=1))


# ---------------------------------------------------------------------------
# PSF：离散归一化 Moffat4 beta=4（生产 snr_science.cpp moffat4Discrete 同式）
# ---------------------------------------------------------------------------
def moffat4_grid(sigma_px: float, half: int | None = None):
    """返回 (P, sum_p2, p_center, half)。P 为 (2h+1)² 归一化轮廓，sum P = 1。"""
    fwhm_eff = sigma_px * K_MOFFAT4_FWHM
    if half is None:
        half = int(max(30, np.ceil(12.0 * fwhm_eff)))
        half = min(half, 256)
    j, i = np.mgrid[-half:half + 1, -half:half + 1]
    r2 = (i.astype(float) ** 2 + j.astype(float) ** 2)
    t = 1.0 + r2 / (2.0 * sigma_px * sigma_px)
    v = t ** -4.0
    P = v / v.sum()
    return P, float((P ** 2).sum()), float(P[half, half]), half


def moffat4_enclosed_fraction(r: float, sigma_px: float) -> float:
    """Moffat4 beta=4 解析圈入流量分数 1-(1+r²/(2σ²))^-3（生产同式）。"""
    u = 1.0 + (r * r) / (2.0 * sigma_px * sigma_px)
    return 1.0 - u ** -3.0


def sigma_from_fwhm_detection(fwhm_px: float) -> float:
    return fwhm_px / K_GAUSS_FWHM


# ---------------------------------------------------------------------------
# 物理前向仿真（电子域 Poisson + 电子域 Gaussian 读出 + 增益）
# ---------------------------------------------------------------------------
def simulate_stamps(n_frames: int, F_e: float, sigma_px: float, sky_e: float,
                    dark_e: float, rn_e: float, gain_e_per_adu: float = 1.0,
                    half: int | None = None, seed_off: int = 0,
                    quantize: bool = False, flat_resp=None):
    """生成 n_frames 个 2D stamp（单位 ADU）。

    物理过程（最高设计 §12.2 第 1 类）：源、天光、暗流在电子域做 Poisson；
    读出噪声在电子域做 Gaussian；电子→ADU 经增益与（可选）量化。
    源信号 = F_e 电子按 Moffat4 轮廓分布；天光/暗流为均匀率。
    """
    P, sum_p2, p_center, half = moffat4_grid(sigma_px, half)
    lam = F_e * P + sky_e + dark_e            # 电子域期望
    shape = (n_frames,) + P.shape
    r = rng(seed_off)
    e = r.poisson(lam, size=shape).astype(np.float64)
    if rn_e > 0:
        e += r.normal(0.0, rn_e, size=shape)
    if flat_resp is not None:
        e = e * flat_resp[None, :, :]
    adu = e / gain_e_per_adu
    if quantize:
        adu = np.round(adu)
    return adu, P, sum_p2, p_center, half


# ---------------------------------------------------------------------------
# Horne 1986 最优提取（oracle 方差已知版）
# ---------------------------------------------------------------------------
def horne_extract(data, P, var_pix):
    """F_hat = Σ(P d/σ²)/Σ(P²/σ²)；Var(F_hat) = 1/Σ(P²/σ²)。data/var 单位一致。"""
    w = 1.0 / var_pix
    den = float((P * P * w).sum())
    num = float((P * w * data).sum())
    return num / den, 1.0 / den


def horne_sigma_f_theory(F_e: float, sky_e: float, dark_e: float, rn_e: float,
                         gain: float, P) -> float:
    """定义式 σ_F（电子域→ADU）：σ_i² = (sky+dark+RN²+F·P_i)/g²；σ_F² = 1/Σ(P_i²/σ_i²)。"""
    var_e = sky_e + dark_e + rn_e ** 2 + np.maximum(F_e * P, 0.0)
    var_adu = var_e / (gain * gain)
    den = float((P * P / var_adu).sum())
    return float(np.sqrt(1.0 / den))


def aperture_snr_theory(F_e, sky_e, dark_e, rn_e, gain, sigma_px,
                        r_ap_px=None, n_sky_px=None):
    """孔径 CCD 方程 + Moffat4 孔径改正（生产 snr_source_snr_f64 同式）。"""
    fwhm_eff = sigma_px * K_MOFFAT4_FWHM
    r = r_ap_px if r_ap_px else 1.5 * fwhm_eff
    f_in = moffat4_enclosed_fraction(r, sigma_px)
    n_pix = np.pi * r * r
    n_sky = n_sky_px if n_sky_px else n_pix
    s_ap_e = F_e * f_in
    var_ap_adu = (n_pix * (sky_e + dark_e + rn_e ** 2) * (1.0 + n_pix / n_sky)
                  + s_ap_e) / (gain * gain)
    sigma_f_ap = np.sqrt(var_ap_adu) / f_in
    return dict(r_ap_px=float(r), f_in=float(f_in), n_pix=float(n_pix),
                snr_ap=float(s_ap_e / gain / np.sqrt(var_ap_adu)),
                sigma_f_ap_adu=float(sigma_f_ap), n_sky=float(n_sky))


# ---------------------------------------------------------------------------
# 生产口径镜像：snr_science.cpp snr_source_snr_f64（gain>0 分支）
# ---------------------------------------------------------------------------
def prod_mirror_snr(F_adu: float, sigma_px: float, sigma_sky_adu: float,
                    gain: float, rn_e: float, half: int | None = None,
                    fwhm_px: float = 0.0) -> dict:
    """逐行镜像 lib/algorithms/noise_snr/cpp/src/snr_science.cpp:149-238。

    与 C++ 驱动（code/prod_snr_driver.cpp）对拍，容差 1e-12（同一公式两条实现）。
    """
    sigma = sigma_from_fwhm_detection(fwhm_px) if fwhm_px > 0 else sigma_px
    fwhm_eff = sigma * K_MOFFAT4_FWHM
    if half is None:
        half = int(max(30, np.ceil(12.0 * fwhm_eff)))
        half = min(half, 256)
    j, i = np.mgrid[-half:half + 1, -half:half + 1]
    r2 = (i.astype(float) ** 2 + j.astype(float) ** 2)
    t = 1.0 + r2 / (2.0 * sigma * sigma)
    v = (t ** -4.0).ravel()
    P = v / v.sum()
    rn_term = (rn_e / gain) ** 2 if rn_e > 0 else 0.0
    var_i = sigma_sky_adu ** 2 + rn_term + np.maximum(F_adu * P, 0.0) / gain
    var_f = 1.0 / float((P * P / var_i).sum())
    sum_p2 = float((P * P).sum())
    p_center = float(P[P.size // 2])
    r = 1.5 * fwhm_eff
    f_in = moffat4_enclosed_fraction(r, sigma)
    n_pix = np.pi * r * r
    var_ap = n_pix * sigma_sky_adu ** 2 * 2.0   # n_sky=n_pix 时 (1+n_pix/n_sky)=2
    s_ap = F_adu * f_in
    var_ap += s_ap / gain
    return dict(sigma_px=float(sigma), half=int(half), sum_p2=sum_p2,
                p_center=p_center, sigma_f_optimal_adu=float(np.sqrt(var_f)),
                snr_optimal=float(F_adu / np.sqrt(var_f)),
                snr_peak=float(F_adu * p_center / sigma_sky_adu),
                flux5_adu=float(5.0 * np.sqrt(var_f)),
                f_in=float(f_in), n_pix=float(n_pix),
                snr_aperture=float(s_ap / np.sqrt(var_ap)),
                sigma_f_aperture_adu=float(np.sqrt(var_ap) / f_in))


def snr_from_ref_flux(F_ref: float, sigma_F: float) -> float:
    """帧级 SNR 定义式：SNR_k(F_ref) = F_ref/σ_F（通量型，Horne 1986）。"""
    return float(F_ref / sigma_F)


def f_ref_from_zeropoint(zp_mag: float, m_ref: float = M_REF) -> float:
    """F_ref,k = 10^(-0.4(m_ref − ZP_k))（07_noise_snr.md §5 逐帧口径）。"""
    return float(10.0 ** (-0.4 * (m_ref - zp_mag)))


# ---------------------------------------------------------------------------
# 真值场 / 三口径估计量（dense / sparse / frame）
# ---------------------------------------------------------------------------
def patch_sigma_field(var_map, P: int, clip: bool = True):
    """逐 P×P patch 的稳健 σ 场（生产 collect_patch_sky+robust_sigma 同口径）。"""
    h, w = var_map.shape
    ny, nx = h // P, w // P
    out = np.full((ny, nx), np.nan)
    for iy in range(ny):
        for ix in range(nx):
            blk = var_map[iy * P:(iy + 1) * P, ix * P:(ix + 1) * P].ravel()
            blk = blk[np.isfinite(blk)]
            if clip:
                blk = sig_clip(blk)
            if blk.size >= 8:
                out[iy, ix] = robust_sigma(blk)
    return out


def cell_sigma_field(var_map, D: int, clip: bool = True):
    """Δ×Δ cell 稳健 σ（稀疏控制点语义）。"""
    return patch_sigma_field(var_map, D, clip=clip)


def sigma_field_fast(img, P: int, clip_sigma: float = 5.0, n_round: int = 2):
    """向量化逐 P×P 块稳健 σ 场（1.4826×MAD + clip_sigma×n_round 裁剪）。"""
    h, w = img.shape
    ny, nx = h // P, w // P
    if ny == 0 or nx == 0:
        return np.full((max(ny, 0), max(nx, 0)), np.nan)
    blk = img[:ny * P, :nx * P].reshape(ny, P, nx, P).transpose(0, 2, 1, 3).reshape(ny, nx, P * P)
    cur = blk.astype(np.float64, copy=True)
    cur[~np.isfinite(cur)] = np.nan
    with np.errstate(all="ignore"), warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)   # 全 NaN 切片（空 cell）是预期语义
        for _ in range(max(1, n_round)):
            # 每轮重新判定有限性：裁剪本身会引入 NaN，用 np.median 会把整个 cell 变成 NaN
            medf = np.median if bool(np.isfinite(cur).all()) else np.nanmedian
            med = medf(cur, axis=2, keepdims=True)
            mad = K_MAD_TO_SIGMA * medf(np.abs(cur - med), axis=2, keepdims=True)
            keep = np.abs(cur - med) <= clip_sigma * mad
            keep &= np.isfinite(cur)
            cur = np.where(keep, cur, np.nan)
        medf = np.median if bool(np.isfinite(cur).all()) else np.nanmedian
        med = medf(cur, axis=2)
        mad = K_MAD_TO_SIGMA * medf(np.abs(cur - med[:, :, None]), axis=2)
        n_valid = np.isfinite(cur).sum(axis=2)
    mad = np.where(np.isfinite(mad) & (mad > 0) & (n_valid >= 8), mad, np.nan)
    return mad


def estimate_ell(field, P: int, fit_max_px: float | None = None):
    """由 σ 场的自相关（FFT，方位平均）拟合 SE 相关长度 ℓ（A(ℓ)=1/e 处）。

    先扣大尺度趋势（大窗中值），再做自相关；返回 (ell_px, n_used)。
    """
    f = np.asarray(field, float).copy()
    m = np.isfinite(f)
    if m.sum() < 64:
        return float("nan"), 0
    f[~m] = np.nanmedian(f)
    # 只扣平面趋势（避免抹掉大尺度结构而人为压缩 ℓ）
    ny0, nx0 = f.shape
    yy, xx = np.mgrid[0:ny0, 0:nx0]
    A = np.vstack([np.ones(f.size), yy.ravel() / max(ny0, 1), xx.ravel() / max(nx0, 1)]).T
    coef = np.linalg.lstsq(A, f.ravel(), rcond=None)[0]
    g = (f.ravel() - A @ coef).reshape(f.shape)
    g = g - g.mean()
    F = np.fft.rfft2(g)
    acf = np.fft.irfft2(F * np.conj(F), s=g.shape)
    acf = np.fft.fftshift(acf, axes=0)
    acf /= acf.max()
    ny, nx = g.shape
    cy = ny // 2
    lags = np.arange(0, min(ny // 2, nx // 2))
    prof = np.array([acf[cy, l] for l in lags])
    target = np.exp(-1.0)
    ell = float("nan")
    for i in range(1, len(prof)):
        if prof[i] <= target:
            x0, x1 = lags[i - 1], lags[i]; y0, y1 = prof[i - 1], prof[i]
            ell = (x0 + (target - y0) * (x1 - x0) / (y1 - y0)) if y1 != y0 else float(x1)
            break
    if not np.isfinite(ell):
        # 回退：对 log ACF ≈ −r²/(2ℓ²) 做原点约束最小二乘（ACF > 0.2 段）
        use = (lags > 0) & (prof > 0.2) & (prof < 1.0)
        if use.sum() >= 4:
            x = (lags[use].astype(float) ** 2)
            y = np.log(prof[use])
            slope = float((x * y).sum() / (x * x).sum())
            if slope < 0:
                ell = float(np.sqrt(-1.0 / (2.0 * slope)))
    return (ell * P if np.isfinite(ell) else float("nan")), int(m.sum())


def bilinear_upsample(field, shape, P: int):
    """规则网格双线性重建（bilinear_regular_grid_v1）：控制点在 cell 中心。"""
    ny, nx = field.shape
    ys = (np.arange(shape[0]) + 0.5) / P - 0.5
    xs = (np.arange(shape[1]) + 0.5) / P - 0.5
    ys = np.clip(ys, 0, ny - 1); xs = np.clip(xs, 0, nx - 1)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    y1 = np.minimum(y0 + 1, ny - 1); x1 = np.minimum(x0 + 1, nx - 1)
    wy = (ys - y0)[:, None]; wx = (xs - x0)[None, :]
    f = np.nan_to_num(field, nan=float(np.nanmedian(field)))
    top = f[y0][:, x0] * (1 - wx) + f[y0][:, x1] * wx
    bot = f[y1][:, x0] * (1 - wx) + f[y1][:, x1] * wx
    return top * (1 - wy) + bot * wy


def rmse_log_rho(est, truth, eval_mask):
    """两侧中位数归一后的 RMSE(log10 ρ)。"""
    m = eval_mask & np.isfinite(est) & np.isfinite(truth) & (est > 0) & (truth > 0)
    if m.sum() < 16:
        return float("nan"), float("nan")
    a = est[m] / np.median(est[m]); b = truth[m] / np.median(truth[m])
    d = np.log10(a) - np.log10(b)
    return float(np.sqrt(np.mean(d * d))), float(np.median(np.log10(est[m] / truth[m])))


def weight_efficiency_loss(sigma_hat, sigma_true, eval_mask):
    """权重效率损失 E = Var_w/Var_opt − 1（逆方差叠加，非退化判据）。

    Var_w = Σ w_i² σ_i²/(Σw_i)², w_i=1/σ̂_i²；Var_opt = 1/Σ(1/σ_i²)。
    全局尺度因子在比值中相消 ⇒ E=0 ⇔ σ̂ ∝ σ_true。
    """
    m = eval_mask & np.isfinite(sigma_hat) & np.isfinite(sigma_true) & (sigma_hat > 0) & (sigma_true > 0)
    if m.sum() < 16:
        return float("nan")
    sh = sigma_hat[m].astype(np.float64); st = sigma_true[m].astype(np.float64)
    w = 1.0 / (sh * sh)
    var_w = float(np.sum(w * w * st * st) / np.sum(w) ** 2)
    var_opt = float(1.0 / np.sum(1.0 / (st * st)))
    return float(var_w / var_opt - 1.0)


def bytes_per_frame(kind: str, n_px: int, delta: int | None = None, extra_var: bool = False):
    """每帧 SNR 层存储字节（float32 值）。"""
    if kind == "dense":
        return 4 * n_px
    if kind == "frame":
        return 4
    if kind == "sparse":
        n_ctrl = int(np.ceil(np.sqrt(n_px) / delta) ** 2)
        return 4 * n_ctrl * (2 if extra_var else 1)
    raise ValueError(kind)


def now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
