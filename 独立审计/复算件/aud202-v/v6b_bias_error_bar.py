#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD202-V / V6-b：给"双计偏差"这个统计量本身定误差棒
（用归档 JSON 自己报的 std 与 N_MC，不引入新随机数）

bias_empirical_rn = mean(sF_ern)/sigma_f_mc - 1
  mean(sF_ern) 的相对标准误 = (std(sF_ern)/mean(sF_ern))/sqrt(N)
  sigma_f_mc   的相对标准误 ~ 1/sqrt(2(N-1))        （正态样本标准差渐近式）
  => SE(bias) = (1+bias) * sqrt(relA^2 + relB^2)
"""
import json
import math
import os

REPO = r"F:\Astro dev\Astro CS Normalization Database"
d = json.load(open(os.path.join(REPO, "实验", "absolute-snr", "results",
                                "b2_noise_terms.json"), encoding="utf-8"))
base = d["frozen_config"]["base"]
N = base["n_mc"]
relB = 1.0 / math.sqrt(2.0 * (N - 1))
print("N_MC = %d   分母 sigma_f_mc 的相对标准误 = 1/sqrt(2(N-1)) = %.4f%%"
      % (N, 100 * relB))
print()
print("%-16s %-9s %-11s %-11s %-11s %-11s %-9s" %
      ("axis", "v", "bias_MC", "闭式pred", "MC-闭式", "SE(bias)[pp]", "偏差/SE"))
rows = []
for axis, rs in d["scans"].items():
    for r in rs:
        mean_ern, std_ern = r["sigma_f_empirical_rn_adu"]
        relA = (std_ern / mean_ern) / math.sqrt(N)
        b = r["bias_empirical_rn"]
        se = (1.0 + b) * math.sqrt(relA ** 2 + relB ** 2)
        rows.append((abs(b - r["pred_doublecount_bias"]), axis, r["axis_value"], b,
                     r["pred_doublecount_bias"], se))
        if abs(r["F_e"] - base["F_e"]) < 1e-12 and abs(r["sky_e_per_px"] - base["sky_e_per_px"]) < 1e-12 \
                and abs(r["read_noise_e"] - base["rn"]) < 1e-12 and abs(r["gain_e_per_adu"] - base["gain"]) < 1e-12 \
                and abs(r["psf_sigma_px"] - base["sigma_psf"]) < 1e-12:
            print("%-16s %-9.4g %-11.4f %-11.4f %-11.4f %-11.4f %-9.2f" %
                  (axis, r["axis_value"], 100 * b, 100 * r["pred_doublecount_bias"],
                   100 * (b - r["pred_doublecount_bias"]), 100 * se,
                   abs(b - r["pred_doublecount_bias"]) / se))
rows.sort(reverse=True)
print()
print("全部 %d 个点：|MC-闭式| 最大 %.2f pp @ (%s, v=%g)，其 SE = %.2f pp ⇒ %.1f sigma"
      % (len(rows), 100 * rows[0][0], rows[0][1], rows[0][2], 100 * rows[0][5],
         rows[0][0] / rows[0][5]))
zs = [r[0] / r[5] for r in rows]
print("|MC-闭式|/SE  的 max/median = %.2f / %.2f（纯噪声下 max(28 点) 期望 ~2.6σ）"
      % (max(zs), sorted(zs)[len(zs) // 2]))
print()
print("结论：该统计量自身的 1σ ≈ %.1f pp ⇒ 归档头条（12.79%%）与闭式（14.50%%）之差在噪声内；" % (100 * rows[0][5]))
print("      『最大偏差 7.0pp』同样是噪声量级，不构成物理反证 —— 真正的问题是")
print("      把带 ~3pp 噪声的实现值当权威数字写进 FROZEN 文档，且无门、无误差棒。")
