#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01 / 问题二（解析 + 蒙特卡洛臂）：逐源 SNR 的估计量错配该怎么修。

候选：
  CUR 现行  : flux = 5x5 **正性截断**盒和（star_detector.cpp::detect s.flux = m00）
              sigma = Horne 最优提取方差 1/sum(P_i^2/V_i)（snr_science.cpp，61x61 网格）
  A   方案A : flux = PSF 解析总通量 2*pi*A*sx*sy/3（module_adapters.cpp::p1_psf_analytic_flux）
              sigma 公式不动
  B   方案B : flux = 盒和（截断按精确截断矩处理或去掉截断）
              sigma = 盒和自身方差 sum_{i in B} V_i + 背景电平项

判据：
  (a) 配对性  —— 报告 sigma 必须等于**所报 flux 估计量**的抽样标准差（MC 实测 std 对拍）
  (b) 截断处理 —— 正性截断的偏置与方差缩减是否被完备处理
  (c) 跨帧可比 —— 报告 flux 是否为**与 seeing 无关**的测光量（ASTROCS_DESIGN §3.1）
  (d) 帧级影响 —— reference_snr_f（模型 F_ref 路径）走的是哪个口径

负例（真值无效应 => 归零）：
  N1 delta-PSF + 1 像素盒：三个估计量逐位相同 => 配对比必须 == 1；
  N2 去掉 v<=0 截断：截断偏置必须归零（<=3 MC sigma）；
  N3 同一 seeing（无效应）：跨帧伪差必须**严格 0**；
  N4 sigma->0（无噪声）：配对比度量必须判**退化**（不得静默通过）。

seed 固定 = 20260924；纯 Python/NumPy/SciPy；不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np
from scipy.optimize import least_squares

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp01_common as C            # noqa: E402

SEED = 20260924
GAIN = 1.3
RN = 10.0
SKY = 12.62          # ADU；= sqrt(B_e + (RN/g)^2) with B_e = 1e2 e-/px
N_SKY_FRAME = 1.6e7  # 生产帧全局背景样本数（star_detector.cpp::estimate_background）
FIT_HALF = 8         # PSF 拟合 stamp 半宽（17x17）


# ---------------------------------------------------------------------------
# 估计量
# ---------------------------------------------------------------------------
def _stamp_profile(F_true: float, fwhm_det: float, fit_half: int = FIT_HALF,
                   beta_true: float = 4.0, model_beta: float = 4.0):
    """构造真值轮廓与模型轮廓（可令 beta_true != model_beta 以注入 PSF 失配）。"""
    sigma = C.detection_sigma_from_fwhm(fwhm_det)
    j, i = np.mgrid[-fit_half:fit_half + 1, -fit_half:fit_half + 1]
    r2 = i.astype(float) ** 2 + j.astype(float) ** 2
    t = 1.0 + r2 / (2.0 * sigma * sigma)
    v = t ** (-beta_true)
    P_true = v / v.sum()                       # 真值轮廓（归一化，sum=1）
    vm = t ** (-model_beta)
    P_model = vm / vm.sum()
    return P_true, P_model, sigma


