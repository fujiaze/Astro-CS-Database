#!/usr/bin/env python3
"""C5 -- Huber delta=1.345 (95% 渐近效率) 与 sigma_eff 标度前提 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §7 (Huber 对称性, delta=1.345, 适用域 ①②③):
  (a) z = r/sigma_eff 必须无量纲, 参考分布高斯时 delta=1.345 给 95% 渐近效率;
  (b) sigma_floor 主导时 (|uncertainty| < sigma_floor=1e-3) z 失去统计尺度意义,
      95% 效率结论不成立 —— 本实验量化其退化;
  (c) uncertainty = +inf 时 sigma_eff = inf => z = 0 => huber_w = 1
      (完全不确定的观测获得满权重, 01 D-04 指认) —— 闭式复算.
方法: 位置模型 MC. 样本 y_i = theta_true + eps_i, eps 高斯 sigma=1;
  Huber M 估计经 IRLS 求解. 效率 = Var(mean)/Var(Huber). 固定 seed.
"""
import json
import math
import os
import numpy as np

SEED = 20260321
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c5_huber_efficiency.json")
N = 100          # 每样本观测数
NMC = 40000      # MC 重复
MAXIT = 100


def huber_irls(y, delta, sigma_eff, tol=1e-10):
    """位置参数的 Huber M 估计 (IRLS), 权 w = psi(z)/z, z = (y-th)/sigma_eff."""
    th = np.median(y)
    for _ in range(MAXIT):
        z = (y - th) / sigma_eff
        az = np.abs(z)
        w = np.where(az <= delta, 1.0, delta / np.maximum(az, 1e-300))
        th_new = np.sum(w * y) / np.sum(w)
        if abs(th_new - th) < tol * max(1.0, abs(th)):
            return th_new
        th = th_new
    return th


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "n_obs": N, "nmc": NMC, "theta_true": 0.0}
    samples = rng.standard_normal((NMC, N))

    # (a) 效率 vs delta 扫描 (sigma_eff = true sigma = 1)
    sweep = {}
    for delta in (0.5, 0.8, 1.0, 1.2, 1.345, 1.5, 2.0, 3.0):
        est = np.array([huber_irls(s, delta, 1.0) for s in samples[:20000]])
        eff = np.var(samples[:20000].mean(axis=1)) / np.var(est)
        sweep[str(delta)] = {"efficiency": float(eff)}
    res["efficiency_vs_delta"] = sweep
    res["delta_1p345_efficiency"] = sweep["1.345"]["efficiency"]
    # 扫描找 0.95 效率的 delta (插值)
    ds = sorted(float(k) for k in sweep)
    es = [sweep[str(d)]["efficiency"] for d in ds]
    tgt = None
    for a, b, ea, eb in zip(ds, ds[1:], es, es[1:]):
        if ea < 0.95 <= eb:
            tgt = a + (0.95 - ea) / (eb - ea) * (b - a)
            break
    res["delta_at_95pct_efficiency"] = tgt

    # (b) sigma_floor 主导: true sigma = 1 但 sigma_eff 被钳到 1e-3
    est_floor = np.array([huber_irls(s, 1.345, 1e-3) for s in samples[:20000]])
    est_mean = samples[:20000].mean(axis=1)
    res["sigma_floor_dominated"] = {
        "var_huber_with_floor": float(np.var(est_floor)),
        "var_huber_correct_scale": float(np.var([huber_irls(s, 1.345, 1.0) for s in samples[:20000]])),
        "efficiency_loss_factor": float(np.var([huber_irls(s, 1.345, 1.0) for s in samples[:20000]])
                                        / np.var(est_floor)),
        "behaviour": "z = r/1e-3 全部落 L1 区 => 估计量退化为 median (高斯下效率 2/pi ~ 0.637)",
        "var_median_reference": float(np.var(np.median(samples[:20000], axis=1))),
    }

    # (c) uncertainty = +inf 闭式: sigma_eff = inf => z = 0 => huber_w(0) = 1
    res["infinite_uncertainty_closed_form"] = {
        "sigma_eff": math.inf, "z": 0.0, "huber_w": 1.0,
        "claim": "完全不确定的观测获得满权重 (D-04), 权重面无法降权 —— 闭式事实, 非随机结论",
    }

    # (d) Huber 对称性: 对称残差下估计量对称 (负例: 真值无偏 => 估计均值≈0)
    est0 = np.array([huber_irls(s, 1.345, 1.0) for s in samples[:20000]])
    res["symmetry_null"] = {"mean_est": float(np.mean(est0)),
                            "abs_mean_over_std": abs(float(np.mean(est0))) / float(np.std(est0))}

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
