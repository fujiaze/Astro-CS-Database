#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q5 Huber delta=1.345（高斯 95% 渐近效率）三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §7（Huber 对称性; δ=1.345 = 高斯参考分布下渐近效率 95%）;
      docs/plugins/algorithms_phase2/11_upm.md §4.4（稳健迭代）。
理论腿（解析推导, 本脚本闭式复算）: 标准正态下 Huber M-估计量的渐近方差
    V(delta) = E[psi^2] / (E[psi'])^2,
    E[psi'] = 2*Phi(delta)-1
    E[psi^2] = (2*Phi(delta)-1) - 2*delta*phi(delta) + 2*delta^2*(1-Phi(delta))
    ARE = 1/V.  delta=1.345 => ARE = 0.9503 (95%).
文献腿: Huber 1964 (DOI 10.1214/aoms/1177703732); Holland & Welsch 1977
    (DOI 10.1080/03610927708827533, IRLS 与 delta 取值表); Huber & Ronchetti 2009。
实验腿: MC IRLS（MAD 标度）实测效率; 污染混合分布下 Huber 相对均值的增益;
    负例（真值无效应⇒度量归零）: 常数数据 => 残差/损失精确为 0;
    尺度等变性: huber(10y) = 10*huber(y)。
固定 seed: SEED = 20260926。纯 numpy（Phi/phi 用 math.erf 向量化）。
运行: python3 exp05_huber_efficiency.py
"""
import json
import os
from math import erf, exp, sqrt, pi

import numpy as np

SEED = 20260926
DELTA = 1.345
N = 1000
REPS = 20000
BATCH = 4000
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q5_huber_efficiency.json")

_erf = np.vectorize(erf)
Phi = lambda z: 0.5 * (1.0 + _erf(np.asarray(z, float) / sqrt(2.0)))
phi = lambda z: np.exp(-np.asarray(z, float) ** 2 / 2.0) / sqrt(2.0 * pi)


def huber_irls(Y, delta):
    """Y: (m, n) 每行一个样本集。返回每行 Huber 位置估计（MAD 标度, IRLS）。"""
    mu = np.median(Y, axis=1)
    s = 1.482602218505602 * np.median(np.abs(Y - mu[:, None]), axis=1)
    s = np.maximum(s, 1e-300)
    for _ in range(50):
        z = (Y - mu[:, None]) / s[:, None]
        w = np.where(np.abs(z) <= delta, 1.0, delta / np.maximum(np.abs(z), 1e-300))
        mu_new = (w * Y).sum(1) / w.sum(1)
        if np.max(np.abs(mu_new - mu)) < 1e-12 * np.max(np.abs(mu) + 1.0):
            mu = mu_new
            break
        mu = mu_new
    return mu


def are_theory(delta):
    Ep = 2.0 * float(Phi(delta)) - 1.0
    E2 = Ep - 2.0 * delta * float(phi(delta)) + 2.0 * delta ** 2 * (1.0 - float(Phi(delta)))
    return Ep ** 2 / E2


def main():
    out = {"seed": SEED, "delta": DELTA, "n": N, "reps": REPS}

    # ---- (a) 理论 ARE 闭式复算 ----
    out["theory_ARE"] = {str(d): are_theory(d) for d in [1.0, 1.345, 1.5, 2.0]}
    out["are_1.345"] = are_theory(1.345)

    # ---- (b) MC: 高斯下实测效率 ----
    rng = np.random.default_rng(SEED)
    hub = np.empty(REPS)
    for i in range(0, REPS, BATCH):
        m = min(BATCH, REPS - i)
        Y = rng.normal(0.0, 1.0, (m, N))
        hub[i:i + m] = huber_irls(Y, DELTA)
    v_h = hub.var()
    v_mean_theory = 1.0 / N
    out["mc_gaussian"] = {
        "var_huber": v_h, "var_mean_theory": v_mean_theory,
        "efficiency_empirical": v_mean_theory / v_h,
        "efficiency_theory": out["are_1.345"],
        "abs_err": abs(v_mean_theory / v_h - out["are_1.345"])}

    # ---- (c) delta 扫描: MC 效率 vs 理论 ----
    sweep = []
    for d in [1.0, 1.345, 1.5, 2.0]:
        rng2 = np.random.default_rng(SEED + 31)
        hub = np.empty(8000)
        for i in range(0, 8000, BATCH):
            m = min(BATCH, 8000 - i)
            Y = rng2.normal(0.0, 1.0, (m, N))
            hub[i:i + m] = huber_irls(Y, d)
        eff = (1.0 / N) / hub.var()
        sweep.append({"delta": d, "eff_mc": eff, "eff_theory": are_theory(d),
                      "abs_err": abs(eff - are_theory(d))})
    out["delta_sweep"] = sweep

    # ---- (d) 污染混合（eps=0.1, sigma_out=3）: Huber 相对均值的增益 ----
    rng3 = np.random.default_rng(SEED + 61)
    hub = np.empty(REPS)
    mean_ = np.empty(REPS)
    for i in range(0, REPS, BATCH):
        m = min(BATCH, REPS - i)
        base = rng3.normal(0.0, 1.0, (m, N))
        cont = rng3.random((m, N)) < 0.1
        Y = base + cont * rng3.normal(0.0, 3.0, (m, N))
        hub[i:i + m] = huber_irls(Y, DELTA)
        mean_[i:i + m] = Y.mean(1)
    out["mc_contaminated"] = {
        "var_mean": float(mean_.var()), "var_huber": float(hub.var()),
        "gain_huber_over_mean": float(mean_.var() / hub.var())}

    # ---- (e) 负例 + 尺度等变性 ----
    c = 3.7
    const = np.full((8, N), c) + 1e-15 * np.random.default_rng(SEED + 5).normal(size=(8, N))
    est = huber_irls(const, DELTA)
    out["negative_constant"] = {
        "max_abs_residual_metric": float(np.max(np.abs(est - const.mean(1)))),
        "note": "常数数据（无效应）=> 估计量=常数, 残差度量=0（机器精度）"}
    rng4 = np.random.default_rng(SEED + 7)
    Y = rng4.normal(0.0, 1.0, (200, N))
    h1 = huber_irls(Y, DELTA)
    h2 = huber_irls(10.0 * Y, DELTA)
    out["scale_equivariance"] = {
        "max_rel_err": float(np.max(np.abs(h2 / 10.0 - h1) / (np.abs(h1) + 1e-12)))}

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps({k: out[k] for k in ["are_1.345", "mc_gaussian", "mc_contaminated",
                                          "negative_constant", "scale_equivariance"]},
                     ensure_ascii=False, indent=1))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