def mc_estimators(rng, F_true: float, fwhm_det: float, sigma_sky: float, gain: float,
                  rn_e: float, n_real: int = 400, src_semantics: int = C.SRC_EMPIRICAL_TOTAL_RMS,
                  beta_true: float = 4.0, fit_full: bool = True) -> Dict[str, Any]:
    """对同一颗真值源做 n_real 次噪声实现，测量三个估计量的均值/方差与配对性。"""
    P_true, P_model, sigma = _stamp_profile(F_true, fwhm_det, beta_true=beta_true)
    b = 1000.0
    # 逐像素方差（用**模型**轮廓，与生产一致：生产只有模型）
    V = C.pixel_variance(P_model, F_true, sigma_sky, gain, rn_e, src_semantics)
    # 生产口径的 sigma_opt（61x61 网格，flux 取所报值）
    P_full, sum_p2_full, half_full, _ = C.profile_for_fwhm(fwhm_det)
    bc = FIT_HALF
    sl = slice(P_full.shape[0] // 2 - bc, P_full.shape[0] // 2 + bc + 1)
    P_full_c = P_full[sl, sl]
    V_full_c = V

    def sigma_opt(F_rep: float) -> float:
        Vf = C.pixel_variance(P_full, F_rep, sigma_sky, gain, rn_e, src_semantics)
        return math.sqrt(C.var_optimal(P_full, Vf))

    box = np.zeros((2 * bc + 1, 2 * bc + 1), dtype=bool)
    c0 = bc
    box[c0 - 2:c0 + 3, c0 - 2:c0 + 3] = True

    # 盒和方差（未截断 + 背景电平项）
    bg = C.var_box_with_background(P_full, F_true, sigma_sky, gain, rn_e,
                                   src_semantics, N_SKY_FRAME)
    bm = C.box_estimator_moments(F_true, P_full, sigma_sky, gain, rn_e, src_semantics)

    mu = F_true * P_true
    sig = np.sqrt(V)
    n = mu.shape[0]
    out = {k: np.empty(n_real) for k in
           ("box_trunc", "box_untrunc", "psf_amp", "psf_full", "snr_cur", "snr_A", "snr_B")}
    n_bound_hit = 0
    for k in range(n_real):
        d = b + mu + rng.normal(0.0, 1.0, size=mu.shape) * sig   # 图像 = 背景 + 源 + 噪声
        y = d - b
        yb = y[box]
        out["box_trunc"][k] = float(np.maximum(yb, 0.0).sum())
        out["box_untrunc"][k] = float(yb.sum())
        # --- 固定形状的加权幅度拟合（= 最优加权和，sum w_i d_i）---
        w = (P_model / V)
        A_amp = float((w * y).sum() / (P_model * P_model / V).sum())
        out["psf_amp"][k] = A_amp * float(P_true.sum())     # 形状归一 => 通量 = 幅度
        # --- 全参数拟合（B, A, cx, cy, sx, sy）---
        if fit_full:
            out["psf_full"][k] = _fit_full_flux(d, b, sigma, F_true)
            n_bound_hit += int(bool(getattr(_fit_full_flux, "last_bound_hit", False)))
        else:
            out["psf_full"][k] = A_amp
        out["snr_cur"][k] = out["box_trunc"][k] / sigma_opt(out["box_trunc"][k])
        out["snr_A"][k] = out["psf_full"][k] / sigma_opt(out["psf_full"][k])
        out["snr_B"][k] = out["box_untrunc"][k] / math.sqrt(bg["var_box_bg"])

    res: Dict[str, Any] = {
        "F_true": F_true, "fwhm_det": fwhm_det, "sigma_sky": sigma_sky,
        "gain": gain, "rn_e": rn_e, "n_real": n_real, "beta_true": beta_true,
        "E_B_model": float(P_model[box].sum()), "E_B_true": float(P_true[box].sum()),
        "sigma_opt_at_Ftrue": sigma_opt(F_true),
        "sigma_box": math.sqrt(bg["var_box_bg"]),
        "var_box_bg_rel": bg["bg_rel"],
        "trunc_bias_analytic": bm["bias_trunc_over_Fbox"],
        "trunc_var_ratio_analytic": bm["var_ratio_trunc_over_untrunc"],
        "fit_bound_hit_frac": n_bound_hit / n_real,
        "estimators": {},
    }
    for k in ("box_trunc", "box_untrunc", "psf_amp", "psf_full"):
        v = out[k]
        rob = float(1.482602218505602 * np.median(np.abs(v - np.median(v))))
        res["estimators"][k] = {
            "mean": float(v.mean()), "std": float(v.std(ddof=1)),
            "std_robust": rob,
            "sem": float(v.std(ddof=1) / math.sqrt(n_real)),
            "bias_rel": float(v.mean() / F_true - 1.0),
            "bias_rel_sem": float(v.std(ddof=1) / math.sqrt(n_real) / F_true),
            "bias_rel_robust": float(np.median(v) / F_true - 1.0),
        }
    # 配对比：报告 sigma / 实测 std（1.0 = 完全配对）
    res["pairing"] = {
        "CUR_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["box_trunc"]["std"],
        "A_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["psf_full"]["std"],
        "A_ampfix_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["psf_amp"]["std"],
        "B_reported_over_measured": res["sigma_box"] / res["estimators"]["box_untrunc"]["std"],
    }
    res["pairing_robust"] = {
        "CUR_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["box_trunc"]["std_robust"],
        "A_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["psf_full"]["std_robust"],
        "A_ampfix_reported_over_measured": res["sigma_opt_at_Ftrue"] / res["estimators"]["psf_amp"]["std_robust"],
        "B_reported_over_measured": res["sigma_box"] / res["estimators"]["box_untrunc"]["std_robust"],
    }
    res["snr"] = {k: {"mean": float(out[k].mean()), "std": float(out[k].std(ddof=1))}
                  for k in ("snr_cur", "snr_A", "snr_B")}
    return res


def _fit_full_flux(d: np.ndarray, b: float, sigma0: float, F_true: float) -> float:
    """全参数最小二乘（B, A, cx, cy, sx, sy）=> flux = 2*pi*A*sx*sy/3。

    与生产 dpsf_fit_batch_f64 同构（Moffat4 beta=4；本臂圆对称故不含 theta）。
    """
    n = d.shape[0]
    jj, ii = np.mgrid[0:n, 0:n]
    A0 = F_true / (2.0 * math.pi * sigma0 * sigma0 / 3.0)

    def resid(p):
        B, A, cx, cy, sx, sy = p
        q = ((ii - cx) / sx) ** 2 + ((jj - cy) / sy) ** 2
        return (B + A * (1.0 + 0.5 * q) ** (-4) - d).ravel()

    lo = np.array([b - 100.0, 0.0, (n - 1) / 2.0 - 1.5, (n - 1) / 2.0 - 1.5, 0.30, 0.30])
    hi = np.array([b + 100.0, 50.0 * A0 + 1.0, (n - 1) / 2.0 + 1.5, (n - 1) / 2.0 + 1.5,
                   5.0, 5.0])
    best = None
    for scale in (1.0, 0.5, 2.0):
        p0 = np.clip(np.array([b, A0 * scale, (n - 1) / 2.0, (n - 1) / 2.0,
                               sigma0, sigma0]), lo + 1e-9, hi - 1e-9)
        try:
            r = least_squares(resid, p0, bounds=(lo, hi), method="trf",
                              xtol=1e-12, ftol=1e-12, gtol=1e-12, max_nfev=600)
        except Exception:
            continue
        if best is None or r.cost < best.cost:
            best = r
    if best is None:
        return float("nan")
    B, A, cx, cy, sx, sy = best.x
    hit = bool(np.any(np.isclose(best.x, lo, atol=1e-9))
               or np.any(np.isclose(best.x, hi, atol=1e-9)))
    _fit_full_flux.last_bound_hit = hit
    return C.psf_analytic_flux(A, sx, sy)


# ---------------------------------------------------------------------------
# 解析臂：三候选的对照表 + 跨帧（seeing）扫描
# ---------------------------------------------------------------------------
def fisher_inflation(F_true: float, fwhm_det: float, sigma_sky: float, gain: float,
                     rn_e: float, src: int = C.SRC_EMPIRICAL_TOTAL_RMS,
                     n_sky: float = 1.6e7, shape=(17, 17)) -> Dict[str, Any]:
    """**解析（CRLB）**：全参数 PSF 拟合的通量方差 / 只拟合幅度（形状位置已知）的通量方差。

    这是不依赖任何拟合器实现的**可证**结论：
      模型  m_i = B + A * (1 + q_i/2)^-4 ,  q_i = ((x_i-cx)/sx)^2 + ((y_i-cy)/sy)^2
      参数  theta = (B, A, cx, cy, sx, sy)
      权重  W_ii = 1/V_i ,  V_i = sigma_sky^2 + F*P_i/g （gain>0 支）
      通量  Fhat = 2*pi*A*sx*sy/3  =>  g = dF/dtheta
      Var_fit(Fhat) = g^T (J^T W J)^-1 g
      Var_amp(Fhat) = (2*pi*sx*sy/3)^2 / sum(P_i^2/V_i)   （只有 A 自由）
      膨胀因子 = sqrt(Var_fit / Var_amp)
    """
    n = shape[0]
    jj, ii = np.mgrid[0:n, 0:n]
    sigma = C.detection_sigma_from_fwhm(fwhm_det)
    A = F_true / (2.0 * math.pi * sigma * sigma / 3.0)
    c = (n - 1) / 2.0
    th = np.array([1000.0, A, c, c, sigma, sigma])

    def model(thv):
        B, Aa, cx, cy, sx, sy = thv
        q = ((ii - cx) / sx) ** 2 + ((jj - cy) / sy) ** 2
        return B + Aa * (1.0 + 0.5 * q) ** (-4.0)

    m0 = model(th)
    P = (m0 - th[0]) / F_true                     # 归一化轮廓（sum P = 1）
    V = C.pixel_variance(P, F_true, sigma_sky, gain, rn_e, src)
    W = 1.0 / V
    # 数值雅可比（中心差分）
    J = np.zeros((n * n, 6))
    for k in range(6):
        h = max(abs(th[k]) * 1e-6, 1e-9)
        tp = th.copy(); tp[k] += h
        tm = th.copy(); tm[k] -= h
        J[:, k] = ((model(tp) - model(tm)) / (2.0 * h)).ravel()
    FIM = J.T @ (W.ravel()[:, None] * J)
    cov = np.linalg.inv(FIM)
    gvec = np.array([0.0, 2.0 * math.pi * th[4] * th[5] / 3.0, 0.0, 0.0,
                     2.0 * math.pi * th[1] * th[5] / 3.0,
                     2.0 * math.pi * th[1] * th[4] / 3.0])
    var_fit = float(gvec @ cov @ gvec)
    # 只拟合幅度（形状/位置固定在真值）时 Var(Fhat) = (2*pi*sx*sy/3)^2 / sum(S_i^2/V_i)，
    # 而 sum(P_i^2/V_i) = sum(S_i^2/V_i)/(sum S)^2 且 sum S ≈ 2*pi*sx*sy/3
    # => Var_amp ≈ 1/sum(P_i^2/V_i) = 生产 var_optimal（数值上一致到 0.5% 以内）。
    var_amp = float(C.var_optimal(P, V))
    return {"F_true": F_true, "fwhm_det": fwhm_det, "sigma_moffat": sigma,
            "var_fit": var_fit, "var_amp": var_amp,
            "sigma_ratio_fit_over_amp": math.sqrt(var_fit / var_amp),
            "var_ratio_fit_over_amp": var_fit / var_amp,
            "pairing_if_no_correction": 1.0 / math.sqrt(var_fit / var_amp),
            "n_sky_used": n_sky}


def analytic_table(F_true: float, fwhm_det: float, sigma_sky: float, gain: float,
                   rn_e: float, src: int) -> Dict[str, Any]:
    t = C.estimator_table(F_true, fwhm_det, sigma_sky, gain, rn_e, src, N_SKY_FRAME)
    P, sum_p2, half, sigma = C.profile_for_fwhm(fwhm_det)
    return {
        "fwhm_det": fwhm_det, "F_true": F_true, "E_B": t["E_B"], "half": half,
        "sum_p2": sum_p2,
        "flux_CUR": t["current_flux"], "flux_A": t["A_flux"], "flux_B": t["B_flux"],
        "flux_over_true_CUR": t["current_flux"] / F_true,
        "flux_over_true_A": t["A_flux"] / F_true,
        "flux_over_true_B": t["B_flux"] / F_true,
        "snr_CUR": t["current_snr"], "snr_A": t["A_snr"], "snr_B": t["B_snr"],
        "snr_truth": t["snr_truth_total_flux"],
        "sigma_opt": t["sigma_opt"], "sigma_box": t["B_sigma"],
        "bg_rel": t["box_bg"]["bg_rel"],
        "trunc_bias_rel": t["box_moments"]["bias_trunc_over_Fbox"],
        "trunc_var_ratio": t["box_moments"]["var_ratio_trunc_over_untrunc"],
    }


def seeing_scan(F_true: float, sigma_sky: float, gain: float, rn_e: float, src: int,
                fwhm_list=(1.6, 2.0, 2.5, 3.0, 3.5, 3.9)) -> Dict[str, Any]:
    rows = [analytic_table(F_true, fw, sigma_sky, gain, rn_e, src) for fw in fwhm_list]
    def spread(key):
        v = np.array([r[key] for r in rows])
        return {"min": float(v.min()), "max": float(v.max()),
                "spread_rel": float(v.max() / v.min() - 1.0)}
    return {"rows": rows,
            "flux_over_true_CUR": spread("flux_over_true_CUR"),
            "flux_over_true_A": spread("flux_over_true_A"),
            "flux_over_true_B": spread("flux_over_true_B"),
            "snr_CUR": spread("snr_CUR"), "snr_A": spread("snr_A"), "snr_B": spread("snr_B")}


# ---------------------------------------------------------------------------
# 负例
# ---------------------------------------------------------------------------
def negative_delta_psf(rng) -> Dict[str, Any]:
    """N1：delta PSF + 1 像素盒 => 三个估计量逐位相同 => 配对比必须 == 1。"""
    P = np.zeros((3, 3)); P[1, 1] = 1.0
    V = np.full((3, 3), 4.0)
    F_true, n = 100.0, 2000
    vals = []
    for _ in range(n):
        d = F_true * P + rng.normal(0.0, 2.0, size=(3, 3))
        vals.append(d[1, 1])
    vals = np.array(vals)
    sig_opt = math.sqrt(C.var_optimal(P, V))
    sig_box = math.sqrt(float(V[1, 1]))
    return {"reported_sigma": sig_opt, "measured_std": float(vals.std(ddof=1)),
            "ratio": sig_opt / float(vals.std(ddof=1)),
            "sigma_opt_equals_sigma_box": bool(abs(sig_opt - sig_box) < 1e-12),
            "bias_rel": float(vals.mean() / F_true - 1.0)}


def negative_no_clip(rng, F_true: float, fwhm_det: float, sigma_sky: float,
                     n_real: int = 4000) -> Dict[str, Any]:
    """N2：去掉 v<=0 截断 => 截断偏置必须归零（<=3 MC sigma）。"""
    P_true, P_model, sigma = _stamp_profile(F_true, fwhm_det)
    V = C.pixel_variance(P_model, F_true, sigma_sky, GAIN, RN, C.SRC_EMPIRICAL_TOTAL_RMS)
    mu = F_true * P_true
    sig = np.sqrt(V)
    n = mu.shape[0]
    c0 = n // 2
    box = np.zeros_like(mu, dtype=bool)
    box[c0 - 2:c0 + 3, c0 - 2:c0 + 3] = True
    tr, un = [], []
    for _ in range(n_real):
        d = 1000.0 + mu + rng.normal(0.0, 1.0, size=mu.shape) * sig
        yb = (d - 1000.0)[box]
        tr.append(float(np.maximum(yb, 0.0).sum()))
        un.append(float(yb.sum()))
    tr, un = np.array(tr), np.array(un)
    return {"bias_trunc_rel": float(tr.mean() / F_true - 1.0),
            "bias_untrunc_rel": float(un.mean() / F_true - 1.0),
            "bias_untrunc_z": float((un.mean() - F_true * P_true[box].sum())
                                    / (un.std(ddof=1) / math.sqrt(n_real))),
            "z_trunc": float((tr.mean() - F_true * P_true[box].sum())
                             / (tr.std(ddof=1) / math.sqrt(n_real)))}


def negative_same_seeing(F_true: float, sigma_sky: float) -> Dict[str, Any]:
    """N3：同一 seeing（无效应）=> 跨帧伪差必须严格 0。"""
    a = analytic_table(F_true, 3.0, sigma_sky, GAIN, RN, C.SRC_EMPIRICAL_TOTAL_RMS)
    b = analytic_table(F_true, 3.0, sigma_sky, GAIN, RN, C.SRC_EMPIRICAL_TOTAL_RMS)
    return {"spurious_flux_CUR": a["flux_over_true_CUR"] / b["flux_over_true_CUR"] - 1.0,
            "spurious_flux_A": a["flux_over_true_A"] / b["flux_over_true_A"] - 1.0,
            "spurious_flux_B": a["flux_over_true_B"] / b["flux_over_true_B"] - 1.0,
            "spurious_snr_CUR": a["snr_CUR"] / b["snr_CUR"] - 1.0}


def negative_zero_noise() -> Dict[str, Any]:
    """N4：sigma->0 => 配对比度量必须判**退化**（不得静默通过）。"""
    P, sum_p2, half, sigma = C.profile_for_fwhm(3.0)
    V = np.full(P.shape, 0.0)
    a = float((P * P / np.where(V > 0, V, np.inf)).sum())
    return {"var_optimal": float("inf") if a == 0 else 1.0 / a,
            "degenerate_flag": bool(a == 0 or not math.isfinite(1.0 / a if a else float("inf")))}


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE.parents[1] / "results" / "exp01_q2a_pairing.json"))
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    n_real = 150 if args.quick else 500
    out: Dict[str, Any] = {"meta": {"seed": SEED, "gain": GAIN, "rn_e": RN, "sky_adu": SKY,
                                    "n_sky_frame": N_SKY_FRAME, "script": Path(__file__).name}}

    # 解析表 + seeing 扫描
    out["fisher_inflation"] = [
        fisher_inflation(F, fw, SKY, GAIN, RN)
        for F in ((300.0, 1000.0, 10000.0) if not args.quick else (1000.0,))
        for fw in ((2.0, 3.0, 3.9) if not args.quick else (3.0,))]
    out["analytic_seeing_scan"] = seeing_scan(1000.0, SKY, GAIN, RN,
                                              C.SRC_EMPIRICAL_TOTAL_RMS)
    out["analytic_flux_scan"] = [analytic_table(F, 3.0, SKY, GAIN, RN,
                                                C.SRC_EMPIRICAL_TOTAL_RMS)
                                 for F in (100.0, 300.0, 1000.0, 3000.0, 10000.0, 100000.0)]
    # 生产默认 gain<=0 分支（无源泊松项）
    out["analytic_gain0_branch"] = [analytic_table(F, 3.0, SKY, 0.0, RN,
                                                   C.SRC_EMPIRICAL_TOTAL_RMS)
                                    for F in (100.0, 1000.0, 10000.0)]

    # MC 配对
    out["mc"] = {}
    # 通量点选择：峰值 SNR = F*P_center/sigma_sky >= 5（生产只对**已检出**源出 SNR）
    for F in (300.0, 1000.0, 10000.0):
        out["mc"]["F=%.0f" % F] = mc_estimators(
            rng, F, 3.0, SKY, GAIN, RN, n_real=n_real,
            fit_full=not args.quick)
    # 生产默认 gain<=0 分支（无源泊松项；方差公式 V_i = sigma_sky^2）
    out["mc_gain0_branch"] = {
        "F=%.0f" % F: mc_estimators(rng, F, 3.0, SKY, 0.0, RN, n_real=n_real,
                                    fit_full=False)
        for F in (100.0, 1000.0, 10000.0)}
    # PSF 失配臂（真值 beta=3，模型 beta=4）
    out["mc_beta_mismatch"] = mc_estimators(rng, 1000.0, 3.0, SKY, GAIN, RN,
                                            n_real=max(60, n_real // 3),
                                            beta_true=3.0, fit_full=not args.quick)

    # 负例
    out["negatives"] = {
        "N1_delta_psf_1px": negative_delta_psf(rng),
        "N2_no_clip": negative_no_clip(rng, 300.0, 3.0, SKY),
        "N3_same_seeing": negative_same_seeing(1000.0, SKY),
        "N4_zero_noise": negative_zero_noise(),
    }
    out["meta"]["elapsed_s"] = time.time() - t0
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps({"seeing_flux_spread": {k: out["analytic_seeing_scan"][k]
                                             for k in ("flux_over_true_CUR", "flux_over_true_A",
                                                       "flux_over_true_B")},
                      "seeing_snr_spread": {k: out["analytic_seeing_scan"][k]
                                            for k in ("snr_CUR", "snr_A", "snr_B")},
                      "negatives": out["negatives"],
                      "mc_pairing": {k: v["pairing"] for k, v in out["mc"].items()}},
                     ensure_ascii=False, indent=1))
    print("wrote", outp, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
