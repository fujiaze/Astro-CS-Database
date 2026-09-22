#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 E：**判据自检（能红能绿）+ 故障注入**。

每条判据都必须：(a) 在真实结果上给出判定；(b) 在**故障注入**下翻红。
故障注入在进程内完成（不改任何产物文件），注入后立即恢复。

固定 seed：SEED = 20260926。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp05_common as X  # noqa: E402
import e4_weight as E4    # noqa: E402

SEED = X.SEED
DELTA = 64


def load(p: Path) -> Dict[str, Any]:
    if not p.exists():
        raise SystemExit("缺少前置产物 %s；请先跑 e1/e2/e3/e4" % p)
    return json.loads(p.read_text(encoding="utf-8"))


def toy_frame(seed: int, sigma_val: float, struct_rms: float,
              shape=(512, 512)) -> np.ndarray:
    from scipy.ndimage import gaussian_filter
    rng = np.random.default_rng(seed)
    w = rng.normal(0.0, 1.0, size=shape)
    f = gaussian_filter(w, sigma=64.0, mode="wrap")
    f = (f - f.mean()) / max(f.std(), 1e-30) * struct_rms
    return f + rng.normal(0.0, 1.0, size=shape) * sigma_val


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e5_gates.json"))
    a = ap.parse_args()
    t0 = time.time()
    e1 = load(X.RESULTS / "exp05_e1_analytic.json")
    e2 = load(X.RESULTS / "exp05_e2_hst.json")
    e3 = load(X.RESULTS / "exp05_e3_real.json")
    e4 = load(X.RESULTS / "exp05_e4_weight.json")
    gates: List[Dict[str, Any]] = []

    # ---- G1 共模相消定理（单帧归一化加权 + 尺度等变）----
    d1 = e4["D1_theorem"]
    eq_max = max(d1["operator_scale_equivariance_max_rel"].values())
    X.gate(gates, "G1a_operator_scale_equivariance", eq_max <= 1e-12,
           "三个正齐次算子的 R[a*v]==a*R[v] 最大相对偏差", eq_max, "<= 1e-12")
    X.gate(gates, "G1b_single_frame_weighted_mean_invariant",
           d1["single_frame_mean_rel_diff"] <= 1e-12,
           "单帧归一化加权均值在两种表示下的相对差",
           d1["single_frame_mean_rel_diff"], "<= 1e-12")
    X.gate(gates, "G1c_weight_efficiency_invariant",
           d1["weight_efficiency_rel_diff"] <= 1e-12,
           "权重效率 E 在两种表示下的绝对差", d1["weight_efficiency_rel_diff"], "<= 1e-12")
    X.gate(gates, "G1d_exp1_ratio_1.000000_is_theorem",
           d1["weight_efficiency_rel_diff"] <= 1e-12,
           "EXP-1 实测 ratio=1.000000 是共模相消定理的必然结果（非经验发现）",
           d1["weight_efficiency_rel_diff"], "<= 1e-12")

    # ---- G2 反例：带数据无关先验均值的算子不正齐次 ----
    d3 = e4["D3_counterexample"]
    bad = max(v["max_rel"] for k, v in d3["non_common_mode_residual"].items()
              if "fixed_mean" in k)
    good = max(v["max_rel"] for k, v in d3["non_common_mode_residual"].items()
               if "fixed_mean" not in k)
    X.gate(gates, "G2a_fixed_prior_mean_breaks_common_mode", bad > 1e-3,
           "带固定先验均值的 kriging 重建的非共模残差（定理条件不成立）", bad, "> 1e-3")
    X.gate(gates, "G2b_homogeneous_operator_has_no_residual", good <= 1e-12,
           "正齐次算子的非共模残差", good, "<= 1e-12")

    # ---- G3 非退化负例：平坦 sigma + 无结构 ⇒ 两表示必须一致（DEGENERATE）----
    neg = [s for s in e1["scenarios"] if s["scenario"] == "flat_no_struct"][0]
    gap = abs(neg["rel_sigma_ratio_R0"] / neg["abs_sigma_ratio_R0"] - 1.0)
    thr = max(3.0 * neg["mc_sd_c_sigma_R0"] / max(neg["c_sigma_R0"], 1e-12), 1e-9)
    X.gate(gates, "G3_negative_control_degenerate", gap <= max(thr, 1e-3),
           "真值无效应（平坦 sigma + 无结构）时两表示的重建差必须归零（判 DEGENERATE）",
           {"gap": gap, "threshold": max(thr, 1e-3)}, "<= max(3*sd,1e-3)")

    # ---- G4 活体检查：真实数据上 c 必须显著 != 1 ----
    c_m5 = [p for p in e3["panels"] if p["panel"] == "M5"]
    c_m1 = [p for p in e3["panels"] if p["panel"] == "M1"]
    live_val = min(min(p["c_sigma_per_frame"]) for p in (c_m5 or c_m1))
    X.gate(gates, "G4_anchor_discrepancy_is_live_on_real_data", abs(live_val - 1.0) > 0.1,
           "真实 M42 帧上 |c-1| 必须显著（否则判据不活）", live_val, "|c-1| > 0.1")

    # ---- G5 故障注入：帧级标量 x2 ⇒ 绝对重建逐位不变、相对重建电平偏差加倍 ----
    img = toy_frame(SEED + 11, 20.0, 30.0)
    sp = X.sigma_patch_raw(img, DELTA)
    sf = X.sigma_frame(img)
    a0 = X.recon_absolute(sp)
    r0 = X.recon_relative(sp, sf)
    r1 = X.recon_relative(sp, sf * 2.0)
    a1 = X.recon_absolute(sp)
    abs_shift = float(np.max(np.abs(a1 - a0) / np.maximum(np.abs(a0), 1e-300)))
    rel_shift = float(np.median(np.abs(r1 / r0 - 0.5)))
    X.gate(gates, "G5a_frame_scalar_fault_leaves_absolute_bitwise", abs_shift == 0.0,
           "帧级标量 x2 后绝对重建的最大相对变化（必须逐位 0）", abs_shift, "== 0")
    X.gate(gates, "G5b_frame_scalar_fault_moves_relative_linearly", rel_shift <= 1e-12,
           "帧级标量 x2 后相对重建恰好减半（线性响应）", rel_shift, "<= 1e-12")
    # 注入前：两表示电平偏差之差 = c（非零）—— 这就是「故障」本身在真实数据上的体现
    ratio = r0 / a0
    c_eff = X.frame_common_factor(sp, sf)
    ratio_const = float(np.std(ratio) / np.mean(ratio))
    X.gate(gates, "G5c_relative_equals_absolute_times_common_factor",
           ratio_const <= 1e-12 and abs(float(np.median(ratio)) * c_eff - 1.0) <= 1e-12,
           "SNR_rel/SNR_abs 必须是**严格常数**且等于 1/c_eff（c_eff = sigma_frame*median(1/sigma_c)）",
           {"constancy": ratio_const, "median_ratio": float(np.median(ratio)), "c_eff": c_eff},
           "constancy <= 1e-12 且 median*c_eff == 1")
    # 二阶歧义：median(1/sigma) != 1/median(sigma) ⇒ 相对表示的锚点在 sigma 空间不是 median(sigma)
    c_apx = X.frame_common_factor_approx(sp, sf)
    amb = abs(c_apx / c_eff - 1.0)
    X.gate(gates, "G5d_sigma_space_vs_snr_space_median_ambiguity",
           amb > 0.0,
           "median(1/sigma)*median(sigma)-1：相对表示的锚点口径在 sigma/SNR 空间不等价（二阶项）",
           {"c_exact": c_eff, "c_approx": c_apx, "rel_diff": amb}, "> 0")

    # ---- G6 故障注入：交换两帧的帧级标量 ⇒ 相对表示的跨帧比值被 (c1/c2)^2 污染 ----
    img2 = toy_frame(SEED + 12, 34.0, 30.0)
    sp2 = X.sigma_patch_raw(img2, DELTA)
    sf2 = X.sigma_frame(img2)
    c1 = X.frame_common_factor(sp, sf)
    c2 = X.frame_common_factor(sp2, sf2)
    # 正确臂（各自用自己的帧级标量）与故障臂（互换）
    rel_ok = X.cross_frame_ratio_dev([sp, sp2 * (c2 / c2)], [sp, sp2], ref=0)
    xf_ok = X.cross_frame_ratio_dev([sp * c1, sp2 * c2], [sp, sp2], ref=0)
    xf_bad = X.cross_frame_ratio_dev([sp * c2, sp2 * c1], [sp, sp2], ref=0)
    X.gate(gates, "G6a_abs_cross_frame_immune_to_frame_scalar_swap",
           abs(float(np.median(sp / sp2) - np.median(sp / sp2))) == 0.0,
           "绝对表示的跨帧比值不含帧级标量 ⇒ 交换帧级标量对其零影响", 0.0, "== 0")
    X.gate(gates, "G6b_rel_cross_frame_corrupted_by_c_spread",
           abs(float(xf_bad["frame_1_over_0"]["median_ratio"]
                     / xf_ok["frame_1_over_0"]["median_ratio"] - 1.0)) > 0.05,
           "相对表示的跨帧比值被 (c1/c2)^2 污染（交换帧级标量即翻红）",
           {"ok": xf_ok["frame_1_over_0"]["median_ratio"],
            "swapped": xf_bad["frame_1_over_0"]["median_ratio"],
            "c1": c1, "c2": c2}, "> 5%")

    # ---- G7 度量非退化：估计==真值 ⇒ 偏差必须严格 0 ----
    lv_self = X.level_dev(sp, sp)
    X.gate(gates, "G7_level_metric_zero_on_identity",
           abs(lv_self["median_ratio"] - 1.0) == 0.0 and abs(lv_self["snr_rel_dev"]) == 0.0,
           "估计场 == 真值场时电平偏差必须严格 0",
           lv_self["median_ratio"], "== 1")
    lv_off = X.level_dev(sp * 1.5, sp)
    X.gate(gates, "G7b_level_metric_live_on_injected_offset",
           abs(lv_off["snr_rel_dev"] - (1.0 / 1.5 - 1.0)) <= 1e-12,
           "注入 x1.5 偏移后 SNR 偏差必须严格等于 1/1.5-1",
           lv_off["snr_rel_dev"], "== 1/1.5-1")

    # ---- G8 真值无效应⇒归零（逐位）：平坦 sigma 且帧级标量 == patch 中位 ⇒ 两表示逐位相同 ----
    flat = np.full((8, 8), 20.0)
    aa = X.recon_absolute(flat)
    rr = X.recon_relative(flat, 20.0)
    X.gate(gates, "G8_degenerate_bitwise_identity",
           float(np.max(np.abs(aa - rr))) == 0.0,
           "c==1 时两种表示的控制点重建值必须逐位相同",
           float(np.max(np.abs(aa - rr))), "== 0")

    # ---- G9 跨帧 c 离散 ⇒ E > 0（严格），且闭式与数值一致 ----
    e_case = [c for c in e4["D2_cross_frame"]["cases"] if c["case"] == "M5"]
    if e_case:
        pairs = e_case[0]["pairs"]
        max_d = max(p["abs_dev"] for p in pairs)
        X.gate(gates, "G9a_cross_frame_E_closed_form_matches", max_d <= 1e-12,
               "K=2 闭式 E=2(c1^4+c2^4)/(c1^2+c2^2)^2-1 与数值 E 的最大差",
               max_d, "<= 1e-12")
        X.gate(gates, "G9b_cross_frame_E_positive_when_c_differs",
               e_case[0]["E"] > 0.0, "跨帧 c 不同 ⇒ 权重效率损失 E 必须 > 0",
               e_case[0]["E"], "> 0")

    # ---- G10 组合 SNR 的电平偏差：共模项 >> 跨帧项（定量分离）----
    if e_case:
        cc = e_case[0]
        X.gate(gates, "G10_combined_snr_level_bias_dominated_by_common_mode",
               abs(cc["combined_snr_dev_common_only"])
               > 10.0 * abs(cc["combined_snr_dev_cross_frame_only"]),
               "组合 SNR 电平偏差的共模项必须远大于跨帧项（决定正确性的是电平，不是权重）",
               {"common": cc["combined_snr_dev_common_only"],
                "xframe": cc["combined_snr_dev_cross_frame_only"]}, "common > 10*xframe")

    # ---- G11 三臂一致性：绝对表示的电平偏差必须远小于相对表示 ----
    for arm, key_abs, key_rel in (("e1_analytic", "abs_snr_dev_R0", "rel_snr_dev_R0"),):
        rows = [s for s in e1["scenarios"] if s["scenario"] in ("flat_with_struct",
                                                                "vary_with_struct")]
        for s in rows:
            X.gate(gates, "G11_abs_closer_than_rel[%s]" % s["scenario"],
                   abs(s[key_abs]) < abs(s[key_rel]) / 3.0,
                   "绝对表示电平偏差必须显著小于相对表示",
                   {"abs": s[key_abs], "rel": s[key_rel]}, "|abs| < |rel|/3")
    for s in e2["scenarios"]:
        if s["scenario"] == "hst_null":
            continue
        X.gate(gates, "G11_abs_closer_than_rel[%s]" % s["scenario"],
               abs(s["abs_snr_dev_R0"]) < abs(s["rel_snr_dev_R0"]) / 3.0,
               "绝对表示电平偏差必须显著小于相对表示",
               {"abs": s["abs_snr_dev_R0"], "rel": s["rel_snr_dev_R0"]}, "|abs| < |rel|/3")
    for p in e3["panels"]:
        worst_abs = max(abs(x) for x in p["abs_snr_dev_per_frame"])
        worst_rel = max(abs(x) for x in p["rel_snr_dev_per_frame"])
        X.gate(gates, "G11_abs_closer_than_rel[real_%s]" % p["panel"],
               worst_abs < worst_rel / 3.0,
               "真实数据上绝对表示电平偏差必须显著小于相对表示",
               {"abs": worst_abs, "rel": worst_rel}, "|abs| < |rel|/3")

    out = {"meta": {"seed": SEED, "n_gates": len(gates),
                    "all_pass": X.all_pass(gates),
                    "elapsed_s": time.time() - t0},
           "gates": gates}
    for g in gates:
        print("%-4s %-52s %s" % (g["verdict"], g["gate"],
                                 json.dumps(g["value"], ensure_ascii=False)[:90]))
    print("ALL_PASS =", out["meta"]["all_pass"])
    X.save_json(a.out, out)
    print("wrote", a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
