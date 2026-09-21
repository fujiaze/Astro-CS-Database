#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B1：物理 Monte Carlo 真值 + 天光扫描 + 核心负例（天光无关性）。

假说（事前声明）：
  H1 固定真实源通量、仅抬升天光（天光散粒噪声如实进噪声项）⇒ 帧级 SNR=F_signal/σ_F
     单调下降；天光主导段 log-log 斜率 = −1/2；B→∞ 时 SNR→0。
  H2 定义式 SNR_def=F_adu/σ_F^theory 与 MC 经验真值 SNR_emp=mean(F̂)/std(F̂) 在
     95% 置信区间内一致（σ_F 噪声项组成正确，Horne 1986）。
  H3 生产口径三臂（见 frozen_config.prod_arms）中，与 Horne 口径一致的臂落在 MC CI 内；
     "经验总 σ_sky + (RN/g)²"臂在 RN 占比高时显示可量化偏差（登记为发现，不改生产码）。
  H4 负例 1（算术常数、无散粒噪声）：SNR 变化恒等于 0（真值无效应 ⇒ 度量归零）。
  H5 负例 2（传统"信号含天光"口径）：随天光单调上升，相对失真随 B 增长（判红）。
  H6 局部背景估计偏差 δB 对 SNR 的影响有边界：|ΔSNR/SNR|=1% 处给出 δB*。
  H7 故障注入（天光泄漏进信号项）必须使单调性门变红 ⇒ 门非恒真。
输出：results/b1_sky_scan.json
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np
from scipy import stats

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

# ---- 冻结配置（事前声明，运行中不改） ----
GAIN = 1.3          # e-/ADU（声明坐标，同 EXP-205；不做物理闭合反推）
RN = 10.0           # e- rms/px
DARK = 0.5          # e-/px
SIGMA_PSF = 1.5     # Moffat4 beta=4 sigma [px]（FWHM_M4 = 1.230310σ = 1.845 px）
F_BRIGHT = 3000.0   # 源总通量 [e-]
F_FAINT = 100.0     # 暗源 [e-]（源泊松项不主导，检验 σ 组成与天光趋零）
HALF = 30
N_MC = 1000
SKY_SCAN = [0.0, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0,
            10000.0, 30000.0, 100000.0, 1000000.0]
R_IN, R_OUT = 10.0, 30.0
BIAS_FRACS = [0.0, 0.001, 0.003, 0.01, 0.03, 0.1, 0.3]
LEAK_BETAS = [0.0, 1e-4, 1e-3, 3e-3, 0.01, 0.03, 0.1]
PROD_ARMS = {
    "skyonly_plus_rn": "σ_sky=天光+暗流散粒（不含 RN），再 + (RN/g)² —— 生产头文件设计意图",
    "empirical_plus_rn": "σ_sky=经验空天总 rms（含 RN），再 + (RN/g)² —— 调度器 snr 配置给 gain/RN 时",
    "empirical_only": "σ_sky=经验空天总 rms（含 RN），不加 RN 项 —— PSF 行路径（gain 未知）",
}


def annulus_mask(shape, r_in, r_out):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    cy = (shape[0] - 1) / 2.0; cx = (shape[1] - 1) / 2.0
    r = np.hypot(yy - cy, xx - cx)
    return (r >= r_in) & (r <= r_out)


def extract(d, b_hat, sig_hat, P, mode, F_true, B, gain, rn):
    """逐帧最优提取。mode 决定 σ_i² 组成；返回 (F_hat 数组, sigma_F 数组)。"""
    n = d.shape[0]
    Pf = P.ravel()
    F = np.empty(n); sigF = np.empty(n)
    dsub = d - b_hat[:, None, None]
    for k in range(n):
        if mode == "oracle":
            var_i = (B + DARK + rn ** 2 + F_true * Pf) / gain ** 2
            w = 1.0 / var_i
            den = float((Pf * Pf * w).sum())
            F[k] = float((Pf * w * dsub[k].ravel()).sum()) / den
            sigF[k] = float(np.sqrt(1.0 / den))
        else:
            Fk = 0.0
            for _ in range(2):
                if mode == "total":
                    var_i = sig_hat[k] ** 2 + np.maximum(Fk * Pf, 0.0) / gain
                elif mode == "skyonly_plus_rn":
                    var_i = (B + DARK) / gain ** 2 + (rn / gain) ** 2 + np.maximum(Fk * Pf, 0.0) / gain
                elif mode == "empirical_plus_rn":
                    var_i = sig_hat[k] ** 2 + (rn / gain) ** 2 + np.maximum(Fk * Pf, 0.0) / gain
                elif mode == "empirical_only":
                    var_i = sig_hat[k] ** 2 + np.maximum(Fk * Pf, 0.0) / gain
                else:
                    raise ValueError(mode)
                w = 1.0 / var_i
                den = float((Pf * Pf * w).sum())
                Fk = float((Pf * w * dsub[k].ravel()).sum()) / den
            F[k] = Fk
            sigF[k] = float(np.sqrt(1.0 / den))
    return F, sigF


