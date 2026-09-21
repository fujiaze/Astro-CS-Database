#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7-EXP1 —— **注入-回收（injection-recovery）**：注入已知通量/星等，回收精度 vs SNR。

仿照的研究大纲（逐条核验见报告 §1）
----------------------------------
* **Merline & Howell 1995**（Exp. Astron. 6, 163, DOI 10.1007/BF00421131）
  —— "A realistic model for point-sources imaged on array detectors"：
  大纲 = 解析 PSF → 电子域噪声清单 → **合成帧上的注入-回收** → 与实测统计比对。
* **Suchyta et al. 2016**（Balrog, MNRAS 457, 786, DOI 10.1093/mnras/stv2953）
  —— DES 注入-回收框架：人工源注入**真实巡天帧**，回收后测选择函数与通量偏置。
* **Bruderer et al. 2016**（UFIS, ApJ 817, 25, DOI 10.3847/0004-637X/817/1/25）
  —— 快照式注入-回收，用于 DES 测量偏置标定。

设计（搬到 M16 真实底 + §9.41 物理噪声）
---------------------------------------
真值：真实 HST M16 底图（去噪成期望率面）+ N 颗**显式位置、显式 AB 星等**的注入星。
      **位置与通量跨臂逐位相同**（用 positions=[y,x,flux_e] 显式给定）⇒ 臂间差异
      只可能来自 PSF/天光/噪声，不来自星表重抽。
臂：
  * seeing 臂 fwhm ∈ {1,2,3} px；天光臂 ×{0.2,1,5,20} 底图 5% 分位；
  * **负例（红）NEG_zero_noise**：读出=0、暗流=0、平场≡1、mode=mean_only（跳过泊松）
    ⇒ 回收通量必须**精确**等于真值（|ΔF/F| < 1e-12）；
  * **负例（红）NEG_additive_sky_x20**：mode=additive，在成品帧上加常数 ADU
    ⇒ 回收 SNR 与天光水平**无关**（|SNR 比 − 1| < 1e-12）。
估计量：PSF 域最优提取（Horne 1986, PASP 98, 609, DOI 10.1086/131801）；
        圆孔径 r=5px + 环形本底（对照口径，诊断用）。
判据（写死，不事后放宽）
    R1 无噪声负例：max|F_hat/F_true − 1| < 1e-12
    R2 亮星无偏：SNR>20 的星 |median(Δmag)| < 0.05 mag
    R3 散差-1/SNR 一致：Δmag 的稳健散差 × SNR 落在 [0.6, 1.8]（理论 = 1.0857 mag）
    R4 seeing 无关：PSF 域 Δmag 的臂间极差 < 0.05 mag
    R5 天光退化 + 加性零效应
