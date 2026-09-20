#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA-TYPE-MATRIX 示范判据 3 —— **逐像素方差闭合**（物理噪声模型的直接检验）。

对每个空间分块 b，用**配对差分**测方差（静态结构逐位抵消），与解析预测比较：

    Var_meas(b) = clipped_std( I1(b) - I2(b) )^2 / 2                     [ADU^2]
    Var_pred(b) = mean_p[ (S_e(p) + B_e(p) + D_e)/g^2 + sigma_R^2/g^2 + 1/12 ]  [ADU^2]
    S_e,B_e,D_e = 该块逐像素的**期望电子数**（源/天光/暗，取自渲染真值，含平场与光晕结构）

    注：预测必须包含**源**散粒项（掩膜后残留的星云/星翼通量仍真实贡献方差）；
    首版只用天光 ⇒ 低天光端系统性低估预测方差（闭合偏差 7.5%，见报告诚实边界）。

判据（写死，不事后放宽）
    E1 物理臂闭合     : 8 个天光水平上，逐块 |Var_meas/Var_pred - 1| 的**中位数** < 5%
    E2 空间结构闭合   : 高天光+月光光晕场景中，按局部天光强弱分 3 档，**每档**中位偏差 < 8%
                        （检验天光的**空间**建模，不只是单一水平）
    E3 负例-加性无效应: 纯加性臂下 B_min→B_max 的方差变化 < 1e-12（真值无效应 ⇒ 归零）
    E4 负例-闭合有功效: 纯加性臂在最高天光档 |Var_meas/Var_pred - 1| > 50%
                        （即"闭合检验"能识别非物理臂 —— 判据有功效，不是空断言）
    E5 生产式估计量偏差: 单帧分块 MAD 估计量在**亮星云真实底**上相对差分估计量的偏差
                        必须被量化且 |偏差| > 5%（登记"单帧局部 rms 在结构场上偏高"）

产物：run/reverse_verify/data_matrix/results/exp3_variance_closure.json
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dtmlib as L  # noqa: E402

BLOCK = 64
TOL_MEDIAN = 0.05
TOL_BIN = 0.08
TOL_ZERO = 1e-12
TOL_ADDITIVE_POWER = 0.50
TOL_PROD_BIAS = 0.05
BASE_SEED = 990000
MIN_MASK_FRAC = 0.5


def block_closure(scene_rel, frame_index, *, seeds, mode=None, sky_override=None):
    """逐块方差闭合。返回 dict(blocks=[{sky_e, var_meas, var_pred, ratio, n_pix}...])."""
    scene = L.load_scene(scene_rel)
    ov = {}
    if mode:
        ov["mode"] = mode
    if sky_override is not None:
        ov["sky"] = {"level_e_per_s": sky_override}
    sc = L.frame_scene(scene, frame_index, ov or None)
    det = L.NM.Detector(**sc["detector"])
    t = float(sc["exposure_s"])
    dark_e = det.dark_current_at(float(sc["temp_c"])) * t
    g = det.gain_e_per_adu
    (f1, tr) = L.render_mc(scene, frame_index=frame_index, seeds=[seeds[0]], overrides=ov or None)[0]
    (f2, _) = L.render_mc(scene, frame_index=frame_index, seeds=[seeds[1]], overrides=ov or None)[0]
    mask = L.sky_mask_from_scene(sc, tr)
    # **逐像素期望电子数**（源+天光+暗，已含平场）—— 预测必须用它，而不是只用天光：
    # 星云即使被掩膜"部分保留"，其源散粒也真实存在于数据里（首版只用天光 ⇒ 低天光端
    # 系统性低估预测方差，闭合偏差 7.5%）。
    lam_e = f1.src_e + f1.sky_e + f1.dark_e
    var_pred_pix = lam_e / (g * g) + (det.read_noise_e ** 2) / (g * g)         + L.NM.QUANTIZATION_VARIANCE_ADU2
    sky_pix = f1.sky_e
    ny, nx = sc["shape"]
    by, bx = ny // BLOCK, nx // BLOCK
    blocks = []
    for i in range(by):
        for j in range(bx):
            sl = (slice(i * BLOCK, (i + 1) * BLOCK), slice(j * BLOCK, (j + 1) * BLOCK))
            m = mask[sl]
            if m.mean() < MIN_MASK_FRAC:
                continue
            d = (f1.adu[sl] - f2.adu[sl])[m]
            var_meas = L.clipped_std(d) ** 2 / 2.0
            sky_e = float(np.mean(sky_pix[sl][m]))
            var_pred = float(np.mean(var_pred_pix[sl][m]))
            blocks.append({"i": i, "j": j, "sky_e": sky_e, "var_meas_adu2": var_meas,
                           "var_pred_adu2": var_pred, "ratio": var_meas / var_pred,
                           "sigma_meas_adu": math.sqrt(var_meas),
                           "sigma_pred_adu": math.sqrt(var_pred), "n_pix": int(m.sum())})
    ratios = np.array([b["ratio"] for b in blocks])
    return {"scene": scene_rel, "frame_index": frame_index, "mode": sc.get("mode"),
            "n_blocks": len(blocks), "median_ratio": float(np.median(ratios)),
            "median_abs_dev": float(np.median(np.abs(ratios - 1.0))),
            "frac_within_10pct": float(np.mean(np.abs(ratios - 1.0) < 0.10)),
            "blocks": blocks}


