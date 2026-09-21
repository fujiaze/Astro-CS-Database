#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ② 合成实验 (已知真值).

合成帧模型 (与生产/真实数据口径一致):
    y_k(p) = a_k * m_k(p) * (T(p) + S0) + g_k(p) + n_k(p)
      T(p)   平滑星云场 (低阶多项式 + 若干高斯团块)
      m_k(p) 已知空间乘法增益 (log10 m 为低阶多项式, pp 5-10%; 线性主导)
      g_k(p) 已知加性梯度 (<=2 阶, pp 4% S0)
      n_k(p) 高斯白噪声 sigma=5 ADU/px
      星点   高斯 PSF (sigma=2px), 逐星真流量对数均匀 1e3..1e5 ADU
星测光: 固定孔径 r=6px + 环形局部天光 10-16px (与生产/Q3 同口径)
  => 局部天光扣除天然消掉光滑加性梯度 g, 星只测 a_k*m_k(p)
  => F_instr,i = a_k*m_k(p_i)*F_true,i + noise

回答:
  (5) 能否恢复 m_true? 恢复误差 (相对真值的 RMS/峰峰) + 无噪声 oracle 投影下界
  (6) 对照: 只用全局标量 vs 全局标量 + 低阶 m; 负例 m_true==1 (含噪声底标定)
  (7) 阶数扫描 1/2/3
  (8) 星数/分布敏感性 N=20/50/100/200, 均匀 vs 聚集
  (可辨识性) 背景上 (m,g) 精确退化; 星点上破缺

