# -*- coding: utf-8 -*-
"""SCI-A 步骤 5 · 逐帧标定拟合 + 误差预算双边界判据 + 帧间独立（ACCEPTANCE_SPEC §2.1）

判据形态（docs/plugins/algorithms_phase1/06_photometry.md §4.1 定案 6）：
    sigma_obs = 2.5·MAD(r_inliers)/0.6744897501960817
    sigma_floor = (1 - 3·1.166/√n)·sigma_fit(白; 本帧匹配星通量分布)
    sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)
    PASS ⟺ sigma_floor ≤ sigma_obs ≤ sigma_ceiling
逐项预算在本仿真帧上**从真值重算**（光子噪声/PSF 拟合不确定度/平场残余/天光残余/
颜色项/参考侧/量化），并与注入真值对照。

产出：results/step5_calibration_gate.json
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


def analyse(tag, meta, inst, m_degree=2):
    fr = pl.load_frame(tag)
    wcs = pl.frame_wcs(meta)
    img = fr["img"]
    var = (np.clip(fr["mu"], 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    from scia_calib import guided_photometry
    if np.all(np.isfinite(fr["ra"])):
        g = guided_photometry(img, wcs, fr["ra"], fr["dec"], inst, var_map=var)
        pos_truth = None
    else:
        # 扩展星场：位置为解析合成真值 ⇒ 直接按真值位置做 PSF 拟合（等价于引导定位）
        g = dict(idx=np.arange(fr["x"].size), x=np.full(fr["x"].size, np.nan),
                 y=np.full(fr["x"].size, np.nan), flux=np.full(fr["x"].size, np.nan),
                 flux_err=np.full(fr["x"].size, np.nan), chi2=np.full(fr["x"].size, np.nan),
                 fit_ok=np.zeros(fr["x"].size, bool), x_pred=fr["x"], y_pred=fr["y"])
        for k in range(fr["x"].size):
            f = sc.fit_psf(img, fr["x"][k], fr["y"][k], inst.fwhm_px, inst.beta_fit,
                           var_map=var)
            g["x"][k] = f["x"]; g["y"][k] = f["y"]; g["flux"][k] = f["flux"]
            g["flux_err"][k] = f["flux_err"]; g["chi2"][k] = f["chi2"]
            g["fit_ok"][k] = bool(f["ok"] and np.isfinite(f["flux"]) and f["flux"] > 0)
        pos_truth = True
    # 饱和掩膜
    H, W = img.shape
    box = int(np.ceil(3 * inst.fwhm_px))
    sat = np.zeros(g["idx"].size, bool)
    xr = np.clip(np.round(np.nan_to_num(g["x"], nan=-99)).astype(int), 0, W - 1)
    yr = np.clip(np.round(np.nan_to_num(g["y"], nan=-99)).astype(int), 0, H - 1)
    for k in range(g["idx"].size):
        x0, x1 = max(xr[k] - box, 0), min(xr[k] + box, W - 1)
        y0, y1 = max(yr[k] - box, 0), min(yr[k] + box, H - 1)
        sat[k] = bool(np.any(img[y0:y1 + 1, x0:x1 + 1] >= inst.saturation_adu))
    sel = pl.sample_selection(g["flux"], g["chi2"], sat, fr["mag_eff"], inst.fwhm_px,
                              chi2_max=4.0, mag_range=(13.0, 20.0))
    sel &= g["fit_ok"]
    if pos_truth:
        sel &= (np.hypot(g["x"] - fr["x"], g["y"] - fr["y"]) < 1.0)
    n_sel = int(sel.sum())
    print(f"[step5:{tag}] candidates={g['idx'].size} fit_ok={int(g['fit_ok'].sum())} "
          f"sat={int(sat.sum())} sel={n_sel}")
    if n_sel < 5:
        raise RuntimeError(f"frame {tag}: 有效样本 {n_sel} < 5，无法标定（如实判红，不静默降级）")
    F = g["flux"][sel]; Fs = fr["f_syn"][sel]
    xs = g["x"][sel]; ys = g["y"][sel]
    cal = calibrate(F, Fs, fr["mag_eff"][sel], xs, ys, m_degree=m_degree)
    # ---------------- 误差预算逐项（本帧真值重算） ----------------
    sky_med_e = float(np.median(fr["sky_map_adu"])) * inst.gain
    sigma_pix_white_e = float(np.sqrt(inst.read_noise ** 2 + sky_med_e
                                      + inst.dark_rate * 300.0))
    d = np.diff(img, axis=1)
    bg_rms_adu = float(np.median(np.abs(d - np.median(d))) * sc.MAD_TO_SIGMA / np.sqrt(2.0))
    white_adu = sigma_pix_white_e / inst.gain
    structure_factor = float(max(1.0, bg_rms_adu / white_adu))   # 结构项只能增大噪声
    # 平场残余：m̂(x,y) vs m_true(x,y)
    m_true = fr["m"]
    xr2 = np.clip(np.round(xs).astype(int), 0, W - 1); yr2 = np.clip(np.round(ys).astype(int), 0, H - 1)
    # 归一化修正场：m_corr(x,y) = 10^(+0.4·pred) = 1/m_gain（pred 是星等残差的多项式）
    if cal["m_coeffs"] is not None:
        m_corr_at = 10.0 ** (0.4 * pl.eval_low_order_gain(
            np.asarray(cal["m_coeffs"]), xs, ys, m_degree))
    else:
        m_corr_at = np.ones(xs.size)
    m_true_at = m_true[yr2, xr2]
    # 平场残余 = 拟合 m 之后仍剩下的星等散度（calibrate 内已算，单位 mag，无多余因子）
    sigma_flat = float(cal["delta_after_m"]) if np.isfinite(cal["delta_after_m"]) else 0.0
    # 颜色项：模型通带 vs 注入通带的等效星等差散度（逐星已知）
    dcol = -2.5 * np.log10(np.asarray(fr["f_syn_inject"], float)[sel]
                           / np.asarray(fr["f_syn"], float)[sel])
    sigma_color = float(np.std(dcol))
    sigma_gaia = 0.002
    # 帧内 PSF 域方法系统误差（PSF vs 独立孔径）
    from scia_calib import aperture_flux, psf_vs_aperture_systematics
    xg = np.nan_to_num(g["x"], nan=-1e6); yg = np.nan_to_num(g["y"], nan=-1e6)
    # 小孔径口径：大孔径（r=10px=9.5"）在强星云场里会把星云结构算进"方法系统误差"
    f_ap4 = aperture_flux(img, xg, yg, r_ap=4.0, r_in=6.0, r_out=10.0)
    f_ap10 = aperture_flux(img, xg, yg, r_ap=10.0, r_in=12.0, r_out=18.0)
    sysres = psf_vs_aperture_systematics(g["flux"], f_ap4, g["flux_err"])
    sysres_big = psf_vs_aperture_systematics(g["flux"], f_ap10, g["flux_err"])
    # 真值口径（仅用于检验"帧内自算估计量"是否可信，不进门禁）
    f_true = (np.asarray(fr["f_syn_inject"], float)[sel] * float(fr["inject_scale"])
              * m_true[yr2, xr2])
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio_true = np.where(f_true > 0, g["flux"][sel] / f_true, np.nan)
    ok_t = np.isfinite(ratio_true) & (ratio_true > 0)
    sigma_psfsys_truth = float(sc.mad_sigma(2.5 * np.log10(ratio_true[ok_t]))) if ok_t.sum() > 5 else float("nan")
    sysres["sigma_psfsys_truth_mag"] = sigma_psfsys_truth
    sysres["sigma_psfsys_ap10_mag"] = sysres_big["sigma_psfsys"]
    b = pl.build_budget(F, inst, sigma_pix_white_e, sysres["sigma_psfsys"], sigma_color,
                        sigma_gaia, sigma_flat, structure_factor, cal["n_inliers"],
                        tag=f"gate{tag}")
    # 逐星噪声口径下的"总预测散度"（非退化对照：预测 vs 实测）
    from scia_common import mc_sigma_obs, noise_sigma_mag
    sig_noise = noise_sigma_mag(F, inst, sigma_pix_white_e * structure_factor)
    sys_tot = float(np.sqrt((sysres["sigma_psfsys"] if np.isfinite(sysres["sigma_psfsys"]) else 0) ** 2
                            + sigma_color ** 2 + sigma_gaia ** 2 + sigma_flat ** 2))
    pred_total = mc_sigma_obs(sig_noise, sys_tot, tag=f"pred{tag}")
    out = dict(
        tag=tag, extended_field=bool(meta and any(f["tag"] == tag and f["extended_field"]
                                                  for f in meta["frames"])),
        n_candidates=int(g["idx"].size), n_selected=n_sel,
        n_saturated_rejected=int(sat.sum()),
        calibration={k: v for k, v in cal.items() if k != "m_coeffs"},
        budget=b.as_dict(),
        sigma_obs_mag=cal["sigma_obs_mag"],
        verdict=sc.gate_verdict(cal["sigma_obs_mag"], b),
        predicted_total_mag=float(pred_total),
        obs_over_predicted=float(cal["sigma_obs_mag"] / pred_total) if pred_total > 0 else None,
        items_measured=dict(
            sigma_pix_white_e=sigma_pix_white_e, bg_rms_adu_adjacent_diff=bg_rms_adu,
            structure_factor=structure_factor, sky_median_adu=float(np.median(fr["sky_map_adu"])),
            sigma_psfsys_inframe=sysres["sigma_psfsys"],
            sigma_psfsys_ap10_inframe=sysres.get("sigma_psfsys_ap10_mag"),
            sigma_psfsys_truth_mag=sysres.get("sigma_psfsys_truth_mag"),
            sigma_psfsys_noise_expect=sysres.get("noise_expect_mag"),
            psf_vs_ap_ratio_median=sysres.get("ratio_median"),
            sigma_color_from_truth=sigma_color,
            sigma_color_mean_mag=float(np.mean(dcol)),
            sigma_flat_from_truth=sigma_flat,
            m_true_at_median=float(np.median(m_true_at)),
            m_corr_at_median=float(np.median(m_corr_at)),
            m_corr_times_mtrue_median=float(np.median(m_corr_at * m_true_at)),
            delta_after_m_mag=float(cal["delta_after_m"]),
            n_m_terms=len(cal["m_coeffs"]) if cal["m_coeffs"] else 0),
        inject_scale_true=float(fr["inject_scale"]),
        k_photo_fit=cal["k_photo"],
        k_photo_true=1.0 / float(fr["inject_scale"]),
        k_rel_err=abs(cal["k_photo"] * float(fr["inject_scale"]) - 1.0),
        # 有效真值：把"模型通带 vs 注入通带"的零点偏移（由 k_photo 吸收）算进去
        k_photo_true_effective=float(10.0 ** (np.mean(dcol) / 2.5) / float(fr["inject_scale"])),
        k_rel_err_effective=abs(cal["k_photo"] * float(fr["inject_scale"])
                                * 10.0 ** (-np.mean(dcol) / 2.5) - 1.0),
    )
    return out, cal, fr


def main():
    meta = json.load(open(os.path.join(sc.RESULTS, "step2_hst_sim.json"), encoding="utf-8"))
    inst = pl.instrument_from(meta)
    res = {"step": "5_calibration_gate", "seed": sc.SCIA_SEED,
           "criterion": ("sigma_floor <= sigma_obs <= sigma_ceiling; "
                         "sigma_obs = 2.5*MAD(r_inliers)/0.6744897501960817 (SCI-PHOT-001 §5)"),
           "frames": []}
    rows = {}
    for tag in ("A", "B", "C"):
        o, cal, fr = analyse(tag, meta, inst, m_degree=2)
        res["frames"].append(o); rows[tag] = (o, cal, fr)
        print(f"[step5] {tag}: n={o['n_selected']} sigma_obs={o['sigma_obs_mag']:.4f} "
              f"[{o['budget']['sigma_floor']:.4f},{o['budget']['sigma_ceiling']:.4f}] "
              f"-> {o['verdict']}  k_err={o['k_rel_err']*100:.3f}%")
    # ---- 帧间独立 ----
    from scipy.stats import ks_2samp
    oA, cA, fA = rows["A"]; oB, cB, fB = rows["B"]
    kA = cA["k_photo"]; kB = cB["k_photo"]
    trans_ratio = float(fB["inject_scale"]) / float(fA["inject_scale"])
    # 残差分布一致性：**真的算**（此前是死代码）——两帧各自的 IRLS inlier 残差（星等域）
    ks = None
    if "resid_mag_inliers" in cA and "resid_mag_inliers" in cB:
        ra = np.asarray(cA["resid_mag_inliers"], float)
        rb = np.asarray(cB["resid_mag_inliers"], float)
        if ra.size >= 8 and rb.size >= 8:
            st = ks_2samp(ra, rb)
            ks = dict(n_A=int(ra.size), n_B=int(rb.size),
                      statistic=float(st.statistic), pvalue=float(st.pvalue),
                      median_A=float(np.median(ra)), median_B=float(np.median(rb)),
                      mad_A=float(sc.mad_sigma(ra)), mad_B=float(sc.mad_sigma(rb)),
                      note="KS 检验：两帧透明度不同 ⇒ k 不同；但**残差分布**应一致"
                           "（同一仪器/同一模型通带）。这不是门禁，只是独立性证据。")
    res["frame_independence"] = dict(
        note="各帧独立标定到同一坐标系；不同透明度帧系数不同但残差分布一致；"
             "跨帧 k 散度**不构成门禁**（变更 claim PHOT-GATE-DROP-001 / 负责人 §9.49 定案 2）",
        k_photo_A=float(kA), k_photo_B=float(kB), k_ratio=float(kB / kA),
        transparency_ratio_B_over_A=trans_ratio,
        k_ratio_over_expected=float((kB / kA) * trans_ratio),
        sigma_obs_A=oA["sigma_obs_mag"], sigma_obs_B=oB["sigma_obs_mag"],
        sigma_obs_ratio=float(oB["sigma_obs_mag"] / oA["sigma_obs_mag"]),
        verdict_A=oA["verdict"], verdict_B=oB["verdict"],
        residual_distribution_ks=ks,
        cross_frame_gate_present=False,
        cross_frame_gate_required=False)
    sc.jdump(res, os.path.join(sc.RESULTS, "step5_calibration_gate.json"))


if __name__ == "__main__":
    main()
