#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA-TYPE-MATRIX 示范判据 1 —— **天光通过泊松散粒压低 SNR**（含纯加性负例）。

对应 GAP_AUDIT §9.41：天光 B↑ 必须**同时**带来方差 B↑（泊松），因此
    SNR(B) = F*sqrt(sum P^2)/sigma_sky(B)，sigma_sky^2 = (B*t + D*t)/g^2 + RN^2/g^2 + 1/12
必须随 B 单调下降；而"纯加性天光"（只加常数）**方差不变 ⇒ SNR 不变 ⇒ 度量恒为 0**。

估计量（关键设计，见"诚实边界"）
    sigma_sky 用**配对差分估计量**：同场景同参数、不同种子的两帧之差 d = I1 - I2，
    Var(d) = 2*Var_single，且**一切静态结构（星点/星云/平场/梯度）在差里逐位抵消** ——
    因此它测到的**就是噪声本身**，不需要任何源掩膜。局部天光水平按**星点所在像素**的
    解析天光面取值（不是帧平均），消除天光梯度带来的失配。
    同时报告"单帧分块 MAD"（生产式估计量）作对照 —— 它在星云主导视场里**有正偏差**，
    该偏差本身作为诚实边界登记（不用于判据）。

判据（写死，不事后放宽）
    C1 单调性   : 8 个天光水平上 SNR 严格单调下降
    C2 定量闭合 : R_meas = SNR(B_min)/SNR(B_max) 与解析预测相对偏差 < 5%
    C3 低 SNR 端: high_sky 场景上所选弱星 SNR < 15（"高天光低 SNR"确实成立）
    N1 负例-加性: 配对同种子 + 成品帧常数 ADU 偏移 ⇒ |R - 1| < 1e-12（真值无效应 ⇒ 归零）
    N2 负例-均值: mean_only 臂（跳过泊松）内 B0 vs B1 ⇒ |R - 1| < 1e-12（配对）
                 且非配对 MC 版本 |median R - 1| < 5%

产物：run/reverse_verify/data_matrix/results/exp1_sky_poisson_snr.json
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dtmlib as L  # noqa: E402

N_SEED = 12                 # 每个天光水平的噪声实现数（两两配对 ⇒ 6 个差分样本）
TOL_RATIO = 0.05
TOL_NEG = 1e-12
TOL_MEAN_ONLY = 0.05
SNR_LOW_MAX = 15.0
BASE_SEED = 770000
BOX = 160                   # 局部差分统计的方框边长 [pix]


def local_diff_sigma(frames, y, x, r_in=12.0, r_out=22.0):
    """配对差分天光 rms：median over pairs of clipped_std(I1-I2)/sqrt(2)，取**环形**区域。

    为什么必须是环（实测教训，两轮才定案）：
      * 大统计方框会圈进**别的星**：512^2 内 250 颗星 ⇒ 160x160 框内期望 ~24 颗，
        其源散粒方差使低天光端 sigma **高估 ~7%**；
      * 排除全部星后框内几乎无干净像素（该星场本就密）；
      * 环形区（与测光背景环同一几何）在 min_sep>=35px 选星条件下**无邻星**，
        而目标星 Moffat4 翼在 r>=12px 只贡献 4.4e-4 的通量 ⇒ 干净。
    """
    ny, nx = frames[0].adu.shape
    y0, y1 = int(max(0, y - r_out - 2)), int(min(ny, y + r_out + 3))
    x0, x1 = int(max(0, x - r_out - 2)), int(min(nx, x + r_out + 3))
    sy, sx = np.mgrid[y0:y1, x0:x1]
    rr2 = (sy - y) ** 2 + (sx - x) ** 2
    keep = (rr2 >= r_in ** 2) & (rr2 <= r_out ** 2)
    est = []
    for i in range(0, len(frames) - 1, 2):
        d = frames[i].adu[y0:y1, x0:x1] - frames[i + 1].adu[y0:y1, x0:x1]
        est.append(L.clipped_std(d[keep]) / math.sqrt(2.0))
    return float(np.median(est)), est


def single_frame_block_sigma(f, truth, scene):
    """生产式单帧估计量：源掩膜 + 分块稳健 rms（仅作对照，不用于判据）。"""
    mask = L.sky_mask_from_scene(scene, truth)
    return L.block_sigma_adu(f.adu, block=32, mask=mask)[0]


