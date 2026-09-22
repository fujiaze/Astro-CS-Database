#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01 / 问题一：天光来源语义缺失时容差 delta 的**证据性取值**。

判据形式（run/SCI-SNR-01 §4.5 档 1）：rho <= rho_tol(delta) = (1+delta)^2 - 1，
rho = (RN/g)^2 / sigma_given^2（**散粒口径**；语义未知时取两候选语义中较大的 rho）。

三个误差项各自独立测量：
  (a) 裁剪低偏 S_clip —— 生产 sigma 生产者（2 轮 median±3*1.4826*MAD 裁剪后 RMS）的相对低偏。
      三类数据：纯解析 MC / HST 真实模板 + 物理前向仿真 / testdata 真实帧。
  (b) 天光估计统计误差 S_stat —— SE(sigma_hat)/sigma 及向 SNR 的**精确**传播弹性 f。
  (c) 来源语义歧义 A(delta) —— 两候选支 sigma_F(C_S)/sigma_F(E)-1 的**实际值**与 T3 界对照。

决策规则（**运行前冻结**，见 DELTA_RULE）：
  delta* = max{ delta in DELTA_GRID : (R1) delta <= S_sys_max 且 (R2) A_eff(delta) <= S_sys_max }
  S_sys_max := 其余**不可约且已刻画**的系统项的最大值（实测 = 裁剪低偏量级）。
  规则两向可判红（negatives_selftest.py 复算）：S_sys_max:=0 => delta* 必须为 0；
  A_eff:=0 => delta* 必须 = S_sys_max。

seed 固定 = 20260924；纯 Python/NumPy/SciPy/astropy；**不运行任何 AstroCS 可执行文件**。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import exp01_common as C            # noqa: E402
import hst_sim as HS                # noqa: E402
import real_products as RP          # noqa: E402

SEED = 20260924
DELTA_GRID = [0.005, 0.0075, 0.01, 0.0125, 0.014, 0.016, 0.02, 0.03, 0.05]
DELTA_RULE = ("delta* = max{delta in DELTA_GRID : delta <= S_sys_max AND A_eff(delta) <= S_sys_max}")

BASE = dict(F_ref=1000.0, fwhm_det=3.0, gain=1.3, rn_e=10.0, n_sky=1.6e7)
SE_CONST_MEAN_SQ = 1.0 / math.sqrt(2.0)     # 高斯样本 rms 相对标准误常数（1/sqrt(2N)）
SE_CONST_MAD_SCALE = 1.44                   # MAD 类尺度估计量相对标准误常数（NOISE_MODEL §5a）


# ===========================================================================
# (a) 裁剪低偏
# ===========================================================================
def term_a_gaussian_mc(rng, sizes, n_real, k=3.0, n_rounds=2) -> List[Dict[str, float]]:
    """纯解析合成臂：高斯白噪声上的生产 recipe 低偏 + 无裁剪负例。"""
    rows = []
    for N, R in zip(sizes, n_real):
        rels, rels0 = [], []
        for _ in range(R):
            x = rng.normal(0.0, 1.0, size=N)
            rels.append(C.production_clip_sigma(x, n_rounds=n_rounds, k=k)["sigma"] - 1.0)
            rels0.append(C.production_clip_sigma(x, n_rounds=0, k=k)["sigma"] - 1.0)
        rows.append({
            "N": int(N), "n_real": int(R),
            "bias_mean": float(np.mean(rels)),
            "bias_sem": float(np.std(rels, ddof=1) / math.sqrt(R)),
            "bias_noclip_mean": float(np.mean(rels0)),
            "bias_noclip_sem": float(np.std(rels0, ddof=1) / math.sqrt(R)),
        })
    return rows


def _bg_sigma_adu(frame, det) -> float:
    """**背景**（天光+暗流+读噪+量化）真值 rms [ADU]，**不含源泊松项**。

    生产 noise_sigma 的语义是 sigma_sky（含读噪，不含源泊松）⇒ 真值必须同口径。
    """
    g = det.gain_e_per_adu
    V = (frame.sky_e + frame.dark_e) / (g * g) + (det.read_noise_e ** 2) / (g * g)
    if det.quantize:
        V = V + 1.0 / 12.0
    m = np.isfinite(frame.adu) & (frame.adu < det.saturation_adu)
    return float(np.sqrt(np.mean(V[m])))


