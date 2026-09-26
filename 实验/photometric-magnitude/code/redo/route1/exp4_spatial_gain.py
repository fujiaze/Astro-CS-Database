#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实验4：空间增益 m(x,y)——order<=2 上限的 why-not-structural 论证（S12）、六降级门槛
50/200/3/5/6/0.5（S12b）、幅度硬界 max|log10 m|<=1.0 dex（S9）。

seed 写死：SEED = 20260929。纯 Python + numpy。
运行：python3 exp4_spatial_gain.py
输出：../results/exp4_spatial_gain.json
"""
import json
import math
import numpy as np

SEED = 20260929
SIGMA_R = 0.018   # dex 逐星残差
MAD_COEF = 0.6744897501960817


def design(xt, yt, order):
    """低阶多项式基（与生产一致：order<=2 → 1,x,y,x^2,xy,y^2 共 5+1 项）。"""
    cols = [np.ones_like(xt), xt, yt]
    if order >= 2:
        cols += [xt * xt, xt * yt, yt * yt]
    if order >= 3:
        cols += [xt ** 3, xt ** 2 * yt, xt * yt ** 2, yt ** 3]
    return np.stack(cols, axis=1)


def fit_gain(xt, yt, g, order, ridge=1e-12):
    """等权最小二乘（Tukey 在小残差下全收；此处只测场形恢复）。"""
    B = design(xt, yt, order)
    coef, *_ = np.linalg.lstsq(B, g, rcond=None)
    return coef


def eval_gain(coef, xt, yt, order):
    return design(xt, yt, order) @ coef


rng = np.random.default_rng(SEED)
n = 2000
x = rng.uniform(-1, 1, n)
y = rng.uniform(-1, 1, n)

# 真场：order-2 形态，幅度 0.1 dex（01/C3 登记的真实污染量级）
g_true = 0.10 * (0.5 * x ** 2 + 0.3 * x * y - 0.4 * y ** 2) + 0.05 * x

order_rows = []
for N in (20, 50, 100, 200, 500, 2000):
    idx = rng.choice(n, size=min(N, n), replace=False)
    xt, yt = x[idx], y[idx]
    g_obs = g_true[idx] + rng.normal(0, SIGMA_R, len(idx))
    # 网格上评估恢复误差（独立评估点，不含训练点）
    xg, yg = np.meshgrid(np.linspace(-1, 1, 41), np.linspace(-1, 1, 41))
    xg, yg = xg.ravel(), yg.ravel()
    g_true_grid = 0.10 * (0.5 * xg ** 2 + 0.3 * xg * yg - 0.4 * yg ** 2) + 0.05 * xg
    row = {"N": len(idx)}
    for order in (0, 1, 2, 3):
        if len(idx) < design(xt, yt, order).shape[1]:
            row[f"order{order}_rms_dex"] = None
            continue
        coef = fit_gain(xt, yt, g_obs, order)
        g_fit = eval_gain(coef, xg, yg, order)
        row[f"order{order}_rms_dex"] = float(np.sqrt(np.mean((g_fit - g_true_grid) ** 2)))
    order_rows.append(row)

# 负例：平场（真值无效应 ⇒ 度量归零）
neg_rows = []
for N in (200, 2000):
    idx = rng.choice(n, size=N, replace=False)
    g_obs = rng.normal(0, SIGMA_R, len(idx))    # 无场
    coef = fit_gain(x[idx], y[idx], g_obs, 2)
    g_fit = eval_gain(coef, x[idx], y[idx], 2)
    neg_rows.append({"N": N, "max_abs_log10m_dex": float(np.max(np.abs(g_fit))),
                     "rms_dex": float(np.sqrt(np.mean(g_fit ** 2)))})

# S9：幅度硬界 1.0 dex 的 binding 性
# 注入 0.1 dex 场（登记量级）与 2.0 dex 场（真值 max|log10 m| ≈ 2.0*max|shape| ≈ 1.1 > 1.0 ⇒ 应判红）
bound_rows = []
for amp in (0.1, 2.0):
    g_big = amp * (0.5 * x ** 2 + 0.3 * x * y - 0.4 * y ** 2)
    g_obs = g_big + rng.normal(0, SIGMA_R, n)
    coef = fit_gain(x, y, g_obs, 2)
    g_fit = eval_gain(coef, x, y, 2)
    bound_rows.append({"field_amplitude_dex": amp,
                       "max_abs_log10m_dex": float(np.max(np.abs(g_fit))),
                       "bound_1.0dex_binds": bool(np.max(np.abs(g_fit)) > 1.0)})

# S12b：降级门槛的统计含义——order1(N=50)/order2(N=200) 的场幅信噪比
gate_rows = []
for order, n_min in ((1, 50), (2, 200)):
    nterms = 3 if order == 1 else 6
    # 场幅估计的标准误 ~ sigma_r * sqrt(trace((B^T B)^{-1}) 展缩)
    for N in (n_min // 2, n_min, n_min * 4):
        idx = rng.choice(n, size=N, replace=False)
        B = design(x[idx], y[idx], order)
        H = B.T @ B
        # 幅度方向（单位范数系数向量）的方差：sigma^2 * v^T H^{-1} v
        v = np.ones(nterms) / math.sqrt(nterms)
        Hinv = np.linalg.inv(H + 1e-12 * np.eye(nterms))
        se_amp = SIGMA_R * math.sqrt(v @ Hinv @ v)
        gate_rows.append({"order": order, "N": N, "n_terms": nterms,
                          "min_N_gate": n_min,
                          "se_amplitude_dex": se_amp,
                          "amp_snr_for_0.1dex_field": 0.1 / se_amp})

out = {
    "seed": SEED,
    "true_field": "0.10*(0.5x^2+0.3xy-0.4y^2)+0.05x  [dex]",
    "S12_order_sweep": order_rows,
    "S12_negative_flat_field": neg_rows,
    "S9_amplitude_bound": bound_rows,
    "S12b_gate_statistics": gate_rows,
}
with open("../results/exp4_spatial_gain.json", "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
