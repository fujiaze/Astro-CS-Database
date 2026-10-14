#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06-A：两对象冻结 SNR_ref(F_ref fixed) 与 SNR_scene(F_scene(x))，并给混用反例。

被估量（冻结）：
  A 参考对象   SNR_ref(x)   = F_ref / sqrt(V_ref(x))，  V_ref(x)   = V_slow(x) + F_ref·P(x)/g
  B 场景对象   SNR_scene(x) = F_scene(x) / sqrt(V_scene(x))，V_scene(x) = V_slow(x) + F_scene(x)·P(x)/g
  权重         w = SNR²/F_ref²，SNR 与 F_ref 必须同源（同一帧、同一参考口径）
单位：F [ADU]，V [ADU²]，SNR [1]，w [ADU⁻²]，g [e⁻/ADU]，P [1]（像素内 PSF 通量占比）。

反例：把「场景分子 + 参考分母」装进同一个 SNR 场（SNR_mix = F_scene/sqrt(V_ref)）后
再按 w = SNR²/F_ref² 出权，得到的不是任何被估量的逆方差；用它在场景通量被估量上定权
会产生可测的权重偏差与方差膨胀。

命令：python3 实验/dense-snr-reconstruct/code/s06/s06_a_two_objects.py
"""
from __future__ import annotations

import numpy as np

from s06_common import (F_REF, GAIN, SEED, E_eff, dump, rms_rel, run_meta,
                        signed_bias_rel, var_ratio)

SCRIPT = "s06_a_two_objects.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_a_two_objects.py"

W = H = 64
V_SLOW = 4.0        # 慢变方差（天光+读出+平场，不含源散粒项）[ADU²]
P_PSF = 0.6         # 像素内 PSF 通量占比 [1]


def scene_flux(W, H, peak):
    """场景通量 F_scene [ADU]：F_ref 平场 + 中心高斯亮源。"""
    y, x = np.mgrid[0:H, 0:W]
    g = np.exp(-((x - W / 2.0) ** 2 + (y - H / 2.0) ** 2) / (2 * 6.0 ** 2))
    return F_REF * (1.0 + (peak - 1.0) * g)


def v_scene(f_scene):
    return V_SLOW + f_scene * P_PSF / GAIN


def v_ref():
    return np.full((H, W), V_SLOW + F_REF * P_PSF / GAIN)


def pct(a):
    a = np.asarray(a, float)
    return {k: float(np.percentile(a, q)) for k, q in
            (("min", 0), ("p05", 5), ("median", 50), ("p95", 95), ("max", 100))}


def evaluate(world_name, f_scene):
    """在给定场景通量上比较四条权重臂；v_true = V_scene（场景通量被估量）。"""
    vr = v_ref()
    vs = v_scene(f_scene)
    arms = {
        "w_scene_matched": 1.0 / vs,                       # 正确配对
        "w_ref_fixed": 1.0 / vr,                           # 参考对象（错配）
        "w_mix_snr": (f_scene / F_REF) ** 2 / vr,          # 场景分子 + 参考分母（混装）
        "w_equal": np.full((H, W), 1.0),
    }
    # 最优合成 = 用正确逆方差对场景通量做加权平均
    opt = float(np.sum(f_scene / vs) / np.sum(1.0 / vs))
    out = {"world": world_name,
           "f_scene_peak_adu": float(f_scene.max()),
           "f_scene_dynamic_range": float(f_scene.max() / f_scene.min()),
           "v_scene_dynamic_range": float(vs.max() / vs.min()),
           "v_ref_adu2": float(vr[0, 0]),
           "optimal_composite_adu": opt,
           "arms": {}}
    for name, w in arms.items():
        comp = float(np.sum(w * f_scene) / np.sum(w))
        out["arms"][name] = {
            "var_ratio_holdout": var_ratio(w, vs),
            "E_eff": E_eff(w, vs),
            "composite_bias_rel": (comp - opt) / opt,
            "rms_rel_weight_vs_scene": rms_rel(w, arms["w_scene_matched"]),
            "weight_ratio_vs_matched": pct(w / arms["w_scene_matched"]),
        }
    # 混装场的隐含方差（1/w_mix）与被估量方差的比
    v_implied = 1.0 / arms["w_mix_snr"]
    out["mix_implied_variance_over_true"] = pct(v_implied / vs)
    out["ref_variance_over_true"] = pct(vr / vs)
    return out


worlds = {
    "zero_effect_negative_F_scene_eq_F_ref": scene_flux(W, H, 1.0),
    "injected_positive_bright_source_peak_1000": scene_flux(W, H, 1000.0),
    "intermediate_peak_10": scene_flux(W, H, 10.0),
}

payload = {
    **run_meta(SCRIPT, COMMAND),
    "estimand": {
        "A_reference_object": {
            "name": "SNR_ref(F_ref fixed)",
            "numerator": "F_ref,k [ADU] — 逐帧固定参考通量（冻结参考星等档 m_ref）",
            "denominator": "sqrt(V_ref,k(x)) [ADU] — 参考通量估计量的条件方差",
            "V_ref(x)": "V_slow(x) + F_ref·P(x)/g  [ADU²]",
            "output_unit": "dimensionless [1]",
            "geometry": "控制点/查询点均在帧像素坐标系（像素中心坐标，0 基）",
            "support": "层覆盖的 cell 并集；越界即 OUT_OF_DOMAIN_REJECTED（不外推）",
        },
        "B_scene_object": {
            "name": "SNR_scene(F_scene(x))",
            "numerator": "F_scene(x) [ADU] — 逐像素实际场景通量（位置相关）",
            "denominator": "sqrt(V_scene(x)) [ADU]",
            "V_scene(x)": "V_slow(x) + F_scene(x)·P(x)/g  [ADU²]",
            "output_unit": "dimensionless [1]",
            "geometry": "同 A（同一帧像素坐标系）",
            "support": "同 A",
        },
        "weight_identity": "w = SNR²/F_ref² = 1/V，仅当分子分母同源（同一帧、同一被估量）时成立",
        "prohibition": "两种口径不得混装成同一 SNR 场（D-07/U-04）",
    },
    "kernel_table": [
        {"operator": "natural_bicubic_spline_clip_v1",
         "input_quantity": "SNR 或 V 的节点值", "input_unit": "[1] 或 [ADU²]",
         "output_quantity": "逐像素重建值", "output_unit": "同输入",
         "geometry": "规则网格 cell_center_v1，可分离自然双三次样条",
         "support_domain": "cell 并集 [gox-0.5, gox+nx·dx-0.5]（最外半 cell 端点常数延拓）",
         "out_of_domain_state": "OUT_OF_DOMAIN_REJECTED（eval 失败，不外推、不回退帧级）",
         "value_range_clip": "[min(node), max(node)]，显式钳制"},
        {"operator": "bilinear_regular_grid_v1",
         "input_quantity": "SNR 或 V 的节点值", "input_unit": "[1] 或 [ADU²]",
         "output_quantity": "逐像素重建值", "output_unit": "同输入",
         "geometry": "规则网格 cell_center_v1，双线性",
         "support_domain": "同 cell 并集", "out_of_domain_state": "OUT_OF_DOMAIN_REJECTED",
         "value_range_clip": "无（对照/回退档）"},
        {"operator": "idw_p1", "input_quantity": "散点控制值", "input_unit": "[1] 或 [ADU²]",
         "output_quantity": "逐像素重建值", "output_unit": "同输入",
         "geometry": "散点坐标（像素或球面角距，须在 manifest 声明）",
         "support_domain": "显式 max_radius_px > 0 的并集；重合点 d<1e-10 精确取节点值",
         "out_of_domain_state": "NOT_APPLICABLE（缺 max_radius 即 fail-closed）",
         "value_range_clip": "无（备选口径，不在生产冻结词表内）"},
        {"operator": "idw_p2", "input_quantity": "散点控制值", "input_unit": "[1] 或 [ADU²]",
         "output_quantity": "逐像素重建值", "output_unit": "同输入",
         "geometry": "同 idw_p1", "support_domain": "同 idw_p1",
         "out_of_domain_state": "NOT_APPLICABLE",
         "value_range_clip": "无；p=2 属无噪/光滑极限口径（规范定位为备选）"},
        {"operator": "constant_frame_reconstruct", "input_quantity": "节点值的稳健中位数",
         "input_unit": "[1] 或 [ADU²]", "output_quantity": "全帧空间常数", "output_unit": "同输入",
         "geometry": "无空间信息", "support_domain": "全帧（无域外概念）",
         "out_of_domain_state": "NOT_APPLICABLE（无域外）", "value_range_clip": "无"},
        {"operator": "dense_caliber(MAD²+cell平面拟合+样条)",
         "input_quantity": "8×8 patch 1.4826·MAD 的平方 → cell 内平面拟合 → 节点方差",
         "input_unit": "[ADU²]", "output_quantity": "逐像素方差重建", "output_unit": "[ADU²]",
         "geometry": "生产配方（Δ=64、patch=8、node=cell 中心）",
         "support_domain": "同 cell 并集", "out_of_domain_state": "OUT_OF_DOMAIN_REJECTED",
         "value_range_clip": "样条段钳到节点值域"},
    ],
    "worlds": [evaluate(k, v) for k, v in worlds.items()],
}

# ---------------- 判据：零效应阴性必须归零、注入阳性必须判红 ----------------
neg = payload["worlds"][0]["arms"]
inject = payload["worlds"][1]["arms"]
tol_zero = 1e-12

gates = {
    "G_A1_zero_effect_matched_weight_exactly_optimal": {
        "observed": {"E_eff_w_scene": neg["w_scene_matched"]["E_eff"],
                     "E_eff_w_ref": neg["w_ref_fixed"]["E_eff"],
                     "bias_w_ref": neg["w_ref_fixed"]["composite_bias_rel"],
                     "rms_rel_w_ref_vs_matched": neg["w_ref_fixed"]["rms_rel_weight_vs_scene"]},
        "pass": bool(abs(neg["w_scene_matched"]["E_eff"]) <= tol_zero
                     and abs(neg["w_ref_fixed"]["E_eff"]) <= tol_zero
                     and abs(neg["w_ref_fixed"]["composite_bias_rel"]) <= tol_zero),
        "meaning": "F_scene ≡ F_ref 时两对象逐位重合 ⇒ 混用无效应（零效应阴性）",
    },
    "G_A2_injected_bright_source_misuse_detected_RED": {
        "observed": {"E_eff_w_ref": inject["w_ref_fixed"]["E_eff"],
                     "E_eff_w_mix": inject["w_mix_snr"]["E_eff"],
                     "E_eff_w_scene": inject["w_scene_matched"]["E_eff"],
                     "bias_w_ref": inject["w_ref_fixed"]["composite_bias_rel"],
                     "bias_w_mix": inject["w_mix_snr"]["composite_bias_rel"],
                     "mix_implied_variance_over_true": payload["worlds"][1]["mix_implied_variance_over_true"]},
        "pass": bool(inject["w_ref_fixed"]["E_eff"] > 1e-6
                     and inject["w_mix_snr"]["E_eff"] > 1e-6
                     and abs(inject["w_scene_matched"]["E_eff"]) <= 1e-9),
        "meaning": "亮源世界下参考对象/混装场的权重偏差与方差膨胀必须显著（注入阳性判红）",
    },
    "G_A3_mixed_field_is_not_any_estimand_inverse_variance": {
        "observed": {"mix_implied_over_true_min": payload["worlds"][1]["mix_implied_variance_over_true"]["min"],
                     "mix_implied_over_true_max": payload["worlds"][1]["mix_implied_variance_over_true"]["max"]},
        "pass": bool(payload["worlds"][1]["mix_implied_variance_over_true"]["max"] > 1.01
                     or payload["worlds"][1]["mix_implied_variance_over_true"]["min"] < 0.99),
        "meaning": "SNR_mix 的 1/w 既不等于 V_ref 也不等于 V_scene ⇒ 混装场不承载任何被估量的逆方差",
    },
}
payload["gates"] = gates
payload["all_pass"] = all(g["pass"] for g in gates.values())

dump("S06_A_two_objects.json", payload)
print(f"[S06-A] all_pass={payload['all_pass']}")