用法:  python3 synth_gain.py [--quick] [--nproc 16]
输出:  ../data/synth_results.json
"""
import os, sys, json, math, time, argparse
import numpy as np
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gainlib import (fit_spatial_gain, design, norm_params, align_shape,
                     shape_error_pct, mad_sigma)

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.abspath(os.path.join(HERE, "..", "data"))
RNG_SEED = 20260919

W = H = 1024
S0 = 1000.0          # 天光 ADU/px
SIG_PX = 5.0         # 读出+天光噪声 ADU/px (Q1 同口径)
AP_R, ANN_IN, ANN_OUT = 6.0, 10.0, 16.0
N_STAR_POOL = 1000
PSF_SIG, PSF_R = 2.0, 6
QUAD_REL = 0.20      # 二次项相对线性项的幅度比 (真实数据 |r2c| << 线性项, 见 Q3 §3.4)


def _disk_masks():
    R = int(math.ceil(ANN_OUT))
    gy, gx = np.mgrid[-R:R + 1, -R:R + 1]
    d2 = gx.astype(float) ** 2 + gy.astype(float) ** 2
    return (d2 <= AP_R * AP_R), ((d2 >= ANN_IN ** 2) & (d2 <= ANN_OUT ** 2)), R


AP_MASK, ANN_MASK, PATCH_R = _disk_masks()
AP_NPIX = int(AP_MASK.sum()); ANN_NPIX = int(ANN_MASK.sum())
SIG_AP = SIG_PX * math.sqrt(AP_NPIX * (1.0 + AP_NPIX / ANN_NPIX))   # 孔径流量噪声 ADU


def aperture_photometry(img, xs, ys):
    """向量化固定孔径测光 + 环形中位天光 (phot_verify/pixel_measure.py 同口径)."""
    Hh, Ww = img.shape
    n = len(xs); R = PATCH_R
    gy, gx = np.mgrid[-R:R + 1, -R:R + 1]
    flux = np.full(n, np.nan); sky = np.full(n, np.nan)
    x0 = np.floor(xs).astype(int); y0 = np.floor(ys).astype(int)
    ix = x0[:, None, None] + gx[None, :, :]
    iy = y0[:, None, None] + gy[None, :, :]
    ok = (ix >= 0) & (ix < Ww) & (iy >= 0) & (iy < Hh)
    patch = np.where(ok, img[np.clip(iy, 0, Hh - 1), np.clip(ix, 0, Ww - 1)].astype(np.float64), np.nan)
    with np.errstate(all='ignore'):
        sk = np.nanmedian(np.where(ANN_MASK[None], patch, np.nan), axis=(1, 2))
        fl = np.nansum(np.where(AP_MASK[None], patch, np.nan) - sk[:, None, None], axis=(1, 2))
    good = np.isfinite(fl) & np.isfinite(sk) & (np.sum(AP_MASK & ok, axis=(1, 2)) == AP_NPIX)
    flux[good] = fl[good]; sky[good] = sk[good]
    return flux, sky


# ---------------------------------------------------------------- 真值场

def _norm_default():
    return (W / 2.0, W / 2.0, H / 2.0, H / 2.0)


def field(terms, x, y, norm=None):
    """terms: {(i,j): coef} -> 多项式值 (log10 m 或 ADU)."""
    if norm is None:
        norm = _norm_default()
    x0, sx, y0, sy = norm
    xn = (np.asarray(x, float) - x0) / sx
    yn = (np.asarray(y, float) - y0) / sy
    z = np.zeros_like(xn)
    for (i, j), c in terms.items():
        z = z + c * (xn ** i) * (yn ** j)
    return z


def _grid(n=128):
    g = np.linspace(0, W, n)
    GX, GY = np.meshgrid(g, g)
    return GX.ravel(), GY.ravel()


def make_mtrue(rng, pp_pct=7.0, kind="lin+quad"):
    """log10 m_true: 线性为主 (+ 小幅二次项); 网格上零均值, pp = pp_pct%."""
    terms = {(1, 0): rng.normal(0, 1), (0, 1): rng.normal(0, 1)}
    if kind == "lin+quad":
        terms[(2, 0)] = QUAD_REL * rng.normal(0, 1)
        terms[(1, 1)] = QUAD_REL * rng.normal(0, 1)
        terms[(0, 2)] = QUAD_REL * rng.normal(0, 1)
    GX, GY = _grid()
    z = field(terms, GX, GY); z -= z.mean()
    span = float(z.max() - z.min())
    scale = math.log10(1.0 + pp_pct / 100.0) / span if span > 0 else 0.0
    return {k: v * scale for k, v in terms.items()}


def make_gtrue(rng, pp_pct=4.0):
    """加性梯度 g(p): <=2 阶多项式, pp = pp_pct% * S0 (ADU), 网格零均值."""
    terms = {(1, 0): rng.normal(0, 1), (0, 1): rng.normal(0, 1),
             (2, 0): 0.3 * rng.normal(0, 1), (1, 1): 0.3 * rng.normal(0, 1),
             (0, 2): 0.3 * rng.normal(0, 1)}
    GX, GY = _grid()
    z = field(terms, GX, GY); z -= z.mean()
    span = float(z.max() - z.min())
    scale = (pp_pct / 100.0 * S0) / span if span > 0 else 0.0
    return {k: v * scale for k, v in terms.items()}


def make_scene(rng, cluster=False):
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    xn = (xx - W / 2) / (W / 2); yn = (yy - H / 2) / (H / 2)
    T = (0.06 * xn + 0.04 * yn + 0.05 * (xn ** 2 - 1 / 3.0) - 0.03 * xn * yn + 0.02 * yn ** 2)
    for _ in range(4):
        cx = rng.uniform(0.1, 0.9) * W; cy = rng.uniform(0.1, 0.9) * H
        sg = rng.uniform(30, 110); amp = rng.uniform(0.2, 1.2)
        T += amp * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * sg * sg))
    scene = S0 * (1.0 + T)
    if cluster:
        cx = rng.uniform(0.35, 0.65) * W; cy = rng.uniform(0.35, 0.65) * H
        x = np.clip(rng.normal(cx, 0.13 * W, N_STAR_POOL), 32, W - 33)
        y = np.clip(rng.normal(cy, 0.13 * H, N_STAR_POOL), 32, H - 33)
    else:
        x = rng.uniform(32, W - 33, N_STAR_POOL)
        y = rng.uniform(32, H - 33, N_STAR_POOL)
    f = 10 ** rng.uniform(3.0, 5.0, N_STAR_POOL)
    gy, gx = np.mgrid[-PSF_R:PSF_R + 1, -PSF_R:PSF_R + 1]
    ker = np.exp(-(gx ** 2 + gy ** 2) / (2 * PSF_SIG ** 2)); ker /= ker.sum()
    for i in range(N_STAR_POOL):
        xi = int(round(x[i])); yi = int(round(y[i]))
        scene[yi - PSF_R:yi + PSF_R + 1, xi - PSF_R:xi + PSF_R + 1] += f[i] * ker
    return scene, x, y, f


def oracle_floor(x, y, logm_true_fn, order):
    """无噪声 oracle: 把真值场投影到 order 阶基上, 返回形状误差 (拟合精度下界)."""
    norm = norm_params(x, y)
    M = design(x, y, order, *norm)
    z = logm_true_fn(x, y)
    c, *_ = np.linalg.lstsq(M, z, rcond=None)
    GX, GY = _grid(96)
    Mg = design(GX, GY, order, *norm)
    fit = Mg @ c
    return shape_error_pct(fit, logm_true_fn(GX, GY))


# ---------------------------------------------------------------- 单次实现

def one_realization(args):
    (seed, order_list, n_star, cluster, m_kind, neg_control, sig_int_dex, pp_m, pp_g) = args
    rng = np.random.default_rng(seed)
    scene, xs, ys, ftrue = make_scene(rng, cluster=cluster)
    sel = np.argsort(-ftrue)[:n_star]
    x, y, f = xs[sel], ys[sel], ftrue[sel]
    mtermsA = {} if neg_control else make_mtrue(rng, pp_m, m_kind)
    mtermsB = {} if neg_control else make_mtrue(rng, pp_m, m_kind)
    gtermsA = make_gtrue(rng, pp_g); gtermsB = make_gtrue(rng, pp_g)
    aA = 1.0; aB = 10 ** rng.uniform(0.05, 0.15)

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    xxr, yyr = xx.ravel(), yy.ravel()

    def render(tm, tg, a):
        mul = a * np.power(10.0, field(tm, xxr, yyr).reshape(H, W)) if tm else a
        gf = field(tg, xxr, yyr).reshape(H, W) if tg else 0.0
        return mul * scene + gf + rng.normal(0.0, SIG_PX, (H, W))

    imgA = render(mtermsA, gtermsA, aA)
    imgB = render(mtermsB, gtermsB, aB)
    fA, _ = aperture_photometry(imgA, x, y)
    fB, _ = aperture_photometry(imgB, x, y)
    del imgA, imgB
    ok = np.isfinite(fA) & np.isfinite(fB) & (fA > 0) & (fB > 0)
    x, y, f, fA, fB = x[ok], y[ok], f[ok], fA[ok], fB[ok]
    n = len(f)

    eps_int = rng.normal(0.0, sig_int_dex, n) if sig_int_dex > 0 else np.zeros(n)
    fsyn = f * np.power(10.0, eps_int)
    sig_r = math.sqrt(2.0) * SIG_AP / (math.log(10.0) * np.maximum(fA, 1e-9))
    rA = np.log10(fA / fsyn); rB = np.log10(fB / fsyn)

    # 真值 (连续场) 在整帧网格上
    GX, GY = _grid(96)
    lgmA_true = field(mtermsA, GX, GY); lgmB_true = field(mtermsB, GX, GY)
    lgmA_true -= lgmA_true.mean(); lgmB_true -= lgmB_true.mean()
    fnA = (lambda gx, gy: field(mtermsA, gx, gy))
    fnB = (lambda gx, gy: field(mtermsB, gx, gy))

    log_ratio_true = (math.log10(aB) + lgmB_true) - (math.log10(aA) + lgmA_true)
    ptp_true = float(log_ratio_true.max() - log_ratio_true.min())
    rms_true = float(np.sqrt(np.mean((log_ratio_true - log_ratio_true.mean()) ** 2)))

    out = {"n_star": int(n), "cluster": bool(cluster), "neg_control": bool(neg_control),
           "sig_int_dex": sig_int_dex, "pp_m": pp_m, "orders": {},
           "ptp_true_field_pct": float((10 ** ptp_true - 1) * 100),
           "rms_true_field_pct": float((10 ** rms_true - 1) * 100)}
    for order in order_list:
        fitA = fit_spatial_gain(x, y, rA, sig_r, order=order)
        fitB = fit_spatial_gain(x, y, rB, sig_r, order=order)
        if fitA is None or fitB is None:
            continue
        # 注意: 拟合出的 m 是**校正因子** = 1/m_true(仪器响应); 故与 -lgm_true 比较
        seA = shape_error_pct(fitA.log_m(GX, GY), -lgmA_true)
        seB = shape_error_pct(fitB.log_m(GX, GY), -lgmB_true)
        # 星位置上的形状误差 (无外推)
        _lgt = field(mtermsA, x, y); _lgt = _lgt - _lgt.mean()
        seA_star = shape_error_pct(fitA.log_m(x, y), -_lgt)
        # 校正后的帧间比值场:  corrected = ratio * gain_B / gain_A
        lr_corr = log_ratio_true + fitB.log_gain(GX, GY) - fitA.log_gain(GX, GY)
        sA0 = fit_spatial_gain(x, y, rA, sig_r, order=0)
        sB0 = fit_spatial_gain(x, y, rB, sig_r, order=0)
        lr_scalar = log_ratio_true + sB0.log_gain(GX, GY) - sA0.log_gain(GX, GY)
        d_corr = (rA + fitA.log_gain(x, y)) - (rB + fitB.log_gain(x, y))
        d_scal = (rA + sA0.log_gain(x, y)) - (rB + sB0.log_gain(x, y))
        out["orders"][str(order)] = dict(
            m_shape_err_A=seA, m_shape_err_B=seB, m_shape_err_A_stars=seA_star,
            oracle_floor=oracle_floor(x, y, fnA, order),
            m_fit_ptp_pct_A=fitA.m_ptp_pct(), m_fit_ptp_pct_B=fitB.m_ptp_pct(),
            kA=fitA.k_photo, kB=fitB.k_photo, n_inlier_A=fitA.n_inlier,
            sigma_dex_A=fitA.sigma,
            field_ptp_uncorr_pct=float((10 ** ptp_true - 1) * 100),
            field_ptp_scalar_pct=float((10 ** float((lr_scalar - lr_scalar.mean()).max()
                                                    - (lr_scalar - lr_scalar.mean()).min()) - 1) * 100),
            field_ptp_corrected_pct=float((10 ** float(lr_corr.max() - lr_corr.min()) - 1) * 100),
            field_rms_scalar_pct=float((10 ** float(np.sqrt(np.mean((lr_scalar - lr_scalar.mean()) ** 2))) - 1) * 100),
            field_rms_corrected_pct=float((10 ** float(np.sqrt(np.mean((lr_corr - lr_corr.mean()) ** 2))) - 1) * 100),
            star_resid_rms_scalar_pct=float((10 ** float(np.sqrt(np.mean((d_scal - d_scal.mean()) ** 2))) - 1) * 100),
            star_resid_rms_corrected_pct=float((10 ** float(np.sqrt(np.mean((d_corr - d_corr.mean()) ** 2))) - 1) * 100),
        )
    return out


# ---------------------------------------------------------------- 聚合

def _stat(vals):
    v = np.asarray([x for x in vals if x is not None and np.isfinite(x)], float)
    if v.size == 0:
        return dict(n=0)
    return dict(n=int(v.size), mean=float(v.mean()), median=float(np.median(v)),
                std=float(v.std()), p05=float(np.percentile(v, 5)), p95=float(np.percentile(v, 95)))


KEYS = ("field_ptp_scalar_pct", "field_ptp_corrected_pct", "field_rms_scalar_pct",
        "field_rms_corrected_pct", "star_resid_rms_scalar_pct",
        "star_resid_rms_corrected_pct", "m_fit_ptp_pct_A", "m_fit_ptp_pct_B",
        "kA", "kB", "n_inlier_A", "sigma_dex_A")


def summarize(chunk):
    orders = sorted({o for c in chunk for o in c["orders"]}, key=int)
    out = {"n_mc": len(chunk), "orders": {}, "n_star": _stat([c["n_star"] for c in chunk]),
           "ptp_true_field_pct": _stat([c["ptp_true_field_pct"] for c in chunk])}
    for o in orders:
        d = {}
        for key in KEYS:
            d[key] = _stat([c["orders"][o][key] for c in chunk if o in c["orders"]])
        for sub in ("m_shape_err_A", "m_shape_err_B", "m_shape_err_A_stars", "oracle_floor"):
            d[sub + "_rms_pct"] = _stat([c["orders"][o][sub]["rms_pct"] for c in chunk if o in c["orders"]])
            d[sub + "_ptp_pct"] = _stat([c["orders"][o][sub]["ptp_pct"] for c in chunk if o in c["orders"]])
        out["orders"][o] = d
    return out


def run_mc(configs, n_mc, nproc, seed0=RNG_SEED, tag=""):
    jobs = []
    for ci, cfg in enumerate(configs):
        for k in range(n_mc):
            jobs.append((seed0 + 1000003 * ci + k,) + tuple(cfg))
    t0 = time.time()
    with Pool(nproc) as pool:
        res = pool.map(one_realization, jobs, chunksize=4)
    agg = [summarize(res[i * n_mc:(i + 1) * n_mc]) for i in range(len(configs))]
    print("  [%s] %d cfg x %d MC in %.1fs" % (tag, len(configs), n_mc, time.time() - t0))
    sys.stdout.flush()
    return agg


# ---------------------------------------------------------------- 可辨识性

def identifiability():
    rng = np.random.default_rng(7)
    x = rng.uniform(0, W, 400); y = rng.uniform(0, H, 400)
    mterms = make_mtrue(rng, 7.0, "lin+quad")
    gterms = make_gtrue(rng, 4.0)
    dterms = {(1, 0): 30.0, (0, 1): -20.0, (2, 0): 12.0, (0, 2): -9.0}
    a = 1.0
    m = np.power(10.0, field(mterms, x, y))
    g = field(gterms, x, y); d = field(dterms, x, y)
    y1 = a * m * S0 + g
    m2 = m + d / (a * S0); g2 = g - d
    y2 = a * m2 * S0 + g2
    ratio = (a * m2) / (a * m)
    # 背景退化: 纯加性模型 vs 纯乘性模型
    mB = 1.0 + (g - g.mean()) / S0
    yA = S0 + g - g.mean()
    yBm = mB * S0
    return dict(
        delta_ptp_adu=float(d.max() - d.min()),
        bg_max_abs_dy_adu=float(np.max(np.abs(y1 - y2))),
        bg_ptp_adu=float(y1.max() - y1.min()),
        star_flux_ratio_median=float(np.median(ratio)),
        star_flux_ratio_ptp_pct=float((ratio.max() - ratio.min()) * 100),
        bg_degeneracy_max_abs_dy_adu=float(np.max(np.abs(yA - yBm))),
        bg_degeneracy_gradient_ptp_adu=float(yA.max() - yA.min()),
        note=("(1) m->m+delta/(a*S0), g->g-delta 给出同一个 y (背景, 机器精度); "
              "(2) 同一对模型在星点上流量比 pp 明显非零 => 只有星点能破缺退化; "
              "(3) 纯加性 vs 纯乘性在背景上不可分辨 (dy << 梯度 pp)"),
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--nproc", type=int, default=16)
    args = ap.parse_args()
    NM = 40 if args.quick else 150
    t0 = time.time()
    results = {"n_mc": NM, "seed0": RNG_SEED, "quad_rel": QUAD_REL,
               "frame": [W, H], "S0": S0, "sig_px": SIG_PX,
               "sig_ap_adu": SIG_AP, "orders_tested": [0, 1, 2, 3]}

    ORD = [0, 1, 2, 3]
    print("[1] 主对照 + 阶数扫描 + 负例")
    configs = [(ORD, 200, False, "lin+quad", False, 0.010, 7.0, 4.0),
               (ORD, 200, False, "lin+quad", True, 0.010, 7.0, 4.0),
               (ORD, 200, False, "lin+quad", True, 0.000, 7.0, 4.0)]
    agg = run_mc(configs, NM, args.nproc, tag="order_scan")
    results["order_scan"] = {"configs": [
        {"label": "SIGNAL: m_true pp7% lin+quad, N=200 uniform, sigma_int=0.010 dex", "summary": agg[0]},
        {"label": "NEG-CTRL: m_true==1, N=200 uniform, sigma_int=0.010 dex", "summary": agg[1]},
        {"label": "NEG-CTRL-2: m_true==1, N=200 uniform, sigma_int=0 (纯噪声底)", "summary": agg[2]},
    ]}

    print("[2] 星数/分布敏感性")
    configs = []; labels = []
    for nst in (20, 50, 100, 200):
        for cl in (False, True):
            configs.append(([0, 1, 2], nst, cl, "lin+quad", False, 0.010, 7.0, 4.0))
            labels.append("N=%d %s" % (nst, "clustered" if cl else "uniform"))
    # 负例噪声底 vs N (证明底是统计精度极限而非过拟合): 同一 N, m_true==1
    for nst in (20, 50, 100, 200):
        configs.append(([0, 1, 2], nst, False, "lin+quad", True, 0.010, 7.0, 4.0))
        labels.append("NEG N=%d uniform (floor)" % nst)
    agg2 = run_mc(configs, NM, args.nproc, seed0=RNG_SEED + 50000000, tag="nstar")
    results["nstar_sensitivity"] = {"configs": [
        {"label": labels[i], "summary": agg2[i]} for i in range(len(configs))]}

    print("[3] 纯线性真值 (m 只含 1 阶)")
    configs = [([0, 1, 2], 200, False, "lin", False, 0.010, 7.0, 4.0),
               ([0, 1, 2], 200, False, "lin", True, 0.010, 7.0, 4.0)]
    agg3 = run_mc(configs, NM, args.nproc, seed0=RNG_SEED + 90000000, tag="lin_only")
    results["lin_only"] = {"configs": [
        {"label": "SIGNAL: m_true pp7% linear only", "summary": agg3[0]},
        {"label": "NEG-CTRL: m_true==1 (linear basis)", "summary": agg3[1]},
    ]}

    print("[4] 可辨识性")
    results["identifiability"] = identifiability()
    results["elapsed_s"] = time.time() - t0
    os.makedirs(DATA, exist_ok=True)
    with open(os.path.join(DATA, "synth_results.json"), "w") as fh:
        json.dump(results, fh, indent=1)
    print("WROTE", os.path.join(DATA, "synth_results.json"), "in %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
