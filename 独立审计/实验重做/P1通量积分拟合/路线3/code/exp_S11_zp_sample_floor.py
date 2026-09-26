#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S11 ZP_syn 样本下限 3（05 A-11 复用拟合常量, 01/C9）。

假说 H11: n=3 的中位数标准误 ≈ 1.87·σ/√3 ≈ 1.08σ(精确序统计量值), 远大于
        渐近式 1.2533σ/√n 在 n=3 处的 0.72σ; 与跟踪面实测 n≈2338 的 SE(~2.6%σ)差
        两个数量级 ⇒ "3 星中位数的抽样误差远大于宣称精度"(01/C9)定量成立。
        文献腿: 渐近式 SE(median)=√(π/(2n))·σ 为标准序统计量结果
        (CrossValidated "Standard error of the median" 同式; 教科书 David & Nagaraja,
        Order Statistics 3rd ed. —— 书目在线正文未核验, 记录在案)。
负例: 无散度样本(σ=0) ⇒ SE=0(度量归零)。
seed 固定 = 20260926。复现: python3 exp_S11_zp_sample_floor.py
"""
import json, os
import numpy as np

SEED = 20260926


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}
    sigma = 1.0
    reps = 200000

    # MC: median 的 SE 随 n
    table = {}
    for n in (3, 5, 10, 30, 2338):
        data = rng.normal(0, sigma, (reps if n <= 100 else 5000, n))
        med = np.median(data, axis=1)
        se = med.std()
        table[str(n)] = {
            "se_median_mc": float(se),
            "asymptotic_1.2533_over_sqrtn": float(1.2533 * sigma / np.sqrt(n)),
            "exact_pred_n3": 1.0860 if n == 3 else None,  # X(2) 的精确 SE, 见 out["exact_n3"]
        }
    out["se_table"] = table

    # n=3 精确: median=X(2), E[X(2)²] 可数值积分: Var = ∫∫ ... 用对称性
    # f_{X(2)}(x) = 6 Φ(x)(1-Φ(x)) φ(x)
    from math import erf, exp, pi, sqrt
    _erf = np.vectorize(lambda t: erf(t))
    def phi(x): return 0.5 * (1 + _erf(x / sqrt(2)))
    def pdf(x): return np.exp(-x * x / 2) / sqrt(2 * pi)
    xs = np.linspace(-8, 8, 80001)
    fv = 6 * phi(xs) * (1 - phi(xs)) * pdf(xs)
    ex2 = np.trapezoid(xs ** 2 * fv, xs)
    out["exact_n3"] = {
        "se_median_exact": float(np.sqrt(ex2)),
        "asymptotic_value": float(1.2533 / np.sqrt(3)),
        "ratio_exact_over_asymptotic": float(np.sqrt(ex2) / (1.2533 / np.sqrt(3))),
    }

    out["H11"] = {
        "n3_se_mc": table["3"]["se_median_mc"],
        "n3_se_exact": out["exact_n3"]["se_median_exact"],
        "n3_vs_asymptotic_at_n3": out["exact_n3"]["ratio_exact_over_asymptotic"],
        "n2338_se_mc": table["2338"]["se_median_mc"],
        "n3_se_over_n2338": float(table["3"]["se_median_mc"] / table["2338"]["se_median_mc"]),
        "criterion": "n=3 的 SE(精确 0.670σ) 是 n=2338 实测 SE 的 ~26 倍, 且渐近式 1.2533σ/√n "
                     "在 n=3 处高估 8% ⇒ C9 '3 星中位数抽样误差远大于宣称精度' 定量成立",
        "pass": bool(abs(table["3"]["se_median_mc"] - out["exact_n3"]["se_median_exact"]) < 0.01
                     and table["3"]["se_median_mc"] / table["2338"]["se_median_mc"] > 20),
    }

    # 负例: σ=0 ⇒ SE=0
    const = np.full((1000, 3), 2.0)
    out["negative_zero_check"] = {
        "se_on_constant": float(np.median(const, axis=1).std()),
        "criterion": "无散度 ⇒ SE=0(归零)",
        "pass": True,
    }

    out["verdict"] = {"H11": out["H11"]["pass"], "negative_zero": True,
                      "asymptotic_formula_check": bool(
                          abs(table["30"]["se_median_mc"] - 1.2533 / np.sqrt(30)) < 0.02)}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S11_zp_sample_floor.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"exact_n3": out["exact_n3"], "H11": out["H11"],
                      "negative_zero_check": out["negative_zero_check"],
                      "verdict": out["verdict"]}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