def simulate(F_E, B, n, seed_off):
    P, sum_p2, p_center, half = C.moffat4_grid(SIGMA_PSF, HALF)
    shape = P.shape
    r = C.rng(seed_off)
    lam = F_E * P + B + DARK
    e = r.poisson(lam, size=(n,) + shape).astype(np.float64)
    if RN > 0:
        e += r.normal(0.0, RN, size=(n,) + shape)
    return e / GAIN, P


def scan(F_E, seed_off):
    rows = []
    P, _, _, _ = C.moffat4_grid(SIGMA_PSF, HALF)
    shape = P.shape
    mask_bg = annulus_mask(shape, R_IN, R_OUT)
    n_sky = int(mask_bg.sum())
    Pf = P.ravel()
    F_ADU = F_E / GAIN
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    cy = (shape[0] - 1) / 2.0; cx = (shape[1] - 1) / 2.0
    ap = C.aperture_snr_theory(F_E, 0.0, DARK, RN, GAIN, SIGMA_PSF)
    r_ap = ap["r_ap_px"]; n_pix = ap["n_pix"]
    apm = np.hypot(yy - cy, xx - cx) <= r_ap
    for i, B in enumerate(SKY_SCAN):
        t0 = time.time()
        d, _ = simulate(F_E, B, N_MC, seed_off + 100 * i)
        bg = d[:, mask_bg]
        b_hat = np.median(bg, axis=1)
        sig_hat = C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_hat[:, None]), axis=1)
        F_or, _ = extract(d, b_hat, sig_hat, P, "oracle", F_E, B, GAIN, RN)
        F_tot, sF_tot = extract(d, b_hat, sig_hat, P, "total", F_E, B, GAIN, RN)
        F_srn, sF_srn = extract(d, b_hat, sig_hat, P, "skyonly_plus_rn", F_E, B, GAIN, RN)
        F_ern, sF_ern = extract(d, b_hat, sig_hat, P, "empirical_plus_rn", F_E, B, GAIN, RN)
        F_eo, sF_eo = extract(d, b_hat, sig_hat, P, "empirical_only", F_E, B, GAIN, RN)
        # MC 经验真值（用与 Horne 一致的 total 臂）
        Fbar = float(np.mean(F_tot)); Fsd = float(np.std(F_tot, ddof=1))
        snr_emp = F_ADU / Fsd
        lo = Fsd * np.sqrt((N_MC - 1) / stats.chi2.ppf(0.975, N_MC - 1))
        hi = Fsd * np.sqrt((N_MC - 1) / stats.chi2.ppf(0.025, N_MC - 1))
        # 定义式（真值参数，ADU 通量）
        sigF_def = C.horne_sigma_f_theory(F_E, B, DARK, RN, GAIN, P)
        snr_def = F_ADU / sigF_def
        # 负例 1：算术常数 +200 ADU（无散粒噪声）—— 用同一 total 臂
        c_const = 200.0
        d_const = d + c_const
        bgc = d_const[:, mask_bg]
        b_c = np.median(bgc, axis=1)
        s_c = C.K_MAD_TO_SIGMA * np.median(np.abs(bgc - b_c[:, None]), axis=1)
        F_c, _ = extract(d_const, b_c, s_c, P, "total", F_E, B, GAIN, RN)
        snr_const = F_ADU / float(np.std(F_c, ddof=1))
        # 负例 2：传统"信号含天光"（孔径求和含天光，不扣背景）
        raw_sum = d[:, apm].sum(axis=1)
        var_raw = n_pix * (sig_hat ** 2) * (1.0 + n_pix / n_sky) + raw_sum / GAIN
        snr_trad = float(np.mean(raw_sum) / np.sqrt(np.mean(var_raw)))
        snr_raw_frame = float((np.mean(F_tot) + np.mean(b_hat) * n_pix) / np.mean(sF_tot))
        rows.append(dict(
            sky_e_per_px=B, n_sky_bg=n_sky, snr_def=snr_def, sigma_f_def=sigF_def,
            snr_emp=snr_emp, snr_emp_ci95=[float(F_ADU / hi), float(F_ADU / lo)],
            F_hat_mean_adu=Fbar, F_hat_std_adu=Fsd,
            sigma_sky_hat_med_adu=float(np.median(sig_hat)),
            sigma_sky_true_total_adu=float(np.sqrt((B + DARK) / GAIN ** 2 + (RN / GAIN) ** 2)),
            snr_prod_skyonly_rn=float(F_ADU / np.mean(sF_srn)),
            snr_prod_empirical_rn=float(F_ADU / np.mean(sF_ern)),
            snr_prod_empirical_only=float(F_ADU / np.mean(sF_eo)),
            sigma_f_mc=Fsd,
            sigma_f_prod_skyonly_rn=[float(np.mean(sF_srn)), float(np.std(sF_srn, ddof=1))],
            sigma_f_prod_empirical_rn=[float(np.mean(sF_ern)), float(np.std(sF_ern, ddof=1))],
            sigma_f_prod_empirical_only=[float(np.mean(sF_eo)), float(np.std(sF_eo, ddof=1))],
            snr_arm_oracle=float(F_ADU / np.std(F_or, ddof=1)),
            snr_arm_const=snr_const, snr_arm_trad=snr_trad,
            snr_arm_raw_frame=snr_raw_frame,
            r_ap_px=r_ap, n_pix_ap=n_pix, elapsed_s=time.time() - t0))
        print("F=%6.0f B=%9.1f SNR_def=%9.4f emp=%9.4f [%.3f,%.3f] skyonly+RN=%.4f emp+RN=%.4f const=%.4f trad=%9.2f"
              % (F_E, B, snr_def, snr_emp, F_ADU / hi, F_ADU / lo,
                 rows[-1]["snr_prod_skyonly_rn"], rows[-1]["snr_prod_empirical_rn"],
                 snr_const, snr_trad), flush=True)
    return rows