def run_level(scene, frame_index, seeds, star, kernel, gain, det, t, temp_c, overrides=None):
    """一个天光水平：返回 SNR / sigma_sky(差分) / sigma_sky(单帧) / F_hat 的中位数。"""
    sc = L.frame_scene(scene, frame_index, overrides)      # **有效场景**（含 per-frame 覆盖）
    frames = [f for f, _ in L.render_mc(scene, frame_index=frame_index, seeds=seeds,
                                        overrides=overrides)]
    truths = [tr for _, tr in L.render_mc(scene, frame_index=frame_index, seeds=seeds[:1],
                                          overrides=overrides)]
    tr0 = truths[0]
    sig_diff, _ = local_diff_sigma(frames, star["y"], star["x"])
    sig_single = single_frame_block_sigma(frames[0], tr0, sc)
    snrs, sig_used = [], []
    for f in frames:
        r = L.extract_star(f.adu, star["y"], star["x"], kernel, gain=gain,
                           sigma_sky_adu=sig_diff, ann_in=12.0, ann_out=22.0)
        snrs.append(r["snr"]); sig_used.append(r["sigma_sky_adu"])
    # 星点所在像素的解析天光率（消除梯度失配）
    sky_surf = L.NM.sky_surface_e_per_s(tr0["shape"], **sc.get("sky", {}))
    iy, ix = int(round(star["y"])), int(round(star["x"]))
    sky_rate_local = float(sky_surf[iy, ix])
    m_local = float(frames[0].flat[iy, ix])          # 固定平场（本底乘性响应）
    sky_e_local = sky_rate_local * t * m_local
    dark_e = det.dark_current_at(temp_c) * t
    sig_pred = L.NM.sky_sigma_adu(sky_e=sky_e_local, dark_e=dark_e, det=det)
    F_hat = float(np.median([L.extract_star(f.adu, star["y"], star["x"], kernel, gain=gain,
                                            sigma_sky_adu=sig_diff, ann_in=12.0,
                                            ann_out=22.0)["F_hat_adu"] for f in frames]))
    # canon 解析预测：**把同一估计量作用在"无噪声期望帧"上**。
    # 这一步同时吃掉三个系统性来源（实测教训）：
    #   ① 源散粒项（亮星在低天光端由源散粒主导，SNR 不再 ~ 1/sigma_sky）；
    #   ② **固定平场 PRNU x 天光基座**的测光偏置（1% PRNU、天光 10^4 ADU 时可达 ~7% 流量偏差）；
    #   ③ 背景环几何/PSF 截断。
    f0 = frames[0]
    model_adu = (f0.src_e + f0.sky_e + f0.dark_e) / gain + det.bias_adu
    r_model = L.extract_star(model_adu, star["y"], star["x"], kernel, gain=gain,
                             sigma_sky_adu=sig_pred, ann_in=12.0, ann_out=22.0)
    P = L.local_psf(kernel, star["y"], star["x"], kernel.shape[0] // 2)
    pred = L.canon_snr_pred(r_model["F_hat_adu"] * gain, sig_pred, P, gain)
    pred["F_model_adu"] = r_model["F_hat_adu"]
    pred["F_truth_adu"] = star["flux_e"] / gain
    pred["flat_sky_bias_frac"] = r_model["F_hat_adu"] / (star["flux_e"] / gain) - 1.0
    return {"snr_median": float(np.median(snrs)), "snr_mc_std": float(np.std(snrs)),
            "snr_pred_canon": pred["snr_pred"],
            "source_term_peak_frac": pred["source_term_peak_frac"],
            "sigma_sky_diff_adu": sig_diff, "sigma_sky_singleframe_adu": sig_single,
            "sigma_sky_pred_adu": sig_pred, "sky_rate_local_e_per_s": sky_rate_local,
            "sky_e_local": sky_e_local, "F_hat_adu": F_hat, "F_true_e": star["flux_e"],
            "F_model_adu": pred["F_model_adu"], "F_truth_adu": pred["F_truth_adu"],
            "flat_sky_bias_frac": pred["flat_sky_bias_frac"],
            "m_local_flat": m_local, "n_seed": len(seeds)}


def main() -> int:
    res = {"experiment": "exp1_sky_poisson_snr", "criteria": [], "arms": {}}

    # ---------------- Regime A：天光扫描（sweep_sky 配方） ----------------
    scene = L.load_scene("synthetic/scenes/sweep_sky.json")
    nlev = len(scene["frames"])
    kernel, psf_meta = L.R.psf_kernel(scene["psf"])
    gain = float(scene["detector"].get("gain_e_per_adu", 1.5))
    t = float(scene["exposure_s"])
    temp_c = float(scene["temp_c"])
    det = L.NM.Detector(**scene["detector"])

    _, truth0 = L.render_mc(scene, frame_index=0, seeds=[BASE_SEED])[0]
    star = L.pick_star(truth0, flux_lo=2.0e3, flux_hi=2.0e4, min_sep_px=35.0)
    if star is None:
        print("[exp1] FAIL: no suitable star")
        return 1

    rows = []
    for k in range(nlev):
        seeds = [BASE_SEED + 1000 * k + s for s in range(N_SEED)]
        r = run_level(scene, k, seeds, star, kernel, gain, det, t, temp_c)
        r.update({"frame": scene["frames"][k]["frame_id"],
                  "sky_rate_e_per_s": float(scene["frames"][k]["sky"]["level_e_per_s"])})
        rows.append(r)
    snrs = [r["snr_median"] for r in rows]
    mono = all(snrs[i + 1] < snrs[i] for i in range(len(snrs) - 1))
    R_meas = snrs[0] / snrs[-1]
    R_pred = rows[0]["snr_pred_canon"] / rows[-1]["snr_pred_canon"]   # canon 解析预测
    rel = abs(R_meas / R_pred - 1.0)
    R_pred_puresky = rows[-1]["sigma_sky_pred_adu"] / rows[0]["sigma_sky_pred_adu"]
    closure = [r["sigma_sky_diff_adu"] / r["sigma_sky_pred_adu"] - 1.0 for r in rows]
    res["arms"]["sky_sweep"] = {"rows": rows, "R_meas": R_meas, "R_pred": R_pred,
                                "R_pred_pure_1_over_sigma_sky": R_pred_puresky,
                                "rel_dev": rel, "sigma_closure_rel_dev": closure,
                                "target_star": star, "psf": psf_meta,
                                "sigma_estimator": "paired-difference (Var(d)=2*Var_single)",
                                "production_estimator_bias": [
                                    r["sigma_sky_singleframe_adu"] / r["sigma_sky_diff_adu"] - 1.0
                                    for r in rows]}
    res["criteria"].append(L.verdict("C1_sky_snr_monotone_decreasing", mono,
                                     "SNR: " + ", ".join("%.1f" % v for v in snrs)))
    res["criteria"].append(L.verdict("C2_ratio_matches_poisson_prediction", rel < TOL_RATIO,
                                     "R_meas=%.4f R_pred=%.4f rel_dev=%.4f ; max|sigma closure|=%.4f"
                                     % (R_meas, R_pred, rel, max(abs(c) for c in closure))))

    # ---------------- Regime B：高天光低 SNR（high_sky_low_snr 配方） ----------------
    sceneB = L.load_scene("synthetic/scenes/high_sky_low_snr.json")
    kernelB, psfB = L.R.psf_kernel(sceneB["psf"])
    gainB = float(sceneB["detector"].get("gain_e_per_adu", 1.5))
    tB = float(sceneB["exposure_s"])
    detB = L.NM.Detector(**sceneB["detector"])
    _, truthB = L.render_mc(sceneB, frame_index=0, seeds=[BASE_SEED + 7])[0]
    starB = L.pick_star(truthB, flux_lo=100.0, flux_hi=800.0, min_sep_px=35.0, prefer="faint")
    rB = run_level(sceneB, 0, [BASE_SEED + 5000 + s for s in range(N_SEED)],
                   starB, kernelB, gainB, detB, tB, float(sceneB["temp_c"]))
    res["arms"]["high_sky_low_snr"] = dict(rB, target_star=starB, psf=psfB)
    res["criteria"].append(L.verdict("C3_low_snr_regime_reached", rB["snr_median"] < SNR_LOW_MAX,
                                     "SNR=%.2f (< %.0f), local sky=%.1f e-/pix"
                                     % (rB["snr_median"], SNR_LOW_MAX, rB["sky_e_local"])))

    # ---------------- 负例 1：纯加性天光（配对同种子，成品帧常数偏移） ----------------
    k0, k1 = 0, nlev - 1
    B0 = float(scene["frames"][k0]["sky"]["level_e_per_s"])
    B1 = float(scene["frames"][k1]["sky"]["level_e_per_s"])
    delta_adu = (B1 - B0) * t / gain
    seeds = [BASE_SEED + 3000 + s for s in range(N_SEED)]
    ratios = []
    for i in range(0, len(seeds) - 1, 2):
        f1, tr = L.render_mc(scene, frame_index=k0, seeds=[seeds[i]])[0]
        f2, _ = L.render_mc(scene, frame_index=k0, seeds=[seeds[i + 1]])[0]
        sig_ref, _ = local_diff_sigma([f1, f2], star["y"], star["x"])
        r_ref = L.extract_star(f1.adu, star["y"], star["x"], kernel, gain=gain,
                               sigma_sky_adu=sig_ref, ann_in=12.0, ann_out=22.0)
        g1 = L.NM.Frame(adu=f1.adu + delta_adu, truth_e=f1.truth_e, src_e=f1.src_e,
                        sky_e=f1.sky_e, dark_e=f1.dark_e, flat=f1.flat,
                        provenance=f1.provenance)
        g2 = L.NM.Frame(adu=f2.adu + delta_adu, truth_e=f2.truth_e, src_e=f2.src_e,
                        sky_e=f2.sky_e, dark_e=f2.dark_e, flat=f2.flat,
                        provenance=f2.provenance)
        sig_add, _ = local_diff_sigma([g1, g2], star["y"], star["x"])
        r_add = L.extract_star(g1.adu, star["y"], star["x"], kernel, gain=gain,
                               sigma_sky_adu=sig_add, ann_in=12.0, ann_out=22.0)
        ratios.append(r_add["snr"] / r_ref["snr"])
    worst = max(abs(r - 1.0) for r in ratios)
    res["arms"]["negative_additive"] = {
        "delta_adu": delta_adu, "equivalent_sky_rate_e_per_s": B1 - B0,
        "snr_ratio_minus_1_max_abs": worst, "ratios": ratios,
        "note": "纯加性：均值 +Delta，方差逐位不变 ⇒ SNR 必须不变"}
    res["criteria"].append(L.verdict("N1_additive_only_metric_vanishes", worst < TOL_NEG,
                                     "max|SNR_ctrl/SNR_ref - 1| = %.3e" % worst))

    # ---------------- 负例 2：mean_only 臂（跳过泊松；均值变、方差不变） ----------------
    mo_paired, mo_unpaired, mo_sig = [], [], []
    ov0 = {"mode": L.NM.MODE_MEAN_ONLY}
    ov1 = {"mode": L.NM.MODE_MEAN_ONLY, "sky": {"level_e_per_s": B1}}
    for i in range(0, len(seeds) - 1, 2):
        s1, s2 = seeds[i], seeds[i + 1]
        a0, tr0 = L.render_mc(scene, frame_index=k0, seeds=[s1], overrides=ov0)[0]
        b0, _ = L.render_mc(scene, frame_index=k0, seeds=[s2], overrides=ov0)[0]
        a1, _ = L.render_mc(scene, frame_index=k0, seeds=[s1], overrides=ov1)[0]
        b1, _ = L.render_mc(scene, frame_index=k0, seeds=[s2], overrides=ov1)[0]
        sig0, _ = local_diff_sigma([a0, b0], star["y"], star["x"])   # 差分估计量（结构抵消）
        sig1, _ = local_diff_sigma([a1, b1], star["y"], star["x"])
        r0 = L.extract_star(a0.adu, star["y"], star["x"], kernel, gain=gain,
                            sigma_sky_adu=sig0, ann_in=12.0, ann_out=22.0)
        r1 = L.extract_star(a1.adu, star["y"], star["x"], kernel, gain=gain,
                            sigma_sky_adu=sig1, ann_in=12.0, ann_out=22.0)
        mo_paired.append(r0["snr"] / r1["snr"])
        mo_sig.append((sig0, sig1))
        # 非配对：B1 用**另一批种子**（天光水平覆盖到 k1），排除"配对太巧"的疑虑
        a1u, _ = L.render_mc(scene, frame_index=k1, seeds=[s1 + 7777], overrides=ov0)[0]
        b1u, _ = L.render_mc(scene, frame_index=k1, seeds=[s2 + 7777], overrides=ov0)[0]
        sig1u, _ = local_diff_sigma([a1u, b1u], star["y"], star["x"])
        r1u = L.extract_star(a1u.adu, star["y"], star["x"], kernel, gain=gain,
                             sigma_sky_adu=sig1u, ann_in=12.0, ann_out=22.0)
        mo_unpaired.append(r0["snr"] / r1u["snr"])
    mo_dev = max(abs(v - 1.0) for v in mo_paired)
    mo_dev_u = abs(float(np.median(mo_unpaired)) - 1.0)
    res["arms"]["negative_mean_only"] = {
        "paired_ratio_minus_1_max_abs": mo_dev, "paired_ratios": mo_paired,
        "unpaired_median_ratio_minus_1": mo_dev_u, "unpaired_ratios": mo_unpaired,
        "sigma_pairs_adu": mo_sig,
        "note": "同一非物理臂内 B0 vs B1：均值变、方差不变 ⇒ 度量必须为 1；"
                "sigma 仍用配对差分估计量（否则会被静态结构污染）"}
    res["criteria"].append(L.verdict("N2_mean_only_arm_shows_no_physical_effect",
                                     mo_dev < TOL_NEG and mo_dev_u < TOL_MEAN_ONLY,
                                     "paired max|ratio-1|=%.3e ; unpaired median|ratio-1|=%.4f"
                                     % (mo_dev, mo_dev_u)))

    res["all_pass"] = all(c["pass"] for c in res["criteria"])
    L.save_result("exp1_sky_poisson_snr", res)
    print("[exp1] %s" % ("ALL PASS" if res["all_pass"] else "HAS FAILURES"))
    return 0 if res["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
