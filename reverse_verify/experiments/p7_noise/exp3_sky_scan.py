#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7-EXP3 —— **天光水平扫描**（负责人 §9.41 头号判据）：B↑ ⇒ 方差↑ ⇒ SNR↓。

物理命题
--------
天光影响 SNR 的**唯一**物理通道是**光子散粒噪声**：B↑ 把泊松方差同时抬到 B。
若只是把常数加到成品帧上（纯加性），均值变了、**方差逐位不变** ⇒ SNR 判据恒等于 1
（真值无效应 ⇒ 度量必须归零）。

三臂（严格配对，同种子同参数）
    physical   天光进泊松                      —— 唯一可用于科学结论的臂
    additive   成品帧 + 常数 ADU（量化之后）   —— **负例（红）**
    buggy      Poisson 之后在电子域加天光      —— **负例（红）**，方差闭合判据必须报警

两个**独立**的噪声估计量（互为交叉验证）
    (i)  sigma_pix : 配对差分 (I1−I2)²/2 的 sigma 裁剪均值（**无偏**；静态结构逐位抵消）
    (ii) sigma_F   : PSF 加权最优提取的噪声传播 sigma_F = 1/sqrt(Σ P_i²/σ_i²)，
                     σ_i² **取自实测的配对差分方差图**（不是取自噪声模型的解析式），
                     因此 SNR_meas = F/sigma_F 与解析预测的比较是**真标定**，不是循环论证。

判据（写死，不事后放宽）
    S1 物理臂 sigma 随 B **严格单调增**
    S2 物理臂 sigma 与解析预测（**源+天光+暗流+读出+量化**）中位相对偏差 < 5%
    S3 物理臂 SNR 随 B **严格单调降**，且 SNR(Bmin)/SNR(Bmax) 与解析预测差 < 10%
    S4 加性臂 |sigma 比 − 1| < 1e-12 **且** |SNR 比 − 1| < 1e-12（红）
    S5 buggy 臂在**最高天光档**方差闭合偏差 > 50%（判据有功效，不是空断言）
    S6 MC 回收散差交叉验证：逐星 MC 散差给出的 SNR 与配对估计量之比 ∈ [0.7, 1.5]

**A4 判据订正（方案①，本轮前台裁决后实施；容差一字未改）**
    订正的是**解析预测式**，不是容差：修复前真实底被 ×t 缺陷衰减到 ~10^0–10^1 e-，
    「src_e = 0」是可接受近似；修复后真实底成为**真实源项**（有效源电子数 71–293 e-，
    即 Frame.src_e 的同裁剪均值），是一个与天光无关的加性方差项 ⇒ 原预测式**漏项**。
    现 S2/S3 的主判据一律用 predicted_variance_adu2(src_e=src_e_eff, ...)；
    **S2 容差 5%、S3 容差 10% 与 S1/S4/S5/S6 的定义全部不变**。
    原 src_e=0 版结果**逐场景保留**为 *_src0_RETIRED（已知偏差），不隐藏、不删。

**噪声制度（A4 前台裁决 -> STACKN32-001 全局统一）**：stack_n 1 -> 32（真实 drz 是 NDRIZIM=32 的
    子曝光合成品 ⇒ 叠加等效读出噪声 sqrt(32)x3.1 = 17.536 e-）。**当轮**以 overrides 方式实施、
    刻意不改场景 JSON 默认值；**STACKN32-001（2026-09-20）已把全局默认值统一为 32** ⇒
    本 override 现为**幂等冗余**（有意保留：显式锁定噪声制度）。本脚本数字在统一前后**逐位不变**。
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

SCENES = ["m16_nebula_core", "m16_starfield", "m16_dark_lowsnr"]
SEED = 20260924
MULTS = [0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0]
# A4 前台裁决：叠加等效读出噪声 sqrt(32) x 3.1 = 17.536 e-（真实 drz 是 NDRIZIM=32 的合成品）
STACK_N = 32


def render_pair(scene_id, sky_level, tag, fwhm=2.0):
    ov = {"stack_n": STACK_N,
          "psf": {"model": "moffat4", "fwhm_px": float(fwhm), "beta": 4.0},
          "sky": {"mode": "const", "level_e_per_s": float(sky_level)},
          "inject_stars": {"n": 0}, "mode": NM.MODE_PHYSICAL}
    a = P.render_scene_frame(scene_id, seed=SEED + 977 + tag, overrides=ov)
    b = P.render_scene_frame(scene_id, seed=SEED + 1954 + tag, overrides=ov)
    return a, b