def term_a_hst(rng, p999_list=(0.0, 60.0, 200.0, 1000.0, 50000.0), fwhm=2.5, sky=0.5,
               n_stars=80, shape=(512, 512), exposure_s=300.0) -> List[Dict[str, Any]]:
    """HST 真实模板 + 完整物理前向仿真臂（§12.2 第 1 类）。五个对照口径：

      pure    无真实底、无星   -> **本征** recipe 低偏（与高斯 MC 对拍）
      nostar  有真实底、无星   -> **结构污染**（星云结构抬高 sigma_hat）
      nobase  无真实底、有星   -> **源污染**
      raw     有真实底、有星   -> 生产实际情形（三者叠加）
      paired  有真实底、无星，两次独立噪声实现之差 -> 结构逐位抵消的独立复核

    真值一律用 **背景口径** sigma_bg（天光+暗流+读噪+量化，不含源泊松），与生产 noise_sigma
    的语义（sigma_sky 含读噪）一致。p999=0 表示不使用真实底。
    """
    rows = []
    # 注意：**不使用** render_frame 的 canvas_cache —— 它以 id(scene) 为键，
    # 而 CPython 会在对象释放后复用 id，跨场景会造成缓存命中错误（本次实测踩到：
    # 配对差里混入上一场景的真实底结构，paired_rel 被抬到 +417%）。
    cache = None
    for p999 in p999_list:
        use_base = p999 > 0
        kw = dict(shape=shape, fwhm_px=fwhm, sky_e_per_s=sky, gain=BASE["gain"],
                  read_noise_e=BASE["rn_e"], exposure_s=exposure_s,
                  target_p999_e=max(p999, 1.0))
        sc = HS.make_scene(n_stars=n_stars, use_hst_base=use_base, **kw)
        sc_nostar = HS.make_scene(n_stars=0, use_hst_base=use_base, **kw)
        sc_nobase = HS.make_scene(n_stars=n_stars, use_hst_base=False, **kw)
        det = HS.detector_of(sc)
        f_raw, _ = HS.render(sc, seed=SEED, cache=cache)
        f_nostar, _ = HS.render(sc_nostar, seed=SEED, cache=cache)
        f_nobase, _ = HS.render(sc_nobase, seed=SEED, cache=cache)
        f_pure, _ = HS.render(HS.make_scene(n_stars=0, use_hst_base=False, **kw),
                              seed=SEED, cache=cache)
        sig = _bg_sigma_adu(f_raw, det)
        base_std = float("nan")
        if use_base:
            rb_rate, _ = HS.RD.real_base_surface(sc["real_base"], tuple(shape), (0, 0), 0)
            base_std = float(np.std(rb_rate)) / det.gain_e_per_adu      # [ADU]
        s_raw = C.production_clip_sigma(f_raw.adu)["sigma"]
        s_nostar = C.production_clip_sigma(f_nostar.adu)["sigma"]
        s_nobase = C.production_clip_sigma(f_nobase.adu)["sigma"]
        s_pure = C.production_clip_sigma(f_pure.adu)["sigma"]
        # paired 仅对 base=0 有明确真值：有真实底时逐像素噪声被星云自身泊松主导，
        # 与 sigma_bg 口径不可比（该情形如实登记 nan，不冒充裁剪低偏）。
        if use_base:
            s_pair = float("nan")
        else:
            f_pair2, _ = HS.render(HS.make_scene(n_stars=0, use_hst_base=False, **kw),
                                   seed=SEED + 1, cache=cache)
            s_pair = C.production_clip_sigma(f_pure.adu - f_pair2.adu)["sigma"]
        rows.append({
            "target_p999_e": p999, "use_base": bool(use_base), "fwhm_px": fwhm,
            "sky_e_per_s": sky, "n_stars": n_stars, "shape": list(shape),
            "sigma_bg_true_adu": sig,
            "pure_rel": s_pure / sig - 1.0,
            "nostar_rel": s_nostar / sig - 1.0,
            "nobase_rel": s_nobase / sig - 1.0,
            "raw_rel": s_raw / sig - 1.0,
            "paired_rel": (s_pair / (math.sqrt(2.0) * sig) - 1.0)
                          if not math.isnan(s_pair) else float("nan"),
            "struct_rms_over_noise": (base_std / sig) if use_base else 0.0,
            "n_keep_frac_raw": C.production_clip_sigma(f_raw.adu)["n_keep"] / f_raw.adu.size,
        })
    return rows