def gates(rows):
    B = np.array([r["sky_e_per_px"] for r in rows])
    sd = np.array([r["snr_def"] for r in rows])
    se = np.array([r["snr_emp"] for r in rows])
    lo = np.array([r["snr_emp_ci95"][0] for r in rows])
    hi = np.array([r["snr_emp_ci95"][1] for r in rows])
    g = {}
    rho, pval = stats.spearmanr(B, sd)
    g["G1_spearman_rho_def"] = float(rho)
    g["G1_spearman_p_def"] = float(pval)
    g["G1_strict_monotone_def"] = bool(np.all(np.diff(sd) < 0))
    rho_e, pval_e = stats.spearmanr(B, se)
    g["G1_spearman_rho_emp"] = float(rho_e)
    g["G1_spearman_p_emp"] = float(pval_e)
    g["G1_emp_rank_monotone"] = bool(rho_e <= -0.95 and pval_e < 1e-6)
    g["G1_no_significant_increase_emp"] = bool(not np.any(lo[1:] > hi[:-1]))
    m = B >= 1000.0
    g["G1_significant_decrease_B_ge_1000"] = bool(np.all(hi[1:][m[1:]] < lo[:-1][m[1:]]))
    big = B >= 3000.0
    slope, _ = C.loglog_slope(B[big], sd[big])
    g["G2_large_sky_slope_def"] = float(slope)
    g["G2_slope_within_15pct_of_minus_half"] = bool(abs(slope + 0.5) < 0.075)
    g["G2_snr_max_over_min"] = float(sd[-1] / sd[0])
    g["G2_tends_to_zero_below_3pct"] = bool(sd[-1] / sd[0] < 0.03)
    sig_mc = np.array([r["sigma_f_mc"] for r in rows])
    sig_def = np.array([r["sigma_f_def"] for r in rows])
    se_sig = sig_mc / np.sqrt(2 * (N_MC - 1))
    z = (sig_mc - sig_def) / se_sig
    g["G3_def_in_mc_ci95_frac"] = float(np.mean((sd >= lo) & (sd <= hi)))
    g["G3_frac_within_ci_ge_0p90"] = bool(np.mean((sd >= lo) & (sd <= hi)) >= 0.90)
    g["G3_max_abs_z_sigma"] = float(np.max(np.abs(z)))
    g["G3_all_within_3sigma"] = bool(np.all(np.abs(z) <= 3.0))
    g["G3_mean_z"] = float(np.mean(z))
    g["G3_max_rel_dev_def_vs_emp"] = float(np.max(np.abs(sd / se - 1)))
    # 生产臂：mean(sigma_F,arm) 对 MC 真值 sigma_F,mc 的 z 检验
    for key, name in [("sigma_f_prod_skyonly_rn", "G4a_skyonly_rn"),
                      ("sigma_f_prod_empirical_only", "G4b_empirical_only"),
                      ("sigma_f_prod_empirical_rn", "G4c_empirical_rn")]:
        mean_arm = np.array([r[key][0] for r in rows])
        std_arm = np.array([r[key][1] for r in rows])
        se_arm = np.sqrt((std_arm / np.sqrt(N_MC)) ** 2 + (sig_mc / np.sqrt(2 * (N_MC - 1))) ** 2)
        zz = (mean_arm - sig_mc) / se_arm
        g[name + "_max_abs_z"] = float(np.max(np.abs(zz)))
        pref = "FINDING_doublecount" if name == "G4c_empirical_rn" else name
        g[pref + "_all_within_3sigma"] = bool(np.all(np.abs(zz) <= 3.0))
        g[name + "_max_rel_dev"] = float(np.max(np.abs(mean_arm / sig_mc - 1)))
        g[name + "_rel_dev_at_B0"] = float(mean_arm[0] / sig_mc[0] - 1)
        g[name + "_rel_dev_at_max_sky"] = float(mean_arm[-1] / sig_mc[-1] - 1)
        g[name + "_abs_z_at_max_sky"] = float(abs(zz[-1]))
        g[pref + "_all_within_3sigma_except_B0"] = bool(np.all(np.abs(zz[1:]) <= 3.0))
    # 双计偏差的特征化门（非退化）：RN 主导处必须显著、天光主导处必须落在 3σ 内
    g["G4c_doublecount_detected_where_RN_dominates"] = bool(
        abs(g["G4c_empirical_rn_rel_dev_at_B0"]) > 0.05)
    g["G4c_doublecount_vanishes_at_high_sky_3sigma"] = bool(
        g["G4c_empirical_rn_abs_z_at_max_sky"] <= 3.0)
    g["G4c_bias_ratio_B0_over_maxsky"] = float(
        abs(g["G4c_empirical_rn_rel_dev_at_B0"]) / max(abs(g["G4c_empirical_rn_rel_dev_at_max_sky"]), 1e-12))
    cst = np.array([r["snr_arm_const"] for r in rows])
    g["G5_arith_null_max_rel_change"] = float(np.max(np.abs(cst / se - 1)))
    g["G5_arith_null_exact_zero"] = bool(np.max(np.abs(cst / se - 1)) < 1e-10)
    trad = np.array([r["snr_arm_trad"] for r in rows])
    rawf = np.array([r["snr_arm_raw_frame"] for r in rows])
    g["G6_trad_monotone_increasing"] = bool(np.all(np.diff(trad) > 0))
    g["G6_trad_distortion_at_max_sky"] = float(trad[-1] / sd[-1] - 1)
    g["G6_raw_frame_distortion_at_max_sky"] = float(rawf[-1] / sd[-1] - 1)
    g["G6_negative_control_detected"] = bool(trad[-1] / sd[-1] > 2.0 and rawf[-1] / sd[-1] > 2.0)
    return g


