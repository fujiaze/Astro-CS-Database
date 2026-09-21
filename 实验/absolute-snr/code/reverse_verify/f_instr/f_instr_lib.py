#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR (裁决 A6) 公共库: 物理正向渲染 + 星点通量口径估计器 + 度量.

只读研究脚本; 不改 lib/ docs/ eng/tests/ ci/; 中间产物落 run/reverse_verify/f_instr/.

======================================================================
噪声模型 (报告中"方法"节照抄)
======================================================================
逐像素 i, 全部噪声过程按物理顺序作用:

  (1) 场景均值 (电子):      lam_i = R_i * g * (S_i + f_i) + d
        S_i  天光+背景均值 [ADU]   (取自真实 L4 帧, 源掩膜 + 分块 sigma-clip 中值平滑)
        f_i  注入源均值     [ADU]   (= F_true * p_i, p 为归一化 PSF)
        R_i  残余相对响应 (平场误差/空间增益), E[R]=1
        g    转换增益 [e-/ADU]
        d    暗电流均值 [e-]  (d = i_dark * t_exp)
  (2) 散粒噪声:             n_i ~ Poisson(lam_i)          <- 光子/电子散粒
  (3) 读出噪声:             m_i ~ Normal(0, RN^2)   [e-]
  (4) 电子->ADU:            a_i = (n_i + m_i) / g
  (5) 饱和:                 a_i = min(a_i, SAT); 饱和位 = (未截断值 >= SAT)
  (6) 量化:                 a_i = floor(a_i + 0.5)        <- 整数 ADU

