#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7-EXP4 —— **散差扫描**：SP-0 判据在"帧间空间形状差异"下的表现（§9.46）。

判据的代数（SP-0，与 实验/absolute-snr/docs/frame-snr-canon.md 一致）
-----------------------------------------------------------------
    逐帧逐像素逆方差权重   w_f(p) = 1/var_f(p)
    帧级标量权重           W_f = 1 / mean_p(var_f(p))
    权重误差               delta_f(p) = W_f/w_f(p) - 1 = W_f*var_f(p) - 1
    标量权重代替空间权重的代价
        R_SP0(p) = [sum_f W_f^2/w_f(p)] * [sum_f w_f(p)] / (sum_f W_f)^2
    形状差异度量           D_shape = median_p std_f( delta_f(p) )

**关键性质（判据基础）**：若各帧形状相同 var_f(p) = A_f·s(p)（A_f 为任意正**缩放**），
则 W_f ∝ 1/A_f、delta_f(p) = s(p)/mean(s) − 1 **与 f 无关** ⇒ std_f(delta_f) = 0
且 R_SP0 ≡ 1 **精确成立**。⇒ SP-0 量的是**形状**差异，**不是水平**差异。

五个臂（全部在真实 HST M16 数据上；**先给负例再看绿例**）
    A 共模（同波段、同参数，只换噪声种子）          —— **负例**：R=1 精确、D=0
    B 纯方差缩放（同波段，天光 ×1/×3/×10）          —— **负例**：在**扣除真实底贡献**的受控图上 R=1、D→0
    C 三个真实波段（Hα/[S II]/[O III]，同 WCS 网格）—— 受控图上**零结果**；原始图上**是正例**（见下）
    D 三个波段 + **逐帧不同平场种子 + 不同天空梯度** —— **绿例**：模拟不同指向/不同探测器位置
    E 同波段 + **逐帧不同月光光晕位置**              —— **绿例**：模拟不同指向的天光空间结构

**A4 臂设计修订（本轮前台裁决后实施；阈值一字未改）**
-----------------------------------------------------------------
M16-SCENE-FIX-001 前，真实底被 ×t 缺陷衰减到 ~10^0–10^1 e- ⇒「真实底对帧方差无贡献」，
于是 B 臂近似"纯缩放 + 加性底"、C 臂三波段近似同形。**修复后真实底成为真实源项**：
其泊松方差 base_var(p) = src_e(p)/g² **逐像素变化、不随天光缩放、且三波段各不相同**
⇒ 两个"负例"的前提被推翻（原始图实测 B: R−1=4.4e-2、D=1.6e-1；C: R−1=3.9e-2、D=8.3e-2）。

修订 = **在扣除/控制真实底贡献后**检验形状不变性，而不是放宽判据：
    受控真值方差图  var_ctrl(p) = var_true(p) − base_var(p)
                            = [sky_f·t + dark·t]·m(p)/g² + sigma_R²/g² + 1/12
（本实验 inject_stars.n = 0 ⇒ Frame.src_e 恰为真实底的期望电子数，扣除量是**独立可得**的物理项，
  不是拟合出来的修正。脚本内 assert 保证这一点。）
受控图上"纯天光缩放"重新严格成立（残余只来自**加性噪声底** c = (sigma_R²+dark·t)/g² + 1/12，
其形状影响是 O(c/A_f)）⇒ **P2/P3 的阈值原样不变**。
原始图（未扣除）的结果**逐臂保留**为 *_RAW_RETIRED（已知偏差），不隐藏、不删。

