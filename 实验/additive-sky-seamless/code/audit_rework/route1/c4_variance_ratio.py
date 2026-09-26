#!/usr/bin/env python3
"""C4 -- 方差比诊断量对电平接缝"原理性失明" 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §9a (方差比的适用面 = 渲染分块伪影粗筛),
docs/plugins/algorithms_phase2/11_upm.md §4.1 ("方差比对电平阶跃原理性失明"),
以及 11_upm.md §8 的 Oracle 要求: "旧方差比口径在同一输入上判绿(盲区复现)".

设计 (解析代数合成, 含负例):
  场 = 常数背景 + 独立高斯像素噪声, 分界线两侧:
    A 电平台阶 (无方差变化): 左侧 +Δ, 右侧 0, 同 sigma.
    B 方差伪影 (无电平变化): 两侧均值同, 右侧 sigma 翻倍.
    C 无伪影 (负例): 完全均匀.
  判据量:
    rel_step  = |median(右) - median(左)| / bg        (电平口径)
    var_ratio = MAD2(右) / MAD2(左)                    (方差口径, MAD^2 稳健方差)
  预期:
    A: rel_step ≈ Δ/bg 可判红; var_ratio ≈ 1 (失明).
    B: var_ratio ≈ 4; rel_step ≈ 0 (方差口径对电平盲, 反向亦然 => 互补).
    C: 两量都归一到无效应基线 (真值无效应 => 度量归零).
"""
import json
import math
import os
import numpy as np

SEED = 20260320
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c4_variance_ratio.json")
SEAM_GATE = 1e-2
VR_GATE = 1.5       # 方差比粗筛门 (示意; 正本未冻结, 报告注明)
NMC = 2000


def mad2(x):
    return float(np.median(np.abs(x - np.median(x))) * 1.4826) ** 2


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "seam_gate": SEAM_GATE, "var_ratio_gate_demo": VR_GATE,
           "n_pix_per_side": 40000, "nmc": NMC}
    bg = 100.0
    sigma = 5.0

    cases = {}
    scenarios = {
        "A_level_step": {"mu_l": bg, "mu_r": bg + 0.02 * bg, "sig_l": sigma, "sig_r": sigma},
        "B_variance_artifact": {"mu_l": bg, "mu_r": bg, "sig_l": sigma, "sig_r": 2 * sigma},
        "C_null": {"mu_l": bg, "mu_r": bg, "sig_l": sigma, "sig_r": sigma},
    }
    for name, p in scenarios.items():
        left = rng.normal(p["mu_l"], p["sig_l"], 40000)
        right = rng.normal(p["mu_r"], p["sig_r"], 40000)
        rs = abs(np.median(right) - np.median(left)) / bg
        vr = mad2(right) / mad2(left)
        # MC 分布: var_ratio 的散布 (n=40000 时很小)
        rs_mc, vr_mc = np.empty(NMC), np.empty(NMC)
        for t in range(NMC):
            l = rng.normal(p["mu_l"], p["sig_l"], 40000)
            r = rng.normal(p["mu_r"], p["sig_r"], 40000)
            rs_mc[t] = abs(np.median(r) - np.median(l)) / bg
            vr_mc[t] = mad2(r) / mad2(l)
        cases[name] = {
            "rel_step": float(rs), "rel_step_red": bool(rs > SEAM_GATE),
            "var_ratio": float(vr), "var_ratio_red_demo": bool(vr > VR_GATE or vr < 1.0 / VR_GATE),
            "rel_step_mc_std": float(np.std(rs_mc)),
            "var_ratio_mc_p1_p99": [float(np.quantile(vr_mc, 0.01)), float(np.quantile(vr_mc, 0.99))],
            "expected": {"A": "rel_step red, var_ratio ~1 (blind)",
                         "B": "var_ratio ~4, rel_step ~0 (blind)",
                         "C": "both at null baseline"}[name.split("_")[0]],
        }
    res["cases"] = cases

    res["conclusions"] = {
        "A_var_ratio_blind": bool(abs(cases["A_level_step"]["var_ratio"] - 1.0) < 0.05
                                   and cases["A_level_step"]["rel_step_red"]),
        "B_rel_step_blind": bool(cases["B_variance_artifact"]["var_ratio"] > 3.0
                                  and not cases["B_variance_artifact"]["rel_step_red"]),
        "C_null_both_baseline": bool(abs(cases["C_null"]["var_ratio"] - 1.0) < 0.05
                                      and not cases["C_null"]["rel_step_red"]),
        "claim": "电平阶跃不改变方差 => 方差比对电平接缝原理性失明 (A); "
                 "方差伪影不改变电平 => rel_step 对其失明 (B); 两者互补, "
                 "方差比的引用面 = 渲染分块伪影粗筛诊断量, 不进电平接缝判据面",
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