**不是算术改像素值**: 天光/源各自过 Poisson, 读出过 Gaussian, 再经增益/饱和/量化.
天光梯度 = S_i 里的低阶空间项 (真实帧自带) + 可注入的受控梯度 (add_sky_gradient).
负例 (真值"无效应") 下度量必须归零: 见 exp3 (seeing 扫描) 与 exp4 的 R=1 支路.
"""

import json
import os
import numpy as np

# ----------------------------------------------------------------------
# 常数与默认值 (全部显式声明, 报告里逐个交代)
# ----------------------------------------------------------------------
GAIN_DEFAULT = 1.0      # e-/ADU   (真实帧头无 GAIN 键 => 声明式取值, 见 exp0)
RN_DEFAULT = 4.0        # e- rms
DARK_RATE = 0.02        # e-/pix/s
SAT_ADU = 55000.0       # ADU 满井 (16bit 域; 与 star_detector.cpp:167 的 50000 同量级)
EXP_TIME = 300.0        # s
PSF_BETA = 3.5          # Moffat 指数 (默认)


# ----------------------------------------------------------------------
# PSF
# ----------------------------------------------------------------------
def moffat_fwhm_to_alpha(fwhm, beta=PSF_BETA):
    """Moffat: p(r)=(beta-1)/(pi a^2)(1+r^2/a^2)^(-beta); FWHM=2a sqrt(2^(1/beta)-1)."""
    return fwhm / (2.0 * np.sqrt(2.0 ** (1.0 / beta) - 1.0))


def moffat_profile(dx, dy, fwhm, beta=PSF_BETA):
    a = moffat_fwhm_to_alpha(fwhm, beta)
    r2 = dx * dx + dy * dy
    return (beta - 1.0) / (np.pi * a * a) * (1.0 + r2 / (a * a)) ** (-beta)


def gaussian_profile(dx, dy, fwhm):
    s = fwhm / 2.3548200450309493
    return np.exp(-0.5 * (dx * dx + dy * dy) / (s * s)) / (2.0 * np.pi * s * s)


def render_psf(shape, x0, y0, fwhm, total=1.0, kind="moffat", beta=PSF_BETA):
    """在 (ny,nx) 网格上渲染中心 (x0,y0) 的 PSF, 数值和 = total."""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    dx = xx - x0
    dy = yy - y0
    if kind == "moffat":
        p = moffat_profile(dx, dy, fwhm, beta)
    elif kind == "gaussian":
        p = gaussian_profile(dx, dy, fwhm)
    elif kind == "empirical":
        raise ValueError("empirical 请用 render_psf_image")
    else:
        raise ValueError(kind)
    return p / p.sum() * total


def render_psf_image(shape, x0, y0, psf_img, total=1.0, sub=1):
    """把经验 PSF 图像 (奇数边长, 中心在 [h//2,w//2]) 放到 (ny,nx) 网格, 数值和 = total."""
    ny, nx = shape
    h, w = psf_img.shape
    cy, cx = h // 2, w // 2
    out = np.zeros((ny, nx), dtype=np.float64)
    for j in range(h):
        for i in range(w):
            yy = int(np.floor(y0)) + (j - cy)
            xx = int(np.floor(x0)) + (i - cx)
            fy = (y0 - np.floor(y0)) if (j - cy) == 0 else 0.0
            fx = (x0 - np.floor(x0)) if (i - cx) == 0 else 0.0
            _ = (fy, fx)
            if 0 <= yy < ny and 0 <= xx < nx:
                out[yy, xx] += psf_img[j, i]
    return out / out.sum() * total


def encircled_energy(fwhm, r, beta=PSF_BETA, kind="moffat", n=6000):
    """CoG(r) = 孔径内能量占比 (0..1)."""
    rr = np.linspace(0.0, r, n)
    if kind == "moffat":
        a = moffat_fwhm_to_alpha(fwhm, beta)
        dens = (beta - 1.0) / (np.pi * a * a) * (1.0 + rr * rr / (a * a)) ** (-beta)
    else:
        s = fwhm / 2.3548200450309493
        dens = np.exp(-0.5 * rr * rr / (s * s)) / (2.0 * np.pi * s * s)
    return float(np.trapezoid(2.0 * np.pi * rr * dens, rr))


# ----------------------------------------------------------------------
# 物理正向渲染
# ----------------------------------------------------------------------
class NoiseModel:
    def __init__(self, gain=GAIN_DEFAULT, rn=RN_DEFAULT, dark_rate=DARK_RATE,
                 sat=SAT_ADU, exp=EXP_TIME):
        self.gain = float(gain)
        self.rn = float(rn)
        self.dark = float(dark_rate) * float(exp)
        self.sat = float(sat)

    def as_dict(self):
        return {"gain_e_per_adu": self.gain, "read_noise_e": self.rn,
                "dark_e": self.dark, "saturation_adu": self.sat}

    def render(self, sky_adu, src_adu=None, response=None, rng=None, quantize=True):
        """正向渲染一帧. 返回 (adu, saturated_mask)."""
        rng = np.random.default_rng() if rng is None else rng
        g = self.gain
        R = np.ones_like(sky_adu) if response is None else response
        src = 0.0 if src_adu is None else src_adu
        lam = R * g * (sky_adu + src) + self.dark
        lam = np.maximum(lam, 0.0)
        e = rng.poisson(lam).astype(np.float64)
        if self.rn > 0:
            e = e + rng.normal(0.0, self.rn, size=e.shape)
        a = e / g
        sat = a >= self.sat
        a = np.minimum(a, self.sat)
        if quantize:
            a = np.floor(a + 0.5)
        return a, sat


def add_sky_gradient(shape, amp_frac=0.15, angle_deg=30.0):
    """受控天光梯度 (乘性场, 作用在 ADU 域天光均值上), 峰峰 = 2*amp_frac."""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    th = np.deg2rad(angle_deg)
    u = (xx / max(nx - 1, 1) - 0.5) * np.cos(th) + (yy / max(ny - 1, 1) - 0.5) * np.sin(th)
    return 1.0 + amp_frac * 2.0 * u


def smooth_sky_map(frame, box=64, clip=3.0):
    """真实帧 -> 无噪声天光均值图: 源掩膜 + 分块 sigma-clip 中值 + 双线性上采样."""
    ny, nx = frame.shape
    nby, nbx = max(ny // box, 1), max(nx // box, 1)
    med = np.zeros((nby, nbx))
    for i in range(nby):
        for j in range(nbx):
            b = frame[i * box:(i + 1) * box, j * box:(j + 1) * box].ravel()
            m = np.median(b)
            s = 1.4826 * np.median(np.abs(b - m)) + 1e-9
            for _ in range(3):
                k = np.abs(b - m) < clip * s
                if k.sum() < 0.2 * b.size:
                    break
                m = np.median(b[k])
                s = 1.4826 * np.median(np.abs(b[k] - m)) + 1e-9
            med[i, j] = m
    yi = np.clip((np.arange(ny) + 0.5) / box - 0.5, 0, nby - 1)
    xi = np.clip((np.arange(nx) + 0.5) / box - 0.5, 0, nbx - 1)
    y0 = np.floor(yi).astype(int); x0 = np.floor(xi).astype(int)
    y1 = np.minimum(y0 + 1, nby - 1); x1 = np.minimum(x0 + 1, nbx - 1)
    fy = (yi - y0)[:, None]; fx = (xi - x0)[None, :]
    return (med[np.ix_(y0, x0)] * (1 - fy) * (1 - fx) + med[np.ix_(y1, x0)] * fy * (1 - fx)
            + med[np.ix_(y0, x1)] * (1 - fy) * fx + med[np.ix_(y1, x1)] * fy * fx)


def robust_sky_stats(patch, clip=3.0, iters=4):
    """sigma-clip 的 (中值, 标准差) —— 生产 cat.background 的代理口径."""
    b = np.asarray(patch, dtype=np.float64).ravel()
    m = np.median(b)
    s = 1.4826 * np.median(np.abs(b - m)) + 1e-9
    for _ in range(iters):
        k = np.abs(b - m) < clip * s
        if k.sum() < 10:
            break
        m = np.median(b[k]); s = 1.4826 * np.median(np.abs(b[k] - m))
    return float(m), float(max(s, 1e-9))


# ----------------------------------------------------------------------
# 局部窗口工具
# ----------------------------------------------------------------------
def local_cutout(frame, x0, y0, R):
    ny, nx = frame.shape
    xi, yi = int(np.floor(x0)), int(np.floor(y0))
    x0b, x1b = max(0, xi - R), min(nx, xi + R + 1)
    y0b, y1b = max(0, yi - R), min(ny, yi + R + 1)
    return frame[y0b:y1b, x0b:x1b].astype(np.float64), x0b, y0b


def _dist2_stack(shape, x0, y0, sub=5):
    """(sub*sub, ny, nx) 子采样平方距离栈 (亚像素精确孔径)."""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    off = (np.arange(sub) + 0.5) / sub - 0.5
    st = np.empty((sub * sub, ny, nx), dtype=np.float64)
    k = 0
    for oy in off:
        for ox in off:
            st[k] = (xx + ox - x0) ** 2 + (yy + oy - y0) ** 2
            k += 1
    return st


# ----------------------------------------------------------------------
# 估计器
# ----------------------------------------------------------------------
def est_box5(frame, x0, y0, bkg=None, **kw):
    """生产口径复刻: 5x5 固定盒 + 正性截断 (star_detector.cpp:139-151 的 m00)."""
    cut, _, _ = local_cutout(frame, x0, y0, 4)
    if bkg is None:
        bkg, _ = robust_sky_stats(cut)
    xi = int(round(x0)) - (int(np.floor(x0)) - 4)
    yi = int(round(y0)) - (int(np.floor(y0)) - 4)
    ny, nx = cut.shape
    x0b, x1b = max(0, xi - 2), min(nx, xi + 3)
    y0b, y1b = max(0, yi - 2), min(ny, yi + 3)
    sub = cut[y0b:y1b, x0b:x1b] - bkg
    return float(np.sum(np.where(sub > 0, sub, 0.0)))


def est_iso(frame, x0, y0, bkg=None, nsigma=5.0, sigma=None, maxpix=400, R=20, **kw):
    """sdet 等照度口径复刻: 阈 = bkg + nsigma*sigma 的连通域内 (a-bkg)>0 求和
    (sdet_detector.cpp:281-294)."""
    cut, x0b, y0b = local_cutout(frame, x0, y0, R)
    if bkg is None:
        b0, s0 = robust_sky_stats(cut)
        bkg = b0
        sigma = s0 if sigma is None else sigma
    sigma = sigma or 1.0
    thr = bkg + nsigma * sigma
    ny, nx = cut.shape
    xi = min(max(int(round(x0)) - x0b, 0), nx - 1)
    yi = min(max(int(round(y0)) - y0b, 0), ny - 1)
    if cut[yi, xi] <= thr:
        y0w, y1w = max(0, yi - 2), min(ny, yi + 3)
        x0w, x1w = max(0, xi - 2), min(nx, xi + 3)
        w = cut[y0w:y1w, x0w:x1w]
        k = np.unravel_index(np.argmax(w), w.shape)
        yi, xi = y0w + k[0], x0w + k[1]
        if cut[yi, xi] <= thr:
            return 0.0, 0
    seen = np.zeros((ny, nx), dtype=bool)
    stack = [(yi, xi)]
    seen[yi, xi] = True
    pix = []
    while stack and len(pix) < maxpix:
        cy, cx = stack.pop()
        pix.append((cy, cx))
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                nyy, nxx = cy + dy, cx + dx
                if 0 <= nyy < ny and 0 <= nxx < nx and not seen[nyy, nxx]:
                    if cut[nyy, nxx] > thr:
                        seen[nyy, nxx] = True
                        stack.append((nyy, nxx))
    f = 0.0
    for (cy, cx) in pix:
        v = cut[cy, cx] - bkg
        if v > 0:
            f += v
    return float(f), len(pix)


def aperture_curve(frame, x0, y0, radii, ann=(8.0, 12.0), sub=5, bkg=None):
    """一次性给出多半径孔径通量 (亚像素精确, 环带本底). 返回 (list[flux], bkg)."""
    rmax = max(max(radii), ann[1] if ann else 0.0)
    R = int(np.ceil(rmax)) + 1
    cut, x0b, y0b = local_cutout(frame, x0, y0, R)
    d2 = _dist2_stack(cut.shape, x0 - x0b, y0 - y0b, sub)
    if bkg is None:
        if ann is not None:
            wann = (d2 >= ann[0] ** 2).all(axis=0) & (d2 <= ann[1] ** 2).all(axis=0)
            if wann.sum() >= 8:
                bkg, _ = robust_sky_stats(cut[wann])
            else:
                bkg, _ = robust_sky_stats(cut)
        else:
            bkg, _ = robust_sky_stats(cut)
    out = []
    for r in radii:
        w = (d2 <= r * r).mean(axis=0)
        out.append(float(np.sum(w * (cut - bkg))))
    return out, float(bkg)


def est_aperture(frame, x0, y0, r, bkg=None, ann=(8.0, 12.0), **kw):
    """圆孔径 (亚像素精确) + 环带本底; 不做孔径改正."""
    f, _ = aperture_curve(frame, x0, y0, [r], ann=ann, bkg=bkg)
    return f[0]


def est_aperture_cog(frame, x0, y0, r, fwhm, beta=PSF_BETA, kind="moffat", **kw):
    """孔径 + 增长曲线改正到无穷孔径 (显式孔径改正)."""
    raw = est_aperture(frame, x0, y0, r, **kw)
    return float(raw / max(encircled_energy(fwhm, r, beta=beta, kind=kind), 1e-6))


def est_psf_nlsq(frame, x0, y0, fwhm, beta=PSF_BETA, kind="moffat", box=None, **kw):
    """PSF 拟合总通量: (A,b) 解析 + 中心 (dx,dy) 非线性最小二乘."""
    from scipy.optimize import least_squares
    rad = box if box else max(6.0, 3.0 * fwhm)
    R = int(np.ceil(rad)) + 1
    cut, x0b, y0b = local_cutout(frame, x0, y0, R)
    yy, xx = np.mgrid[0:cut.shape[0], 0:cut.shape[1]]
    lx, ly = x0 - x0b, y0 - y0b
    m = (xx - lx) ** 2 + (yy - ly) ** 2 <= rad * rad

    def resid(p):
        A, b, dx, dy = p
        pr = render_psf(cut.shape, lx + dx, ly + dy, fwhm, total=1.0, kind=kind, beta=beta)
        return (b + A * pr)[m] - cut[m]

    b0, _ = robust_sky_stats(cut)
    A0 = max(np.sum(np.where(cut - b0 > 0, cut - b0, 0.0)), 1.0)
    try:
        sol = least_squares(resid, x0=np.array([A0, b0, 0.0, 0.0]), method="lm", max_nfev=300)
        return float(sol.x[0])
    except Exception:
        return float("nan")


def est_psf_optimal(frame, x0, y0, fwhm, beta=PSF_BETA, kind="moffat", box=None,
                    bkg=None, sigma=None, **kw):
    """Naylor/Horne 最优提取 (PSF 加权): F = sum(p(d-b)/var) / sum(p^2/var)."""
    rad = box if box else max(6.0, 4.0 * fwhm)
    R = int(np.ceil(rad)) + 1
    cut, x0b, y0b = local_cutout(frame, x0, y0, R)
    if bkg is None:
        b0, s0 = robust_sky_stats(cut)
        bkg = b0
        sigma = s0 if sigma is None else sigma
    sigma = sigma or 1.0
    yy, xx = np.mgrid[0:cut.shape[0], 0:cut.shape[1]]
    lx, ly = x0 - x0b, y0 - y0b
    m = (xx - lx) ** 2 + (yy - ly) ** 2 <= rad * rad
    pr = render_psf(cut.shape, lx, ly, fwhm, total=1.0, kind=kind, beta=beta)
    var = sigma * sigma
    num = np.sum(pr[m] * (cut[m] - bkg))
    den = np.sum(pr[m] * pr[m])
    return float(num / max(den, 1e-30) / var * var)


def est_kron(frame, x0, y0, bkg=None, k=2.5, rmax=15.0, **kw):
    """SExtractor FLUX_AUTO 式 Kron 自适应孔径."""
    cut, x0b, y0b = local_cutout(frame, x0, y0, int(rmax) + 2)
    if bkg is None:
        bkg, _ = robust_sky_stats(cut)
    yy, xx = np.mgrid[0:cut.shape[0], 0:cut.shape[1]]
    r1 = np.hypot(xx - (x0 - x0b), yy - (y0 - y0b))
    sub = np.where((r1 <= 6.0) & (cut - bkg > 0), cut - bkg, 0.0)
    tot = sub.sum()
    if tot <= 0:
        return 0.0
    rk = float(np.sum(sub * r1) / tot)
    rk = min(max(rk, 1.5), rmax / k)
    return est_aperture(frame, x0, y0, k * rk, bkg=bkg)


# ----------------------------------------------------------------------
# 度量
# ----------------------------------------------------------------------
def dmag(f_rec, f_true):
    """-2.5 log10(F_rec/F_true); 非法值 -> nan."""
    f_rec = np.asarray(f_rec, dtype=np.float64)
    ok = np.isfinite(f_rec) & (f_rec > 0)
    out = np.full(f_rec.shape, np.nan)
    out[ok] = -2.5 * np.log10(f_rec[ok] / f_true)
    return out


def robust_loc_scale(x):
    x = np.asarray(x, dtype=np.float64)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return float("nan"), float("nan")
    m = float(np.median(x))
    s = float(1.4826 * np.median(np.abs(x - m)))
    return m, s


def save_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=2, ensure_ascii=False, default=float)
    return path
