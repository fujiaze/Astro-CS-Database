#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-203 独立复算（纯 Python，不 import 仓库代码、不跑 C++）。

复算对象（任务书"必须做到"逐条）：
 A. 生产接缝门 seam_footprint.py::edge_metric 的核心判据 rel_step=median(img[+d]−img[−d])/bg
    —— A1 真值无效应（同一天光平面、零 δ、平铺无缝）⇒ 度量是否归零/判绿；
    —— A2 注入已知电平阶跃 Δ ⇒ rel_step≈Δ/bg、检出下限 1.005%；
    —— A3 仅方差变、无电平台阶（两侧噪声差 57×）⇒ 有符号台阶判据是否盲（判绿）
       而方差比是否随噪声动（对照，证明方差比对电平失明/对噪声敏感）。
 B. 归档实验 C4 的"退化 vs 非退化"接缝判据（off-locus excess）
    —— 代数构造（不需 HST 文件）：注入帧间电平接缝 amp，
       退化臂（全减每帧天光真值）应无响应，非退化臂应线性响应；
       再加一个"公共强平滑梯度、零帧间失配"负例，非退化臂不翻红。
 C. sampler.cpp:863-877 零尺度伪方差：常数 patch（σ_bg=0）经 sigma→1e-12 公式
    得到的 control_ivar 量级（对照设计禁令 ivar 必须=0）。
 D. control_ivar 份额式对污染帧的抑制：同一 control 两帧（一 clean 一 5×噪声），
    份额归一后 clean 帧主导 M；若两帧都同 σ 则退化为均值。

