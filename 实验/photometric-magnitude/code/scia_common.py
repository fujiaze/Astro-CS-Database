# -*- coding: utf-8 -*-
"""SCI-A 实验单元 · 公共数值核心（AstroCS 测光校准到测光星等坐标系）

权威依据（只读，不在本实验内改动）：
  - ASTROCS_DESIGN.md §2.1 / §4.2 / §4.4 / §12.1-12.3
  - ACCEPTANCE_SPEC.md §2.1（SCI-A 判据表）
  - docs/science/PHOTOMETRY.md (SCI-PHOT-001)：IRLS/Tukey 零点估计、sigma_residual 定义
  - docs/plugins/algorithms_phase1/06_photometry.md §4.1：测光一致性双边界判据形态
  - run/RELEASE-02/parallel/06.md：误差预算逐项推导（本实验把它移植到"真值已知"的仿真上）

本模块只放"定义"，不放实验流程。所有实验步骤脚本 import 本模块。
固定 seed：SCIA_SEED。所有随机数走 numpy.random.default_rng(SCIA_SEED + 派生整数)。
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
import time
from dataclasses import dataclass, field

import numpy as np

# --------------------------------------------------------------------------
# 0. 常量与路径
# --------------------------------------------------------------------------
SCIA_SEED = 20260921                      # 固定 seed（任务书要求）
MAD_TO_SIGMA = 1.0 / 0.6744897501960817   # SCI-PHOT-001 §14 全精度值
TUKEY_C = 4.685                           # SCI-PHOT-001 §5，95% 高斯渐近效率
IRLS_TOL = 1e-6
IRLS_MAX_ITER = 50
MAG_TOLERANCE = 3.0                       # SCI-PHOT-001 §5 星等一致性预过滤
SD_MAD_OVER_MAD = 1.166                   # SD(MAD)/MAD 正态渐近常数（06.md §2.1）
NSIGMA_SAMPLING = 3.0                     # 抽样允差（06.md §2.1 唯一约定性选择）
PHOTON_MAG = 2.5 / np.log(10.0)           # 1.0857362...  d(mag)/d(ln F)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXP = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CODE = os.path.join(EXP, "code")
RESULTS = os.path.join(EXP, "results")
DATA = os.path.join(EXP, "data")
RUN = os.path.join(REPO, "run", "SCI-401")
CACHE = os.path.join(RUN, "data_cache")
for _d in (RESULTS, RUN, CACHE):
    os.makedirs(_d, exist_ok=True)


def rng(tag: str) -> np.random.Generator:
    """确定性派生 RNG：同 tag 永远同序列，与调用顺序无关。"""
    h = hashlib.sha256(tag.encode("utf-8")).digest()
    return np.random.default_rng(SCIA_SEED + int.from_bytes(h[:4], "big"))


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def jdump(obj, path: str) -> None:
    """写 JSON；**非有限浮点（NaN/Inf）一律转 null**，保证产物是合法 JSON（jq 可解析）。"""
    with open(path, "w", encoding="utf-8") as f:
        json.dump(_sanitize(obj), f, ensure_ascii=False, indent=2, default=_json_default,
                  allow_nan=False)
    print(f"[scia] wrote {os.path.relpath(path, REPO)}")


def _sanitize(o):
    if isinstance(o, float):
        return o if np.isfinite(o) else None
    if isinstance(o, (np.floating,)):
        v = float(o)
        return v if np.isfinite(v) else None
    if isinstance(o, dict):
        return {k: _sanitize(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_sanitize(v) for v in o]
    if isinstance(o, np.ndarray):
        return _sanitize(o.tolist())
    return o


def _json_default(o):
    if isinstance(o, (np.floating,)):
        return float(o)
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, (np.bool_,)):
        return bool(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    raise TypeError(str(type(o)))


# --------------------------------------------------------------------------
# 1. 通带曲线：滤镜（eng/packaging/config/filters.json）与 QE（qe_curves.json）
# --------------------------------------------------------------------------
def load_filter(name: str):
    """从仓内 eng/packaging/config/filters.json 取滤镜曲线（逐字转录自 response_curves/filters.json）。"""
    with open(os.path.join(REPO, "config", "filters.json"), encoding="utf-8") as f:
        lib = json.load(f)
    if name not in lib["filters"]:
        raise KeyError(f"filter {name!r} not in eng/packaging/config/filters.json; have {len(lib['filters'])}")
    e = lib["filters"][name]
    return np.asarray(e["wavelength_nm"], float), np.asarray(e["value"], float)


def load_qe(name: str):
    with open(os.path.join(REPO, "lib", "algorithms", "photometry", "data",
                           "response_curves", "qe_curves.json"), encoding="utf-8") as f:
        lib = json.load(f)
    if name not in lib:
        raise KeyError(f"QE {name!r} not found; have {sorted(lib)}")
    e = lib[name]
    return np.asarray(e["wavelength_nm"], float), np.asarray(e["value"], float)


# --------------------------------------------------------------------------
# 2. 冻结积分约定：F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ  (SCI-PHOT-001 §2 符号表)
#    插值 = Akima 子样条(fill=0)，积分 = 复合 Simpson 1/3（末尾 3/8 / n==1 梯形）
#    与 lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp 同定义。
# --------------------------------------------------------------------------
def _prepare_curve(wl, val):
    wl = np.asarray(wl, float); val = np.asarray(val, float)
    o = np.argsort(wl, kind="stable")
    wl, val = wl[o], val[o]
    keep = np.concatenate(([True], np.diff(wl) > 0.0))
    return wl[keep], val[keep]


def akima_interp(x_src, y_src, x_dst, fill=0.0):
    """Akima 子样条插值（定义与生产 spectrum_integrator.cpp 一致）。"""
    x_src = np.asarray(x_src, float); y_src = np.asarray(y_src, float)
    x_dst = np.asarray(x_dst, float)
    n = x_src.size
    y_dst = np.full(x_dst.shape, float(fill))
    if n < 2:
        return y_dst
    slope = np.diff(y_src) / np.diff(x_src)
    ext = np.empty(n + 3)
    if n >= 3:
        ext[0] = 3.0 * slope[0] - 2.0 * slope[1]
        ext[1] = 2.0 * slope[0] - slope[1]
        ext[n + 1] = 2.0 * slope[n - 2] - slope[n - 3]
        ext[n + 2] = 3.0 * slope[n - 2] - 2.0 * slope[n - 3]
    else:
        ext[0] = ext[1] = slope[0]
        ext[n + 1] = ext[n + 2] = slope[n - 2]
    ext[2:n + 1] = slope
    t = np.empty(n)
    for i in range(n):
        ml2, ml1, mc, mr = ext[i], ext[i + 1], ext[i + 2], ext[i + 3]
        w1 = abs(mr - mc); w2 = abs(ml1 - ml2)
        t[i] = 0.5 * (ml1 + mc) if (w1 + w2) == 0.0 else (w1 * ml1 + w2 * mc) / (w1 + w2)
    lo = np.clip(np.searchsorted(x_src, x_dst, side="right") - 1, 0, n - 2)
    x0, x1 = x_src[lo], x_src[lo + 1]
    y0, y1 = y_src[lo], y_src[lo + 1]
    dx = x1 - x0
    s = (x_dst - x0) / dx
    h00 = (2.0 * s - 3.0) * s * s + 1.0
    h10 = ((s - 2.0) * s + 1.0) * s
    h01 = (-2.0 * s + 3.0) * s * s
    h11 = (s - 1.0) * s * s
    inside = (x_dst >= x_src[0]) & (x_dst <= x_src[-1])
    out = np.where(inside, h00 * y0 + h10 * dx * t[lo] + h01 * y1 + h11 * dx * t[lo + 1], fill)
    return out


def simpson_integrate(x, y):
    """复合 Simpson 1/3（末尾奇数区间用 3/8；n==1 退梯形），与生产一致。"""
    x = np.asarray(x, float); y = np.asarray(y, float)
    npts = x.size
    if npts < 2:
        return 0.0
    h = (x[-1] - x[0]) / (npts - 1)
    nint = npts - 1
    if nint % 2 == 0:
        i = np.arange(1, npts - 1)
        s = y[0] + y[-1] + np.sum(np.where(i % 2 == 1, 4.0, 2.0) * y[i])
        return s * h / 3.0
    if nint >= 3:
        n13 = nint - 3
        i = np.arange(1, n13)
        s = y[0] + y[n13] + np.sum(np.where(i % 2 == 1, 4.0, 2.0) * y[i])
        tot = s * h / 3.0
        tot += (y[n13] + 3.0 * y[n13 + 1] + 3.0 * y[n13 + 2] + y[-1]) * 3.0 * h / 8.0
        return tot
    return 0.5 * h * (y[0] + y[1])


def f_syn(spectrum, wl_nm, T=None, T_wl=None, Q=None, Q_wl=None, mag_g=None):
    """F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4·magG)   [冻结约定]"""
    wl = np.asarray(wl_nm, float)
    S = np.asarray(spectrum, float)
    tr = np.ones_like(wl) if T is None else akima_interp(*_prepare_curve(T_wl, T), wl, 0.0)
    qr = np.ones_like(wl) if Q is None else akima_interp(*_prepare_curve(Q_wl, Q), wl, 0.0)
    v = simpson_integrate(wl, S * tr * qr * wl)
    if mag_g is not None:
        v *= 10.0 ** (-0.4 * float(mag_g))
    return v


def f_syn_dense(spectrum, wl_nm, T=None, T_wl=None, Q=None, Q_wl=None,
                mag_g=None, oversample=64):
    """独立 Oracle：线性插值到 oversample 倍密网格 + 复合梯形（不复用 Akima/Simpson）。

    用于验证冻结积分约定的收敛性（相对偏差应随 oversample 下降）。
    """
    wl = np.asarray(wl_nm, float)
    S = np.asarray(spectrum, float)
    lo, hi = wl[0], wl[-1]
    n = (wl.size - 1) * oversample + 1
    xg = np.linspace(lo, hi, n)
    Sg = np.interp(xg, wl, S)
    if T is None:
        Tg = np.ones_like(xg)
    else:
        tw, tv = _prepare_curve(T_wl, T)
        Tg = np.interp(xg, tw, tv, left=0.0, right=0.0)
    if Q is None:
        Qg = np.ones_like(xg)
    else:
        qw, qv = _prepare_curve(Q_wl, Q)
        Qg = np.interp(xg, qw, qv, left=0.0, right=0.0)
    v = np.trapezoid(Sg * Tg * Qg * xg, xg)
    if mag_g is not None:
        v *= 10.0 ** (-0.4 * float(mag_g))
    return v


# --------------------------------------------------------------------------
# 3. 稳健统计与 IRLS/Tukey 零点（SCI-PHOT-001 §5 逐式）
# --------------------------------------------------------------------------
def mad_sigma(x):
    x = np.asarray(x, float)
    if x.size == 0:
        return 0.0
    return float(np.median(np.abs(x - np.median(x))) * MAD_TO_SIGMA)


def irls_tukey(r, c=TUKEY_C, tol=IRLS_TOL, max_iter=IRLS_MAX_ITER):
    """返回 dict(location, S, sigma_residual, inliers(mask), n_in, iterations, degenerate)"""
    r = np.asarray(r, float)
    n = r.size
    out = dict(location=float("nan"), S=0.0, sigma_residual=0.0,
               inliers=np.zeros(n, bool), n_in=0, iterations=0, degenerate=True)
    if n == 0:
        return out
    S = mad_sigma(r)
    loc = float(np.median(r))
    if not (S > 0.0):
        out.update(location=loc, S=0.0, inliers=np.ones(n, bool), n_in=n, degenerate=True)
        return out
    w = np.ones(n)
    it = 0
    for it in range(1, max_iter + 1):
        u = (r - loc) / (c * S)
        w = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
        sw = w.sum()
        if sw <= 0:
            break
        new = float(np.sum(w * r) / sw)
        if abs(new - loc) < tol:
            loc = new
            break
        loc = new
    inl = w > 0
    sig = mad_sigma(r[inl]) if inl.sum() >= 2 else 0.0
    out.update(location=loc, S=S, sigma_residual=sig, inliers=inl,
               n_in=int(inl.sum()), iterations=it, degenerate=False)
    return out


def mag_consistency_prefilter(delta, mag_tolerance=MAG_TOLERANCE):
    """SCI-PHOT-001 §5 星等一致性预过滤：|delta_i - median(delta)| > 3.0 mag 拒绝。"""
    delta = np.asarray(delta, float)
    if delta.size == 0:
        return np.zeros(0, bool)
    med = np.median(delta)
    return np.abs(delta - med) <= mag_tolerance


# --------------------------------------------------------------------------
# 4. PSF：Moffat（生产默认族，见 eng/packaging/config/defaults.json / docs/plugins/04_psf.md）
# --------------------------------------------------------------------------
def moffat_profile(dx, dy, fwhm, beta=4.0):
    """归一化 Moffat：∫ I dA = 1（像素单位面积）。alpha 由 FWHM 定义。"""
    alpha = fwhm / (2.0 * np.sqrt(2.0 ** (1.0 / beta) - 1.0))
    return (beta - 1.0) / (np.pi * alpha * alpha) * (1.0 + (dx * dx + dy * dy) / (alpha * alpha)) ** (-beta)


def render_star(img, x0, y0, flux, fwhm, beta=4.0):
    """把一颗星以精确像素积分（超采样）加到 img 上。"""
    sub = 5
    r = int(np.ceil(4.0 * fwhm)) + 1
    ix0, ix1 = int(np.floor(x0 - r)), int(np.ceil(x0 + r))
    iy0, iy1 = int(np.floor(y0 - r)), int(np.ceil(y0 + r))
    H, W = img.shape
    ix0 = max(ix0, 0); iy0 = max(iy0, 0); ix1 = min(ix1, W - 1); iy1 = min(iy1, H - 1)
    if ix1 < ix0 or iy1 < iy0:
        return
    xs = np.arange(ix0, ix1 + 1)
    ys = np.arange(iy0, iy1 + 1)
    off = (np.arange(sub) + 0.5) / sub - 0.5
    acc = np.zeros((ys.size, xs.size))
    for oy in off:
        for ox in off:
            dx = (xs[None, :] + ox) - x0
            dy = (ys[:, None] + oy) - y0
            acc += moffat_profile(dx, dy, fwhm, beta)
    img[iy0:iy1 + 1, ix0:ix1 + 1] += flux * acc / (sub * sub)


def psf_stamp_model(params, xs, ys, fwhm, beta):
    """PSF + 常数局部背景（生产口径：背景已在 PSF/测光上游处理，此处显式建模）。"""
    x0, y0, amp, bg = params
    return amp * moffat_profile(xs - x0, ys - y0, fwhm, beta) + bg


def fit_psf(img, x0, y0, fwhm, beta=4.0, box=None, var_map=None, max_iter=60):
    """在 (x0,y0) 附近做 (x,y,amp) 三参数 PSF 最小二乘拟合。

    返回 dict(flux, x, y, flux_err, chi2, ok)。flux_err 由 Jacobian 协方差给出。
    box: 半宽（像素）；默认 ceil(3*fwhm)。
    """
    from scipy.optimize import least_squares
    H, W = img.shape
    if box is None:
        box = int(np.ceil(3.0 * fwhm))
    ix0 = max(int(np.floor(x0 - box)), 0); ix1 = min(int(np.ceil(x0 + box)), W - 1)
    iy0 = max(int(np.floor(y0 - box)), 0); iy1 = min(int(np.ceil(y0 + box)), H - 1)
    if ix1 - ix0 < 3 or iy1 - iy0 < 3:
        return dict(flux=np.nan, x=np.nan, y=np.nan, flux_err=np.nan, chi2=np.nan, ok=False)
    xs, ys = np.meshgrid(np.arange(ix0, ix1 + 1, dtype=float),
                         np.arange(iy0, iy1 + 1, dtype=float))
    data = img[iy0:iy1 + 1, ix0:ix1 + 1].ravel()
    if var_map is None:
        sig = np.ones_like(data)
    else:
        sig = np.sqrt(np.maximum(var_map[iy0:iy1 + 1, ix0:ix1 + 1].ravel(), 1e-12))
    xsf, ysf = xs.ravel(), ys.ravel()

    def resid(p):
        return (psf_stamp_model(p, xsf, ysf, fwhm, beta) - data) / sig

    # 背景初值：stamp 边缘环带中位（免疫中心源）
    edge = np.concatenate([data[:xs.shape[0]], data[-xs.shape[0]:],
                           data[::xs.shape[1]], data[xs.shape[1] - 1::xs.shape[1]]])
    bg0 = float(np.median(edge))
    p0 = np.array([x0, y0, max(float(np.sum(data - bg0)), 1e-6), bg0])
    lo = np.array([x0 - 2.0, y0 - 2.0, 0.0, -np.inf])
    hi = np.array([x0 + 2.0, y0 + 2.0, np.inf, np.inf])
    try:
        res = least_squares(resid, p0, bounds=(lo, hi), xtol=1e-10, ftol=1e-10,
                            gtol=1e-10, max_nfev=max_iter * 4)
    except Exception:
        return dict(flux=np.nan, x=np.nan, y=np.nan, flux_err=np.nan, chi2=np.nan, ok=False)
    if not res.success or not np.all(np.isfinite(res.x)):
        return dict(flux=np.nan, x=np.nan, y=np.nan, flux_err=np.nan, chi2=np.nan, ok=False)
    J = res.jac
    try:
        cov = np.linalg.inv(J.T @ J)
        ferr = float(np.sqrt(max(cov[2, 2], 0.0)))
    except np.linalg.LinAlgError:
        ferr = float("nan")
    chi2 = float(np.sum(res.fun ** 2) / max(res.fun.size - 4, 1))
    return dict(flux=float(res.x[2]), x=float(res.x[0]), y=float(res.x[1]),
                bg=float(res.x[3]), flux_err=ferr, chi2=chi2, ok=True)


def psf_flux_variance_theory(flux_adu, fwhm, beta, sigma_pix_e, gain_e_per_adu):
    """固定形状、幅度-only 拟合的理论通量方差 [ADU²]（Horne 1986 结构的一阶展开）。

        Var(F_adu) = 1 / Σ_i ( P_i² / σ_i,adu² ),  σ_i,adu² = σ_pix_e²/g² + F_adu·P_i/g
        ⇒ Var(F_adu) ≈ σ_pix_e²·N_eff/g² + F_adu·w/g
    """
    inst = _InstShim(gain=gain_e_per_adu, fwhm_px=fwhm, beta_fit=beta)
    return float(psf_fit_variance_exact(np.atleast_1d(np.asarray(flux_adu, float)),
                                        inst, sigma_pix_e)[0])


class _InstShim:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def _sum_psf_sq(fwhm, beta, half=None):
    if half is None:
        half = int(np.ceil(3.0 * fwhm))
    xs = np.arange(-half, half + 1, dtype=float)
    dx, dy = np.meshgrid(xs, xs)
    p = moffat_profile(dx, dy, fwhm, beta)
    return float(np.sum(p * p))


def _mean_psf_weighted(fwhm, beta, half=None):
    if half is None:
        half = int(np.ceil(3.0 * fwhm))
    xs = np.arange(-half, half + 1, dtype=float)
    dx, dy = np.meshgrid(xs, xs)
    p = moffat_profile(dx, dy, fwhm, beta)
    return float(np.sum(p ** 3) / np.sum(p ** 2))


# --------------------------------------------------------------------------
# 5. 误差预算（判据形态见 docs/plugins/algorithms_phase1/06_photometry.md §4.1；
#    逐项口径与实测出处见 run/RELEASE-02/parallel/06.md §2；各项在平方和中各计一次）
# --------------------------------------------------------------------------
@dataclass
class Budget:
    sigma_fit_white: float = 0.0     # 光子+天光+读出+拟合自由度（白噪声口径）[mag]
    sigma_fit_robust: float = 0.0    # 同上但含天光面结构 [mag]
    sigma_psfsys: float = 0.0        # PSF 域通量方法系统误差（帧内实测）[mag]
    sigma_color: float = 0.0         # 通带失配颜色项 [mag]
    sigma_gaia: float = 0.0          # 参考侧（XP 合成通量）误差 [mag]
    sigma_flat: float = 0.0          # 平场残余 [mag]
    sigma_skyres: float = 0.0        # 天光扣除残差 [mag]
    sigma_q: float = 0.0             # 量化 [mag]
    n: int = 0
    rho_lo: float = 1.0
    rho_hi: float = 1.0
    items: dict = field(default_factory=dict)

    @property
    def sigma_ceiling(self):
        s2 = (self.sigma_fit_robust ** 2 + self.sigma_psfsys ** 2 + self.sigma_color ** 2
              + self.sigma_gaia ** 2 + self.sigma_flat ** 2 + self.sigma_skyres ** 2
              + self.sigma_q ** 2)
        return self.rho_hi * float(np.sqrt(s2))

    @property
    def sigma_floor(self):
        return self.rho_lo * self.sigma_fit_white

    def as_dict(self):
        d = dict(n=self.n, rho_lo=self.rho_lo, rho_hi=self.rho_hi,
                 sigma_fit_white=self.sigma_fit_white, sigma_fit_robust=self.sigma_fit_robust,
                 sigma_psfsys=self.sigma_psfsys, sigma_color=self.sigma_color,
                 sigma_gaia=self.sigma_gaia, sigma_flat=self.sigma_flat,
                 sigma_skyres=self.sigma_skyres, sigma_q=self.sigma_q,
                 sigma_floor=self.sigma_floor, sigma_ceiling=self.sigma_ceiling)
        d.update(self.items)
        return d


def psf_fit_variance_exact(flux_adu, inst, sigma_pix_e, half=None):
    """(幅度, 常数背景) 线性 PSF 拟合的**精确** Fisher 通量方差 [ADU²]。

    数据模型 d_i = F·P_i + b（ADU），逐像素方差
        sigma_i² = sigma_pix_e²/g²  +  F·P_i/g          [ADU²]
                   └ 背景/读出 ┘        └ 源泊松（正确加权）┘
    Fisher 矩阵 H = JᵀWJ, J=[P, 1], W=diag(1/sigma_i²)：
        Var(F̂) = H11/(H00·H11 - H01²)

    两个极限：
      · 背景主导（F→0）：Var → sigma_pix_e²·N_eff/g²（Horne 常数噪声口径）
      · 源泊松主导      ：Var → F/g（正确 Poisson 加权下 w_eff→1）

    与权威文档的关系（本实验核对过，措辞以文档原文为准）：
      · `run/RELEASE-02/parallel/06.md` §2.2（第 100 行）给的是 **Horne 1986 最优提取结构**
        `sigma_F/F = sqrt(1/(g·F) + N_eff·sigma_pix²/F²)`——它假定**背景已知**、无自由背景参数；
      · 本函数多了一个**自由常数背景**参数（J=[P,1]），因此多出 H01 交叉项，方差更大。
        两者之差就是"自由背景简并"的代价，不是文档错误（06.md 的 sigma_fit=0.0140 是
        生产拟合器 200 次重复拟合的**实测**值，不是该解析式的取值）。
    """
    F = np.asarray(flux_adu, float)
    if half is None:
        half = int(np.ceil(3.0 * inst.fwhm_px))
    g = np.arange(-half, half + 1, dtype=float)
    dx, dy = np.meshgrid(g, g)
    P = moffat_profile(dx, dy, inst.fwhm_px, inst.beta_fit)
    sig_b2 = sigma_pix_e ** 2 / inst.gain ** 2
    out = np.empty(F.shape, float)
    Pf = P.ravel()
    for k, f in enumerate(np.atleast_1d(F)):
        s2 = sig_b2 + np.clip(f, 0.0, None) * Pf / inst.gain
        w = 1.0 / np.maximum(s2, 1e-30)
        H00 = float(np.sum(Pf * Pf * w)); H01 = float(np.sum(Pf * w)); H11 = float(np.sum(w))
        det = H00 * H11 - H01 * H01
        out[k] = H11 / det if det > 0 else np.nan
    return out if np.ndim(flux_adu) else float(out[0])


def noise_sigma_mag(flux_adu, inst, sigma_pix_e, n_eff=None, w_psf=None):
    """逐星噪声引起的星等不确定度 [mag]（用精确 Fisher 方差，见 psf_fit_variance_exact）。"""
    F = np.asarray(flux_adu, float)
    var_adu = psf_fit_variance_exact(F, inst, sigma_pix_e)
    return PHOTON_MAG * np.sqrt(np.clip(var_adu, 0, None)) / np.maximum(F, 1e-30)


def mc_sigma_obs(sigma_mag, extra_sigma_mag=0.0, n_mc=400, tag="mc_sigma_obs"):
    """把"逐星 sigma"合成为"整帧 sigma_obs = 2.5·MAD/0.6745"的预测（Monte Carlo）。

    06.md §2.1 的合成形态用**中位通量**代表整帧亮度分布；当匹配样本通量跨度大时
    该近似会低估 sigma_obs。本函数按逐星 sigma 分布合成，是对**合成方式**的细化
    （不改任何冻结常数）。
    """
    s = np.sqrt(np.asarray(sigma_mag, float) ** 2 + float(extra_sigma_mag) ** 2)
    if s.size == 0:
        return float("nan")
    r = rng(tag)
    vals = np.empty(n_mc)
    for i in range(n_mc):
        # sigma_mag 已是**星等**单位；2.5·MAD(r_dex)/0.6745 ≡ MAD(x_mag)/0.6745
        vals[i] = mad_sigma(r.normal(0.0, 1.0, s.size) * s)
    return float(np.median(vals))


def sampling_rho(n, k=NSIGMA_SAMPLING, sd_over_mad=SD_MAD_OVER_MAD):
    """rho_lo = 1 - k·1.166/√n, rho_hi = 1 + k·1.166/√n  (06.md §2.1)"""
    if n <= 0:
        return 1.0, 1.0
    d = k * sd_over_mad / np.sqrt(n)
    return float(1.0 - d), float(1.0 + d)


def gate_verdict(sigma_obs, budget: Budget):
    f, c = budget.sigma_floor, budget.sigma_ceiling
    if not np.isfinite(sigma_obs):
        return "NO_DATA"
    if sigma_obs < f:
        return "BELOW_FLOOR"
    if sigma_obs > c:
        return "ABOVE_CEILING"
    return "PASS"


# --------------------------------------------------------------------------
# 6. WCS 辅助（astropy 独立实现，不用生产 plate_solve）
# --------------------------------------------------------------------------
def make_wcs(crval, crpix, cd, shape=None, ctype=("RA---TAN", "DEC--TAN")):
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.crval = list(crval)
    w.wcs.crpix = list(crpix)
    w.wcs.cd = np.asarray(cd, float)
    w.wcs.ctype = list(ctype)
    if shape is not None:
        w.array_shape = shape
    return w


def world_to_pix(w, ra, dec):
    x, y = w.all_world2pix(np.asarray(ra, float), np.asarray(dec, float), 0)
    return np.asarray(x, float), np.asarray(y, float)


def pix_to_world(w, x, y):
    ra, dec = w.all_pix2world(np.asarray(x, float), np.asarray(y, float), 0)
    return np.asarray(ra, float), np.asarray(dec, float)


# --------------------------------------------------------------------------
# 7. 盲检测（对照路径：median + k·bgnoise 阈值 + 连通域 + 质心）
# --------------------------------------------------------------------------
def blind_detect(img, k_sigma=5.0, smooth_sigma=2.0, min_area=2, box=3):
    """返回 (x[], y[], peak[])；纯 numpy 实现，与生产 sdet_api 无关（独立对照口径）。"""
    from scipy.ndimage import find_objects, gaussian_filter, label
    sm = gaussian_filter(np.asarray(img, float), smooth_sigma, mode="nearest")
    # 全局背景噪声 RMS：相邻像素差稳健尺度（免疫结构，与 06.md σ_pix 同法）
    d = np.diff(sm, axis=1)
    bg = float(np.median(np.abs(d - np.median(d))) * MAD_TO_SIGMA / np.sqrt(2.0))
    thr = float(np.median(sm)) + k_sigma * bg
    mask = sm > thr
    lab, n = label(mask)
    if n == 0:
        return np.zeros(0), np.zeros(0), np.zeros(0)
    xs, ys, pk = [], [], []
    sizes = np.bincount(lab.ravel(), minlength=n + 1)
    # find_objects 给包围盒，避免对每个标签做全图 argwhere（大帧上是 O(N_label·N_pix)）
    for i, sl in enumerate(find_objects(lab), start=1):
        if sl is None or sizes[i] < min_area:
            continue
        y0, y1 = sl[0].start, sl[0].stop - 1
        x0, x1 = sl[1].start, sl[1].stop - 1
        sub = np.where(lab[y0:y1 + 1, x0:x1 + 1] == i, sm[y0:y1 + 1, x0:x1 + 1], -np.inf)
        j = np.unravel_index(np.argmax(sub), sub.shape)
        xs.append(x0 + j[1]); ys.append(y0 + j[0]); pk.append(float(sub[j]))
    return np.asarray(xs, float), np.asarray(ys, float), np.asarray(pk, float)


def centroid_refine(img, x0, y0, box=3):
    """一阶矩质心（背景由环带中位扣除），用于盲检测路径的定位。"""
    H, W = img.shape
    ix0 = max(int(round(x0)) - box, 0); ix1 = min(int(round(x0)) + box, W - 1)
    iy0 = max(int(round(y0)) - box, 0); iy1 = min(int(round(y0)) + box, H - 1)
    if ix1 <= ix0 or iy1 <= iy0:
        return x0, y0
    sub = np.asarray(img[iy0:iy1 + 1, ix0:ix1 + 1], float)
    xs = np.arange(ix0, ix1 + 1, dtype=float)
    ys = np.arange(iy0, iy1 + 1, dtype=float)
    bkg = float(np.median(sub))
    w = np.clip(sub - bkg, 0, None)
    s = w.sum()
    if s <= 0:
        return x0, y0
    return float((w.sum(1) @ ys) / s), float((w.sum(0) @ xs) / s)


def match_catalogs(xa, ya, xb, yb, tol):
    """最近邻匹配（tol 像素），返回 (idx_a, idx_b, sep)；贪心，按距离升序。"""
    from scipy.spatial import cKDTree
    if xa.size == 0 or xb.size == 0:
        return np.zeros(0, int), np.zeros(0, int), np.zeros(0)
    ta = cKDTree(np.c_[xa, ya]); tb = cKDTree(np.c_[xb, yb])
    d, i = tb.query(np.c_[xa, ya], distance_upper_bound=tol)
    ok = np.isfinite(d)
    ia = np.nonzero(ok)[0]
    ib = i[ok]
    order = np.argsort(d[ok])
    ia, ib = ia[order], ib[order]
    seen = set(); ka, kb = [], []
    for a, b in zip(ia, ib):
        if b in seen:
            continue
        seen.add(b); ka.append(a); kb.append(b)
    ka = np.asarray(ka, int); kb = np.asarray(kb, int)
    sep = np.hypot(xa[ka] - xb[kb], ya[ka] - yb[kb]) if ka.size else np.zeros(0)
    return ka, kb, sep


class Timer:
    def __init__(self, label):
        self.label = label
    def __enter__(self):
        self.t0 = time.perf_counter(); return self
    def __exit__(self, *a):
        self.dt = time.perf_counter() - self.t0
        print(f"[scia] {self.label}: {self.dt:.3f} s")