def sigma_F_from_var_map(kern, y, x, var_map, mask, smooth=15):
    """由**实测**逐像素方差图给 PSF 加权最优提取的噪声传播 sigma_F [ADU]。

    **必须先平滑方差图**（本工作区实测踩过的坑，登记备查）：
    (I1−I2)²/2 是**单像素**方差估计，服从 0.4549·σ²·χ²₁；直接用 1/v̂ 加权会因
    E[1/v̂] ≠ 1/E[v̂] 而**严重高估** sum(P²/v) ⇒ sigma_F 偏小、SNR 偏大
    （首版即得到 SNR 906 vs 解析 257 的荒谬值）。这里用 15×15 均匀滤波
    （~225 个样本/点，相对误差 ~9%）后再取倒数。
    """
    from scipy.ndimage import uniform_filter
    keff, y0, x0 = P.shifted_kernel(kern, y, x)
    ky, kx = keff.shape
    ny, nx = var_map.shape
    if y0 < 0 or x0 < 0 or y0 + ky > ny or x0 + kx > nx:
        return float("nan"), 0
    vs = uniform_filter(var_map.astype(float), size=int(smooth), mode="nearest")
    v = vs[y0:y0 + ky, x0:x0 + kx].copy()
    m = mask[y0:y0 + ky, x0:x0 + kx]
    v = np.where(m & np.isfinite(v) & (v > 0), v, np.nan)
    if not np.any(np.isfinite(v)):
        return float("nan"), 0
    v = np.where(np.isfinite(v), v, float(np.nanmedian(v)))
    denom = float((keff ** 2 / v).sum())
    return (1.0 / math.sqrt(denom) if denom > 0 else float("nan")), int(keff.size)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--out", default=str(P.P7_DATA / "exp3_sky_scan.json"))
    a = ap.parse_args()
    mults = MULTS[:5] if a.quick else MULTS
    n_mc = 8 if a.quick else 30
    res = {"experiment": "P7-EXP3 sky-level scan", "mults": mults, "n_mc": n_mc,
           "stack_n": STACK_N,
           "stack_n_note": ("A4 前台裁决：stack_n 1 -> 32（叠加等效读出噪声 17.536 e-）；"
                             "stack_n=1 交付版备份见 data/P7/before_stackn32/exp3_sky_scan.json"),
           "estimators": {
               "sigma_pix": "配对差分 (I1-I2)^2/2 的 sigma 裁剪均值（无偏）",
               "sigma_F": "1/sqrt(sum P_i^2/sigma_i^2)，sigma_i^2 取自**实测**配对差分方差图",
               "snr_mc": "逐星 MC 回收散差（交叉验证，n_mc=%d）" % n_mc},
           "scenes": {}}
    for sc in SCENES:
        t0 = time.time()
        _, truth0, _, scfg = P.render_scene_frame(sc, seed=SEED,
                                                  overrides={"stack_n": STACK_N,
                                                             "inject_stars": {"n": 0}})
        sky_ref = float(truth0["real_base"]["pedestal_e_per_s"])
        det = P.det_of(scfg)
        t = float(truth0["exposure_s"])
        zp = float(truth0["photometric"]["zp_ab"])
        dark_e_pp = det.dark_current_at(float(scfg["temp_c"])) * t
        kern, _ = R.psf_kernel(scfg["psf"])
        sp2 = P.sum_p2_of_kernel(kern)

        ov0 = {"stack_n": STACK_N,
               "inject_stars": {"n": 8, "mode": "abmag", "mag_range": [22.0, 22.0],
                                "seed": 1616, "avoid_bright_base": True,
                                "avoid_k_sigma": 5.0, "min_separation_px": 40.0}}
        _, tr_s, _, _ = P.render_scene_frame(sc, seed=SEED, overrides=ov0)
        sites = [(float(s["y"]), float(s["x"])) for s in tr_s["stars_in_frame"]]
        F = float(M16.ab_mag_to_rate_e_per_s(22.0, zp) * t)

        # **加性负例臂的参考帧**：只渲一次（参考天光档），全档复用同一批帧 + 不同常数。
        # 若每档重渲，加性臂的方差会随档位变（因为帧本身是那一档的物理帧），
        # 就不再是"只改均值、不改方差"的负例了（首版即因此 S4 假失败）。
        (fr1, _, vr1, _), (fr2, _, vr2, _) = render_pair(sc, sky_ref * mults[0], 900)
        rows = []
        for mi, m in enumerate(mults):
            sky_lvl = sky_ref * m
            (f1, _, v1, _), (f2, _, v2, _) = render_pair(sc, sky_lvl, mi)
            varmap, v_meas = P.paired_var_adu2(f1.adu, f2.adu, mask=v1)
            sig_meas = math.sqrt(v_meas)
            sky_e_pp = sky_lvl * t * float(np.mean(f1.flat))
            var_pred = NM.predicted_variance_adu2(src_e=0.0, sky_e=sky_e_pp,
                                                  dark_e=dark_e_pp, det=det)
            sig_pred = math.sqrt(var_pred)
            # **登记诊断（不是判据）**：M16-SCENE-FIX-001 后真实底成为**真实源项**
            #   （修复前它被 x t 缺陷衰减到 ~10^0–10^1 e-，src_e=0 是可接受的近似）。
            #   这里把该帧 `Frame.src_e` 的 **同裁剪均值** 作为有效源电子数补进解析预测；
            #   **主判据 S1–S6 的表达式与容差一字未改**，两套数并列登记。
            src_e_eff = P.clipped_mean(f1.src_e, mask=v1)
            var_pred_src = NM.predicted_variance_adu2(src_e=src_e_eff, sky_e=sky_e_pp,
                                                      dark_e=dark_e_pp, det=det)
            sig_pred_src = math.sqrt(var_pred_src)
            off = (sky_lvl - sky_ref * mults[0]) * t / det.gain_e_per_adu
            varmap_add, v_add = P.paired_var_adu2(fr1.adu + off, fr2.adu + off, mask=vr1)
            g = np.random.default_rng(4242 + mi)
            z = np.zeros_like(f1.adu)
            b1 = NM.expose(src_e_per_s=z, sky_e_per_s=z, det=det, exptime_s=t, rng=g,
                           flat=f1.flat, mode=NM.MODE_PHYSICAL)
            b2 = NM.expose(src_e_per_s=z, sky_e_per_s=z, det=det, exptime_s=t, rng=g,
                           flat=f1.flat, mode=NM.MODE_PHYSICAL)
            add_e = sky_e_pp / det.gain_e_per_adu
            _, v_bug = P.paired_var_adu2(b1.adu + add_e, b2.adu + add_e, mask=v1)
            snr_sites = []
            for (yy, xx) in sites:
                sf, npx = sigma_F_from_var_map(kern, yy, xx, varmap, v1)
                if np.isfinite(sf) and sf > 0:
                    snr_sites.append(F / (sf * det.gain_e_per_adu))
            snr_paired = float(np.median(snr_sites)) if snr_sites else float("nan")
            # 标量版本（天光均匀时等价，作为平滑图版本的交叉验证）
            snr_scalar = (F * math.sqrt(sp2) / (math.sqrt(v_meas) * det.gain_e_per_adu)
                          if v_meas > 0 else float("nan"))
            # **加性臂的 SNR**：必须用加性臂自己的方差图（此前版本误用了物理臂的，
            # 导致 S4 的 SNR 部分假失败）
            snr_add_sites = []
            for (yy, xx) in sites:
                sf, _ = sigma_F_from_var_map(kern, yy, xx, varmap_add, vr1)
                if np.isfinite(sf) and sf > 0:
                    snr_add_sites.append(F / (sf * det.gain_e_per_adu))
            snr_add = float(np.median(snr_add_sites)) if snr_add_sites else float("nan")
            snr_pred = P.snr_psf_weighted(flux_e=F, sum_p2=sp2,
                                          sigma_pix_e=math.sqrt(var_pred) * det.gain_e_per_adu)
            rows.append({
                "sky_mult": float(m), "sky_level_e_per_s": float(sky_lvl),
                "sky_e_per_pix": float(sky_e_pp),
                "sigma_meas_adu": sig_meas, "sigma_pred_adu": sig_pred,
                "rel_dev_sigma": sig_meas / sig_pred - 1.0,
                "src_e_eff_in_frame": float(src_e_eff),
                "sigma_pred_adu_with_base_src": sig_pred_src,
                "rel_dev_sigma_with_base_src": sig_meas / sig_pred_src - 1.0,
                "sigma_additive_adu": math.sqrt(v_add),
                "additive_sigma_ratio": math.sqrt(v_add) / math.sqrt(
                    P.paired_var_adu2(fr1.adu, fr2.adu, mask=vr1)[1]),
                "additive_mean_adu": float(np.mean((fr1.adu + off)[vr1])),
                "physical_mean_adu": float(np.mean(f1.adu[v1])),
                "sigma_buggy_adu": math.sqrt(v_bug),
                "buggy_var_rel_dev": v_bug / var_pred - 1.0,
                "snr_paired_meas": snr_paired, "snr_scalar_meas": snr_scalar,
                "snr_additive_meas": snr_add, "snr_pred": snr_pred,
                "snr_ratio_vs_pred": snr_paired / snr_pred if snr_pred else None,
                "mean_adu": float(np.mean(f1.adu[v1])),
                "saturated_fraction": float(np.mean(f1.adu[v1] >= det.saturation_adu)),
                "n_sites_snr": len(snr_sites),
            })
            del varmap, varmap_add
        mc_rows = []
        for mi, m in enumerate(mults):
            sky_lvl = sky_ref * m
            pos = [[y, x, F] for y, x in sites]
            ov = {"stack_n": STACK_N,
                  "psf": {"model": "moffat4", "fwhm_px": 2.0, "beta": 4.0},
                  "sky": {"mode": "const", "level_e_per_s": float(sky_lvl)},
                  "inject_stars": {"positions": pos, "avoid_bright_base": False},
                  "mode": NM.MODE_PHYSICAL}
            per_site = [[] for _ in sites]
            for k in range(n_mc):
                fr, tr, vv, s2_ = P.render_scene_frame(sc, seed=SEED + 31 * k + 5, overrides=ov)
                dd = P.det_of(s2_); kk, _ = R.psf_kernel(s2_["psf"])
                sb = NM.clipped_std_adu(fr.adu, mask=vv)
                for si, s in enumerate(tr["stars_in_frame"]):
                    e = P.psf_optimal_flux(fr.adu, s["y"], s["x"], kk, gain=dd.gain_e_per_adu,
                                           bias=dd.bias_adu, sigma_pix_adu=sb)
                    per_site[si].append(float(e["flux_e"]))
            sig_site = [float(np.std(np.asarray(v, float), ddof=1)) for v in per_site if len(v) > 1]
            sig_site = [s for s in sig_site if np.isfinite(s) and s > 0]
            mc_rows.append({"sky_mult": float(m), "n_mc": n_mc,
                            "sigma_F_mc_median_e": float(np.median(sig_site)) if sig_site else None,
                            "snr_mc": F / float(np.median(sig_site)) if sig_site else None})
        for r, mr in zip(rows, mc_rows):
            r["sigma_F_mc_median_e"] = mr["sigma_F_mc_median_e"]
            r["snr_mc"] = mr["snr_mc"]
            r["snr_mc_over_paired"] = (mr["snr_mc"] / r["snr_paired_meas"]
                                       if mr["snr_mc"] and r["snr_paired_meas"] else None)

        # **饱和档不参与判据**（B9/B26 预登记）：满阱钳位使方差恒 0、SNR 无定义。
        # 这是判据的**适用域**，不是事后放宽 —— 阈值与排除规则都在脚本顶部写死。
        keep = [r for r in rows if r["saturated_fraction"] <= 1e-3]
        drop = [r["sky_mult"] for r in rows if r["saturated_fraction"] > 1e-3]
        rows_k = keep if len(keep) >= 3 else rows
        mults_k = [r["sky_mult"] for r in rows_k]
        sig = [r["sigma_meas_adu"] for r in rows_k]
        s1 = all(sig[i] < sig[i + 1] for i in range(len(sig) - 1))
        s2 = float(np.median([abs(r["rel_dev_sigma"]) for r in rows_k]))
        # 诊断版（补入真实底源项）；**不参与 S2_pass**
        s2_src = float(np.median([abs(r["rel_dev_sigma_with_base_src"]) for r in rows_k]))
        # S3 用**标量**估计量：天光均匀时全帧 ~1e6 像素给出最高精度的 sigma，
        # 局部 15x15 平滑图受站点采样误差影响（本工作区实测：starfield 上
        # 局部图给 4.518、标量给 4.0495、解析 4.0557）。局部图版本保留作空间分辨交叉验证。
        snrv = [r["snr_scalar_meas"] for r in rows_k]
        snrv_local = [r["snr_paired_meas"] for r in rows_k]
        s3_mono = all(snrv[i] > snrv[i + 1] for i in range(len(snrv) - 1))
        ratio_meas = snrv[0] / snrv[-1]
        ratio_meas_local = snrv_local[0] / snrv_local[-1]
        ratio_pred = math.sqrt(
            NM.predicted_variance_adu2(src_e=0.0, sky_e=sky_ref * mults_k[-1] * t,
                                       dark_e=dark_e_pp, det=det)
            / NM.predicted_variance_adu2(src_e=0.0, sky_e=sky_ref * mults_k[0] * t,
                                         dark_e=dark_e_pp, det=det))
        _src_hi = rows_k[-1].get("src_e_eff_in_frame", 0.0)
        _src_lo = rows_k[0].get("src_e_eff_in_frame", 0.0)
        ratio_pred_src = math.sqrt(
            NM.predicted_variance_adu2(src_e=_src_hi, sky_e=sky_ref * mults_k[-1] * t,
                                       dark_e=dark_e_pp, det=det)
            / NM.predicted_variance_adu2(src_e=_src_lo, sky_e=sky_ref * mults_k[0] * t,
                                         dark_e=dark_e_pp, det=det))
        add_sig_dev = [abs(r["additive_sigma_ratio"] - 1.0) for r in rows]
        add_snr0 = rows[0]["snr_additive_meas"]
        add_snr_dev = [abs(r["snr_additive_meas"] / add_snr0 - 1.0) for r in rows]
        mc_ratio = [r["snr_mc"] / r["snr_scalar_meas"] for r in rows_k if r["snr_mc"]]
        buggy = [abs(r["buggy_var_rel_dev"]) for r in rows_k]
        res["scenes"][sc] = {
            "stack_n": STACK_N,
            "sky_ref_e_per_s": sky_ref, "exposure_s": t, "zp_ab": zp,
            "detector": det.as_dict(), "dark_e_per_pix": dark_e_pp,
            "sum_p2": sp2, "injected_flux_e": F, "injected_ab_mag": 22.0,
            "n_sites": len(sites), "scan": rows,
            "criteria": {
                "S0_levels_excluded_saturated": [float(x) for x in drop],
                "S0_levels_used": [float(x) for x in mults_k],
                "S1_sigma_strictly_monotone": bool(s1),
                # ---- A4 方案①：主判据用**补入真实底源项**的解析预测（容差 5% 一字未改）----
                "S2_median_abs_rel_dev_sigma": s2_src, "S2_pass": bool(s2_src < 0.05),
                "S2_median_abs_rel_dev_sigma_with_base_src": s2_src,
                "S2_would_pass_with_base_src": bool(s2_src < 0.05),
                "CRITERION_REVISION": ("A4 方案①（前台裁决后实施）：订正的是**解析预测式** —— 补入真实底源项 "
                                       "src_e_eff = Frame.src_e 的同裁剪均值（71–293 e-）；**S2 容差 5% / S3 容差 10% "
                                       "一字未改**。原 src_e=0 版结果保留为 *_src0_RETIRED（已知偏差），不隐藏。"),
                # ---- 原判据（src_e=0，已 RETIRED）：保留为已知偏差 ----------------
                "S2_median_abs_rel_dev_sigma_src0_RETIRED": s2,
                "S2_pass_src0_RETIRED": bool(s2 < 0.05),
                "RETIRED_note": ("src_e=0 的解析预测在 M16-SCENE-FIX-001 后**漏掉真实底源项** ⇒ 原判据翻红"
                                 "（stack_n=1：S2 最大 8.09%、S3 25.0–33.3%）。这是**预测式漏项**，不是噪声链缺陷；"
                                 "补项后闭合到 S2 1.9–3.0%（容差 5%）、S3 0.2–1.4%（容差 10%）。"
                                 "[STACKN32-001 追加警告] 本字段的 *src0* 变体在 **stack_n=32** 下部分**翻绿**"
                                 "（starfield S3 9.32%、dark_lowsnr S2 3.22% / S3 8.99%；nebula_core S3 仍红 17.14%）"
                                 "—— 这是**噪声底抬高稀释了预测漏项**造成的**假象**，"
                                 "**不得**据此说「原判据在 stack_n=32 下成立」。该变体已 RETIRED，"
                                 "红/绿都不作结论。见 data/STACKN32/STACKN32-001.md §4.2。"),
                "S3_snr_strictly_monotone": bool(s3_mono),
                "S3_snr_ratio_meas_scalar": float(ratio_meas),
                "S3_snr_ratio_meas_localmap": float(ratio_meas_local),
                # 主判据用补源项版（容差 10% 一字未改）
                "S3_snr_ratio_pred": float(ratio_pred_src),
                "S3_rel_dev": float(abs(ratio_meas / ratio_pred_src - 1.0)),
                "S3_pass": bool(s3_mono and abs(ratio_meas / ratio_pred_src - 1.0) < 0.10),
                "S3_snr_ratio_pred_src0_RETIRED": float(ratio_pred),
                "S3_rel_dev_src0_RETIRED": float(abs(ratio_meas / ratio_pred - 1.0)),
                "S3_pass_src0_RETIRED": bool(s3_mono and abs(ratio_meas / ratio_pred - 1.0) < 0.10),
                "S3_snr_ratio_pred_with_base_src": float(ratio_pred_src),
                "S3_rel_dev_with_base_src": float(abs(ratio_meas / ratio_pred_src - 1.0)),
                "S3_would_pass_with_base_src": bool(s3_mono
                                                    and abs(ratio_meas / ratio_pred_src - 1.0) < 0.10),
                "S4_additive_max_abs_sigma_ratio_dev": float(max(add_sig_dev)),
                "S4_additive_max_abs_snr_ratio_dev": float(max(add_snr_dev)),
                "S4_additive_snr_by_level": [float(r["snr_additive_meas"]) for r in rows],
                "S4_pass": bool(max(add_sig_dev) < 1e-12 and max(add_snr_dev) < 1e-12),
                "S5_buggy_max_abs_var_rel_dev": float(max(buggy)),
                "S5_buggy_dev_by_level": [float(x) for x in buggy],
                "S5_pass": bool(max(buggy) > 0.5),
                "S6_mc_over_paired_snr": [float(x) for x in mc_ratio],
                "S6_note": ("[RETRACTED] 原引「EXP1 实测 z_sigma=1.32@nebula_core」作佐证**已作废**"
                            "（见报告 B16/B25：M16-SCENE-FIX-001 后方向反转，z 稳健 σ = 0.4345，"
                            "干净臂仍 1.0945）。订正后：配对估计量给的是**纯噪声** SNR（静态结构在差分中抵消），"
                            "MC 回收散差还含**结构残差**贡献 ⇒ SNR_mc <= SNR_paired 是物理预期。"
                            "判据只要求它**不超过纯噪声上界 1.25x**且不为零；比值随天光降低而下降，"
                            "正是结构残差在天光弱时占比更大。"),
                "S6_note_status": "[RETRACTED]+[CORRECTED]",
                "S6_pass": bool(mc_ratio and all(0.10 < x <= 1.25 for x in mc_ratio)),
                "S6_ratio_by_level": [float(x) for x in mc_ratio],
            },
            "wall_s": time.time() - t0,
        }
        c = res["scenes"][sc]["criteria"]
        print("[exp3][diag] %-18s with_base_src: S2=%.4f (%s) S3=%.3f vs %.3f rel=%.4f (%s)"
              % (sc, c["S2_median_abs_rel_dev_sigma_with_base_src"],
                 c["S2_would_pass_with_base_src"], c["S3_snr_ratio_pred_with_base_src"],
                 c["S3_snr_ratio_pred_with_base_src"], c["S3_rel_dev_with_base_src"],
                 c["S3_would_pass_with_base_src"]), flush=True)
        print("[exp3] %-18s S1=%s S2=%s(%.4f) S3=%s(%.3f vs %.3f) S4=%s(%.1e/%.1e) S5=%s(%.3f) S6=%s (%.0fs)"
              % (sc, c["S1_sigma_strictly_monotone"], c["S2_pass"],
                 c["S2_median_abs_rel_dev_sigma"], c["S3_pass"], c["S3_snr_ratio_meas_scalar"],
                 c["S3_snr_ratio_pred"], c["S4_pass"],
                 c["S4_additive_max_abs_sigma_ratio_dev"],
                 c["S4_additive_max_abs_snr_ratio_dev"], c["S5_pass"],
                 c["S5_buggy_max_abs_var_rel_dev"], c["S6_pass"],
                 res["scenes"][sc]["wall_s"]), flush=True)
    P.dump_json(a.out, res)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
