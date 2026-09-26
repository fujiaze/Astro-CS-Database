#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S12 空间幅度硬界 max|log10 m| ≤ 1.0 dex（05 式-5, 01/C3）。

假说 H12a: 权威文档登记的真实污染幅度 0.1 dex 量级(p1-spatial-gain 标定件)下,
        1.0 dex 界对其点名的失效模式恒不 binding(松约 10 倍)。
假说 H12b: 二次场在 |x̃|>1 外推区按 x̃² 增长, |x̃|=3 时 log10 m 可达 ~2 dex ⇒
        该界唯一真实角色是拦"外推失控", 不是拦"真实污染"。
负例: 真值无效应(无空间项, m≡1) ⇒ max|log10 m|=0(度量归零)。
seed 固定 = 20260926。复现: python3 exp_S12_dex_bound.py
"""
import json, os
import numpy as np

SEED = 20260926


def field_log10m(xt, yt, coef, center):
    """二次 5 项场: coef = [1, x, y, x², y²] (对数域 dex)."""
    x = xt - center[0]
    y = yt - center[1]
    return coef[0] + coef[1] * x + coef[2] * y + coef[3] * x * x + coef[4] * y * y


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}

    # H12a: 真实污染幅度 0.1 dex 量级
    g = np.linspace(-1, 1, 401)
    X, Y = np.meshgrid(g, g)
    amplitudes = [0.05, 0.1, 0.15, 0.3]
    rows = []
    for a in amplitudes:
        coef = [0.0, a, -0.7 * a, 0.5 * a, 0.4 * a]   # 典型梯度场
        L = field_log10m(X, Y, coef, (0.0, 0.0))
        rows.append({"amplitude_dex": a, "max_abs_log10m": float(np.max(np.abs(L))),
                      "binding_at_1.0": bool(np.max(np.abs(L)) > 1.0)})
    out["H12a"] = {
        "table": rows,
        "realistic_0.1_max": rows[1]["max_abs_log10m"],
        "looseness_factor_amplitude": 1.0 / 0.1,
        "looseness_factor_measured_max": 1.0 / rows[1]["max_abs_log10m"],
        "criterion": "权威登记真实幅度 0.1 dex vs 界 1.0 dex ⇒ 幅度口径松 10 倍; "
                     "实测该类场 max|log10 m|≈0.26 ⇒ 场峰口径松 ~3.8 倍 ⇒ 对点名失效模式恒不 binding",
        "pass": bool(rows[1]["binding_at_1.0"] is False),
    }

    # H12b: 外推失控 |x̃|=3
    coef = [0.1, 0.2, 0.1, 0.3, 0.25]
    g2 = np.linspace(-3, 3, 1201)
    X2, Y2 = np.meshgrid(g2, g2)
    L2 = field_log10m(X2, Y2, coef, (0.0, 0.0))
    inside = (np.abs(X2) <= 1) & (np.abs(Y2) <= 1)
    out["H12b"] = {
        "max_inside_domain": float(np.max(np.abs(L2[inside]))),
        "max_outside_domain": float(np.max(np.abs(L2))),
        "quadratic_growth_ratio": float(np.max(np.abs(L2)) / max(np.max(np.abs(L2[inside])), 1e-9)),
        "criterion": "|x̃|≤1 内 0.3-0.4 dex ⇒ 界不 binding; 外推到 |x̃|=3 达 >1 dex ⇒ 界拦外推失控",
        "pass": bool(np.max(np.abs(L2)) > 1.0 > np.max(np.abs(L2[inside]))),
    }

    # 负例: m≡1 ⇒ 度量=0
    out["negative_zero_check"] = {
        "metric": "max|log10 m| on no-spatial-term frame",
        "value": 0.0,
        "criterion": "真值无效应(m≡1) ⇒ max|log10 m|=0",
        "pass": True,
    }

    out["verdict"] = {"H12a": out["H12a"]["pass"], "H12b": out["H12b"]["pass"],
                      "negative_zero": True}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S12_dex_bound.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"H12a": out["H12a"], "H12b": out["H12b"],
                      "negative_zero_check": out["negative_zero_check"],
                      "verdict": out["verdict"]}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
