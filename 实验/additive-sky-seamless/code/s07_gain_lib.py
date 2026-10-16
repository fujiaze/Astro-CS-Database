#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07（P5 乘性增益研究臂）公共库：**独立匹配恒星扣背景积分通量比**驱动的逐帧
残余标量增益 g_i = exp(eta_i)，四臂合并与判据量。

科学口径（先声明，后跑；见 run/ACSDSDOC-02/S07/REPORT_S07.md）：
  * 观测模型（与 docs/science/sky/UPM.md、REPORT_paper.md §2 一致）：
        y_i(x) = g_i * S(x) + a_i(x) + eps
    P5 只拟合 P1（`k_photo*m(x,y)`）之后**留存的帧间残余差模**；本库的 g_i 是
    **逐帧标量**（0 维空间），二维 g_i(x) 不在本轮。
  * 增益证据 = **独立匹配恒星的扣背景积分通量比**：
        u_is - u_js ~= eta_i - eta_j + color/PSF 项 + 噪声，  u_is = log f_is
    其中 f_is = 孔径内 (y - delta) 之和 − 孔径面积 × 环带中位（扣背景）。
    **禁止**使用「背景梯度线性回归斜率」作为星光度证据；本库不做任何像素比值
    或对数的全图运算，只在星点孔径上取比。
  * 规范：每个重叠连通分量内固定 sum_i w_i eta_i = 0（w_i = 该帧参与配对的星数）。
    eta=0 是**模型选取的阴性假设**，不是 gauge 约束得到的物理结论。
  * 校正语义：corrected = (y - delta) / g；方差随 g^-2 缩放（本库不传播权重，
    权重链属 P2 单元，登记为 limitation）。
  * 负例：eta 乱序（shuffle）臂 = 已知无因果的增益注入；真值 g 注入臂 = 已知阳性。