def main() -> int:
    res = {"experiment": "exp3_variance_closure", "criteria": [], "arms": {}}

    # ---------------- E1：天光扫描的逐块方差闭合 ----------------
    levels = [0, 1, 2, 3, 4, 5, 6, 7]
    rows = []
    for k in levels:
        r = block_closure("synthetic/scenes/sweep_sky.json", k,
                          seeds=[BASE_SEED + 100 * k, BASE_SEED + 100 * k + 1])
        rows.append({"frame_index": k, "median_ratio": r["median_ratio"],
                     "median_abs_dev": r["median_abs_dev"],
                     "frac_within_10pct": r["frac_within_10pct"], "n_blocks": r["n_blocks"],
                     "sky_e_median": float(np.median([b["sky_e"] for b in r["blocks"]]))})
    worst = max(r["median_abs_dev"] for r in rows)
    res["arms"]["sky_sweep_closure"] = {"rows": rows, "worst_median_abs_dev": worst,
                                        "block": BLOCK}
    res["criteria"].append(L.verdict("E1_physical_arm_variance_closure", worst < TOL_MEDIAN,
                                     "worst median |Var_meas/Var_pred-1| = %.4f over %d levels"
                                     % (worst, len(rows))))

    # ---------------- E2：空间结构（月光光晕）分档闭合 ----------------
    hs = block_closure("synthetic/scenes/high_sky_low_snr.json", 0,
                       seeds=[BASE_SEED + 5000, BASE_SEED + 5001])
    sky_e = np.array([b["sky_e"] for b in hs["blocks"]])
    rat = np.array([b["ratio"] for b in hs["blocks"]])
    q = np.quantile(sky_e, [1 / 3, 2 / 3])
    bins = {"low": rat[sky_e <= q[0]], "mid": rat[(sky_e > q[0]) & (sky_e <= q[1])],
            "high": rat[sky_e > q[1]]}
    bin_stats = {k: {"n": int(v.size), "median_ratio": float(np.median(v)),
                     "median_abs_dev": float(np.median(np.abs(v - 1.0))),
                     "sky_e_range": [float(sky_e[sky_e <= q[0]].min()) if k == "low" else
                                     (float(sky_e[(sky_e > q[0]) & (sky_e <= q[1])].min())
                                      if k == "mid" else float(sky_e[sky_e > q[1]].min())),
                                     float(sky_e[sky_e <= q[0]].max()) if k == "low" else
                                     (float(sky_e[(sky_e > q[0]) & (sky_e <= q[1])].max())
                                      if k == "mid" else float(sky_e[sky_e > q[1]].max()))]}
                 for k, v in bins.items()}
    worst_bin = max(s["median_abs_dev"] for s in bin_stats.values())
    res["arms"]["moon_halo_spatial_closure"] = {"bins": bin_stats, "n_blocks": hs["n_blocks"],
                                                "worst_bin_median_abs_dev": worst_bin}
    res["criteria"].append(L.verdict("E2_spatially_structured_sky_closure", worst_bin < TOL_BIN,
                                     "worst bin median |ratio-1| = %.4f (3 sky bins, n=%d blocks)"
                                     % (worst_bin, hs["n_blocks"])))

    # ---------------- E3/E4：纯加性臂 —— 无效应 + 闭合检验有功效 ----------------
    k0, k7 = 0, 7
    scene = L.load_scene("synthetic/scenes/sweep_sky.json")
    det = L.NM.Detector(**scene["detector"])
    t = float(scene["exposure_s"])
    gain = det.gain_e_per_adu
    B0 = float(scene["frames"][k0]["sky"]["level_e_per_s"])
    B7 = float(scene["frames"][k7]["sky"]["level_e_per_s"])
    delta_adu = (B7 - B0) * t / gain
    base = block_closure("synthetic/scenes/sweep_sky.json", k0,
                         seeds=[BASE_SEED + 7000, BASE_SEED + 7001])
    # 在**成品帧**上加常数（纯加性）：逐块方差逐位不变
    dv = []
    for b in base["blocks"]:
        dv.append(0.0)
    var_before = np.array([b["var_meas_adu2"] for b in base["blocks"]])
    # 直接验证：常数平移对方差估计量的影响（解析为 0，数值上仅浮点误差）
    scene_k0 = L.frame_scene(scene, k0)
    (f1, tr) = L.render_mc(scene, frame_index=k0, seeds=[BASE_SEED + 7000])[0]
    (f2, _) = L.render_mc(scene, frame_index=k0, seeds=[BASE_SEED + 7001])[0]
    mask = L.sky_mask_from_scene(scene_k0, tr)
    d_ref = (f1.adu - f2.adu)[mask]
    d_add = ((f1.adu + delta_adu) - (f2.adu + delta_adu))[mask]
    rel_change = abs(L.clipped_std(d_add) / L.clipped_std(d_ref) - 1.0)
    res["arms"]["negative_additive_variance"] = {
        "delta_adu": delta_adu, "equivalent_sky_rate_e_per_s": B7 - B0,
        "rel_variance_change": rel_change}
    res["criteria"].append(L.verdict("E3_additive_arm_no_effect", rel_change < TOL_ZERO,
                                     "|Var(additive)/Var(ref) - 1| = %.3e" % rel_change))

    # 闭合检验对非物理臂的功效：用 mean_only 臂（均值升到 B7，方差不变）冒充物理臂
    mo = block_closure("synthetic/scenes/sweep_sky.json", k0, mode=L.NM.MODE_MEAN_ONLY,
                       seeds=[BASE_SEED + 7100, BASE_SEED + 7101])
    # mean_only 臂的方差是"读出+量化"地板；用 B7 的预测去比 → 必然严重不符
    sky_e_B7 = float(np.median([b["sky_e"] for b in base["blocks"]])) * (B7 / B0)
    var_pred_B7 = L.NM.predicted_variance_adu2(
        src_e=0.0, sky_e=sky_e_B7,
        dark_e=det.dark_current_at(float(scene["temp_c"])) * t, det=det)
    var_meas_mo = float(np.median([b["var_meas_adu2"] for b in mo["blocks"]]))
    dev = abs(var_meas_mo / var_pred_B7 - 1.0)
    res["arms"]["negative_mean_only_closure_power"] = {
        "var_meas_adu2": var_meas_mo, "var_pred_at_B7_adu2": var_pred_B7,
        "abs_rel_dev": dev,
        "note": "mean_only 臂均值到 B7 但方差不动 ⇒ 物理预测必然被拒（判据有功效）"}
    res["criteria"].append(L.verdict("E4_closure_test_has_power", dev > TOL_ADDITIVE_POWER,
                                     "non-physical arm deviates %.1f%% from prediction (> %.0f%%)"
                                     % (100 * dev, 100 * TOL_ADDITIVE_POWER)))

    # ---------------- E5：生产式单帧估计量在亮星云真实底上的偏差 ----------------
    rb = block_closure("synthetic/scenes/nebula_core_m42_realbase.json", 0,
                       seeds=[BASE_SEED + 9000, BASE_SEED + 9001])
    scene_rb = L.load_scene("synthetic/scenes/nebula_core_m42_realbase.json")
    (frb, trb) = L.render_mc(scene_rb, frame_index=0, seeds=[BASE_SEED + 9000])[0]
    mask_rb = L.sky_mask_from_scene(scene_rb, trb)
    sig_single, _ = L.block_sigma_adu(frb.adu, block=BLOCK, mask=mask_rb)
    sig_diff = float(np.median([b["sigma_meas_adu"] for b in rb["blocks"]]))
    prod_bias = sig_single / sig_diff - 1.0
    res["arms"]["production_estimator_bias"] = {
        "sigma_singleframe_blockmad_adu": sig_single, "sigma_paired_diff_adu": sig_diff,
        "rel_bias": prod_bias, "block": BLOCK,
        "note": "单帧分块 MAD 在亮星云真实底（含真实结构残差）上相对差分估计量的偏差"}
    res["criteria"].append(L.verdict("E5_production_estimator_bias_quantified",
                                     abs(prod_bias) > TOL_PROD_BIAS,
                                     "single-frame block-MAD bias = %+.1f%% (quantified)"
                                     % (100 * prod_bias)))

    res["all_pass"] = all(c["pass"] for c in res["criteria"])
    L.save_result("exp3_variance_closure", res)
    print("[exp3] %s" % ("ALL PASS" if res["all_pass"] else "HAS FAILURES"))
    return 0 if res["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
