# -*- coding: utf-8 -*-
"""SCI-A 步骤 1 · 纯解析代数合成数据（最高设计 §12.2 第 2 类）

真值完全已知：已知星等星场 + 已知 inject_scale（⇒ k_photo = 1/inject_scale）+ 已知 m(x,y)，
无噪声与多噪声两组。用途：
  (a) 冻结积分约定的求积收敛性 + 与独立插值/求积 Oracle 的差；
  (b) IRLS/Tukey 零点恢复 Oracle（SCI-PHOT-001 §11）+ 鲁棒性 + S=0 退化门；
  (c) 非退化负例 N0：真值无效应 ⇒ 度量归零并被判红（BELOW_FLOOR）；
  (d) 多噪声组标度律（天光全部进 Poisson，不用算术常数代替）。

产出：results/step1_analytic.json
"""
from __future__ import annotations

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_gaia as sg
import scia_sim as ss
from scia_calib import calibrate

SHAPE = (512, 512)
NSTAR = 600
TARGET_MEDIAN_ADU = 1.0e4


def quadrature_only_check(wl, S, Tv, Tw, Qv, Qw):
    """纯求积收敛：同一被积函数网格上 Simpson(冻结) vs 复合梯形。"""
    from scia_common import akima_interp, simpson_integrate
    tr = akima_interp(*sc._prepare_curve(Tw, Tv), wl, 0.0)
    qr = akima_interp(*sc._prepare_curve(Qw, Qv), wl, 0.0)
    y = S * tr * qr * wl
    simp = simpson_integrate(wl, y)
    trap = float(np.trapezoid(y, wl))
    return dict(simpson=simp, trapezoid=trap, rel_diff=abs(simp - trap) / trap)


