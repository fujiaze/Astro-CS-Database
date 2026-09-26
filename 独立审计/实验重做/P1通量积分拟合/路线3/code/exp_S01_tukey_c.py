#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S01 Tukey biweight 形状参数 c=4.685 的文献值腿复算。

假说 H1: c=4.685 使 biweight 位置估计量在正态分布下达到 95% 渐近效率
        (Kafadar 1983, J Res NBS 88(2):105-116, doi:10.6028/jres.088.006)。
方法:
  (a) 数值积分 A(c) = ∫ψ²φ / (∫ψ'φ)², ARE(c)=1/A(c), 扫 c 并反解 ARE=0.95;
  (b) 蒙特卡洛: n=20 正态样本, biweight(c=4.685) vs 样本均值的方差比 ≈ 0.95;
  (c) 负例(真值无效应⇒度量归零): 无污染对称数据下 |bias(biweight)| 应与 0
      在 MC 误差内不可区分 —— metric_bias_clean 归零判据。
  (d) 鲁棒性: 40% 离群污染下 biweight 仍回到真值(对照: 均值被拉飞)。
seed 固定 = 20260926。复现: python3 exp_S01_tukey_c.py
"""
import json, os
import numpy as np

SEED = 20260926
PHI_N = 200001


def normal_pdf(x):
    return np.exp(-0.5 * x * x) / np.sqrt(2.0 * np.pi)


def are_of_c(c):
    """ARE(c) = (∫ψ'φ)² / ∫ψ²φ, ψ(x)=x(1-(x/c)²)²[|x|<c] (c·S 已归一到 x 空间)."""
    x = np.linspace(-8.0, 8.0, PHI_N)
    phi = normal_pdf(x)
    inside = np.abs(x) < c
    u = 1.0 - (x[inside] / c) ** 2
    psi = np.zeros_like(x)
    psi[inside] = x[inside] * u * u
    dpsi = np.zeros_like(x)
    dpsi[inside] = u * (u - 4.0 * (x[inside] / c) ** 2)
    i_psi2 = np.trapezoid(psi ** 2 * phi, x)
    i_dpsi = np.trapezoid(dpsi * phi, x)
    return i_dpsi ** 2 / i_psi2


def biweight_location(x, c=4.685, tol=1e-8, max_iter=200):
    loc = np.median(x)
    mad = np.median(np.abs(x - loc)) / 0.6744897501960817
    if mad <= 0:
        return loc
    for _ in range(max_iter):
        u = (x - loc) / (c * mad)
        w = (1.0 - u * u) ** 2 * (np.abs(u) < 1.0)
        sw = w.sum()
        if sw <= 0:
            break
        new = np.dot(w, x) / sw
        if abs(new - loc) < tol:
            loc = new
            break
        loc = new
    return loc


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "hypothesis": "H1: ARE(4.685)=0.95 (Kafadar 1983)"}

    # (a) 扫 c 反解 ARE=0.95
    cs = np.linspace(1.0, 8.0, 141)
    ares = np.array([are_of_c(c) for c in cs])
    c_star = float(np.interp(0.95, ares, cs))
    out["numeric_root_c_star"] = c_star
    out["ARE_at_4.685"] = are_of_c(4.685)
    out["ARE_table"] = {f"{c:.2f}": float(a) for c, a in zip(cs[::10], ares[::10])}

    # (b) MC 效率(渐近值属 n→∞; n=20 存在有限样本效率损失, Kafadar 1983 正文讨论)
    effs = {}
    for n, reps in ((20, 40000), (200, 5000)):
        data = rng.standard_normal((reps, n))
        means = data.mean(axis=1)
        bws = np.array([biweight_location(row) for row in data])
        effs[str(n)] = float(means.var() / bws.var())
    out["mc_efficiency_by_n"] = effs
    eff = effs["200"]   # H1 判据用大 n 逼近渐近值
    out["mc_efficiency_at_4.685_n20"] = effs["20"]
    out["mc_efficiency_at_4.685_n200"] = effs["200"]

    # (c) 负例: 无效应⇒归零 (偏差 |median bias| 相对 MC 标准误)
    bias = float(abs(bws.mean()))
    se = float(bws.std() / np.sqrt(reps))
    out["negative_zero_check"] = {
        "metric": "abs_mean_bias_of_biweight_on_clean_gaussian",
        "value": bias, "mc_se": se,
        "ratio_to_se": bias / se,
        "criterion": "value 与 0 在 3*mc_se 内不可区分 ⇒ 归零(判绿)",
        "pass": bool(bias < 3 * se),
    }

    # (d) 40% 污染
    reps2 = 4000
    cont = rng.standard_normal((reps2, n))
    n_out = int(0.4 * n)
    idx = rng.choice(n, n_out, replace=False)
    cont[:, idx] += 30.0
    bw_c = np.array([biweight_location(row) for row in cont])
    mean_c = cont.mean(axis=1)
    out["contamination_40pct"] = {
        "biweight_median_abs_err": float(np.median(np.abs(bw_c))),
        "mean_median_abs_err": float(np.median(np.abs(mean_c))),
        "criterion": "biweight 误差应 <0.1 dex 且远小于均值",
    }

    out["verdict"] = {
        "H1_pass": bool(abs(out["ARE_at_4.685"] - 0.95) < 5e-4
                        and abs(eff - 0.95) < 0.03
                        and abs(c_star - 4.685) < 0.05),
        "detail": f"数值根 c*={c_star:.4f}; ARE(4.685)={out['ARE_at_4.685']:.6f}; "
                  f"MC 效率 n=20: {out['mc_efficiency_by_n']['20']:.4f}, "
                  f"n=200: {eff:.4f} (seed={SEED})",
    }
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S01_tukey_c.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({k: out[k] for k in ("numeric_root_c_star", "ARE_at_4.685",
                                          "mc_efficiency_at_4.685_n20",
                                          "negative_zero_check", "contamination_40pct",
                                          "verdict")}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