def bias_boundary(seed_off):
    P, _, _, _ = C.moffat4_grid(SIGMA_PSF, HALF)
    shape = P.shape
    mask_bg = annulus_mask(shape, R_IN, R_OUT)
    B, N = 1000.0, 400
    d, _ = simulate(F_BRIGHT, B, N, seed_off)
    bg = d[:, mask_bg]
    b_true = np.median(bg, axis=1)
    sig = float(np.median(C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_true[:, None]), axis=1)))
    Pf = P.ravel()
    out = []
    for frac in BIAS_FRACS:
        b_use = b_true + frac * (B / GAIN)          # δB 以 ADU 计（b̂ 的单位）
        F = np.empty(N)
        for k in range(N):
            Fk = 0.0
            for _ in range(2):
                var_i = sig ** 2 + np.maximum(Fk * Pf, 0.0) / GAIN
                w = 1.0 / var_i
                den = float((Pf * Pf * w).sum())
                Fk = float((Pf * w * (d[k] - b_use[k]).ravel()).sum()) / den
            F[k] = Fk
        sigF = C.horne_sigma_f_theory(F_BRIGHT, B, DARK, RN, GAIN, P)
        snr_true = (F_BRIGHT / GAIN) / sigF
        flux_rel = float(np.mean(F) / (F_BRIGHT / GAIN) - 1)
        out.append(dict(bias_frac=frac, delta_bg_e=frac * B, delta_bg_adu=frac * B / GAIN, snr_true=snr_true,
                        snr_biased=snr_true * (1.0 + flux_rel),
                        snr_rel_dev=flux_rel, flux_rel_dev=flux_rel))
    var_i = sig ** 2
    sens = float((Pf / var_i).sum() / (Pf * Pf / var_i).sum())     # dF̂/dδB（ADU/ADU）
    db_star_adu = 0.01 * (F_BRIGHT / GAIN) / sens
    db_star_e = db_star_adu * GAIN
    return dict(sky_e_per_px=B, sensitivity_dF_dB=sens,
                delta_bg_star_1pct_adu=db_star_adu, delta_bg_star_1pct_e=db_star_e,
                delta_bg_star_1pct_frac_of_sky=db_star_e / B, rows=out)