**噪声制度（A4 前台裁决 -> STACKN32-001 全局统一）**：stack_n 1 -> 32（真实 drz 是 NDRIZIM=32 的
子曝光合成品 ⇒ 叠加等效读出噪声 sqrt(32)×3.1 = 17.536 e-，加性噪声底 c = (σ_R² + dark·t)/g² + 1/12 升到
**145.29 ADU²**（t = 9600 s；t = 14400/16000 s 时 149.56 / 150.99）。**订正（STACKN32-001）**：
本文件原写 "c = σ_R²/g² + 1/12 = 145.3" —— **值对、公式标签漏了 dark·t 项**
（σ_R²/g²+1/12 = 136.759，不是 145.3）。本场景（m16_band_matrix）三帧 t 不同 ⇒ c 也逐帧不同。
见 data/STACKN32/STACKN32-001.md §5.4。
**当轮**以 overrides 方式实施、刻意不改场景 JSON 默认值（避免把 A6/P9/交付数据集的变化混进来）；
**STACKN32-001（2026-09-20）已把全局默认值统一为 32** ⇒ 本 override 现为**幂等冗余**。
本脚本数字在统一前后**逐位相同**（已实测）。

**实测层**：方差图由配对差分 (I1−I2)²/2 得到，**必须先平滑**（31×31）再做 SP-0，
否则单像素 χ²₁ 估计噪声本身就会造出巨大的假形状差异（首版即因此得到 R≈4 的假绿）。

判据（写死，不事后放宽）
    P1 A 臂 |R−1| < 1e-12 且 D_shape < 1e-12（真值层，原始图）
    P2 B 臂 |R−1| < 1e-6 且 D_shape < 1e-3（真值层，**受控图**）
    P3 C 臂 |R−1| < 0.005 且 D_shape < 0.01（真值层，**受控图**，"共模平场 + 受控底 ⇒ 无形状差异"）
    P4 D 臂 R_SP0_median > 1.005 且 D_shape > 0.05（绿，真值层）
    P5 E 臂 R_SP0_median > 1.005 且 D_shape > 0.05（绿，真值层）
    P6 可从帧本身测得：实测层 D 臂 R > 1.005 且 > A 臂实测 R + 0.005
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

SEED = 20260924
SCENE = "m16_band_matrix"
# A4 前台裁决：叠加等效读出噪声 sqrt(32) x 3.1 = 17.536 e-
STACK_N = 32


def render_one(frame_index, seed, *, flat_seed=None, sky=None):
    ov = {"stack_n": STACK_N, "inject_stars": {"n": 0}, "mode": NM.MODE_PHYSICAL}
    if flat_seed is not None:
        ov["flat"] = {"seed": int(flat_seed)}
    if sky is not None:
        ov["sky"] = sky
    return P.render_scene_frame(SCENE, seed=seed, frame_index=frame_index, overrides=ov)