def main():
    res = {"step": "1_analytic", "seed": sc.SCIA_SEED}
    inst = ss.Instrument()
    r = sc.rng("analytic:field")
    xs = r.uniform(10, SHAPE[1] - 10, NSTAR)
    ys = r.uniform(10, SHAPE[0] - 10, NSTAR)
    mags = 14.0 + 5.0 * r.random(NSTAR) ** 1.6

    cat = sg.XpCatalog(sg.dump_cone(274.7216, -13.8415, 0.075, 21.5, "m16"))
    ok = np.array([np.ptp(cat.spectrum(i)) > 0 for i in range(cat.n_src)])
    src_idx = r.choice(np.nonzero(ok)[0], NSTAR, replace=True)
    wl = cat.wl_nm
    Tw, Tv = sc.load_filter("Baader R")
    Qw, Qv = sc.load_qe("KAF-16803")
    sed = [cat.spectrum(int(i)) for i in src_idx]
    # XP 谱自带其源的绝对通量刻度（各源 G 不同 ⇒ 绝对刻度差 1e4 量级）。
    # 正确用法：只用**相对星等偏移** mag_assigned - magG_source，把谱当作 SED 形状，
    # 否则"同一 assigned mag 的两颗星"会因源星本身亮度不同而通量差几个量级。
    dmag = mags - cat.magG[src_idx]
    f_syn_v = np.array([sc.f_syn(s, wl, Tv, Tw, Qv, Qw, dm) for s, dm in zip(sed, dmag)])

    inject_scale = TARGET_MEDIAN_ADU / float(np.median(f_syn_v))
    k_photo_true = 1.0 / inject_scale
    res["catalog"] = cat.summary()
    res["n_degenerate_spectra"] = int((~ok).sum())
    res["field"] = dict(n=NSTAR, mag_min=float(mags.min()), mag_max=float(mags.max()),
                        f_syn_median=float(np.median(f_syn_v)),
                        inject_scale=inject_scale, k_photo_true=k_photo_true,
                        median_F_instr_adu=float(np.median(inject_scale * f_syn_v)))

    # ---- (a) 积分约定 ----
    conv = []
    for ov in (4, 16, 64, 256):
        v2 = np.array([sc.f_syn_dense(s, wl, Tv, Tw, Qv, Qw, m, oversample=ov)
                       for s, m in zip(sed[:60], dmag[:60])])
        rel = np.abs(f_syn_v[:60] - v2) / v2
        conv.append(dict(oversample=ov, median_rel=float(np.median(rel)),
                         max_rel=float(rel.max()),
                         median_abs_mag=float(np.median(2.5 / np.log(10) * rel))))
    res["integral_convention_vs_dense_linear"] = dict(
        note="冻结约定(Akima+Simpson) vs 独立高分辨率线性插值+梯形；随 oversample 收敛到常数"
             "⇒ 残差来自 Akima 与线性插值的曲线重建差，不是求积误差", rows=conv)
    qc = [quadrature_only_check(wl, sed[i], Tv, Tw, Qv, Qw) for i in range(20)]
    res["quadrature_only_convergence"] = dict(
        note="同一被积函数网格上 Simpson(冻结) vs 复合梯形", 
        median_rel_diff=float(np.median([q["rel_diff"] for q in qc])),
        max_rel_diff=float(np.max([q["rel_diff"] for q in qc])))

    # ---- (b1) 零点恢复 Oracle（m≡1，无噪声）----
    t0 = ss.Truth(inject_scale=inject_scale, m_coeffs=None, seed_tag="an_m1")
    f0, _ = ss.analytic_frame(SHAPE, xs, ys, f_syn_v, t0, inst, noise=False, tag="g0")
    c0 = calibrate(f0, f_syn_v, mags, xs, ys, m_degree=0)
    res["oracle_zero_point_m1"] = dict(
        note="SCI-PHOT-001 §11 合成注入门：估计 location≈log10(inject_scale)（rtol 1e-4）",
        location=c0["location"], location_true=float(np.log10(inject_scale)),
        rtol=abs(c0["location"] / np.log10(inject_scale) - 1.0),
        k_photo=c0["k_photo"], k_photo_true=k_photo_true,
        k_rel_err=abs(c0["k_photo"] / k_photo_true - 1.0),
        sigma_obs_mag=c0["sigma_obs_mag"], n_inliers=c0["n_inliers"])

    # ---- (c) 负例 N0：真值无效应 ⇒ 归零 + 判红 ----
    b0 = sc.Budget(sigma_fit_white=0.014)
    b0.n = c0["n_inliers"]; b0.rho_lo, b0.rho_hi = sc.sampling_rho(c0["n_inliers"])
    res["negative_N0_no_effect"] = dict(
        note="真值无效应：F_instr=inject_scale·F_syn 精确、m≡1、无噪声 ⇒ r_i 全等 ⇒ "
             "sigma_obs 必须归零，且判据必须判红（BELOW_FLOOR）",
        sigma_obs_mag=c0["sigma_obs_mag"], sigma_floor=b0.sigma_floor,
        verdict=sc.gate_verdict(c0["sigma_obs_mag"], b0), n_inliers=c0["n_inliers"])

    # ---- (b2) 低阶空间增益恢复 ----
    mcoef = np.array([1.0, 0.05, -0.04, 0.02, 0.01, -0.015])
    t1 = ss.Truth(inject_scale=inject_scale, m_coeffs=mcoef, m_degree=2, seed_tag="an_m2")
    f1, _ = ss.analytic_frame(SHAPE, xs, ys, f_syn_v, t1, inst, noise=False, tag="g1")
    c1 = calibrate(f1, f_syn_v, mags, xs, ys, m_degree=2)
    c1n = calibrate(f1, f_syn_v, mags, xs, ys, m_degree=0)
    res["spatial_gain_recovery"] = dict(
        note="已知 m(x,y)（deg2）；m_degree=0 时 sigma_obs 即 m 引起的散度；"
             "m_degree=2 拟合后 delta_after_m 应显著下降",
        sigma_obs_no_m_model=c1n["sigma_obs_mag"],
        sigma_obs_with_m_model=c1["delta_after_m"],
        location_with_m=c1["location"], location_true=float(np.log10(inject_scale)),
        loc_offset_dex=float(c1["location"] - np.log10(inject_scale)),
        m_coeffs_true=mcoef.tolist(), m_coeffs_fit=c1["m_coeffs"],
        m_pred_expected=[None if i == 0 else round(float(-2.5 * np.log10(1 + v)), 6)
                         for i, v in enumerate(mcoef)])

    # ---- (b3) 鲁棒性 20% 离群 ----
    f2 = f1.copy()
    ro = sc.rng("analytic:outlier")
    oi = ro.choice(NSTAR, int(0.2 * NSTAR), replace=False)
    f2[oi] *= 10.0 ** (ro.normal(0, 0.3, oi.size))
    c2 = calibrate(f2, f_syn_v, mags, xs, ys, m_degree=0)
    res["oracle_robustness_20pct"] = dict(
        note="注入 20% 离群 ⇒ |Δlocation| < 0.1 dex（SCI-PHOT-001 §7/§11）",
        location=c2["location"], location_clean=c1n["location"],
        dloc=float(abs(c2["location"] - c1n["location"])), outlier_rate=c2["outlier_rate"])

    # ---- (b4) S=0 退化门 ----
    c3 = calibrate(np.full(NSTAR, 5.0) * f_syn_v, f_syn_v, mags, xs, ys)
    res["oracle_S0_degenerate"] = dict(note="常数 r 场 ⇒ S=0 ⇒ 取 median，不迭代",
                                       S=c3["S"], location=c3["location"],
                                       location_expected=float(np.log10(5.0)))

    # ---- (d) 多噪声组标度律（窄星等窗：匹配样本通量跨度 ~ 10×） ----
    from scia_common import _sum_psf_sq
    from scia_calib import budget_from_frame
    from scia_common import _mean_psf_weighted
    n_eff = 1.0 / _sum_psf_sq(inst.fwhm_px, inst.beta_fit)
    w_psf_avg = _mean_psf_weighted(inst.fwhm_px, inst.beta_fit)
    narrow = (mags >= 15.0) & (mags <= 17.0)
    rows = []
    for sky_mult in (0.25, 1.0, 4.0, 16.0):
        tr = ss.Truth(inject_scale=inject_scale, m_coeffs=None, m_degree=0,
                      sky_adu=199.6 * sky_mult, seed_tag=f"an_n{sky_mult}")
        f_all = tr.inject_scale * f_syn_v          # 真值通量 [ADU]
        fN, okN = ss.stamp_photometry(xs, ys, f_all, tr, inst, tag=f"n{sky_mult}")
        good = okN & narrow
        cN = calibrate(fN[good], f_syn_v[good], mags[good], xs[good], ys[good], m_degree=0)
        # sigma_pix_e 是**逐像素**噪声（电子域）；星点级方差由 noise_sigma_mag 乘 N_eff 得到
        sigma_pix_e = float(np.sqrt(inst.read_noise ** 2
                                    + tr.sky_adu * inst.gain
                                    + inst.dark_rate * tr.exp_time))
        b = budget_from_frame(fN[good], inst, sigma_pix_e, n_eff=n_eff,
                              tag=f"an_n{sky_mult}")
        b.n = cN["n_inliers"]; b.rho_lo, b.rho_hi = sc.sampling_rho(cN["n_inliers"])
        rows.append(dict(sky_mult=sky_mult, sky_adu=tr.sky_adu, n_matched=cN["n_inliers"],
                         sigma_obs_mag=cN["sigma_obs_mag"],
                         sigma_floor=b.sigma_floor, sigma_ceiling=b.sigma_ceiling,
                         verdict=sc.gate_verdict(cN["sigma_obs_mag"], b),
                         ratio_to_floor=float(cN["sigma_obs_mag"] / b.sigma_floor),
                         sigma_fit_white_mc=b.items["sigma_fit_white_mc"],
                         flux_med=b.items["flux_median_adu"],
                         flux_p10=b.items["flux_p10_adu"], flux_p90=b.items["flux_p90_adu"],
                         k_photo=cN["k_photo"],
                         k_rel_err=float(abs(cN["k_photo"] / k_photo_true - 1.0))))
    # ---- (d2) sigma_fit 合成形态对比：一阶展开 vs 精确 Fisher vs 实测 ----
    from scia_common import _mean_psf_weighted, psf_fit_variance_exact
    cmp_rows = []
    for sky_mult in (0.25, 1.0, 4.0, 16.0):
        tr = ss.Truth(inject_scale=inject_scale, sky_adu=199.6 * sky_mult,
                      seed_tag=f"an_cmp{sky_mult}")
        sp = float(np.sqrt(inst.read_noise ** 2 + tr.sky_adu * inst.gain
                           + inst.dark_rate * tr.exp_time))
        Fm = float(np.median(inject_scale * f_syn_v[narrow]))
        v_exact = float(psf_fit_variance_exact(np.array([Fm]), inst, sp)[0])
        v_first = sp ** 2 * n_eff / inst.gain ** 2 + Fm * w_psf_avg / inst.gain
        cmp_rows.append(dict(sky_mult=sky_mult, F_median_adu=Fm,
                             sigma_mag_first_order=float(sc.PHOTON_MAG * np.sqrt(v_first) / Fm),
                             sigma_mag_exact_fisher=float(sc.PHOTON_MAG * np.sqrt(v_exact) / Fm),
                             underestimate_factor=float(np.sqrt(v_exact / v_first))))
    res["sigma_fit_composition_form"] = dict(
        note="**自由背景简并的代价**（不是文档错误，见 DOC_CORRECTIONS C8）：权威文档 "
             "run/RELEASE-02/parallel/06.md §2.2 给的是 Horne 1986 最优提取结构 "
             "sigma_F/F = sqrt(1/(gF) + N_eff·sigma_pix²/F²)，它假定**背景已知**、无自由背景参数；"
             "本实验的精确式是 (幅度, 常数背景) 线性拟合的 Fisher 矩阵解 Var=H11/(H00·H11−H01²)，"
             "多一个自由背景参数 ⇒ 多出 H01 交叉项，方差更大。两者之差即下表的比值。"
             "（06.md 的 sigma_fit=0.0140 是生产拟合器 200 次重复拟合的实测值，不是该解析式的取值。）",
        rows=cmp_rows)

    res["noise_scaling"] = dict(
        note="天光抬升 64×（全部经 Poisson 散粒）⇒ sigma_obs 必须单调上升且落在双边界内；"
             "窄星等窗 15–17 mag 保证匹配样本通量跨度与真实生产样本同量级",
        rows=rows)

    # ---- (e) 宽通量跨度样本：检验 06.md "中位通量代表整帧" 近似的偏差 ----
    wide = np.ones(NSTAR, bool)
    tr = ss.Truth(inject_scale=inject_scale, m_coeffs=None, m_degree=0,
                  sky_adu=199.6, seed_tag="an_wide")
    fWall = tr.inject_scale * f_syn_v
    fW, okW = ss.stamp_photometry(xs, ys, fWall, tr, inst, tag="wide")
    cW = calibrate(fW[okW], f_syn_v[okW], mags[okW], xs[okW], ys[okW], m_degree=0)
    sigma_pix_e = float(np.sqrt(inst.read_noise ** 2 + 199.6 * inst.gain
                                + inst.dark_rate * tr.exp_time))
    bW = budget_from_frame(fW[okW], inst, sigma_pix_e, n_eff=n_eff, tag="an_wide")
    bW.n = cW["n_inliers"]; bW.rho_lo, bW.rho_hi = sc.sampling_rho(cW["n_inliers"])
    res["flux_distribution_composition"] = dict(
        note="同一帧、同一噪声：星等窗 14–19（跨度 100×）时，06.md 的『中位通量代表整帧』"
             "合成口径与逐星 MC 合成口径的差；用于判断该近似是否需要订正（→ DOC_CORRECTIONS）",
        sigma_obs_mag=cW["sigma_obs_mag"], n_matched=cW["n_inliers"],
        sigma_fit_white_median_flux_form=bW.items["sigma_fit_white_median_flux_form"],
        sigma_fit_white_mc=bW.items["sigma_fit_white_mc"],
        ratio_mc_over_median_form=float(bW.items["sigma_fit_white_mc"]
                                        / bW.items["sigma_fit_white_median_flux_form"]),
        verdict_with_median_form=sc.gate_verdict(cW["sigma_obs_mag"], sc.Budget(
            sigma_fit_white=bW.items["sigma_fit_white_median_flux_form"],
            sigma_fit_robust=bW.items["sigma_fit_robust_median_flux_form"],
            n=cW["n_inliers"], rho_lo=bW.rho_lo, rho_hi=bW.rho_hi)),
        verdict_with_mc_form=sc.gate_verdict(cW["sigma_obs_mag"], bW))
    sc.jdump(res, os.path.join(sc.RESULTS, "step1_analytic.json"))


if __name__ == "__main__":
    main()