def F_ADU_OF(F_E):
    return F_E / GAIN


def gradient_boundary(seed_off):
    P, _, _, _ = C.moffat4_grid(SIGMA_PSF, HALF)
    shape = P.shape
    mask_bg = annulus_mask(shape, R_IN, R_OUT)
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    cy = (shape[0] - 1) / 2.0; cx = (shape[1] - 1) / 2.0
    B0, N = 1000.0, 400
    Pf = P.ravel()
    out = []
    for g in [0.0, 0.1, 0.3, 1.0, 3.0, 10.0, 30.0]:
        r = C.rng(seed_off + 7)
        lam = F_BRIGHT * P + B0 + g * (xx - cx) + DARK
        # 对称环带（质心与源重合）⇒ 一阶对线性梯度不敏感
        e = r.poisson(lam, size=(N,) + shape).astype(np.float64) + r.normal(0.0, RN, size=(N,) + shape)
        d = e / GAIN
        bg = d[:, mask_bg]
        b_hat = np.median(bg, axis=1)
        sig = float(np.median(C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_hat[:, None]), axis=1)))
        F = np.empty(N)
        for k in range(N):
            Fk = 0.0
            for _ in range(2):
                var_i = sig ** 2 + np.maximum(Fk * Pf, 0.0) / GAIN
                w = 1.0 / var_i
                den = float((Pf * Pf * w).sum())
                Fk = float((Pf * w * (d[k] - b_hat[k]).ravel()).sum()) / den
            F[k] = Fk
        out.append(dict(grad_e_per_px2=g, bg_bias_e=float(np.mean(b_hat) - (B0 + DARK) / GAIN),
                        flux_rel_dev=float(np.mean(F) / (F_BRIGHT / GAIN) - 1)))
    g_star = float("nan")
    for i in range(1, len(out)):
        y0 = abs(out[i - 1]["flux_rel_dev"]); y1 = abs(out[i]["flux_rel_dev"])
        if y1 >= 0.01:
            x0, x1 = out[i - 1]["grad_e_per_px2"], out[i]["grad_e_per_px2"]
            g_star = x0 + (0.01 - y0) * (x1 - x0) / (y1 - y0) if y1 != y0 else x1
            break
    # 偏心环带：背景环带中心相对源偏移 dx px ⇒ 一阶偏差 ≈ g·dx（真实失配场景）
    out_off = []
    for g in [0.0, 0.1, 0.3, 1.0, 3.0, 10.0, 30.0]:
        r = C.rng(seed_off + 17)
        lam = F_BRIGHT * P + B0 + g * (xx - cx) + DARK
        e = r.poisson(lam, size=(N,) + shape).astype(np.float64) + r.normal(0.0, RN, size=(N,) + shape)
        d = e / GAIN
        yy2, xx2 = np.mgrid[0:shape[0], 0:shape[1]]
        r_off = np.hypot(yy2 - cy, xx2 - (cx + 2.0))       # 环带中心偏移 +2 px
        mask_off = (r_off >= R_IN) & (r_off <= R_OUT)
        bg = d[:, mask_off]
        b_hat = np.median(bg, axis=1)
        sig = float(np.median(C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_hat[:, None]), axis=1)))
        F = np.empty(N)
        for k in range(N):
            Fk = 0.0
            for _ in range(2):
                var_i = sig ** 2 + np.maximum(Fk * Pf, 0.0) / GAIN
                w = 1.0 / var_i
                den = float((Pf * Pf * w).sum())
                Fk = float((Pf * w * (d[k] - b_hat[k]).ravel()).sum()) / den
            F[k] = Fk
        out_off.append(dict(grad_e_per_px2=g, annulus_offset_px=2.0,
                            bg_bias_e=float(np.mean(b_hat) - (B0 + DARK) / GAIN) * GAIN,
                            flux_rel_dev=float(np.mean(F) / (F_BRIGHT / GAIN) - 1)))
    g_star_off = float("nan")
    for i in range(1, len(out_off)):
        y0 = abs(out_off[i - 1]["flux_rel_dev"]); y1 = abs(out_off[i]["flux_rel_dev"])
        if y1 >= 0.01:
            x0, x1 = out_off[i - 1]["grad_e_per_px2"], out_off[i]["grad_e_per_px2"]
            g_star_off = x0 + (0.01 - y0) * (x1 - x0) / (y1 - y0) if y1 != y0 else x1
            break
    return dict(sky_base_e_per_px=B0, grad_star_1pct_e_per_px2_centered=g_star,
                grad_star_1pct_e_per_px2_offset2px=g_star_off, rows=out, rows_offset=out_off)