def _checkerboard_ratio(block: np.ndarray) -> float:
    """单块棋盘分半的 sigma 比 (a/b - 1)（结构在一阶上抵消）。"""
    yy, xx = np.mgrid[0:block.shape[0], 0:block.shape[1]]
    m = (yy + xx) % 2 == 0
    a = C.production_clip_sigma(block[m])["sigma"]
    b = C.production_clip_sigma(block[~m])["sigma"]
    return a / b - 1.0


def term_a_real(paths, crop=2048, max_frames=8, nb=8) -> List[Dict[str, Any]]:
    """testdata 真实数据臂：生产 recipe vs 稳健尺度（逐块，抗结构）+ 棋盘分半统计误差。"""
    from astropy.io import fits
    rows = []
    for p in paths[:max_frames]:
        with fits.open(p, memmap=False) as hdul:
            d = np.asarray(hdul[0].data, dtype=np.float64)
        ny, nx = d.shape
        y0, x0 = max((ny - crop) // 2, 0), max((nx - crop) // 2, 0)
        sub = d[y0:y0 + crop, x0:x0 + crop]
        bs = crop // nb
        blocks = sub[:nb * bs, :nb * bs].reshape(nb, bs, nb, bs).transpose(0, 2, 1, 3).reshape(-1, bs, bs)
        prod, madr, se, madq = [], [], [], []
        for b in blocks:
            med = float(np.median(b))
            prod.append(C.production_clip_sigma(b)["sigma"])
            aq = float(np.median(np.abs(b - med)))          # 量化后的中位绝对偏差
            madr.append(C.MAD_TO_SIGMA * aq)
            madq.append(aq)
            se.append(_checkerboard_ratio(b))
        prod, madr, se, madq = np.array(prod), np.array(madr), np.array(se), np.array(madq)
        r = C.production_clip_sigma(sub)
        rows.append({
            "file": p.name, "shape": [int(ny), int(nx)], "crop": int(crop), "n_block": int(nb * nb),
            "block_size": int(bs),
            "sigma_prod_frame": r["sigma"], "median_frame": r["background"],
            "n_keep_frac_frame": r["n_keep"] / r["n_in"],
            "prod_over_mad_block_p05": float(np.percentile(prod / madr - 1.0, 5)),
            "prod_over_mad_block_p50": float(np.median(prod / madr - 1.0)),
            "prod_over_mad_block_p95": float(np.percentile(prod / madr - 1.0, 95)),
            "block_sigma_p50": float(np.median(prod)),
            "block_sigma_p95_over_p05": float(np.percentile(prod, 95) / np.percentile(prod, 5)),
            "se_checkerboard_rms": float(np.sqrt(np.mean(se ** 2)) / math.sqrt(2.0)),
            "se_checkerboard_median_abs": float(np.median(np.abs(se)) / 0.6744897501960817 / math.sqrt(2.0)),
            "se_pred_1_over_sqrt2N": SE_CONST_MEAN_SQ / math.sqrt(bs * bs / 2.0),
            "se_pred_144_over_sqrtN": SE_CONST_MAD_SCALE / math.sqrt(bs * bs / 2.0),
            # 16bit 整数数据上 MAD 统计量的**量化粒度**（median|v-med| 取整）：
            # 相对粒度 = MAD_TO_SIGMA*0.5/mad  => 本口径能分辨的最小偏差
            "mad_quantization_granularity_rel": float(
                C.MAD_TO_SIGMA * 0.5 / np.median(madr)),
            "mad_median_abs_is_integer": bool(np.allclose(madq, np.round(madq))),
        })
    return rows


# ===========================================================================
# (b) 天光估计统计误差
# ===========================================================================
def term_b_stat_mc(rng, sizes, n_real) -> List[Dict[str, float]]:
    """生产 recipe 的 SE + 四个变体分解（定位偏离 1/sqrt(2N) 的来源）。"""
    rows = []
    for N, R in zip(sizes, n_real):
        v_prod, v_noclip_mean, v_noclip_med, v_clip_mean = [], [], [], []
        for _ in range(R):
            x = rng.normal(0.0, 1.0, size=N)
            v_prod.append(C.production_clip_sigma(x, n_rounds=2, k=3.0)["sigma"])
            v_noclip_mean.append(float(np.sqrt(np.mean((x - x.mean()) ** 2))))
            v_noclip_med.append(float(np.sqrt(np.mean((x - np.median(x)) ** 2))))
            med = float(np.median(x)); mad = float(np.median(np.abs(x - med)))
            keep = np.abs(x - med) <= 3.0 * C.MAD_TO_SIGMA * mad
            y = x[keep]
            v_clip_mean.append(float(np.sqrt(np.mean((y - y.mean()) ** 2))))
        rows.append({
            "N": int(N), "n_real": int(R),
            "se_prod": float(np.std(v_prod, ddof=1)),
            "se_prod_sem": float(np.std(v_prod, ddof=1) / math.sqrt(2.0 * (R - 1))),
            "se_noclip_meancenter": float(np.std(v_noclip_mean, ddof=1)),
            "se_noclip_medcenter": float(np.std(v_noclip_med, ddof=1)),
            "se_clip_meancenter": float(np.std(v_clip_mean, ddof=1)),
            "se_pred_1_over_sqrt2N": SE_CONST_MEAN_SQ / math.sqrt(N),
            "se_pred_144_over_sqrtN": SE_CONST_MAD_SCALE / math.sqrt(N),
        })
    return rows


def elasticity_fprop(F_adu, fwhm_det, sigma_sky, gain, rn_e, src) -> Dict[str, float]:
    """sigma_sky -> sigma_F 的**精确**对数弹性 f = sigma_sky^2 * sum(P^2/V^2) / sum(P^2/V)。

    替换 run/SCI-SNR-01 §4.2 的启发式 f_prop = shot^2/pix^2（审稿 S3-22 已登记其为非推导量）。
    """
    P, sum_p2, half, sigma = C.profile_for_fwhm(fwhm_det)
    V = C.pixel_variance(P, F_adu, sigma_sky, gain, rn_e, src)
    a = float((P * P / V).sum())
    b = float((P * P / (V * V)).sum())
    f_exact = (sigma_sky ** 2) * b / a
    v_pix = sigma_sky ** 2
    shot = v_pix - (rn_e / gain) ** 2 if gain > 0 else v_pix
    f_heur = max(shot, 0.0) / v_pix
    h = 1e-6
    v1 = C.var_optimal(P, C.pixel_variance(P, F_adu, sigma_sky * (1 + h), gain, rn_e, src))
    v0 = C.var_optimal(P, V)
    f_num = 0.5 * (math.log(v1) - math.log(v0)) / h
    return {"f_exact": f_exact, "f_heuristic": f_heur, "f_numeric": f_num,
            "ratio_heur_over_exact": f_heur / f_exact if f_exact else float("nan"),
            "rel_diff_exact_numeric": f_exact / f_num - 1.0 if f_num else float("nan")}


# ===========================================================================
# (c) 语义歧义项
# ===========================================================================
def _sigma_f(F_adu, fwhm_det, sigma_given, gain, rn_e, truth_is_E) -> float:
    src = C.SRC_EMPIRICAL_TOTAL_RMS if truth_is_E else C.SRC_SHOT_ONLY
    return C.sigma_optimal(F_adu, fwhm_det, sigma_given, gain, rn_e, src)["sigma_f_adu"]


def term_c_ambiguity(F_ref, fwhm_det, gain, rn_e, rho, n_flux=13) -> Dict[str, Any]:
    """给定 rho 处：保守支（真值 E 用 C_S）与反保守支（真值 S 用 C_E）的实际偏差 + T3 界。"""
    rn_adu = rn_e / gain
    sig_shot = rn_adu / math.sqrt(rho) if rho > 0 else float("inf")
    sig_E = math.sqrt(sig_shot ** 2 + rn_adu ** 2)
    flux = np.logspace(math.log10(F_ref / 100.0), math.log10(F_ref * 100.0), n_flux)
    cons, anti = [], []
    for F in flux:
        s_E = _sigma_f(F, fwhm_det, sig_E, gain, rn_e, True)
        cons.append(_sigma_f(F, fwhm_det, sig_E, gain, rn_e, False) / s_E - 1.0)
        s_S = _sigma_f(F, fwhm_det, sig_shot, gain, rn_e, False)
        anti.append(s_S / _sigma_f(F, fwhm_det, sig_shot, gain, rn_e, True) - 1.0)
    bound = math.sqrt(1.0 + rho) - 1.0
    cons = np.array(cons)
    return {"rho": rho, "sigma_shot": sig_shot, "sigma_E": sig_E, "bound": bound,
            "flux": flux.tolist(), "conservative_actual": cons.tolist(),
            "anticonservative_actual": anti,
            "A_at_F_ref": float(cons[n_flux // 2]),
            "A_max_over_grid": float(cons.max()),
            "A_min_over_grid": float(cons.min()),
            "tightness_max": float(cons.max() / bound) if bound > 0 else float("nan"),
            "tightness_at_F_ref": float(cons[n_flux // 2] / bound) if bound > 0 else float("nan")}


def term_c_negative_controls(gain, rn_e) -> Dict[str, float]:
    """负例（真值无效应 => 归零）：RN=0 时两候选支逐像素方差逐点相同 => 比值必须 == 0。"""
    a0 = C.sigma_optimal(1000.0, 3.0, 1.0, gain, 0.0, C.SRC_SHOT_ONLY)["var_f"]
    b0 = C.sigma_optimal(1000.0, 3.0, 1.0, gain, 0.0, C.SRC_EMPIRICAL_TOTAL_RMS)["var_f"]
    rn_adu = rn_e / gain
    a = C.sigma_optimal(1000.0, 3.0, 1.0, gain, rn_e, C.SRC_SHOT_ONLY)["var_f"]
    b = C.sigma_optimal(1000.0, 3.0, 1.0, gain, rn_e, C.SRC_EMPIRICAL_TOTAL_RMS)["var_f"]
    return {"rel_dev_rn_zero": math.sqrt(a0 / b0) - 1.0,
            "rel_dev_with_rn": math.sqrt(a / b) - 1.0,
            "rho_with_rn": (rn_adu / 1.0) ** 2}


def _bg_term_rel(n_sky: float, F_true: float = 1000.0, fwhm: float = 3.0,
                 sigma_sky: float = 12.62) -> float:
    """背景电平项（相对）：盒和方差里 Var(b_hat) 与 Cov(d_i,b_hat) 的净贡献 / sum_V。"""
    P, _, _, _ = C.profile_for_fwhm(fwhm)
    b = C.var_box_with_background(P, F_true, sigma_sky, BASE["gain"], BASE["rn_e"],
                                  C.SRC_EMPIRICAL_TOTAL_RMS, n_sky)
    return float(b["bg_rel"])


def term_a_hst_star_scan(rng, n_stars_list=(0, 1, 5, 20, 80, 320), fwhm=2.5, sky=0.5,
                         shape=(512, 512), exposure_s=300.0,
                         flux_log10=(3.5, 5.5)) -> List[Dict[str, Any]]:
    """**源污染**对 sigma_hat 的定量扫描：星越密，Moffat4 的重翼把越多"低于 3sigma 阈值"
    的像素留在保留集里，把裁剪低偏往正方向拉。

    这解释了为什么"星场实测"与"纯噪声实测"会给出不同的净偏置：
    净偏置 = 裁剪低偏（负，本征 ~-1.4%）+ 源污染（正，随源密度增长）。
    **delta 预算取二者的上界绝对值（即裁剪低偏本身），是保守选择。**
    """
    rows = []
    for nst in n_stars_list:
        sc = HS.make_scene(shape=shape, fwhm_px=fwhm, sky_e_per_s=sky, gain=BASE["gain"],
                           read_noise_e=BASE["rn_e"], exposure_s=exposure_s,
                           n_stars=nst, use_hst_base=False, target_p999_e=1.0,
                           flux_log10=flux_log10)
        det = HS.detector_of(sc)
        fr, tr = HS.render(sc, seed=SEED, cache=None)
        st = C.production_clip_sigma(fr.adu)
        sig = _bg_sigma_adu(fr, det)
        # 星占的像素份额（用真值位置 ±5 px 的并集近似）
        cov = np.zeros(shape, dtype=bool)
        for s in tr["stars_in_frame"]:
            xi, yi = int(round(s["x"])), int(round(s["y"]))
            cov[max(0, yi - 5):yi + 6, max(0, xi - 5):xi + 6] = True
        rows.append({"n_stars": nst, "n_stars_in_frame": tr.get("n_stars_in_frame"),
                     "star_pixel_frac": float(cov.mean()),
                     "sigma_bg_true_adu": sig, "sigma_hat_adu": st["sigma"],
                     "rel": st["sigma"] / sig - 1.0,
                     "n_keep_frac": st["n_keep"] / st["n_in"],
                     "max_adu": float(np.nanmax(fr.adu))})
    return rows


def rho_threshold(delta: float) -> float:
    return (1.0 + delta) ** 2 - 1.0


def delta_needed(rho: float) -> float:
    return math.sqrt(1.0 + rho) - 1.0


def rho_threshold_selftest() -> List[Dict[str, float]]:
    rows = []
    for d in (0.005, 0.01, 0.014, 0.02, 0.05):
        rt = rho_threshold(d)
        rows.append({"delta": d, "rho_tol": rt, "sigma_sky_over_rnadu": 1.0 / math.sqrt(rt),
                     "Be_over_RN2_shot": 1.0 / rt, "Be_over_RN2_totalrms": 1.0 / rt - 1.0,
                     "bound_at_threshold": math.sqrt(1.0 + rt) - 1.0})
    return rows


def rho_feasibility_real(frames, delta, instruments) -> List[Dict[str, Any]]:
    """真实帧上判据的可满足性：实测 sigma_sky[ADU] + 假设 (g, RN) => rho => 判档 1。"""
    rt = rho_threshold(delta)
    rows = []
    for g, rn in instruments:
        rn_adu = rn / g
        n_pass, rhos, need = 0, [], []
        for f in frames:
            # 两套帧集的字段名不同：产物帧用 noise_sigma，a_real 裁剪臂用 sigma_prod_frame
            s = f.get("noise_sigma", f.get("sigma_prod_frame"))
            shot2 = s * s - rn_adu * rn_adu
            if shot2 <= 0:
                rhos.append(float("inf")); need.append(float("inf")); continue
            rho = rn_adu * rn_adu / shot2
            rhos.append(rho); need.append(delta_needed(rho))
            if rho <= rt:
                n_pass += 1
        rows.append({"gain": g, "rn_e": rn, "rn_adu": rn_adu, "delta": delta,
                     "n_frames": len(frames), "n_pass": n_pass,
                     "pass_frac": n_pass / len(frames) if frames else float("nan"),
                     "rho_p50": float(np.median(rhos)) if rhos else float("nan"),
                     "delta_needed_p50": float(np.median(need)) if need else float("nan"),
                     "delta_needed_min": float(np.min(need)) if need else float("nan")})
    return rows


# ===========================================================================
# 决策
# ===========================================================================
def decide_delta(S_sys_max: float, A_eff: Dict[str, float],
                 grid: List[float] = DELTA_GRID) -> Dict[str, Any]:
    ok, table = [], []
    for d in grid:
        a = float(A_eff.get(str(d), A_eff.get(d, float("nan"))))
        r1 = bool(d <= S_sys_max)
        r2 = bool(a <= S_sys_max)
        table.append({"delta": d, "A_eff": a, "R1_delta_le_Ssys": r1,
                      "R2_Aeff_le_Ssys": r2, "admissible": bool(r1 and r2)})
        if r1 and r2:
            ok.append(d)
    return {"rule": DELTA_RULE, "S_sys_max": S_sys_max, "table": table,
            "delta_star": (max(ok) if ok else None), "n_admissible": len(ok)}


def decide_delta_selftest() -> Dict[str, Any]:
    z = {str(d): 0.0 for d in DELTA_GRID}
    a = decide_delta(0.0, z)
    b = decide_delta(0.01, z)
    c = decide_delta(0.01, {str(d): 0.02 for d in DELTA_GRID})
    return {"S_sys0_Aeff0_delta_star": a["delta_star"],
            "S_sys1pct_Aeff0_delta_star": b["delta_star"],
            "S_sys1pct_Aeff2pct_delta_star": c["delta_star"],
            "green": (a["delta_star"] is None and b["delta_star"] == 0.01
                      and c["delta_star"] is None)}


# ===========================================================================
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE.parents[1] / "results" / "exp01_q1_delta.json"))
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    out: Dict[str, Any] = {"meta": {"seed": SEED, "base_point": BASE,
                                    "delta_grid": DELTA_GRID, "delta_rule": DELTA_RULE,
                                    "script": Path(__file__).name}}

    out["a_gaussian_mc"] = term_a_gaussian_mc(
        rng, sizes=((100_000, 1_000_000) if args.quick else (100_000, 1_000_000, 4_000_000, 16_000_000)),
        n_real=((30, 15) if args.quick else (60, 30, 12, 5)))
    out["a_analytic_single_round"] = {str(k): C.analytic_clip_bias(k) for k in (2.0, 3.0, 5.0, 10.0)}
    out["a_hst_star_scan"] = term_a_hst_star_scan(
        rng, n_stars_list=((0, 80) if args.quick else (0, 1, 5, 20, 80, 320)))
    out["a_hst"] = term_a_hst(rng, p999_list=((0.0, 50000.0) if args.quick
                                              else (0.0, 60.0, 200.0, 1000.0, 50000.0)))
    real_paths = sorted((RP.ROOT / "testdata/M42_T2T3_mosaic_Flying_dutchman/T2").rglob("*-300S-Red.fts"))
    out["a_real_files"] = [str(p.relative_to(RP.ROOT)) for p in real_paths[:8]]
    out["a_real"] = term_a_real(real_paths, max_frames=(3 if args.quick else 8))

    out["b_stat_mc"] = term_b_stat_mc(
        rng, sizes=((9_216, 100_000) if args.quick else (9_216, 100_000, 1_000_000, 16_000_000)),
        n_real=((800, 400) if args.quick else (2000, 800, 200, 20)))
    out["b_elasticity"] = {}
    for fw in (1.6, 2.5, 3.0, 3.9):
        for be in (1e2, 1e3, 1e4):
            rn_adu = BASE["rn_e"] / BASE["gain"]
            sig = math.sqrt(be + rn_adu ** 2)
            out["b_elasticity"]["fwhm=%.1f,Be=%.0f" % (fw, be)] = elasticity_fprop(
                BASE["F_ref"], fw, sig, BASE["gain"], BASE["rn_e"], C.SRC_EMPIRICAL_TOTAL_RMS)

    out["c_negative_controls"] = term_c_negative_controls(BASE["gain"], BASE["rn_e"])
    out["c_threshold_selftest"] = rho_threshold_selftest()
    out["c_ambiguity"], A_ref, A_max = {}, {}, {}
    for d in DELTA_GRID:
        rec = term_c_ambiguity(BASE["F_ref"], BASE["fwhm_det"], BASE["gain"], BASE["rn_e"],
                               rho_threshold(d))
        out["c_ambiguity"][str(d)] = rec
        A_ref[str(d)] = rec["A_at_F_ref"]
        A_max[str(d)] = rec["A_max_over_grid"]
    out["c_A_eff_at_F_ref"] = A_ref
    out["c_A_eff_max_over_flux_grid"] = A_max

    prods = RP.find_products(min_bytes=50_000_000, limit=(2 if args.quick else 4))
    dist: Dict[str, Any] = {"files": [str(p.relative_to(RP.ROOT)) for p in prods], "frames": []}
    for p in prods:
        for fr in RP.iter_frames(p):
            fw, fl = fr["src"]["fwhm_px"], fr["src"]["flux"]
            if fw.size:
                dist["frames"].append({
                    "file": p.name, "n_src": int(fw.size),
                    "fwhm_p05": float(np.percentile(fw, 5)), "fwhm_p50": float(np.median(fw)),
                    "fwhm_p95": float(np.percentile(fw, 95)),
                    "flux_p05": float(np.percentile(fl, 5)), "flux_p50": float(np.median(fl)),
                    "flux_p95": float(np.percentile(fl, 95)),
                    "noise_sigma": fr["meta"].get("noise_sigma"),
                })
    dist["n_frames"] = len(dist["frames"])
    _sig = [f["noise_sigma"] for f in dist["frames"] if f.get("noise_sigma")]
    dist["noise_sigma_min"] = float(min(_sig)) if _sig else None
    dist["noise_sigma_max"] = float(max(_sig)) if _sig else None
    dist["provenance"] = ("run/PERF-401/work/p1_blk{1..4}/p1_sources.json（**性能轮产物**，非专门科学取证产物）")
    out["c_real_distribution"] = dist
    INSTR = ((1.3, 10.0), (1.3, 7.0), (1.3, 15.0), (0.5, 10.0), (2.0, 10.0),
             (1.0, 5.0), (1.0, 3.0), (1.0, 1.5), (0.5, 2.0), (2.0, 3.0))
    # **两套帧集分开报**（独立审稿 I1）：
    #   A) 16 帧 run/PERF-401 产物（sigma 18.3~38.2 ADU）—— 上面这套，帧数多、噪声低；
    #   B) 8 帧 testdata/M42 T2 中心裁剪（sigma 15.6~64.0 ADU）—— a_real 臂的那 8 帧，含 M2 星云亮帧。
    # 两者结论方向一致（常规读噪下档 1 大面积不满足），但逐格计数不同，必须分开呈现。
    out["c_rho_feasibility_real"] = {
        str(d): rho_feasibility_real(dist["frames"], d, INSTR)
        for d in (0.005, 0.01, 0.014, 0.02, 0.05)}
    out["c_rho_feasibility_real_8frames"] = {
        str(d): rho_feasibility_real(out["a_real"], d, INSTR)
        for d in (0.005, 0.01, 0.014, 0.02, 0.05)}

    S = {
        "S_clip_gaussian_MC_largestN": abs(out["a_gaussian_mc"][-1]["bias_mean"]),
        "S_clip_analytic_single_round_k3": abs(C.analytic_clip_bias(3.0)),
        "S_clip_hst_pure": abs([r for r in out["a_hst"] if r["target_p999_e"] == 0.0][0]["pure_rel"]),
        "S_clip_hst_paired": [abs(r["paired_rel"]) for r in out["a_hst"]
                              if not math.isnan(r["paired_rel"])][0],
        "S_struct_contamination_max": max(abs(r["nostar_rel"]) for r in out["a_hst"]),
        "S_source_contamination_max": max(abs(r["nobase_rel"]) for r in out["a_hst"]),
        "S_clip_real_block_p50_max": max(abs(r["prod_over_mad_block_p50"]) for r in out["a_real"]),
        "S_stat_1p6e7_corrected": SE_CONST_MEAN_SQ / math.sqrt(1.6e7),
        "S_stat_1p6e7_documented": SE_CONST_MAD_SCALE / math.sqrt(1.6e7),
        "S_stat_9216_documented": SE_CONST_MAD_SCALE / math.sqrt(9216),
        # 背景电平项：**直接取** var_box_with_background 的精确合成（含与 b_hat 的协方差），
        # 不再用 7.459*kappa/n_sky 的近似（旧式漏掉协方差项且把 n_B 误写成 A_NEA）。
        "S_bg_nsky_1p6e7": abs(_bg_term_rel(1.6e7)),
        "S_bg_nsky_9216": abs(_bg_term_rel(9216.0)),
        "S_zp_1mmag": 0.4 * math.log(10.0) * 0.001,
    }
    out["S_terms"] = S
    S_sys = max(S["S_clip_hst_pure"], S["S_clip_hst_paired"],
                S["S_clip_gaussian_MC_largestN"], S["S_stat_1p6e7_corrected"], S["S_zp_1mmag"])
    out["S_sys_max_used"] = S_sys
    out["S_sys_max_definition"] = ("max(HST pure 本征裁剪低偏, HST paired 结构抵消裁剪低偏, "
                                   "纯解析 MC 裁剪低偏, sigma_hat 统计误差(1/sqrt(2N), N=1.6e7), "
                                   "F_ref/ZP 1mmag)")
    out["decision_A_at_F_ref"] = decide_delta(S_sys, A_ref)
    out["decision_A_max"] = decide_delta(S_sys, A_max)
    out["decision_selftest"] = decide_delta_selftest()
    out["decision_sensitivity"] = {
        "S_sys_1pct": decide_delta(0.01, A_ref)["delta_star"],
        "S_sys_0p77pct": decide_delta(0.0077, A_ref)["delta_star"],
        "S_sys_stat_documented_9216": decide_delta(S["S_stat_9216_documented"], A_ref)["delta_star"],
    }
    out["meta"]["elapsed_s"] = time.time() - t0
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps({"S_terms": S, "S_sys_max_used": S_sys,
                      "decision_selftest": out["decision_selftest"],
                      "delta_star_at_F_ref": out["decision_A_at_F_ref"]["delta_star"],
                      "delta_star_A_max": out["decision_A_max"]["delta_star"],
                      "sensitivity": out["decision_sensitivity"]},
                     ensure_ascii=False, indent=1))
    print("wrote", outp, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