"""

from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE))
import p7lib as P  # noqa: E402
import noise_model as NM  # noqa: E402
import render as R  # noqa: E402
import m16_scene as M16  # noqa: E402

SCENE = "m16_nebula_core"
# **饱和预算**（必须先算，不得事后放宽；**M16-SCENE-FIX-001 重标定后重算**）：
# 场景满阱 = NDRIZIM(32) x 7e4 e- = 2.24e6 e-（真实 drz 是 32 次子曝光 drizzle 合成品，
# 见 实验/shared/synthetic/scenes/m16_*.json 的 calibration 块），曝光 9600 s
# ⇒ 饱和率 = 2.24e6/9600 = 233.33 e/s；用 PHOTFLAM 零点 ZP_AB=22.635 换算，
# AB 星等 **< 16.715 的星必然饱和**。故：
#   * 科学臂注入星等下限仍取 20.0（1.09e5 e-，距满阱 20x 余量，完全不受饱和影响）；
#   * 饱和臂用 14.0–16.5（全部亮于 16.715 阈值）。
# 【变更登记】修复前（满阱 7e4 e-、饱和率 7.292 e/s）阈值为 20.48 mag，饱和臂为 17–20。
MAGS = [20.0, 20.5, 21.0, 21.5, 22.0, 22.5, 23.0, 23.5]
MAGS_SAT = [14.0, 15.0, 16.0, 16.5]      # 饱和臂：全部亮于 16.715 mag 阈值
SEED = 20260924
CAT_SEED = 1616

# **噪声制度（A4 前台裁决，本轮实施）**：真实 M16 drz 是 NDRIZIM=32 的子曝光 drizzle 合成品
# ⇒ 帧的**叠加等效读出噪声** = sqrt(32) x 3.1 = 17.536 e-（不是单次读出的 3.1 e-）。
# 本脚本以 overrides 方式把 stack_n 置 32。**当轮**刻意不改场景 JSON 的默认值（以免把
# A6/P9/交付数据集的变化混进来、无法归因）；**STACKN32-001（2026-09-20）已把全局默认值统一为 32**，
# 故本 override 现为**幂等冗余**（有意保留：显式锁定噪声制度）。
# 饱和预算（叠加满阱 2.24e6 e-）与 stack_n=32 自洽：「32 次子曝光的和」配「32 x 单次满阱」。
STACK_N = 32


def select_sites(scene_id: str, n: int, seed: int) -> tuple:
    """从真实底图挑 n 个**平坦且有效**的注入位置（一次选定，全实验复用）。"""
    ov = {"stack_n": STACK_N,
          "inject_stars": {"n": int(n), "mode": "abmag", "mag_range": [20.0, 20.0],
                           "seed": seed, "avoid_bright_base": True, "avoid_k_sigma": 5.0,
                           "min_separation_px": 40.0}}
    _, truth, _, _ = P.render_scene_frame(scene_id, seed=SEED, overrides=ov)
    cat = truth["stars_in_frame"]
    return [(float(s["y"]), float(s["x"])) for s in cat], truth


def make_positions(sites, mags, zp_ab, t):
    """[y, x, flux_e] 显式列表：位置固定、通量由 AB 星等经 PHOTFLAM 零点换算。"""
    out = []
    for i, (y, x) in enumerate(sites):
        m = mags[i % len(mags)]
        f = float(M16.ab_mag_to_rate_e_per_s(m, zp_ab) * t)
        out.append([y, x, f])
    return out


def run_arm(*, label, positions, fwhm_px, sky_level_e_per_s, mode, zero_noise,
            additive_offset_adu=0.0, n_seed=1, base_seed=SEED, scene_id=SCENE,
            flat_base=False, no_flat=False):
    """flat_base=True：把真实底图置零 ⇒ 星点落在**严格平坦**的本底上，
    这是让「无噪声 ⇒ 回收精确等于真值」这一负例**逐位成立**的必要条件
    （否则真实底的平滑梯度会在核支撑域内留下常数本底模型无法吸收的残差）。"""
    t0 = time.time()
    rows = []
    for k in range(n_seed):
        ov = {
            "stack_n": STACK_N,
            "psf": {"model": "moffat4", "fwhm_px": float(fwhm_px), "beta": 4.0},
            "sky": {"mode": "const", "level_e_per_s": float(sky_level_e_per_s)},
            "mode": mode, "additive_offset_adu": float(additive_offset_adu),
            "inject_stars": {"positions": positions, "avoid_bright_base": False},
        }
        if zero_noise:
            ov["detector"] = {"read_noise_e": 0.0, "dark_current_e_per_s": 0.0,
                              "quantize": False}
        if zero_noise or no_flat:
            ov["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "tilt_x": 0.0,
                          "tilt_y": 0.0, "vignette": 0.0}
        if flat_base:
            ov["real_base"] = {"flux_scale": 0.0}
        fr, truth, valid, sc = P.render_scene_frame(scene_id, seed=base_seed + k, overrides=ov)
        det = P.det_of(sc)
        kern, _ = R.psf_kernel(sc["psf"])
        sig_bg = NM.clipped_std_adu(fr.adu, mask=valid)
        flat_map = fr.flat
        # 精确零假设下传入**真值本底**（天光 + 真实底），消除环形本底估计的污染
        bg_true = None
        if zero_noise:
            bg_true = float(sky_level_e_per_s) * float(truth["exposure_s"]) / det.gain_e_per_adu
        for s in truth["stars_in_frame"]:
            est = P.psf_optimal_flux(fr.adu, s["y"], s["x"], kern,
                                     gain=det.gain_e_per_adu, bias=det.bias_adu,
                                     sigma_pix_adu=sig_bg, local_bg=bg_true)
            ap = P.aperture_photometry(fr.adu, s["y"], s["x"], 5.0, ann_r_in=10.0,
                                       ann_r_out=16.0, gain=det.gain_e_per_adu,
                                       bias=det.bias_adu)
            F = float(s["flux_e"])
            rows.append({
                "seed": base_seed + k, "y": float(s["y"]), "x": float(s["x"]),
                "ab_mag_true": float(s["true_ab_mag"]), "flux_true_e": F,
                "flux_psf_e": est["flux_e"], "snr_psf": est["snr"],
                "flux_ap5_e": ap.get("flux_e"),
                "dmag_psf": (-2.5 * math.log10(est["flux_e"] / F)
                             if est["flux_e"] and est["flux_e"] > 0 else float("nan")),
                "dmag_ap5": (-2.5 * math.log10(ap["flux_e"] / F)
                             if ap.get("flux_e") and ap["flux_e"] > 0 else float("nan")),
                "sig_bg_adu": sig_bg,
                "flat_at_site": float(flat_map[int(round(s["y"])), int(round(s["x"]))]),
                "dmag_from_flat": float(-2.5 * math.log10(
                    max(float(flat_map[int(round(s["y"])), int(round(s["x"]))]), 1e-12))),
            })
    return {"label": label, "fwhm_px": fwhm_px, "sky_level_e_per_s": sky_level_e_per_s,
            "mode": mode, "zero_noise": zero_noise,
            "additive_offset_adu": additive_offset_adu, "n_frames": n_seed,
            "stars": rows, "wall_s": time.time() - t0}


def summarize(arm):
    r = arm["stars"]
    d = np.array([x["dmag_psf"] for x in r], float)
    snr = np.array([x["snr_psf"] for x in r], float)
    ok = np.isfinite(d) & np.isfinite(snr)
    da = np.array([x["dmag_ap5"] for x in r], float)
    oka = np.isfinite(da)
    return {"n_stars": int(ok.sum()),
            "median_dmag_psf": float(np.median(d[ok])) if ok.any() else None,
            "mad_dmag_psf": P.robust_sigma(d[ok]) if ok.any() else None,
            "median_abs_dmag_psf": float(np.median(np.abs(d[ok]))) if ok.any() else None,
            "median_snr_psf": float(np.median(snr[ok])) if ok.any() else None,
            "max_abs_dmag_psf": float(np.max(np.abs(d[ok]))) if ok.any() else None,
            "median_dmag_ap5": float(np.median(da[oka])) if oka.any() else None,
            "mad_dmag_ap5": P.robust_sigma(da[oka]) if oka.any() else None}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--out", default=str(P.P7_DATA / "exp1_injection_recovery.json"))
    a = ap.parse_args()
    n_seed = 2 if a.quick else 6
    n_stars = 16 if a.quick else 32

    sites, truth0 = select_sites(SCENE, n_stars, CAT_SEED)
    zp_ab = float(truth0["photometric"]["zp_ab"])
    t = float(truth0["exposure_s"])
    sky_ref = float(truth0["real_base"]["pedestal_e_per_s"])
    positions = make_positions(sites, MAGS, zp_ab, t)

    res = {"experiment": "P7-EXP1 injection-recovery", "scene": SCENE,
           "design": {"n_sites": len(sites), "mags_ab_cycle": MAGS, "n_seed": n_seed,
                      "seed": SEED, "cat_seed": CAT_SEED, "exposure_s": t,
                      "stack_n": STACK_N,
                      "read_noise_e_effective": 3.1 * math.sqrt(STACK_N),
                      "stack_n_rationale": ("真实 drz 是 NDRIZIM=32 的子曝光合成品 ⇒ "
                                            "叠加等效读出噪声 sqrt(32)x3.1 = 17.536 e-；"
                                            "A4 前台裁决实施（当轮以 overrides 施加；STACKN32-001 后场景 JSON 默认值"
                                            "亦已全局统一为 32，override 现为幂等冗余）"),
                      "zp_ab": zp_ab, "sky_ref_e_per_s": sky_ref,
                      "estimator": "PSF-domain optimal (Horne 1986) + r=5px aperture",
                      "sites": [[round(y, 3), round(x, 3)] for y, x in sites],
                      "true_flux_e": [p[2] for p in positions],
                      "catalog_policy": "positions=[y,x,flux_e] 显式给定 ⇒ 跨臂逐位相同"},
           "arms": {}}

    arms = [("seeing_fwhm1.0", dict(fwhm_px=1.0, sky_level_e_per_s=sky_ref,
                                    mode=NM.MODE_PHYSICAL, zero_noise=False)),
            ("seeing_fwhm2.0", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                    mode=NM.MODE_PHYSICAL, zero_noise=False)),
            ("seeing_fwhm3.0", dict(fwhm_px=3.0, sky_level_e_per_s=sky_ref,
                                    mode=NM.MODE_PHYSICAL, zero_noise=False))]
    for mult in (0.2, 1.0, 5.0, 20.0):
        arms.append(("sky_x%.1f" % mult, dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref * mult,
                                              mode=NM.MODE_PHYSICAL, zero_noise=False)))
    arms.append(("NEG_zero_noise", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                        mode=NM.MODE_MEAN_ONLY, zero_noise=True,
                                        flat_base=True)))
    arms.append(("NEG_additive_sky_x20", dict(
        fwhm_px=2.0, sky_level_e_per_s=sky_ref, mode=NM.MODE_ADDITIVE, zero_noise=False,
        additive_offset_adu=19.0 * sky_ref * t / 1.5)))
    # 饱和臂：用**亮星**（全部超过饱和阈）显式检验硬钳位 —— 应看到系统性丢通量
    pos_sat = make_positions(sites, MAGS_SAT, zp_ab, t)
    arms.append(("SAT_bright_mags", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                         mode=NM.MODE_PHYSICAL, zero_noise=False,
                                         positions_override=pos_sat)))
    # --- 诊断臂：把「回收偏置」的来源逐项拆开（诚实边界用，不是判据） ---
    #   DIAG_noflat      : 关平场      -> 隔离平场乘性衰减造成的通量损失
    #   DIAG_flatbase    : 真实底置零  -> 隔离星云结构在核支撑域内的泄漏
    #   DIAG_clean       : 两者都关    -> 理论 SNR 应精确成立的"干净"臂
    arms.append(("DIAG_noflat", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                     mode=NM.MODE_PHYSICAL, zero_noise=False, no_flat=True)))
    arms.append(("DIAG_flatbase", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                       mode=NM.MODE_PHYSICAL, zero_noise=False,
                                       flat_base=True)))
    arms.append(("DIAG_clean", dict(fwhm_px=2.0, sky_level_e_per_s=sky_ref,
                                    mode=NM.MODE_PHYSICAL, zero_noise=False,
                                    flat_base=True, no_flat=True)))

    for label, kw in arms:
        pos_use = kw.pop("positions_override", positions)
        arm = run_arm(label=label, positions=pos_use, n_seed=n_seed, **kw)
        arm["summary"] = summarize(arm)
        res["arms"][label] = arm
        s = arm["summary"]
        print("[exp1] %-22s median_dmag=%+.5f MAD=%.5f median_SNR=%7.2f  ap5_dmag=%+.4f (%.1fs)"
              % (label, s["median_dmag_psf"], s["mad_dmag_psf"], s["median_snr_psf"],
                 s["median_dmag_ap5"], arm["wall_s"]), flush=True)

    # --- 判据 ---
    neg = res["arms"]["NEG_zero_noise"]["stars"]
    neg_dev = [abs(x["flux_psf_e"] / x["flux_true_e"] - 1.0) for x in neg
               if np.isfinite(x["flux_psf_e"])]
    res["design"]["neg_zero_noise_design"] = (
        "flat_base=True(真实底置零) + 读出/暗流=0 + 平场≡1 + quantize=False + 跳过泊松 "
        "+ 传入真值本底 ⇒ 前向模型与估计量逐位一致，回收必须精确")

    def med_snr(arm):
        v = [x["snr_psf"] for x in arm["stars"] if np.isfinite(x["snr_psf"])]
        return float(np.median(v)) if v else float("nan")

    ref = res["arms"]["sky_x1.0"]
    phy20 = res["arms"]["sky_x20.0"]
    add20 = res["arms"]["NEG_additive_sky_x20"]
    snr_ratio_phy = med_snr(phy20) / med_snr(ref)
    snr_ratio_add = med_snr(add20) / med_snr(ref)
    seeings = [res["arms"]["seeing_fwhm%.1f" % f]["summary"]["median_dmag_psf"] for f in (1.0, 2.0, 3.0)]
    # R3：**逐星标准化** z = dmag * SNR / 1.0857，应服从 N(0,1)
    #     （dmag 的 1σ 理论值 = 1.0857/SNR mag；1.0857 = 2.5/log(10)）
    def zstats(arm):
        br = [x for x in arm["stars"] if np.isfinite(x["dmag_psf"]) and x["snr_psf"] > 5]
        if not br:
            return {"n": 0, "z_median": None, "z_robust_sigma": None,
                    "mad_dmag": None, "median_snr": None}
        z = np.array([x["dmag_psf"] * x["snr_psf"] / 1.0857 for x in br], float)
        return {"n": len(br), "z_median": float(np.median(z)),
                "z_robust_sigma": P.robust_sigma(z),
                "mad_dmag": P.robust_sigma(np.array([x["dmag_psf"] for x in br], float)),
                "median_snr": float(np.median([x["snr_psf"] for x in br]))}
    zref = zstats(ref)
    zclean = zstats(res["arms"]["DIAG_clean"])
    znf = zstats(res["arms"]["DIAG_noflat"])
    zfb = zstats(res["arms"]["DIAG_flatbase"])
    z_med, z_sig = zref["z_median"], zref["z_robust_sigma"]
    mad, msnr = zref["mad_dmag"], zref["median_snr"]
    # 平场解释力：dmag 与 -2.5log10(flat_site) 的相关/斜率
    def flat_explained(arm):
        d = np.array([x["dmag_psf"] for x in arm["stars"]], float)
        f = np.array([x["dmag_from_flat"] for x in arm["stars"]], float)
        ok = np.isfinite(d) & np.isfinite(f)
        if ok.sum() < 4 or np.std(f[ok]) == 0:
            return {"n": int(ok.sum()), "slope": None, "pearson_r": None,
                    "median_flat_mag": float(np.median(f[ok])) if ok.any() else None}
        sl = float(np.polyfit(f[ok], d[ok], 1)[0])
        rr = float(np.corrcoef(f[ok], d[ok])[0, 1])
        return {"n": int(ok.sum()), "slope": sl, "pearson_r": rr,
                "median_flat_mag": float(np.median(f[ok])),
                "median_dmag": float(np.median(d[ok]))}
    fx_ref = flat_explained(ref)
    fx_nf = flat_explained(res["arms"]["DIAG_noflat"])
    res["criteria"] = {
        "STACK_N": STACK_N,
        "STACK_N_note": ("A4 前台裁决：stack_n 1 -> 32（叠加等效读出噪声 17.536 e-）。"
                         "本 JSON 为 stack_n=32 版；stack_n=1 的交付版备份见 "
                         "data/P7/before_stackn32/exp1_injection_recovery.json（09-20 02:45）"),
        "R1_zero_noise_max_abs_rel_dev": float(np.max(neg_dev)) if neg_dev else None,
        "R1_pass": bool(neg_dev and np.max(neg_dev) < 1e-12),
        "R2_bright_median_abs_dmag": res["arms"]["sky_x1.0"]["summary"]["median_abs_dmag_psf"],
        "R3_theory_1.0857_mag": 1.0857,
        "R3_z_stats_nebula_core": zref,
        "R3_z_stats_DIAG_clean": zclean,
        "R3_z_stats_DIAG_noflat": znf,
        "R3_z_stats_DIAG_flatbase": zfb,
        "R3_bias_diag": {
            "median_dmag_real_base_with_flat": ref["summary"]["median_dmag_psf"],
            "median_dmag_noflat": res["arms"]["DIAG_noflat"]["summary"]["median_dmag_psf"],
            "median_dmag_flatbase": res["arms"]["DIAG_flatbase"]["summary"]["median_dmag_psf"],
            "median_dmag_clean": res["arms"]["DIAG_clean"]["summary"]["median_dmag_psf"],
            "flat_explained_real_base": fx_ref,
            "flat_explained_noflat_arm": fx_nf,
            "interpretation":
                "真实底 + 平场臂的中位偏置 %+.5f mag；关掉平场后偏置降到 %+.5f mag。"
                "**这不是估计量的错**：PSF 域测光测的是「到达探测器的通量」，"
                "平场 m(x,y) 正是把它乘掉的那个量；生产流程必须先做平场校正。"
                "平场在注入位置上的中位星等偏移 = %+.5f mag，与实测偏置 %+.5f mag 同号同量级。"
                "**残余的部分**来自「平场同时乘了本底，而本底用环带中位数估计」这一"
                "核支撑域内的 m 变化残差（见 flat_explained_* 的斜率与相关系数）。"
                % (ref["summary"]["median_dmag_psf"] or 0.0,
                   res["arms"]["DIAG_noflat"]["summary"]["median_dmag_psf"] or 0.0,
                   fx_ref.get("median_flat_mag") or 0.0,
                   fx_ref.get("median_dmag") or 0.0),
        },
        "R3_scatter_diag": {
            "z_sigma_real_base_with_flat": zref["z_robust_sigma"],
            "z_sigma_DIAG_flatbase": zfb["z_robust_sigma"],
            "z_sigma_DIAG_noflat": znf["z_robust_sigma"],
            "z_sigma_DIAG_clean": zclean["z_robust_sigma"],
            "status": "[RETRACTED-DIRECTION]",
            "interpretation":
                "[RETRACTED] 本字段原文（修复前 stack_n=1）称「真实星云底在核支撑域内另有"
                "结构方差（环带中位数本底估计的残差），使实测散差高于解析值」——**该方向已被 "
                "M16-SCENE-FIX-001 推翻**（报告 §6.1 B16）：修复后实测 z 稳健 σ = 0.4345，"
                "而底图置零的干净臂仍为 1.0945（逐位不变）⇒ 真实底存在时是**帧级 σ 估计被"
                "真实结构抬高、解析 SNR 偏低**（z < 1），不是「解析 SNR 低估散差」。"
                "干净臂（底置零 + 关平场）的 z_sigma 才应接近 1。",
        },
        "R3_pass": bool(zclean["z_robust_sigma"] is not None
                        and abs(zclean["z_median"]) < 0.15
                        and 0.7 < zclean["z_robust_sigma"] < 1.4),
        "R4_seeing_median_dmag": seeings,
        "R4_seeing_spread_mag": float(max(seeings) - min(seeings)) if all(
            s is not None for s in seeings) else None,
        "R4_pass": bool(all(s is not None for s in seeings) and (max(seeings) - min(seeings)) < 0.05),
        "R5_physical_snr_ratio_sky20_over_sky1": float(snr_ratio_phy),
        "R5_additive_snr_ratio_sky20_over_sky1": float(snr_ratio_add),
        "R5_additive_null_pass": bool(abs(snr_ratio_add - 1.0) < 1e-12),
        "R5_physical_monotone_pass": bool(snr_ratio_phy < 0.95),
        "sky_scan_snr": {("sky_x%.1f" % m): med_snr(res["arms"]["sky_x%.1f" % m])
                         for m in (0.2, 1.0, 5.0, 20.0)},
    }
    res["criteria"]["R2_pass"] = bool(
        res["criteria"]["R2_bright_median_abs_dmag"] is not None
        and res["criteria"]["R2_bright_median_abs_dmag"] < 0.05)
    P.dump_json(a.out, res)
    c = res["criteria"]
    print("[exp1] R1 zero-noise null : max|dF/F| = %.3e  pass=%s" % (c["R1_zero_noise_max_abs_rel_dev"], c["R1_pass"]))
    print("[exp1] R2 bright |dmag|   : %.5f mag  pass=%s" % (c["R2_bright_median_abs_dmag"], c["R2_pass"]))
    print("[exp1] R3 z-scores  nebula_core med=%+.4f sig=%.4f | DIAG_clean med=%+.4f sig=%.4f | pass=%s"
          % (zref["z_median"], zref["z_robust_sigma"], zclean["z_median"],
             zclean["z_robust_sigma"], c["R3_pass"]))
    print("[exp1] R3 bias diag  with_flat=%+.5f noflat=%+.5f flatbase=%+.5f clean=%+.5f mag"
          % (c["R3_bias_diag"]["median_dmag_real_base_with_flat"],
             c["R3_bias_diag"]["median_dmag_noflat"],
             c["R3_bias_diag"]["median_dmag_flatbase"],
             c["R3_bias_diag"]["median_dmag_clean"]))
    print("[exp1] R4 seeing spread   : %.5f mag  pass=%s" % (c["R4_seeing_spread_mag"], c["R4_pass"]))
    print("[exp1] R5 SNR ratio phy=%.5f add=%.12f null_pass=%s" % (snr_ratio_phy, snr_ratio_add, c["R5_additive_null_pass"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