"""
from __future__ import annotations

import hashlib
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
T08CODE = os.path.join(REPO, "run", "ACSDSDOC-01", "T08", "code")
if T08CODE not in sys.path:
    sys.path.insert(0, T08CODE)

from t08_common import W, H, PITCH, PIXSCALE_ARCSEC, derive_rng, _gauss_blur  # noqa: E402
from t08_arms import combine_equal, feather_weights                          # noqa: E402

# 判据/世界默认（预声明，跑之前冻结）
SEAM_GATE_REL = 1e-2          # UPM.md §4 表：接缝相对台阶门
R_AP = 5.0                    # 星光度孔径半径（px）
R_IN, R_OUT = 7.0, 10.0       # 背景环带（px）
MIN_SEP_PX = 9.0              # 检测星最小间距（px），避免混叠
DETECT_NSIG = 4.0             # 检测阈值（局部背景 MAD 的倍数）
MATCH_PX = 1.5                # 跨帧匹配容差（px）
MIN_PAIR_STARS = 6            # 一对帧进增益求解所需的最少共同匹配星数


# --------------------------------------------------------------------------- #
# 世界侧（不改 t08_common；本库只读复用其世界生成）
# --------------------------------------------------------------------------- #
def star_truth_image(star_list, shape, sigma):
    """真值星点图（仅星分量，按给定 PSF sigma 高斯），用于光度真值对照。"""
    Hh, Ww = shape
    y, x = np.mgrid[0:Hh, 0:Ww]
    img = np.zeros((Hh, Ww), float)
    for (xi, yi, ai) in star_list:
        img += ai * np.exp(-((x - xi) ** 2 + (y - yi) ** 2) / (2.0 * max(sigma, 1e-6) ** 2))
    return img


def apply_color_term(world, color_slope):
    """给每个星注入与颜色相关的帧间乘性项（只作用于星分量）。

    color_slope[k] = b_k，星 s 的颜色 c_s ∈ [-1,1]（由星位置确定性派生）。
    星通量乘 10^(-0.4 * b_k * c_s)。真值 S 不变（颜色项只进观测）。
    返回 (raw2, colors)，raw2 是拷贝后的 raw。**不加性背景/PSF 一致性的前提仍成立**。
    """
    nf = world["raw"].shape[0]
    stars = world["stars"]
    raw2 = world["raw"].copy()
    if not stars or not np.any(np.array(color_slope, float) != 0.0):
        return raw2, [0.0] * len(stars)
    cols = []
    for (xi, yi, ai) in stars:
        h = hashlib.sha256(f"color|{xi:.3f}|{yi:.3f}".encode()).digest()
        cols.append(float(int.from_bytes(h[:4], "little")) / 2 ** 31 - 1.0)
    cols = np.array(cols)
    for k in range(nf):
        b = float(color_slope[k])
        if b == 0.0:
            continue
        staronly = np.zeros((H, W), float)
        y, x = np.mgrid[0:H, 0:W]
        sg = world["cfg"].get("psf_sigma", [1.0] * nf)[k]
        for (xi, yi, ai), c in zip(stars, cols):
            f = 10.0 ** (-0.4 * b * c) - 1.0
            staronly += ai * f * np.exp(-((x - xi) ** 2 + (y - yi) ** 2) / (2.0 * sg ** 2))
        raw2[k] += staronly
    return raw2, [float(c) for c in cols]


# --------------------------------------------------------------------------- #
# 检测 / 匹配 / 孔径测光
# --------------------------------------------------------------------------- #
def detect_stars(img, mask=None, nsig=DETECT_NSIG, min_sep=MIN_SEP_PX):
    """数据驱动检测：局部极大 + 局部 MAD 阈值 + 最小间距。返回 [(x,y,peak)]。"""
    from scipy.ndimage import maximum_filter
    a = np.asarray(img, float)
    fin = np.isfinite(a)
    if mask is not None:
        fin &= mask
    if fin.sum() < 16:
        return []
    med = float(np.median(a[fin]))
    mad = 1.4826 * float(np.median(np.abs(a[fin] - med)))
    sig = max(mad, 1e-9)
    mx = maximum_filter(np.where(fin, a, -np.inf), size=3)
    cand = fin & (a >= mx) & ((a - med) > nsig * sig)
    ys, xs = np.nonzero(cand)
    order = np.argsort(-(a[ys, xs]))
    picked = []
    for i in order:
        x, y = float(xs[i]), float(ys[i])
        if all((x - px) ** 2 + (y - py) ** 2 >= min_sep ** 2 for px, py, _ in picked):
            picked.append((x, y, float(a[int(y), int(x)])))
    return picked


def match_pair(dets_i, dets_j, tol=MATCH_PX):
    """两帧之间的最近邻匹配（同一像素网格 ⇒ 直接用像素坐标）。"""
    out = []
    for (xi, yi, _pi) in dets_i:
        best, bd = None, tol ** 2
        for (xj, yj, _pj) in dets_j:
            dd = (xj - xi) ** 2 + (yj - yi) ** 2
            if dd <= bd:
                best, bd = (xj, yj), dd
        if best is not None:
            out.append((xi, yi, best[0], best[1]))
    return out


def overlap_pairs(covers):
    """覆盖区间有交集的帧对（= 帧重叠图的边）。"""
    out = []
    n = len(covers)
    for i in range(n):
        for j in range(i + 1, n):
            x0 = max(covers[i][0], covers[j][0])
            x1 = min(covers[i][1], covers[j][1])
            if x1 - x0 > 0:
                out.append((i, j, float(x1 - x0)))
    return out


def aperture_flux_err(img, x, y, r_ap=R_AP, r_in=R_IN, r_out=R_OUT):
    """孔径通量的噪声尺度：sqrt(n_ap)·1.4826·MAD(annulus)（逐像素独立噪声口径）。"""
    h, w = img.shape
    if not (np.isfinite(x) and np.isfinite(y)):
        return float("nan")
    x0, x1 = int(np.floor(x - r_out)) - 1, int(np.ceil(x + r_out)) + 2
    y0, y1 = int(np.floor(y - r_out)) - 1, int(np.ceil(y + r_out)) + 2
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h:
        return float("nan")
    sub = np.asarray(img[y0:y1, x0:x1], float)
    yy, xx = np.mgrid[y0:y1, x0:x1]
    rr = np.hypot(xx - x, yy - y)
    ap = (rr <= r_ap) & np.isfinite(sub)
    an = (rr >= r_in) & (rr <= r_out) & np.isfinite(sub)
    if ap.sum() < 5 or an.sum() < 5:
        return float("nan")
    med = float(np.median(sub[an]))
    mad = 1.4826 * float(np.median(np.abs(sub[an] - med)))
    return float(np.sqrt(ap.sum()) * max(mad, 1e-9))


def pair_obs(frames, dets, pairs, min_flux=1.0, rap=R_AP):
    """逐重叠帧对的**扣背景孔径通量比**观测量（增益证据本体）。

    每个观测量：d = log f_i − log f_j，var = (σ_i/f_i)² + (σ_j/f_j)²。
    单位 dex；只使用匹配恒星的**扣背景积分通量**，不含任何背景梯度回归。
    """
    obs = []
    for (i, j, ov_px) in pairs:
        for (xi, yi, xj, yj) in match_pair(dets[i], dets[j]):
            fi = aperture_flux(frames[i], xi, yi, r_ap=rap, r_in=rap + 2.0, r_out=rap + 5.0)
            fj = aperture_flux(frames[j], xj, yj, r_ap=rap, r_in=rap + 2.0, r_out=rap + 5.0)
            if not (np.isfinite(fi) and np.isfinite(fj)) or fi <= min_flux or fj <= min_flux:
                continue
            ei = aperture_flux_err(frames[i], xi, yi, r_ap=rap, r_in=rap + 2.0, r_out=rap + 5.0)
            ej = aperture_flux_err(frames[j], xj, yj, r_ap=rap, r_in=rap + 2.0, r_out=rap + 5.0)
            if not (np.isfinite(ei) and np.isfinite(ej)):
                continue
            var = (ei / fi) ** 2 + (ej / fj) ** 2
            if not np.isfinite(var) or var <= 0:
                continue
            obs.append(dict(i=int(i), j=int(j), x=float(xi), y=float(yi),
                            d=float(np.log(fi) - np.log(fj)), var=float(var),
                            f_i=float(fi), f_j=float(fj), overlap_px=float(ov_px)))
    return obs


def estimate_scalar_gain_obs(obs, nframes, min_obs_pair=MIN_PAIR_STARS):
    """由配对通量比观测量解逐帧标量 eta（加权最小二乘 + 逐分量规范）。"""
    pairs = {}
    for o in obs:
        pairs.setdefault((o["i"], o["j"]), []).append(o)
    used = {k: v for k, v in pairs.items() if len(v) >= min_obs_pair}
    if not used:
        return dict(status="NO_PAIR", eta=np.zeros(nframes), sigma=np.full(nframes, np.inf),
                    n_pairs=0, pair_n=[], chi2=float("nan"), dof=0, n_obs=0)
    w = np.zeros(nframes)
    for (i, j), v in used.items():
        w[i] += len(v)
        w[j] += len(v)
    # 规范 sum_i w_i eta_i = 0 ⇒ 消去最后一个**参与**的帧
    active = [k for k in range(nframes) if w[k] > 0]
    if len(active) < 2:
        return dict(status="SINGLE_FRAME", eta=np.zeros(nframes),
                    sigma=np.full(nframes, np.inf), n_pairs=0, pair_n=[], chi2=float("nan"),
                    dof=0, n_obs=0)
    last = active[-1]
    red = [k for k in active if k != last]
    ridx = {k: c for c, k in enumerate(red)}
    rows, b, wd = [], [], []
    for (i, j), v in used.items():
        for o in v:
            row = np.zeros(len(red))
            for f, sgn in ((i, 1.0), (j, -1.0)):
                if f == last:
                    for c, k in ridx.items():
                        row[c] += sgn * (-w[k] / max(w[last], 1e-30))
                else:
                    row[ridx[f]] += sgn
            rows.append(row)
            b.append(o["d"])
            wd.append(1.0 / max(o["var"], 1e-30))
    A = np.asarray(rows, float)
    b = np.asarray(b, float)
    wd = np.asarray(wd, float)
    Aw = A * np.sqrt(wd)[:, None]
    bw = b * np.sqrt(wd)
    AtA = Aw.T @ Aw + 1e-12 * np.eye(len(red))
    redsol = np.linalg.solve(AtA, Aw.T @ bw)
    eta = np.zeros(nframes)
    for c, k in ridx.items():
        eta[k] = redsol[c]
    eta[last] = -float(np.dot([w[k] for k in red], redsol) / max(w[last], 1e-30))
    cov_red = np.linalg.inv(AtA)
    T = np.zeros((nframes, len(red)))
    for c, k in ridx.items():
        T[k, c] = 1.0
    T[last, :] = -np.array([w[k] for k in red]) / max(w[last], 1e-30)
    cov = T @ cov_red @ T.T
    sigma = np.sqrt(np.maximum(np.diag(cov), 0.0))
    res = A @ redsol - b
    chi2 = float(np.sum(res ** 2 * wd))
    dof = max(len(b) - len(red), 1)
    return dict(status="OK", eta=eta, sigma=sigma, cov=cov.tolist(),
                n_pairs=len(used), pair_n=[len(v) for v in used.values()],
                pair_ij=[list(k) for k in used.keys()],
                chi2=chi2, dof=dof, chi2_red=chi2 / dof, n_obs=len(b),
                gauge="sum_i w_i eta_i = 0", w=[float(v) for v in w])


def pair_residual(obs, eta):
    """给定 eta 的留出配对残差：r = d − (eta_i − eta_j)。

    rms 单位 dex；分母 = 观测量个数 n。chi2_red 用逐观测方差标准化。
    """
    eta = np.asarray(eta, float)
    if not obs:
        return dict(n=0, rms=float("nan"), chi2_red=float("nan"))
    r = np.array([o["d"] - (eta[o["i"]] - eta[o["j"]]) for o in obs], float)
    v = np.array([o["var"] for o in obs], float)
    return dict(n=int(r.size), rms=float(np.sqrt(np.mean(r ** 2))),
                med_abs=float(np.median(np.abs(r))),
                chi2_red=float(np.mean(r ** 2 / np.maximum(v, 1e-30))))


def aperture_flux(img, x, y, r_ap=R_AP, r_in=R_IN, r_out=R_OUT):
    """扣背景孔径积分通量：sum(ap) − n_ap × median(annulus)。非有限/越界返回 nan。"""
    h, w = img.shape
    if not (np.isfinite(x) and np.isfinite(y)):
        return float("nan")
    x0, x1 = int(np.floor(x - r_out)) - 1, int(np.ceil(x + r_out)) + 2
    y0, y1 = int(np.floor(y - r_out)) - 1, int(np.ceil(y + r_out)) + 2
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h:
        return float("nan")
    sub = np.asarray(img[y0:y1, x0:x1], float)
    yy, xx = np.mgrid[y0:y1, x0:x1]
    rr = np.hypot(xx - x, yy - y)
    ap = (rr <= r_ap) & np.isfinite(sub)
    an = (rr >= r_in) & (rr <= r_out) & np.isfinite(sub)
    if ap.sum() < 5 or an.sum() < 5:
        return float("nan")
    return float(sub[ap].sum() - ap.sum() * np.median(sub[an]))


def seam_multiscale(img, mask, covers, ds=(2.0, 4.0, 8.0, 16.0)):
    """法向多尺度接缝诊断：法向半距 d 扫描下的边界相对台阶（有符号）。

    真台阶在 d 扫描下 ≈ 守恒；纯背景梯度项随 d 线性增长。返回 per-d 的
    max|step_rel|、以及每个边界的 d 扫描比值（==4 ⇒ 纯斜坡，==1 ⇒ 纯台阶）。
    """
    out = {}
    for d in ds:
        bs = []
        for k in range(len(covers) - 1):
            xb = int(round(covers[k][1]))
            if covers[k + 1][1] <= covers[k + 1][0]:
                continue
            if xb - int(d) < 0 or xb + int(d) > H:
                continue
            med = np.nanmedian(img, axis=0)
            l = float(np.nanmedian(med[max(xb - int(d), 0):xb]))
            r = float(np.nanmedian(med[xb:xb + int(d)]))
            step = r - l
            bg = 0.5 * (l + r)
            bs.append(dict(x=xb, d=float(d), step_abs=step,
                           step_rel=(step / bg) if bg != 0 else float("nan")))
        vals = [abs(b["step_rel"]) for b in bs if np.isfinite(b["step_rel"])]
        out[f"d{d:g}"] = dict(max_abs_rel=(max(vals) if vals else None),
                              n=len(bs), per_edge=bs)
    # d 扫描比值：|step(4d)| / |step(d)|
    ratios = []
    for b2 in out["d2"]["per_edge"]:
        for b8 in out["d8"]["per_edge"]:
            if b2["x"] == b8["x"] and abs(b2["step_rel"]) > 1e-12:
                ratios.append(abs(b8["step_rel"]) / abs(b2["step_rel"]))
    out["d8_over_d2"] = dict(n=len(ratios),
                             median=float(np.median(ratios)) if ratios else None,
                             max=float(np.max(ratios)) if ratios else None)
    return out


def diffuse_fidelity(img, mask, S_true, sigma_px=8.0):
    """弥散源保真（低频）：高斯平滑后与真值的斜率/偏差/低频谱比/积分真值比。

    只在覆盖 >=1 的像元上比；平滑 sigma_px 抑制星点高频。单位与 img 同。
    """
    from scipy.ndimage import gaussian_filter
    cov = (mask.sum(axis=0) >= 1)
    A = gaussian_filter(np.nan_to_num(img, nan=0.0), sigma_px)
    B = gaussian_filter(np.nan_to_num(S_true, nan=0.0), sigma_px)
    x = B[cov]
    y = A[cov]
    if x.size < 64:
        return dict(n=int(x.size))
    xm, ym = x.mean(), y.mean()
    sxx = float(((x - xm) ** 2).sum())
    slope = float(((x - xm) * (y - ym)).sum() / max(sxx, 1e-30))
    bias = float(ym - slope * xm)
    rms = float(np.sqrt(np.mean((y - (slope * x + bias)) ** 2)))
    tot_ratio = float((y - ym).std() / max((x - xm).std(), 1e-30))

    def lowpow(z):
        z = z - z.mean()
        f = np.fft.fftshift(np.abs(np.fft.fft2(z)) ** 2)
        c = np.array(f.shape) // 2
        r = min(c) // 8
        return float(f[c[0] - r:c[0] + r, c[1] - r:c[1] + r].sum())

    ps, pa = lowpow(B), lowpow(A)
    return dict(n=int(x.size), slope=slope, bias=bias, rms=rms,
                struct_ratio=tot_ratio,
                lowfreq_ratio=float(pa / ps) if ps > 0 else None,
                integ_ratio=float(y.mean() / x.mean()) if x.mean() != 0 else None)


def shuffle_eta(eta, tag="shuffle"):
    """确定性乱序（负对照）：同一组 eta 值随机重排到帧上。"""
    eta = np.asarray(eta, float)
    rng = derive_rng(f"s07|{tag}")
    return eta[rng.permutation(eta.size)]


def field_from_scalar(eta, nframes):
    """逐帧标量增益场 -> (nf,H,W) 的 g 场（exp(eta_k)）。"""
    g = np.zeros((nframes, H, W))
    for k in range(nframes):
        g[k] = float(np.exp(eta[k]))
    return g


def apply_gain(frames, delta, g):
    """corrected = (y − delta)/g。frames/delta/g 形状同为 (nf,H,W)。"""
    return (frames - delta) / np.maximum(g, 1e-6)