def base_poisson_var_adu2(frame, det):
    """真实底的泊松方差贡献 base_var(p) = src_e(p)/g² [ADU²]。

    **前提**：本实验 inject_stars.n = 0 ⇒ Frame.src_e 恰为真实底的期望电子数
    （expose: src_e = t·src_rate·m，src_rate = base_rate）。调用处有 assert。
    """
    return np.asarray(frame.src_e, dtype=float) / (det.gain_e_per_adu ** 2)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(P.P7_DATA / "exp4_scatter_sp0.json"))
    a = ap.parse_args()
    res = {"experiment": "P7-EXP4 SP-0 scatter scan",
           "criterion": "SP-0 (实验/absolute-snr/docs/frame-snr-canon.md)",
           "scene": SCENE, "smooth_px": 31, "stack_n": STACK_N,
           "stack_n_note": ("A4 前台裁决：stack_n 1 -> 32（叠加等效读出噪声 17.536 e-）；"
                            "stack_n=1 交付版备份见 data/P7/before_stackn32/exp4_scatter_sp0.json"),
           "arm_revision": ("A4 臂设计修订：B/C 臂的判据改在**扣除真实底贡献**的受控真值方差图上"
                            "（var_ctrl = var_true − src_e/g²），阈值一字未改；原始图结果保留为 *_RAW_RETIRED。"),
           "arms": {}}

    def do_arm(name, spec):
        """spec: list of dict(frame_index, label, seed, flat_seed, sky)"""
        frames, valids, dets, var_true, var_ctrl = [], [], [], [], []
        for sp in spec:
            fr, tr, vv, scfg = render_one(sp["frame_index"], sp["seed"],
                                          flat_seed=sp.get("flat_seed"), sky=sp.get("sky"))
            # 受控扣除的前提：帧里没有注入星 ⇒ src_e 就是真实底的期望电子数
            assert not tr.get("stars_in_frame"), "controlled arm requires inject_stars.n = 0"
            frames.append(fr); valids.append(vv); dets.append(P.det_of(scfg))
            var_true.append(P.truth_variance_adu2(fr, dets[-1]))
            var_ctrl.append(var_true[-1] - base_poisson_var_adu2(fr, dets[-1]))
        s = P.sp0_ratio(np.stack(var_true))
        sc = P.sp0_ratio(np.stack(var_ctrl))
        # 实测层：每帧再渲一次（不同种子）配对差分 -> 平滑 -> SP-0
        from scipy.ndimage import uniform_filter
        vmaps = []
        for i, sp in enumerate(spec):
            fr2, _, _, _ = render_one(sp["frame_index"], sp["seed"] + 7777,
                                      flat_seed=sp.get("flat_seed"), sky=sp.get("sky"))
            vm, _ = P.paired_var_adu2(frames[i].adu, fr2.adu, mask=valids[i])
            vmaps.append(np.maximum(uniform_filter(vm, size=31, mode="nearest"), 1e-9))
        sm = P.sp0_ratio(np.stack(vmaps))
        frac = [float(np.median(base_poisson_var_adu2(frames[i], dets[i])[valids[i]]
                                / var_true[i][valids[i]])) for i in range(len(spec))]
        return {"labels": [sp["label"] for sp in spec],
                "flat_seeds": [sp.get("flat_seed") for sp in spec],
                "truth_layer": {k: v for k, v in s.items() if k != "R_SP0_map"},
                "truth_layer_base_deducted": {k: v for k, v in sc.items() if k != "R_SP0_map"},
                "measured_layer": {k: v for k, v in sm.items() if k != "R_SP0_map"},
                "base_poisson_var_fraction_median": frac,
                "var_true_frame_median": [float(np.median(v)) for v in var_true],
                "var_ctrl_frame_median": [float(np.median(v)) for v in var_ctrl],
                "var_true_frame_std_over_median": [float(np.std(v) / np.median(v)) for v in var_true]}

    # A 共模
    res["arms"]["A_common_mode"] = do_arm("A", [
        {"frame_index": 0, "label": "ha_a", "seed": SEED + 1},
        {"frame_index": 0, "label": "ha_b", "seed": SEED + 2},
        {"frame_index": 0, "label": "ha_c", "seed": SEED + 3}])
    # B 纯方差缩放（同帧、同平场，只改天光水平）
    res["arms"]["B_scale_only"] = do_arm("B", [
        {"frame_index": 0, "label": "ha_s1", "seed": SEED + 11},
        {"frame_index": 0, "label": "ha_s3", "seed": SEED + 12,
         "sky": {"mode": "const", "level_e_per_s": 0.5, "grad_x_e_per_s": 0.0}},
        {"frame_index": 0, "label": "ha_s10", "seed": SEED + 13,
         "sky": {"mode": "const", "level_e_per_s": 1.8, "grad_x_e_per_s": 0.0}}])
    # C 三真实波段（同一平场、各波段自身天光基座）
    res["arms"]["C_three_bands"] = do_arm("C", [
        {"frame_index": 0, "label": "band_ha", "seed": SEED + 21},
        {"frame_index": 1, "label": "band_sii", "seed": SEED + 22},
        {"frame_index": 2, "label": "band_oiii", "seed": SEED + 23}])
    # D 三波段 + 逐帧不同平场种子 + 不同天空梯度（模拟不同指向/探测器位置）
    res["arms"]["D_diff_pointing_like"] = do_arm("D", [
        {"frame_index": 0, "label": "ha_p1", "seed": SEED + 31, "flat_seed": 101,
         "sky": {"mode": "const", "level_e_per_s": 0.30, "grad_x_e_per_s": 0.25,
                 "theta_deg": 0.0}},
        {"frame_index": 1, "label": "sii_p2", "seed": SEED + 32, "flat_seed": 202,
         "sky": {"mode": "const", "level_e_per_s": 0.10, "grad_x_e_per_s": 0.25,
                 "theta_deg": 120.0}},
        {"frame_index": 2, "label": "oiii_p3", "seed": SEED + 33, "flat_seed": 303,
         "sky": {"mode": "const", "level_e_per_s": 0.16, "grad_y_e_per_s": 0.25,
                 "theta_deg": 240.0}}])
    # E 同波段 + 逐帧不同月光光晕位置
    res["arms"]["E_moon_halo_moving"] = do_arm("E", [
        {"frame_index": 0, "label": "moon_a", "seed": SEED + 41,
         "sky": {"mode": "const", "level_e_per_s": 0.20, "moon_halo_e_per_s": 0.60,
                 "moon_center": [150.0, 150.0], "moon_scale_px": 380.0}},
        {"frame_index": 0, "label": "moon_b", "seed": SEED + 42,
         "sky": {"mode": "const", "level_e_per_s": 0.20, "moon_halo_e_per_s": 0.60,
                 "moon_center": [870.0, 300.0], "moon_scale_px": 380.0}},
        {"frame_index": 0, "label": "moon_c", "seed": SEED + 43,
         "sky": {"mode": "const", "level_e_per_s": 0.20, "moon_halo_e_per_s": 0.60,
                 "moon_center": [500.0, 900.0], "moon_scale_px": 380.0}}])

    def T(k): return res["arms"][k]["truth_layer"]
    def TC(k): return res["arms"][k]["truth_layer_base_deducted"]
    def M(k): return res["arms"][k]["measured_layer"]
    res["criteria"] = {
        "P1_A": {"R": T("A_common_mode")["R_SP0_median"], "D": T("A_common_mode")["D_shape_median"]},
        "P1_pass": bool(abs(T("A_common_mode")["R_SP0_median"] - 1) < 1e-12
                        and T("A_common_mode")["D_shape_median"] < 1e-12),
        # --- P2/P3：判据在**扣除真实底贡献**的受控图上（阈值一字未改） ---
        "P2_B_base_deducted": {
            "R": TC("B_scale_only")["R_SP0_median"], "D": TC("B_scale_only")["D_shape_median"],
            "var_ratios": [float(x / res["arms"]["B_scale_only"]["var_ctrl_frame_median"][0])
                           for x in res["arms"]["B_scale_only"]["var_ctrl_frame_median"]],
            "base_var_fraction_median": res["arms"]["B_scale_only"]["base_poisson_var_fraction_median"]},
        "P2_pass": bool(abs(TC("B_scale_only")["R_SP0_median"] - 1) < 1e-6
                        and TC("B_scale_only")["D_shape_median"] < 1e-3),
        "P2_note": ("受控图 var_ctrl = var_true − src_e/g² = [sky_f·t + dark·t]·m(p)/g² + c，"
                    "c = (sigma_R² + dark·t)/g² + 1/12 "
                    "= (307.520 + 0.002*t)/2.25 + 1/12 = 145.29 / 149.56 / 150.99 ADU² "
                    "（stack_n=32；本场景 m16_band_matrix 三帧 t = 9600/14400/16000 s）。"
                    "[CORRECTED by STACKN32-001: 原文写作 \"c = sigma_R²/g² + 1/12 = 145.3 ADU²\" —— "
                    "**值对（t=9600 帧）、公式标签漏了 dark·t 项**：sigma_R²/g² + 1/12 = 136.759 ADU²。"
                    "同理 P7 报告 §6.4 的 \"c 由 4.36 → 145.3（×33）\" 两端定义不一致"
                    "（4.36 是不含 dark 的 stack_n=1 值）：同口径比值为 **×31.4**（不含 dark）或 "
                    "**×11.3**（含 dark，t=9600）。详见 data/STACKN32/STACKN32-001.md §5.4。] "
                    "纯天光缩放 ⇒ A_f·m(p) + c，"
                    "残余形状差异 = O(c/A_f) ⇒ D ~ 7e-4，"
                    "**这是真实物理（加性噪声底），不是判据失效**；严格 D=0 只出现在逐位相同的帧（A 臂）。"),
        "P2_B_RAW_RETIRED": {
            "R": T("B_scale_only")["R_SP0_median"], "D": T("B_scale_only")["D_shape_median"],
            "note": ("[RETIRED] 未扣除真实底的原始图：真实底泊松方差 base_var(p) 逐像素变化且"
                     "不随天光缩放 ⇒ 前提「真实底对帧方差无贡献」被 M16-SCENE-FIX-001 推翻。"
                     "该值保留为**已知偏差**，不是判据。")},
        "P3_C_base_deducted": {
            "R": TC("C_three_bands")["R_SP0_median"], "D": TC("C_three_bands")["D_shape_median"],
            "base_var_fraction_median": res["arms"]["C_three_bands"]["base_poisson_var_fraction_median"]},
        "P3_pass": bool(abs(TC("C_three_bands")["R_SP0_median"] - 1) < 0.005
                        and TC("C_three_bands")["D_shape_median"] < 0.01),
        "P3_note": ("受控图上三波段的方差形状差异只剩 c/A_f 项（各波段天光基座不同）"
                    "⇒ 「共模平场 + 受控底 ⇒ 无形状差异」在受控图上成立。"),
        "P3_C_RAW_RETIRED": {
            "R": T("C_three_bands")["R_SP0_median"], "D": T("C_three_bands")["D_shape_median"],
            "note": ("[RETIRED] 原始图上三波段**确实不同形**（真实物理结果：Hα 亮丝状星云 / "
                     "[S II] 暗 / [O III] 平坦弥散）；原「零结果」结论作废（见报告 B27）。"
                     "该值保留为**已知偏差/正例登记**，不是判据。")},
        "P4_D": {"R": T("D_diff_pointing_like")["R_SP0_median"],
                 "D": T("D_diff_pointing_like")["D_shape_median"]},
        "P4_pass": bool(T("D_diff_pointing_like")["R_SP0_median"] > 1.005
                        and T("D_diff_pointing_like")["D_shape_median"] > 0.05),
        "P5_E": {"R": T("E_moon_halo_moving")["R_SP0_median"],
                 "D": T("E_moon_halo_moving")["D_shape_median"]},
        "P5_pass": bool(T("E_moon_halo_moving")["R_SP0_median"] > 1.005
                        and T("E_moon_halo_moving")["D_shape_median"] > 0.05),
        "P6_measured": {k: {"R": M(k)["R_SP0_median"], "D": M(k)["D_shape_median"]}
                        for k in res["arms"]},
        "P6_pass": bool(M("D_diff_pointing_like")["R_SP0_median"] > 1.005
                        and M("D_diff_pointing_like")["R_SP0_median"]
                        > M("A_common_mode")["R_SP0_median"] + 0.005),
    }
    P.dump_json(a.out, res)
    for k in ("A_common_mode", "B_scale_only", "C_three_bands",
              "D_diff_pointing_like", "E_moon_halo_moving"):
        t, tc, m = T(k), TC(k), M(k)
        print("[exp4] %-22s raw R=%.9f D=%.5f | base-deducted R=%.9f D=%.5f | measured R=%.5f D=%.5f"
              % (k, t["R_SP0_median"], t["D_shape_median"],
                 tc["R_SP0_median"], tc["D_shape_median"],
                 m["R_SP0_median"], m["D_shape_median"]))
    c = res["criteria"]
    print("[exp4] P1=%s P2=%s P3=%s P4=%s P5=%s P6=%s"
          % (c["P1_pass"], c["P2_pass"], c["P3_pass"], c["P4_pass"], c["P5_pass"], c["P6_pass"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
