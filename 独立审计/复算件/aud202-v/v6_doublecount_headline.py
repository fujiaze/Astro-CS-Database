#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD202-V / V6：双计头条数字是否取自单次 MC 实现值 + 最大偏差是否有门

(1) 我自己独立推的闭式双计偏差（不抄实验推导）
(2) 从 b2_noise_terms.json 取"同一物理点"在各扫描轴上的 MC 值
(3) 核 gates 里 max|meas-pred| 是否被任何布尔门约束
"""
import json
import math
import os
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
K_M4_FWHM = 1.230310


def moffat4_normalized(sigma_px, half):
    a2 = 2.0 * sigma_px * sigma_px
    v = []
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            v.append((1.0 + (i * i + j * j) / a2) ** -4)
    s = sum(v)
    return [x / s for x in v]


def sigma_f_from(P, var):
    """sigma_F = [Σ P_i^2 / var_i]^{-1/2}（Horne 最优提取，我按定义自建）。"""
    return 1.0 / math.sqrt(sum(p * p / vv for p, vv in zip(P, var)))


def my_analytic_doublecount(F_e, B_e, D_e, RN_e, g, sigma_px, half):
    """基准点闭式：电子域 CCD 方程 -> ADU^2 逐像素方差。
       正确臂： var_i = (B+D+RN^2 + F*P_i)/g^2
       双计臂： 经验总 rms（已含 RN）再 + (RN/g)^2
              => var_i' = var_i + RN^2/g^2
       偏差 = sigma_F(var')/sigma_F(var) - 1。"""
    P = moffat4_normalized(sigma_px, half)
    var_ok = [(B_e + D_e + RN_e ** 2 + F_e * p) / g ** 2 for p in P]
    var_dc = [v + RN_e ** 2 / g ** 2 for v in var_ok]
    return sigma_f_from(P, var_dc) / sigma_f_from(P, var_ok) - 1.0


def main():
    path = os.path.join(REPO, "实验", "absolute-snr", "results", "b2_noise_terms.json")
    d = json.load(open(path, encoding="utf-8"))
    base = d["frozen_config"]["base"]
    print("冻结基准点（JSON frozen_config.base）：", json.dumps(base))
    F, B, D, RN, g, sig, half = (base["F_e"], base["sky_e_per_px"], base["dark"],
                                 base["rn"], base["gain"], base["sigma_psf"], base["half"])
    print()
    print("(1) 我独立推的闭式双计偏差 @ 基准点")
    mine = my_analytic_doublecount(F, B, D, RN, g, sig, half)
    print("    我的式子：bias = sqrt(ΣP²/var_ok / ΣP²/(var_ok+RN²/g²)) - 1"
          " = %.6f  (%.3f%%)" % (mine, 100 * mine))
    print("    退化上限核对：源项=0 时 bias -> sqrt(1+RN²/(B+D)) - 1 = %.6f"
          % (math.sqrt(1 + RN ** 2 / (B + D)) - 1))
    print()
    print("(2) 同一物理点在各扫描轴上的记录（轴值=基准值的那一行）")
    print("    %-18s %-10s %-14s %-14s %-14s" %
          ("axis", "axis_value", "bias_empirical_rn(MC)", "pred_doublecount(闭式)",
           "z_empirical_rn_vs_mc"))
    same_point = []
    for axis, rows in d["scans"].items():
        key = {"read_noise_e": "rn", "dark_e_per_px": "dark_e_per_px",
               "psf_sigma_px": "sigma_psf", "gain_e_per_adu": "gain_e_per_adu",
               "source_flux_e": "F_e"}[axis]
        want = {"read_noise_e": RN, "dark_e_per_px": D, "psf_sigma_px": sig,
                "gain_e_per_adu": g, "source_flux_e": F}[axis]
        for r in rows:
            if abs(float(r["axis_value"]) - float(want)) <= 1e-12:
                print("    %-18s %-10.4g %-20.6f %-20.6f %-14.3f" %
                      (axis, r["axis_value"], r["bias_empirical_rn"],
                       r["pred_doublecount_bias"], r["z_empirical_rn_vs_mc"]))
                same_point.append((axis, r["bias_empirical_rn"], r["pred_doublecount_bias"]))
    mc = [x[1] for x in same_point]
    pr = [x[2] for x in same_point]
    if mc:
        print("    ⇒ 同一物理点 MC 值跨轴极差 = %.2f 个百分点 (min %.2f%% @ max %.2f%%)"
              % (100 * (max(mc) - min(mc)), 100 * min(mc), 100 * max(mc)))
        print("    ⇒ 闭式值跨轴极差 = %.3e（应恒 0：它不含随机数）" % (max(pr) - min(pr)))
        print("    ⇒ MC 均值 = %.4f%% vs 闭式 = %.4f%%" % (100 * sum(mc) / len(mc), 100 * pr[0]))
    print()
    print("(3) 门核对：max|meas-pred| 是否被约束")
    gates = d["gates"]
    for k in sorted(gates):
        if "G4c" in k or "doublecount" in k.lower() or "FINDING" in k:
            print("    %-52s = %s" % (k, json.dumps(gates[k])))
    bools = {k: v for k, v in gates.items() if isinstance(v, bool)}
    print("    全部布尔门：", sorted(bools.keys()))
    print("    含 'pred_vs_measured' 的布尔门：",
          [k for k in bools if "pred_vs_measured" in k] or "无")
    dmax = gates.get("G4c_pred_vs_measured_mc_max_abs_diff")
    print("    G4c_pred_vs_measured_mc_max_abs_diff = %s（**纯记录值，无对应布尔门**）"
          % (dmax if dmax is None else "%.6f (= %.2f pp)" % (dmax, 100 * dmax)))
    print()
    print("(4) 全扫描点上 MC 与闭式的偏差分布（我按 JSON 重算）")
    diffs = []
    for axis, rows in d["scans"].items():
        for r in rows:
            diffs.append((abs(r["bias_empirical_rn"] - r["pred_doublecount_bias"]),
                          axis, r["axis_value"], r["bias_empirical_rn"],
                          r["pred_doublecount_bias"], r["z_empirical_rn_vs_mc"]))
    diffs.sort(reverse=True)
    for x in diffs[:6]:
        print("    |Δ|=%.4f  %-16s v=%-8.4g  MC=%+.4f pred=%+.4f z_emp=%+.2f"
              % (x[0], x[1], x[2], x[3], x[4], x[5]))
    print("    max|Δ| = %.4f (%.2f pp)，N_MC=%d" % (diffs[0][0], 100 * diffs[0][0], base["n_mc"]))
    # 该统计量自身的抽样噪声：bias = mean_arm/sigma_mc - 1，
    # 分子跨种子稳（下用 JSON 自己报的 std/mean/sqrt(N) 核），噪声几乎全来自分母
    # sigma_mc = std(1000 个实现) ⇒ 相对标准误 = 1/sqrt(2(N-1))
    # ⇒ SE(bias) = (1+bias)/sqrt(2(N-1))
    se_pp = (1.0 + diffs[0][4]) / math.sqrt(2 * (base["n_mc"] - 1))
    print("    该统计量的 1σ 下限 = (1+bias)/sqrt(2(N-1)) = %.2f pp"
          "（分母是 1000 次实现的样本标准差）" % (100 * se_pp))
    print("    核对：用各点 mean/std 反推的相对噪声 ——")
    for axis, rows_ in d["scans"].items():
        for r in rows_:
            if abs(r["axis_value"] - base["rn"]) < 1e-12 and axis == "read_noise_e":
                m, s = r["sigma_f_empirical_rn_adu"]
                print("      分子 mean(sF_ern)=%.6f std=%.6f ⇒ mean 的相对标准误=%.4f%%"
                      % (m, s, 100 * (s / m) / math.sqrt(base["n_mc"])))
                print("      分母 sigma_f_mc=%.6f ⇒ 其相对标准误=%.4f%%"
                      % (r["sigma_f_mc_adu"], 100 / math.sqrt(2 * (base["n_mc"] - 1))))


if __name__ == "__main__":
    main()
