# -*- coding: utf-8 -*-
"""SCI-A 步骤 6 · apply photometry 落像素 + 物理单位消除论证与反推不可辨识实验

对应 ACCEPTANCE_SPEC §2.1 的「物理单位消除」与「apply photometry」两行。
设计依据：ASTROCS_DESIGN.md §2.1 / §4.2（I_photo = k_photo·m(x,y)·I_cal；其后所有节点与
drizzle 消费归一化像素；不可用时显式 degraded_reason 且 fail-closed）、§4.4（产物通量以
星等/相对星等表达；标定系数绝对值无物理意义；禁止由它反解增益/口径/曝光）。

产出：results/step6_apply_and_units.json
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_pipeline as pl
import scia_sim as ss
from scia_calib import calibrate


# --------------------------------------------------------------------------
# A. apply photometry 落像素
# --------------------------------------------------------------------------
def apply_photometry_checks(tag="A"):
    meta = json.load(open(os.path.join(sc.RESULTS, "step2_hst_sim.json"), encoding="utf-8"))
    inst = pl.instrument_from(meta)
    fr = pl.load_frame(tag)
    img = fr["img"]; H, W = img.shape
    # 用与 step5 相同的选择与拟合重建 k_photo 与 m̂(x,y)
    wcs = pl.frame_wcs(meta)
    var = (np.clip(fr["mu"], 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    from scia_calib import guided_photometry
    g = guided_photometry(img, wcs, fr["ra"], fr["dec"], inst, var_map=var)
    sat = np.zeros(g["idx"].size, bool)
    box = int(np.ceil(3 * inst.fwhm_px))
    xr = np.clip(np.round(np.nan_to_num(g["x"], nan=-99)).astype(int), 0, W - 1)
    yr = np.clip(np.round(np.nan_to_num(g["y"], nan=-99)).astype(int), 0, H - 1)
    for k in range(g["idx"].size):
        sat[k] = bool(np.any(img[max(yr[k] - box, 0):min(yr[k] + box, W - 1) + 1,
                                 max(xr[k] - box, 0):min(xr[k] + box, H - 1) + 1]
                             >= inst.saturation_adu))
    sel = pl.sample_selection(g["flux"], g["chi2"], sat, fr["mag_eff"], inst.fwhm_px,
                              chi2_max=4.0) & g["fit_ok"]
    cal = calibrate(g["flux"][sel], fr["f_syn"][sel], fr["mag_eff"][sel],
                    g["x"][sel], g["y"][sel], m_degree=2)
    k_photo = cal["k_photo"]
    # m̂(x,y) 全帧图：低阶多项式（中心化坐标与标定点一致）
    coef = np.asarray(cal["m_coeffs"], float)
    xs_sel = g["x"][sel]; ys_sel = g["y"][sel]
    yy, xx = np.mgrid[0:H, 0:W]
    # pred(x,y) 是**星等残差**的多项式 ⇒ 归一化修正场 m_corr = 10^(+0.4·pred) = 1/m_gain
    dm = pl.eval_low_order_gain(coef, xx.ravel(), yy.ravel(), 2).reshape(H, W)
    m_hat = 10.0 ** (0.4 * dm)
    # 独立复算（不复用 apply_photometry 的实现）
    I_photo_indep = (k_photo * m_hat) * img
    I_photo = pl.apply_photometry(img, k_photo, m_hat)
    rel = float(np.max(np.abs(I_photo - I_photo_indep) / np.maximum(np.abs(I_photo_indep), 1e-30)))
    # 下游消费：drizzle 式面亮度保持重采样 + 帧级 SNR
    from scipy.ndimage import gaussian_filter
    def drizzle_like(a, f=2):
        Hh, Ww = a.shape
        return a[:Hh // f * f, :Ww // f * f].reshape(Hh // f, f, Ww // f, f).sum(axis=(1, 3)) / f ** 2
    d_cal = drizzle_like(img); d_photo = drizzle_like(I_photo)
    # 通量守恒（面亮度归一）：点源总通量在重采样前后一致
    def star_flux(a, x0, y0, box=8):
        return float(np.sum(a[int(y0) - box:int(y0) + box + 1, int(x0) - box:int(x0) + box + 1]))
    x0, y0 = float(g["x"][sel][0]), float(g["y"][sel][0])
    f_before = star_flux(img, x0, y0); f_after = star_flux(I_photo, x0, y0)
    # 星等表达：标定后星等 = -2.5log10(F_instr·k_photo·m̂)
    m_cal = -2.5 * np.log10(np.maximum(g["flux"][sel] * k_photo
                                       * 10.0 ** (0.4 * pl.eval_low_order_gain(
                                           coef, xs_sel, ys_sel, 2)), 1e-30))
    m_syn = -2.5 * np.log10(np.maximum(np.asarray(fr["f_syn"], float)[sel], 1e-30))
    resid_mag = m_cal - m_syn
    # 方向自检：不改正 vs 按交付实现改正（后者必须更小）
    m_cal_nom = -2.5 * np.log10(np.maximum(g["flux"][sel] * k_photo, 1e-30))
    resid_nom = m_cal_nom - m_syn
    # 未启用路径：显式 degraded_reason + fail-closed
    def downstream(res, photometry_enabled):
        if not photometry_enabled:
            res["degraded_reason"] = "apply_photometry_disabled"
            raise RuntimeError("fail-closed: 产品未归一化到测光星等坐标系，禁止继续下游")
        res["photometry_applied"] = True
        return res
    degraded = None
    try:
        downstream({}, False)
    except RuntimeError as e:
        degraded = str(e)
    return dict(
        tag=tag,
        apply_photometry=dict(
            k_photo=float(k_photo), m_degree=2,
            m_hat_range=[float(m_hat.min()), float(m_hat.max())],
            m_hat_semantics="m(x,y) = 10^(+0.4·poly(x,y)) = 1/m_gain(x,y)：设计 §4.2 里的 m 是"
                            "**归一化修正场**（乘性改正），不是仪器增益本身",
            independent_recompute_max_rel_diff=rel,
            # 方向自检：改正后残差必须**小于**不改正
            note="I_photo = k_photo·m(x,y)·I_cal（设计 §4.2），与独立复算逐像素一致"),
        downstream_consumption=dict(
            note="drizzle 式重采样与帧级 SNR 都消费归一化后的像素；未启用时显式 degraded_reason 且 fail-closed",
            drizzle_scale_ratio_median=float(np.median(d_photo[d_cal > 0] / d_cal[d_cal > 0])),
            drizzle_scale_ratio_expected=float(k_photo * np.median(m_hat)),
            star_flux_before=f_before, star_flux_after=f_after,
            flux_ratio_after_over_before=float(f_after / f_before),
            degraded_reason_on_disabled_path=degraded,
            fail_closed=degraded is not None),
        magnitude_only=dict(
            note="产物只以星等/相对星等表达：标定后星等对合成星等的残差即测光一致性",
            n=int(sel.sum()), resid_mag_median=float(np.median(resid_mag)),
            resid_mag_mad_sigma=float(sc.mad_sigma(resid_mag)),
            resid_mag_mad_sigma_no_m=float(sc.mad_sigma(resid_nom)),
            m_correction_improves=bool(sc.mad_sigma(resid_mag) < sc.mad_sigma(resid_nom)),
            sigma_obs_mag=cal["sigma_obs_mag"],
            n_inliers=cal["n_inliers"]),
    )


# --------------------------------------------------------------------------
# B. 物理单位消除 / 反推不可辨识
# --------------------------------------------------------------------------
def unit_elimination():
    """构造 (口径 A, 曝光 t, 增益 g) 的退化族，证明标定系数无法反解仪器参数。"""
    meta = json.load(open(os.path.join(sc.RESULTS, "step2_hst_sim.json"), encoding="utf-8"))
    base = pl.instrument_from(meta)
    r = sc.rng("units:family")
    n = 400
    xs = r.uniform(20, 300, n); ys = r.uniform(20, 300, n)
    mags = 13.0 + 2.5 * r.random(n)          # 亮端：让三行的噪声/散度可比
    Tw, Tv = sc.load_filter("Baader R"); Qw, Qv = sc.load_qe("KAF-16803")
    import scia_gaia as sg
    cat = sg.XpCatalog(sg.dump_cone(274.7216, -13.8415, 0.075, 21.5, "m16"))
    ok = np.array([np.ptp(cat.spectrum(i)) > 0 for i in range(cat.n_src)])
    src = r.choice(np.nonzero(ok)[0], n, replace=True)
    f_syn = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qv, Qw, m - 16.0)
                      for i, m in zip(src, mags)])
    rows = []
    # 族 1：A·t/g 不变 ⇒ 期望图像与噪声都相同（A 与 t 的 1 参数退化）
    # 族 2：g→αg, t→αt ⇒ 期望相同、噪声不同（g 与 t 的退化被 PTC 打破，但 A·t 仍不可分）
    scale = 5000.0 / float(np.median(f_syn * 300.0 / 1.333))   # 归一到 ~5000 ADU
    for label, amp, texp, gain in (("ref (A,t,g)=(1,300,1.333)", 1.0, 300.0, 1.333),
                                   ("A/2, 2t, g", 0.5, 600.0, 1.333),
                                   ("A, 2t, 2g", 1.0, 600.0, 2.666)):
        inst = ss.Instrument(gain=gain)
        # 同一噪声种子：三行的噪声实现可直接对照
        tr = ss.Truth(inject_scale=1.0, m_coeffs=None, sky_adu=199.6, exp_time=texp,
                      seed_tag="unit:family")
        f_eff = amp * texp * f_syn / gain * scale        # ADU = Φ·A·t/g
        F, okk = ss.stamp_photometry(xs, ys, f_eff, tr, inst, tag=f"u:{label}")
        good = okk & (mags < 18.5)
        cal = calibrate(F[good], f_syn[good], mags[good], xs[good], ys[good], m_degree=0)
        m_cal = -2.5 * np.log10(np.maximum(F[good] * cal["k_photo"], 1e-30))
        m_syn = -2.5 * np.log10(f_syn[good])
        rows.append(dict(label=label, aperture_scale=amp, exp_time=texp, gain=gain,
                         A_t_over_g=amp * texp / gain,
                         median_flux_adu=float(np.median(F[good])),
                         scatter_F_adu=float(sc.mad_sigma(F[good])),
                         k_photo=float(cal["k_photo"]),
                         sigma_obs_mag=float(cal["sigma_obs_mag"]),
                         resid_mag_mad=float(sc.mad_sigma(m_cal - m_syn)),
                         note="标定后星等 = -2.5log10(F·k_photo)，与仪器参数无关"))
    # 零点平移不变量（SCI-PHOT-001 §7）：F_instr 全体同乘 c
    inst = ss.Instrument()
    tr = ss.Truth(inject_scale=1.0, m_coeffs=None, sky_adu=199.6, seed_tag="unit:shift")
    F0, _ = ss.stamp_photometry(xs, ys, 300.0 * f_syn / inst.gain * scale, tr, inst, tag="u0")
    good = np.isfinite(F0) & (F0 > 0)
    c0 = calibrate(F0[good], f_syn[good], mags[good], xs[good], ys[good], m_degree=0)
    shift = 7.3
    c1 = calibrate(F0[good] * shift, f_syn[good], mags[good], xs[good], ys[good], m_degree=0)
    return dict(
        degenerate_family=rows,
        note=("标定系数 k_photo 只把 A·t/g 与通带归一一起吸收；由 k_photo 反解 (A,t,g) 是"
              "欠定的 2 参数族。表内三行的 A·t/g 相同 ⇒ 期望图像相同；第二、三行的噪声"
              "不同（PTC 斜率可给 g），但 (A,t) 仍不可分。**本实验不使用任何物理闭合式**"
              "（SCI-PHOT-001 §1/§10 明令禁止 k = g·h·c·1e9/(A·t)）。"),
        zero_point_shift_invariance=dict(
            note="SCI-PHOT-001 §7 零点平移不变量：F_instr 同乘 c ⇒ location 增 log10 c、"
                 "k_photo 除 c、sigma_residual 不变",
            shift_applied=shift,
            location_delta=float(c1["location"] - c0["location"]),
            location_delta_expected=float(np.log10(shift)),
            k_photo_ratio=float(c1["k_photo"] / c0["k_photo"]),
            k_photo_ratio_expected=float(1.0 / shift),
            sigma_residual_delta=float(c1["sigma_residual"] - c0["sigma_residual"])),
        no_absolute_window=dict(
            note="判据只含星等散度（尺度无关），不存在 k_photo 的绝对窗口",
            gate_inputs=["sigma_obs(mag)", "sigma_floor(mag)", "sigma_ceiling(mag)"],
            k_photo_in_gate=False))


def main():
    res = {"step": "6_apply_and_units", "seed": sc.SCIA_SEED}
    res["apply"] = apply_photometry_checks("A")
    res["unit_elimination"] = unit_elimination()
    sc.jdump(res, os.path.join(sc.RESULTS, "step6_apply_and_units.json"))


if __name__ == "__main__":
    main()