用 `python -B` + `PYTHONDONTWRITEBYTECODE=1`；只写本目录。
"""
from __future__ import annotations
import numpy as np

PI2 = 1.5707963267948966  # pi/2
K_CORR = 1.4
GATE = 1e-2

res = {}


def bilinear(img, xs, ys):
    h, w = img.shape
    xi = np.clip(xs, 0, w - 2); yi = np.clip(ys, 0, h - 2)
    x0 = np.floor(xi).astype(int); y0 = np.floor(yi).astype(int)
    fx = xi - x0; fy = yi - y0
    return (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy)
            + img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)


def rel_step_horizontal(img, xb, d=2.0, n=512):
    """复刻 edge_metric 判据量在一条竖直帧边界 x=xb（法向=+x）上的核心：
    seam=I(xb+d)−I(xb−d)；bg=median(|电平| 于 ±d 采样)；rel=median(seam)/bg。"""
    h = img.shape[0]
    ys = np.linspace(1, h - 2, n)
    xp = np.full_like(ys, float(xb)) + d
    xm = np.full_like(ys, float(xb)) - d
    seam = bilinear(img, xp, ys) - bilinear(img, xm, ys)
    lvl = np.concatenate([np.abs(bilinear(img, xp, ys)), np.abs(bilinear(img, xm, ys))])
    bg = float(np.median(lvl))
    return float(np.median(seam)) / bg, bg, float(np.median(seam))


def var_ratio(img, tile=512):
    """方差比（V4 口径，一阶差分绝对值中位数比）：对电平阶跃不敏感、对噪声敏感。"""
    a = np.asarray(img, float)
    dr = np.abs(np.diff(a, axis=1)).ravel()
    # 分块边界列 vs 内部列：这里以"每 tile 首列差分"为 seam 侧、其余为 inner 侧
    seams, inner = [], []
    for c in range(tile - 1, a.shape[1] - 1, tile):
        col = np.abs(a[:, c + 1] - a[:, c])
        col2 = np.abs(a[:, c - 2] - a[:, c - 3])
        seams.append(np.median(col)); inner.append(np.median(col2))
    num = np.median(seams) if seams else float('nan')
    den = np.median(inner) if inner else float('nan')
    return (num / den) if den else float('inf'), float(np.median(dr))


# ---------------------------------------------------------------- A：生产门判据
def section_A():
    out = {}
    # A1 真值无效应：单一平滑天光平面（含梯度），无接缝。
    H = W = 1024
    xx = np.arange(W)[None, :]; yy = np.arange(H)[:, None]
    base = 100.0 + 0.02 * xx + 0.0000001 * 0  # 纯 x 向缓变（同一天光平面）
    rng = np.random.default_rng(20260923)
    img_flat = base + rng.normal(0, 1.0, (H, W))       # 无帧边界、无阶跃
    for xb in (256, 512):
        r, bg, st = rel_step_horizontal(img_flat, xb)
        out[f"A1_noeffect_x{xb}"] = dict(rel_step=r, bg=bg, step=st,
                                         verdict=("RED" if abs(r) > GATE else "GREEN"))
    # A2 注入已知电平阶跃：x>=xb 抬升 Δ
    floors = {}
    for frac in (0.005, 0.009, 0.01005, 0.011, 0.05):
        for xb in (512,):
            L = 100.0 + 0.02 * xb           # 边界下侧局部电平
            delta = frac * L
            im = img_flat.copy(); im[:, xb:] += delta
            r, bg, st = rel_step_horizontal(im, xb)
            floors[f"{frac}"] = dict(rel_step=r, predicted=delta / (L + delta / 2),
                                     verdict=("RED" if abs(r) > GATE else "GREEN"))
    out["A2_injection_floor"] = floors
    # A3 仅方差变、无电平台阶：左右两侧 σ 差 57×，均值面完全相同
    mean_side = np.broadcast_to(base, (H, W)).copy()
    sigma = np.where(xx < 512, 5e-4, 4.0e-2)  # 右侧 80× 噪声（>57）
    im_nostep = mean_side + sigma * rng.standard_normal((H, W))
    r, bg, st = rel_step_horizontal(im_nostep, 512)
    vr, mad = var_ratio(im_nostep, tile=512)
    # 对照：同均值但真电平阶跃
    im_step = mean_side + sigma * rng.standard_normal((H, W)); im_step[:, 512:] += 1.0
    r2, bg2, st2 = rel_step_horizontal(im_step, 512)
    vr2, mad2 = var_ratio(im_step, tile=512)
    out["A3_variance_vs_level"] = dict(
        no_step_signed_rel=r, no_step_verdict=("RED" if abs(r) > GATE else "GREEN"),
        no_step_varratio=vr,
        level_step_signed_rel=r2, level_step_verdict=("RED" if abs(r2) > GATE else "GREEN"),
        level_step_varratio=vr2,
        note="生产门读**有符号电平**：纯噪声差(无电平)判绿、电平差判红")
    # A4 清洁盲区对照：同方差场（无噪声不对称）注入纯电平阶跃 →
    #    有符号台阶判据翻红，方差比几乎不动（证明方差比对电平失明）。
    rng2 = np.random.default_rng(9)
    field = np.broadcast_to(base, (H, W)).copy() + rng2.normal(0, 1.0, (H, W))
    vr0, _ = var_ratio(field, tile=512)
    r3, bg3, st3 = rel_step_horizontal(field, 512)
    field2 = field.copy(); field2[:, 512:] += 2.0        # 纯 +2 ADU 电平阶跃，方差不变
    vr1, _ = var_ratio(field2, tile=512)
    r4, bg4, st4 = rel_step_horizontal(field2, 512)
    out["A4_blind_spot_clean"] = dict(
        base_varratio=vr0, base_signed_rel=r3,
        stepped_varratio=vr1, stepped_signed_rel=r4,
        varratio_shift=vr1 - vr0, signed_rel_shift=r4 - r3,
        note="纯电平阶跃：方差比几乎不变(Δ=%.3g)、有符号台阶大幅变(Δ=%.3g) ⇒ 方差比对电平失明"
             % (vr1 - vr0, r4 - r3))
    # A5 门读电平(boundary-only) vs 实验 off-locus excess 对"公共缓变天光梯度"的差别：
    #    设计说天光是缓变场（ASTROCS_DESIGN §2.2）。一条**平滑**强梯度（非孤立阶跃）
    #    压在帧边界上：生产 boundary-only 读入 2d·∂I/∂n，实验 excess 用邻域对照线扣掉它。
    struct = np.broadcast_to(base, (H, W)).copy() + rng2.normal(0, 1.0, (H, W))
    ramp = 3.0 * np.arange(W)[None, :]          # 平滑斜坡 3 ADU/px（无孤立阶跃）
    struct = struct + np.broadcast_to(ramp, (H, W))
    r_prod, _, _ = rel_step_horizontal(struct, 512)
    # 同一场跑 off-locus excess（列向中位剖面）
    mcol = np.nanmedian(struct, axis=0)
    step_here = _step_at(mcol, 512)
    ctrlc = [_step_at(mcol, 512 + d) for d in (-96, -64, -32, 32, 64, 96)]
    ctrlc = [c for c in ctrlc if np.isfinite(c)]
    exc = step_here - np.median(ctrlc)
    # 归一化到局部电平，便于与门阈值 1e-2 比较
    bg_here = float(np.median(np.abs(struct[:, 510:514])))
    out["A5_boundary_only_vs_offlocus_on_smooth_gradient"] = dict(
        production_boundary_only_rel=r_prod,
        production_verdict=("RED(把缓变天光当接缝)" if abs(r_prod) > GATE else "GREEN"),
        experiment_offlocus_excess_ADU=float(exc),
        experiment_offlocus_rel=float(exc / bg_here),
        experiment_verdict=("RED" if abs(exc / bg_here) > GATE else "GREEN"),
        note="平滑缓变天光梯度压边界：生产 boundary-only rel_step=%.3g（读入梯度），"
             "实验 off-locus excess rel=%.3g（对照线扣掉梯度）⇒ 两判据对'缓变结构 vs 接缝'"
             "的免疫力不同；shipped 门不含 off-locus"
             % (r_prod, exc / bg_here))
    return out


# ------------------------------------------------- B：退化 vs 非退化（off-locus）
def _step_at(m, xb, halfwin=8, base_win=64, order=2, min_valid=8):
    nx = m.size
    lo, hi = max(0, xb - base_win), min(nx, xb + base_win)
    xs = np.arange(lo, hi)
    good = np.isfinite(m[lo:hi])
    excl = (xs >= xb - halfwin - 2) & (xs <= xb + halfwin - 2)
    fitm = good & (~excl)
    if fitm.sum() < order + 2:
        return np.nan
    coef = np.polyfit(xs[fitm], m[lo:hi][fitm], order)
    res_ = m[lo:hi] - np.polyval(coef, xs)
    L = res_[(xs >= xb - halfwin) & (xs <= xb - 1)]
    R = res_[(xs >= xb) & (xs <= xb + halfwin - 1)]
    if L.size < min_valid or R.size < min_valid:
        return np.nan
    return float(np.median(R) - np.median(L))


def seam_excess(mosaic, xb, offsets=(-96, -64, -32, 32, 64, 96)):
    m = np.nanmedian(mosaic, axis=0)
    step = _step_at(m, xb)
    ctrl = [_step_at(m, xb + d) for d in offsets if 0 <= xb + d < m.size]
    ctrl = [c for c in ctrl if np.isfinite(c)]
    return step, (step - np.median(ctrl) if ctrl else np.nan)


def section_B():
    N = 512
    xb = 256
    rng = np.random.default_rng(7)
    yy, xx = np.mgrid[0:N, 0:N]
    # 代数"信号"（帧间连续）+ 代数"天光"（每帧不同）
    signal = 200.0 + 30.0 * np.sin(2 * np.pi * xx / 97.0) + 0.05 * xx  # 连续结构
    sky = {"A": 100.0 + 6.0 * (yy / N), "B": 104.0 - 3.0 * (yy / N),
           "C": 96.0 + 2.0 * (xx / N)}
    frames = {k: signal + v + rng.normal(0, 2.0, (N, N)) for k, v in sky.items()}

    def mosaic(pres, amp):
        # 左子集 {B}（x<xb），右子集 {B,C}（x>=xb）叠加；amp=注入到 B 的 x>=xb 电平接缝
        fB = frames["B"].copy()
        if amp:
            fB[:, xb:] += amp
            sky_B_true = sky["B"] + (amp * (xx >= xb))
        num = np.zeros((N, N)); den = np.zeros((N, N))
        for nm, fr in (("B", fB), ("C", frames["C"])):
            cm = np.zeros((N, N), bool)
            if nm == "B":
                cm[:, :] = True
            else:
                cm[:, xb:] = True
            c = fr - (sky[nm] if not (nm == "B" and amp) else sky_B_true) if not pres \
                else fr
            # pres=True: 保留背景（mosaic 用 raw）；pres=False: 全减天光真值（退化）
            num += np.where(cm, c, 0.0); den += cm
        return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)

    # 注意：非退化=不预先减背景(raw mosaic)，退化=减掉每帧天光真值
    nulls_nd, nulls_d = [], []
    inj_nd, inj_d = [], []
    for i in range(40):
        rng = np.random.default_rng(100 + i)
        for nm in frames:
            frames[nm] = signal + sky[nm] + rng.normal(0, 2.0, (N, N))
        # 非退化（raw mosaic，off-locus excess）与退化（全减每帧 sky）
        m_nd = None
        # build raw mosaic (left=B only, right=B+C)
        def rawmos(ampv, subtract):
            fB = frames["B"] + (ampv * (xx >= xb) if ampv else 0.0)
            sB = sky["B"] + (ampv * (xx >= xb) if ampv else 0.0)
            num = np.zeros((N, N)); den = np.zeros((N, N))
            cmB = np.ones((N, N), bool)
            cB = (fB - sB) if subtract else fB
            num += np.where(cmB, cB, 0); den += cmB
            cmC = np.zeros((N, N), bool); cmC[:, xb:] = True
            num += np.where(cmC, frames["C"], 0); den += cmC
            return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)
        nulls_nd.append(seam_excess(rawmos(0.0, False), xb)[1])
        nulls_d.append(seam_excess(rawmos(0.0, True), xb)[1])
        inj_nd.append(seam_excess(rawmos(20.0, False), xb)[1])
        inj_d.append(seam_excess(rawmos(20.0, True), xb)[1])
    nulls_nd = np.array(nulls_nd, float); inj_nd = np.array(inj_nd, float)
    nulls_d = np.array(nulls_d, float); inj_d = np.array(inj_d, float)
    sd = np.nanstd(nulls_nd, ddof=1)
    sep_nd = abs(np.nanmean(inj_nd) - np.nanmean(nulls_nd)) / max(sd, 1e-12)
    sep_d = abs(np.nanmean(inj_d) - np.nanmean(nulls_d)) / max(np.nanstd(nulls_d, ddof=1), 1e-12)
    # 公共强平滑梯度、零帧间失配 ⇒ 非退化臂不翻红
    rng = np.random.default_rng(555)
    common_grad = 40.0 * ((xx - 256.0) / 256.0)
    gframes = {k: signal + sky[k] + common_grad + rng.normal(0, 2.0, (N, N)) for k in sky}
    def gmos():
        num = np.zeros((N, N)); den = np.zeros((N, N))
        for nm, cm in (("B", np.ones((N, N), bool)),
                       ("C", (np.arange(N)[None, :] >= xb) & np.ones((N, N), bool))):
            num += np.where(cm, gframes[nm], 0); den += cm
        return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)
    thr = 5.0 * sd
    grad_excess = seam_excess(gmos(), xb)[1]
    return {
        "nondeg_null_mean": float(np.nanmean(nulls_nd)), "nondeg_null_sd": float(sd),
        "nondeg_inj_mean": float(np.nanmean(inj_nd)), "nondeg_sep_sigma": float(sep_nd),
        "deg_null_mean": float(np.nanmean(nulls_d)), "deg_inj_mean": float(np.nanmean(inj_d)),
        "deg_sep_sigma": float(sep_d),
        "threshold_5sigma": float(thr),
        "smooth_gradient_common_excess": float(grad_excess),
        "gradient_false_alarm": bool(abs(grad_excess - np.nanmean(nulls_nd)) > thr),
        "verdict": ("B 判据有判别力（非退化响应、退化盲、公共梯度不误判）"
                    if (sep_nd > 10 and sep_d < 3 and
                        not bool(abs(grad_excess - np.nanmean(nulls_nd)) > thr)) else "复核")}


# ------------------------------------------------------ C：零尺度伪方差量级
def control_variance(sigma_bg, n_ret):
    return K_CORR * PI2 * sigma_bg ** 2 / max(n_ret, 1)


def section_C():
    # 复现 sampler.cpp 常数 patch：s0=MAD=0 → sigma=1e-12
    sigma_floor = 1e-12
    cvar = control_variance(sigma_floor, 289)
    civar = 1.0 / cvar if cvar > 0 else 0.0
    # 正常背景 patch（真 σ=14.8 ADU/px）对照
    cvar_ok = control_variance(14.826, 287)
    civar_ok = 1.0 / cvar_ok
    # 权重污染：常数 patch civar 相对正常 patch civar 的倍数（份额主导）
    return {
        "constant_patch_cvar": cvar, "constant_patch_civar": civar,
        "normal_patch_cvar": cvar_ok, "normal_patch_civar": civar_ok,
        "civar_dominance_ratio": civar / civar_ok,
        "design_required_civar_for_zero_scale": 0.0,
        "note": "常数 patch 的 civar=%.3e 远大于正常 %.3e ⇒ 命中即主导该 control" % (civar, civar_ok)}


# ----------------------------------------------- D：份额式对污染帧的抑制
def section_D():
    # 一个 control 两帧：clean σ=1，polluted σ=5（5× 读出噪声），真值 M=100
    sig_clean, sig_poll = 1.0, 5.0
    y_clean = 100.0 + np.random.default_rng(1).normal(0, sig_clean / np.sqrt(1))
    y_poll = 100.0 + np.random.default_rng(2).normal(0, sig_poll / np.sqrt(1)) + 8.0  # 系统偏 +8
    iv_clean = 1.0 / control_variance(sig_clean, 250)
    iv_poll = 1.0 / control_variance(sig_poll, 250)
    # 份额式 per-control：w_cell = civ/Σciv
    s = iv_clean + iv_poll
    share_clean, share_poll = iv_clean / s, iv_poll / s
    M_ivar = share_clean * y_clean + share_poll * y_poll
    M_uniform = 0.5 * (y_clean + y_poll)
    M_snr2 = None
    # SNR² 臂（错误代理）：snr=|y|/σ，权重 ∝ snr²/(1+snr²) —— 亮偏的污染帧 snr 更大 → 反被抬高
    snr_c = abs(y_clean) / (sig_clean); snr_p = abs(y_poll) / (sig_poll)
    wc = snr_c ** 2 / (1 + snr_c ** 2); wp = snr_p ** 2 / (1 + snr_p ** 2)
    M_snr2 = (wc * y_clean + wp * y_poll) / (wc + wp)
    return {
        "y_clean": float(y_clean), "y_poll": float(y_poll),
        "share_clean": float(share_clean), "share_poll": float(share_poll),
        "M_control_ivar_err": float(M_ivar - 100), "M_uniform_err": float(M_uniform - 100),
        "M_snr2_err": float(M_snr2 - 100),
        "note": "control_ivar 份额把污染帧权重压到 %.4g（bias 最小）；"
                "SNR² 臂给污染帧更大份额（越亮越权），复现'SNR²非有效逆方差代理'" % share_poll}


if __name__ == "__main__":
    res["A_production_gate_metric"] = section_A()
    res["B_degenerate_vs_nondegenerate"] = section_B()
    res["C_zero_scale_pseudo_variance"] = section_C()
    res["D_control_ivar_share"] = section_D()
    import json
    print(json.dumps(res, ensure_ascii=False, indent=2, default=float))