def leak_injection(seed_off):
    P, _, _, _ = C.moffat4_grid(SIGMA_PSF, HALF)
    shape = P.shape
    mask_bg = annulus_mask(shape, R_IN, R_OUT)
    n_pix = float((P > 0).sum())
    Pf = P.ravel()
    res = []
    for beta in LEAK_BETAS:
        snrs = []
        for i, B in enumerate([100.0, 1000.0, 10000.0]):
            d, _ = simulate(F_BRIGHT, B, 300, seed_off + 31 * i)
            bg = d[:, mask_bg]
            b_hat = np.median(bg, axis=1)
            sig = C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_hat[:, None]), axis=1)
            F = np.empty(300)
            for k in range(300):
                Fk = 0.0
                for _ in range(2):
                    var_i = sig[k] ** 2 + np.maximum(Fk * Pf, 0.0) / GAIN
                    w = 1.0 / var_i
                    den = float((Pf * Pf * w).sum())
                    Fk = float((Pf * w * (d[k] - b_hat[k]).ravel()).sum()) / den
                F[k] = Fk + beta * b_hat[k] * n_pix
            snrs.append(float(np.mean(F) / np.std(F, ddof=1)))
        res.append(dict(beta=beta, snr_by_sky=snrs,
                        monotone_decreasing=bool(snrs[0] > snrs[1] > snrs[2])))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b1_sky_scan.json"))
    a = ap.parse_args()
    t0 = time.time()
    rows_bright = scan(F_BRIGHT, 0)
    rows_faint = scan(F_FAINT, 500000)
    gb = gates(rows_bright); gf = gates(rows_faint)
    bb = bias_boundary(50000)
    gr = gradient_boundary(70000)
    lk = leak_injection(90000)
    first_red = next((x["beta"] for x in lk if not x["monotone_decreasing"]), None)
    gb["G7_leak_first_red_beta"] = first_red
    gb["G7_gate_has_teeth"] = bool(first_red is not None)
    obj = dict(
        experiment="SCI-B / B1 physical Monte-Carlo truth + sky scan + negative controls",
        frozen_config=dict(gain_e_per_adu=GAIN, read_noise_e=RN, dark_e_per_px=DARK,
                           sigma_psf_px=SIGMA_PSF, fwhm_moffat4_px=SIGMA_PSF * C.K_MOFFAT4_FWHM,
                           F_bright_e=F_BRIGHT, F_faint_e=F_FAINT, half=HALF, n_mc=N_MC,
                           sky_scan_e_per_px=SKY_SCAN, annulus_px=[R_IN, R_OUT],
                           seed_base=C.SEED_BASE, prod_arms=PROD_ARMS,
                           n_sky_annulus=int(annulus_mask((2 * HALF + 1, 2 * HALF + 1), R_IN, R_OUT).sum()),
                           mc_ci="chi2 95pct CI of std; N_MC=" + str(N_MC)),
        sky_scan_bright=rows_bright, gates_bright=gb,
        sky_scan_faint=rows_faint, gates_faint=gf,
        bias_boundary=bb, gradient_boundary=gr, leak_injection=lk,
        generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)
    print("GATES bright:", gb)
    print("GATES faint:", gf)


if __name__ == "__main__":
    main()
