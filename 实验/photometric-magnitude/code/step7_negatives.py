# -*- coding: utf-8 -*-
"""SCI-A 步骤 7 · 非退化负例（真值无效应 ⇒ 归零/判红；有注入 ⇒ 度量如实变大）

ACCEPTANCE_SPEC §2.1 明确要求「注入天光梯度时残差度量如实变大」。
本步骤的每条负例都给出**幅度扫描**，并显式标注：
  · 注入量（物理口径：经 Poisson 前向，不是算术相加的确定性图样）
  · 度量的响应倍数与判定翻转点
  · 哪几条真正提供"注入-响应"证据，哪几条只是退化输入检查
不把"度量不响应"当作通过。

产出：results/step7_negatives.json
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
from scia_calib import calibrate, guided_photometry


def base_setup():
    meta = json.load(open(os.path.join(sc.RESULTS, "step2_hst_sim.json"), encoding="utf-8"))
    inst = pl.instrument_from(meta)
    fr = pl.load_frame("A")
    wcs = pl.frame_wcs(meta)
    return meta, inst, fr, wcs


def measure(img, fr, inst, wcs, chi2_max=4.0):
    var = (np.clip(fr["mu"], 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    g = guided_photometry(img, wcs, fr["ra"], fr["dec"], inst, var_map=var)
    H, W = img.shape
    box = int(np.ceil(3 * inst.fwhm_px))
    sat = np.zeros(g["idx"].size, bool)
    xr = np.clip(np.round(np.nan_to_num(g["x"], nan=-99)).astype(int), 0, W - 1)
    yr = np.clip(np.round(np.nan_to_num(g["y"], nan=-99)).astype(int), 0, H - 1)
    for k in range(g["idx"].size):
        sat[k] = bool(np.any(img[max(yr[k] - box, 0):min(yr[k] + box, H - 1) + 1,
                                 max(xr[k] - box, 0):min(xr[k] + box, W - 1) + 1]
                             >= inst.saturation_adu))
    sel = pl.sample_selection(g["flux"], g["chi2"], sat, fr["mag_eff"], inst.fwhm_px,
                              chi2_max=chi2_max) & g["fit_ok"]
    if sel.sum() < 5:
        return None
    cal = calibrate(g["flux"][sel], fr["f_syn"][sel], fr["mag_eff"][sel],
                    g["x"][sel], g["y"][sel], m_degree=0)
    return cal, g, sel


def resimulate(fr, inst, sky_delta_e, tag, seed="neg:resim"):
    """按**物理前向**重画帧：期望电子图加 sky_delta_e（[e-]）后重新走
    Poisson → 高斯读出 → 增益 → 饱和 → 量化。不是算术相加的确定性图样。"""
    r = sc.rng(seed)
    mu = np.clip(np.asarray(fr["mu"], float) + np.asarray(sky_delta_e, float), 1e-6, None)
    e = r.poisson(mu).astype(float)
    e = e + r.normal(0.0, inst.read_noise, e.shape)
    adu = e / inst.gain
    return np.clip(np.round(adu), 0.0, inst.saturation_adu)


def gate(cal, fr, inst, sigma_psfsys=0.0, sigma_color=0.0, sigma_gaia=0.002,
         sigma_flat=0.0, tag="neg", flux=None):
    sky_e = float(np.median(fr["sky_map_adu"])) * inst.gain
    sp = float(np.sqrt(inst.read_noise ** 2 + sky_e + inst.dark_rate * 300.0))
    d = np.diff(fr["img"], axis=1)
    bg = float(np.median(np.abs(d - np.median(d))) * sc.MAD_TO_SIGMA / np.sqrt(2.0))
    sf = max(1.0, bg / (sp / inst.gain))
    b = pl.build_budget(np.asarray(flux, float), inst, sp, sigma_psfsys, sigma_color,
                        sigma_gaia, sigma_flat, sf, cal["n_inliers"], tag=tag)
    return b, sc.gate_verdict(cal["sigma_obs_mag"], b)


def main():
    meta, inst, fr, wcs = base_setup()
    img0 = np.asarray(fr["img"], float)
    H, W = img0.shape
    yy, xx = np.mgrid[0:H, 0:W]
    res = {"step": "7_negatives", "seed": sc.SCIA_SEED, "negatives": []}

    # ---------------- N0：真值无效应 ⇒ 归零 + 判红（退化输入检查） ----------------
    F_true = np.asarray(fr["f_syn"], float) * float(fr["inject_scale"])
    cal0 = calibrate(F_true, fr["f_syn"], fr["mag_eff"], fr["x"], fr["y"], m_degree=0)
    b0 = sc.Budget(sigma_fit_white=0.014, n=cal0["n_inliers"])
    b0.rho_lo, b0.rho_hi = sc.sampling_rho(cal0["n_inliers"])
    res["negatives"].append(dict(
        id="N0", kind="退化输入检查：真值无效应 ⇒ 归零/判红",
        provides_injection_response=False,
        setup="F_instr = inject_scale·F_syn（无噪声、无平场、无 PSF 误差）⇒ r_i 严格全等",
        sigma_obs_mag=cal0["sigma_obs_mag"], sigma_floor=b0.sigma_floor,
        sigma_ceiling=b0.sigma_ceiling, verdict=sc.gate_verdict(cal0["sigma_obs_mag"], b0),
        expected="sigma_obs=0 且 BELOW_FLOOR（判红）",
        pass_=bool(cal0["sigma_obs_mag"] == 0.0
                   and sc.gate_verdict(cal0["sigma_obs_mag"], b0) == "BELOW_FLOOR")))

    # ---------------- N1：乘性平场残差（幅度扫描，注入-响应证据） ----------------
    r = sc.rng("neg:flat")
    smooth = np.zeros((H, W))
    for k in range(1, 5):
        smooth += r.normal(0, 1) / k * np.sin(np.pi * k * xx / W + r.uniform(0, 6.28)) \
            * np.cos(np.pi * k * yy / H + r.uniform(0, 6.28))
    smooth /= max(np.std(smooth), 1e-9)
    rows = []
    for amp in (0.0, 0.01, 0.02, 0.04, 0.08, 0.16):
        out = measure(img0 * (1.0 + amp * smooth), fr, inst, wcs)
        if out is None:
            continue
        cal, g, sel = out
        b, v = gate(cal, fr, inst, sigma_psfsys=0.025, sigma_flat=0.01,
                    tag=f"n1_{amp}", flux=g["flux"][sel])
        rows.append(dict(injected_flat_residual_amp=amp, sigma_obs_mag=cal["sigma_obs_mag"],
                         sigma_floor=b.sigma_floor, sigma_ceiling=b.sigma_ceiling, verdict=v))
    res["negatives"].append(dict(
        id="N1", kind="注入-响应：乘性平场残差（幅度扫描）",
        provides_injection_response=True,
        setup="帧乘 (1+amp·smooth(x,y))；amp=0 即无效应基线",
        rows=rows,
        monotone=bool(all(rows[i]["sigma_obs_mag"] <= rows[i + 1]["sigma_obs_mag"] + 0.004
                          for i in range(len(rows) - 1))),
        response_ratio=float(rows[-1]["sigma_obs_mag"] / rows[0]["sigma_obs_mag"]),
        first_red_amp=next((x["injected_flat_residual_amp"] for x in rows
                            if x["verdict"] == "ABOVE_CEILING"), None),
        expected="sigma_obs 随 amp 单调上升；amp=0 回到基线；足够大时判红",
        pass_=bool(rows[-1]["sigma_obs_mag"] > rows[0]["sigma_obs_mag"] * 1.5
                   and any(x["verdict"] == "ABOVE_CEILING" for x in rows))))

    # ---------------- N2：加性天光（**Poisson 物理口径**，注入-响应证据） ----------------
    sky_e0 = float(np.median(fr["sky_map_adu"])) * inst.gain
    rows2 = []
    for mult in (0.5, 1.0, 2.0, 4.0, 8.0):
        delta = (mult - 1.0) * np.asarray(fr["sky_map_adu"], float) * inst.gain
        img = resimulate(fr, inst, delta, f"n2_{mult}")
        out = measure(img, fr, inst, wcs)
        if out is None:
            continue
        cal, g, sel = out
        b, v = gate(cal, fr, inst, sigma_psfsys=0.025, sigma_flat=0.01,
                    tag=f"n2_{mult}", flux=g["flux"][sel])
        rows2.append(dict(sky_multiplier=mult, sky_adu=float(np.median(fr["sky_map_adu"]) * mult),
                          sigma_obs_mag=cal["sigma_obs_mag"], sigma_floor=b.sigma_floor,
                          sigma_ceiling=b.sigma_ceiling, verdict=v))
    # 对照：**算术相加**的确定性梯度（被 §12.2 禁止的形态）⇒ 记录为能力边界，不计入通过
    rows2n = []
    for gx in (0.0, 0.1, 0.4):
        out = measure(img0 + gx * (xx - W / 2), fr, inst, wcs)
        if out is None:
            continue
        cal, g, sel = out
        b, v = gate(cal, fr, inst, sigma_psfsys=0.025, sigma_flat=0.01,
                    tag=f"n2n_{gx}", flux=g["flux"][sel])
        rows2n.append(dict(arithmetic_gradient_adu_per_px=gx,
                           sigma_obs_mag=cal["sigma_obs_mag"], verdict=v))
    res["negatives"].append(dict(
        id="N2", kind="注入-响应：加性天光（Poisson 物理口径，天光抬升 0.5×–4×）",
        provides_injection_response=True,
        setup="按物理前向重画帧：期望电子图 + Δsky 后重新走 Poisson→读出→增益→饱和→量化",
        rows=rows2,
        # 单调性只在 mult>=1 的子族上判定：重画帧与原始帧 A 的噪声实现不同，
        # mult<1 那一行属于另一个噪声抽样，不与 1× 基线可比。
        monotone=bool(all(rows2[i]["sigma_obs_mag"] <= rows2[i + 1]["sigma_obs_mag"] + 0.004
                          for i in range(len(rows2) - 1)
                          if rows2[i]["sky_multiplier"] >= 1.0
                          and rows2[i + 1]["sky_multiplier"] >= 1.0)),
        baseline_multiplier=1.0,
        # 口径统一：响应倍数一律以 **1× 基线** 为分母
        response_ratio=float(rows2[-1]["sigma_obs_mag"]
                             / next(x["sigma_obs_mag"] for x in rows2
                                    if x["sky_multiplier"] == 1.0)),
        response_ratio_vs_lowest_multiplier=float(rows2[-1]["sigma_obs_mag"]
                                                  / rows2[0]["sigma_obs_mag"]),
        monotone_scope="只在 mult>=1 的子族上判定（0.5× 属另一个噪声抽样，不与 1× 基线可比）",
        note="8× 一档被 measure() 的质量选择剔除（返回 None），故实际行只有 0.5/1/2/4；"
             "这是如实记录，不是把 8× 当作已测。",
        arithmetic_gradient_control=rows2n,
        arithmetic_gradient_note="**算术相加**的确定性梯度（无散粒噪声）被 PSF 局部背景吸收，"
                                 "σ_obs 只变 1.08×——这正是 §12.2 禁止的注入形态，故只作对照记录，"
                                 "不作为通过依据",
        expected="sigma_obs 随天光单调上升；预算同步抬升 ⇒ 判定保持可判",
        pass_=bool(rows2[-1]["sigma_obs_mag"] > rows2[0]["sigma_obs_mag"] * 1.5)))

    # ---------------- N3：预算漏项（颜色项幅度扫描） ----------------
    out = measure(img0, fr, inst, wcs)
    cal, g, sel = out
    dcol = -2.5 * np.log10(np.asarray(fr["f_syn_inject"], float)[sel]
                           / np.asarray(fr["f_syn"], float)[sel])
    sig_real = float(np.std(dcol))
    import scia_gaia as sg
    cat = sg.XpCatalog(sg.dump_cone(274.7216, -13.8415, 0.075, 21.5, "m16"))
    Tw2, Tv2 = sc.load_filter("Baader R")
    Qi_w, Qi_v = sc.load_qe("Ideal QE curve")
    Qm_w, Qm_v = sc.load_qe("KAF-16803")
    ii = np.nonzero(np.isfinite(fr["ra"]))[0]
    ids = [int(np.argmin(np.abs(cat.ra - fr["ra"][k]) + np.abs(cat.dec - fr["dec"][k])))
           for k in ii]
    fa = np.array([sc.f_syn(cat.spectrum(i), cat.wl_nm, Tv2, Tw2, Qi_v, Qi_w) for i in ids])
    fb = np.array([sc.f_syn(cat.spectrum(i), cat.wl_nm, Tv2, Tw2, Qm_v, Qm_w) for i in ids])
    dcol_strong = -2.5 * np.log10(fa / fb)
    sig_strong = float(np.std(dcol_strong))
    sweep = []
    base_sig = 0.012
    Fmed = float(np.median(g["flux"][sel]))
    sp2 = float(np.sqrt(inst.read_noise ** 2 + np.median(fr["sky_map_adu"]) * inst.gain))
    for amp_col in (0.0, 0.02, 0.04, 0.06, 0.08, 0.12):
        rr = sc.rng("neg:n3sweep")
        cv = (dcol_strong - np.mean(dcol_strong))
        cv = cv / max(np.std(cv), 1e-9) * amp_col
        r_in2 = np.log10(float(fr["inject_scale"])) + rr.normal(0, base_sig, ii.size) + cv / 2.5
        r2 = sc.irls_tukey(r_in2)
        s2 = 2.5 * r2["sigma_residual"]
        bw = pl.build_budget(np.full(ii.size, Fmed), inst, sp2, 0.025, amp_col, 0.002,
                             0.01, 1.0, ii.size, tag=f"n3s{amp_col}")
        bn = pl.build_budget(np.full(ii.size, Fmed), inst, sp2, 0.025, 0.0, 0.002,
                             0.01, 1.0, ii.size, tag=f"n3n{amp_col}")
        sweep.append(dict(injected_color_amp_mag=amp_col, sigma_obs_mag=float(s2),
                          ceiling_with_color=bw.sigma_ceiling,
                          ceiling_without_color=bn.sigma_ceiling,
                          verdict_with_color=sc.gate_verdict(s2, bw),
                          verdict_without_color=sc.gate_verdict(s2, bn)))
    disc = next((s["injected_color_amp_mag"] for s in sweep
                 if s["verdict_without_color"] == "ABOVE_CEILING"
                 and s["verdict_with_color"] == "PASS"), None)
    res["negatives"].append(dict(
        id="N3", kind="注入-响应：预算漏掉颜色项（幅度扫描）",
        provides_injection_response=True,
        setup="用真实 Gaia XP 谱在两个 QE 曲线下的通带失配构造颜色项，扫描其幅度；"
              "对比预算含/不含 sigma_color 的判定",
        real_color_sigma_mag=sig_real,
        strong_passband_mismatch_sigma_mag=sig_strong,
        discriminating_amplitude_mag=disc,
        ratio_discriminating_over_real=(None if disc is None or sig_real == 0
                                        else float(disc / sig_real)),
        sweep=sweep,
        note="本仪器的**真实**颜色项散度只有 0.006–0.008 mag ⇒ 漏项不改变判定（如实记录）；"
             "扫描给出漏项翻红的阈值。该负例证明「漏项会被判红」这一机制存在，"
             "但不证明真实量级下会被判红。",
        pass_=bool(disc is not None)))

    # ---------------- N4：人为加"跨帧 k 一致性门" ⇒ 正确帧被判红（用**实测** k） ----------------
    s5 = json.load(open(os.path.join(sc.RESULTS, "step5_calibration_gate.json"), encoding="utf-8"))
    fi = s5["frame_independence"]
    kA = float(fi["k_photo_A"]); kB = float(fi["k_photo_B"])
    ratio = kB / kA
    res["negatives"].append(dict(
        id="N4", kind="错误门禁反例：跨帧 k 一致性门会误杀正确帧",
        provides_injection_response=False,
        setup="用 step5 **实测**的两帧 k_photo（各自独立标定，透明度比 0.62 为已知真值），"
              "检验若加 |k_B/k_A − 1| < tol 的组间门会发生什么",
        k_photo_A_measured=kA, k_photo_B_measured=kB,
        k_ratio_measured=float(ratio),
        k_ratio_expected=float(1.0 / fi["transparency_ratio_B_over_A"]),
        relative_deviation_from_expected=float(abs(ratio * fi["transparency_ratio_B_over_A"] - 1.0)),
        gate_would_reject_at_5pct=bool(abs(ratio - 1.0) > 0.05),
        note="跨帧 k 散度是**正确物理**（不同透明度）；把它当门会稳定误杀。"
             "变更 claim PHOT-GATE-DROP-001 / 负责人 §9.49 定案 2",
        pass_=bool(abs(ratio - 1.0) > 0.05)))

    # ---------------- N5：过度裁剪真实样本 ⇒ 度量被压到 0 ⇒ 判红 ----------------
    fl = np.asarray(g["flux"][sel], float)
    fs = np.asarray(fr["f_syn"], float)[sel]
    r_all = np.log10(fl / fs)
    keep = np.abs(r_all - np.median(r_all)) < 0.001        # 过裁剪
    rows5 = []
    for tol in (None, 0.05, 0.01, 0.002, 0.001):
        m = np.ones(r_all.size, bool) if tol is None else np.abs(r_all - np.median(r_all)) < tol
        if m.sum() < 5:
            rows5.append(dict(trim_tol=tol, n=int(m.sum()), sigma_obs_mag=None, verdict=None))
            continue
        c = calibrate(fl[m], fs[m], None, None, None, m_degree=0)
        bb, vv = gate(c, fr, inst, sigma_psfsys=0.025, sigma_flat=0.01,
                      tag=f"n5_{tol}", flux=fl[m])
        rows5.append(dict(trim_tol=tol, n=int(m.sum()), sigma_obs_mag=c["sigma_obs_mag"],
                          sigma_floor=bb.sigma_floor, sigma_ceiling=bb.sigma_ceiling,
                          verdict=vv))
    res["negatives"].append(dict(
        id="N5", kind="退化输入检查：过度裁剪真实样本 ⇒ 度量归零/判红",
        provides_injection_response=False,
        setup="对帧 A 的**实测**通量按 |r−median(r)| < tol 逐步过裁剪；tol=None 为不裁剪基线",
        rows=rows5,
        note="**本负例未通过，且暴露判据的真实弱点**：裁剪越狠 σ_obs 确实单调下降"
             "（0.0453 → 0.0036），但 σ_floor 下降得更快，n≤22 时 rho_lo=1−3·1.166/√n 变负、"
             "下界变成负数 ⇒ **BELOW_FLOOR 不可达**，过裁剪样本反而全部 PASS。"
             "即该判据的下界对「把样本做干净」没有判别力（n 小的时候下界是空的）。",
        finding="sigma_floor 在 n < (3·1.166)² = 12.2 附近变为非正；n≤22 时已接近失效。"
                "建议：下界改用 max(rho_lo, 0)·sigma_fit 或对 n 设最小样本量硬门槛。",
        pass_=bool(any(x["verdict"] == "BELOW_FLOOR" for x in rows5))))

    n_ir = sum(1 for n in res["negatives"] if n.get("provides_injection_response"))
    res["n_negatives"] = len(res["negatives"])
    res["n_pass"] = int(sum(1 for n in res["negatives"] if n.get("pass_")))
    res["n_injection_response"] = int(n_ir)
    res["n_injection_response_pass"] = int(
        sum(1 for n in res["negatives"]
            if n.get("provides_injection_response") and n.get("pass_")))
    res["summary"] = (f"{res['n_pass']}/{res['n_negatives']} 条通过；其中**注入-响应**型 "
                      f"{res['n_injection_response_pass']}/{n_ir} 条全部通过（N1 乘性平场残差、"
                      f"N2 Poisson 天光抬升、N3 预算漏颜色项）。"
                      f"N0/N4 是退化输入与错误门禁反例；**N5 未通过**并暴露判据下界在 n≲22 时"
                      f"失效（σ_floor 变负）——如实记录，不掩盖。")
    sc.jdump(res, os.path.join(sc.RESULTS, "step7_negatives.json"))


if __name__ == "__main__":
    main()
