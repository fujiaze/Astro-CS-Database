#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S02 MAD→σ 系数 0.6744897501960817 与有限样本偏差。

假说 H2a: 0.6744897501960817 = Φ⁻¹(3/4)（正态一致常数 1/0.67449≈1.4826,
          Wikipedia "Median absolute deviation" 与 Rousseeuw & Croux 1993 JASA 同口径）。
假说 H2b: MAD 型尺度估计在 n 小时系统性偏低（02 式-3 断言 n=3 偏低约 1.49 倍），
          E[MAD_n]/σ → 0.6745 仅当 n→∞（文献: 有限样本校正因子概念,
          Akinshin arXiv:2207.12005; Croux & Rousseeuw 1992）。
负例: 常数数据(真值无散度) ⇒ MAD=0 ⇒ σ̂=0, 偏差度量归零。
seed 固定 = 20260926。复现: python3 exp_S02_mad_consistency.py
"""
import json, os
import numpy as np
from math import erf, sqrt

SEED = 20260926
CONST = 0.6744897501960817


def phi(x):
    return 0.5 * (1.0 + erf(x / sqrt(2.0)))


def phi_inv(p):
    lo, hi = -12.0, 12.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if phi(mid) < p:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def main():
    out = {"seed": SEED}
    # H2a: 二分求 Φ⁻¹(3/4) 与代码常数比
    q = phi_inv(0.75)
    out["phi_inv_3_4"] = q
    out["code_constant"] = CONST
    out["abs_diff"] = abs(q - CONST)
    out["rel_diff"] = abs(q - CONST) / CONST
    out["reciprocal"] = 1.0 / CONST
    out["H2a_pass"] = bool(abs(q - CONST) < 1e-12)

    # H2b: MC E[MAD_n]/sigma 随 n
    rng = np.random.default_rng(SEED)
    reps = 200000
    table = {}
    for n in (3, 5, 10, 20, 30, 100):
        data = rng.standard_normal((reps, n))
        med = np.median(data, axis=1, keepdims=True)
        mad = np.median(np.abs(data - med), axis=1)
        table[str(n)] = {
            "E_MAD_over_sigma": float(mad.mean()),
            "sigma_hat_bias_factor": float(mad.mean() / CONST),  # E[σ̂]/σ
            "se_of_mean": float(mad.std() / sqrt(reps)),
        }
    out["E_MAD_table"] = table
    out["n3_deficit_factor"] = CONST / table["3"]["E_MAD_over_sigma"]  # 1/偏差
    out["H2b_pass"] = bool(abs(out["n3_deficit_factor"] - 1.49) < 0.06
                           and table["100"]["E_MAD_over_sigma"] > 0.66)

    # 负例: 无效应⇒归零 —— 常数数据 MAD=0 ⇒ σ̂=0
    const_data = np.full((1000, 5), 3.14)
    med = np.median(const_data, axis=1, keepdims=True)
    mad = np.median(np.abs(const_data - med), axis=1)
    out["negative_zero_check"] = {
        "metric": "sigma_hat_on_constant_data",
        "max_value": float(mad.max()),
        "criterion": "真值无散度 ⇒ σ̂=0(归零)",
        "pass": bool(mad.max() == 0.0),
    }

    # 顺带核 02 式-8 闭式: c/0.6744897501960817 = 6.945...
    out["c_over_madscale"] = 4.685 / CONST

    out["verdict"] = {
        "H2a_pass": out["H2a_pass"],
        "H2b_pass": out["H2b_pass"],
        "detail": f"Φ⁻¹(3/4)={q:.16f}; n=3 缺损因子={out['n3_deficit_factor']:.3f} "
                  f"(02 断言 ≈1.49); c·S 闭式系数={out['c_over_madscale']:.6f} (02 记 6.945)",
    }
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S02_mad_consistency.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({k: out[k] for k in ("phi_inv_3_4", "abs_diff", "reciprocal",
                                          "n3_deficit_factor", "E_MAD_table",
                                          "negative_zero_check", "c_over_madscale",
                                          "verdict")}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
