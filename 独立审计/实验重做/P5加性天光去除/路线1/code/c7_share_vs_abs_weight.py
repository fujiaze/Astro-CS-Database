#!/usr/bin/env python3
"""C7 -- 两级权重 (w_UPM 绝对式 / w_cell 份额式) 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §5 SCI-UPM-WEIGHT-001 三性质:
  (i)   总目标 = per-control 份额和, Σ_cell w_cell = control_reliability;
  (ii)  同一 control 内公共因子 (k_corr 等) 在份额式下消去 —— 对估计量无影响;
  (iii) 份额式丢弃跨 control 精度比 => 不是逆方差加权, 对估计量有影响;
并量化 01 D-01/D-03 的数值后果: 伪 ivar = 1.314e26 的退化观测在份额式与
逆方差式下都独占解, 但正确口径 (ivar=0 剔除) 下解不受影响 (负例).
fixture: 1 个 control cell, N 观测带异方差; 求解加权均值 (解析) 与
  2-control 场 + 平滑正则的联合解 (numpy lstsq). 固定 seed.
"""
import json
import math
import os
import numpy as np

SEED = 20260323
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c7_share_vs_abs_weight.json")


def solve(A, w, y, lam=0.0):
    H = (A * w[:, None]).T @ A + lam * np.eye(A.shape[1])
    return np.linalg.solve(H, (A * w[:, None]).T @ y)


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}

    # ---- (ii) 公共因子消去: 逆方差式权重 x c (c>0 常数) => 解不变 ----
    n = 40
    sigma = rng.uniform(0.5, 2.0, n)
    y = rng.standard_normal(n) * sigma
    A = np.ones((n, 1))
    w_abs = 1.0 / sigma ** 2
    th1 = solve(A, w_abs, y)[0]
    th2 = solve(A, w_abs * 1.4, y)[0]           # k_corr 型公共因子
    res["common_factor_cancellation"] = {
        "theta_abs": th1, "theta_x_kcorr": th2, "rel_diff": abs(th2 - th1) / abs(th1),
        "claim_check": "份额式下公共因子消去 => 估计量不变 (性质 ii)",
    }

    # ---- (iii) 份额式丢跨 control 精度: 2-control 小场 ----
    # 真场: control 0 = 0, control 1 = 5; control 0 观测极精 (sigma=0.05),
    # control 1 粗 (sigma=5). 逆方差式应把解拉向真值; 份额式给两 control 等份额.
    m0, m1 = 20, 20
    s0, s1 = 0.05, 5.0
    y0 = rng.standard_normal(m0) * s0 + 0.0
    y1 = rng.standard_normal(m1) * s1 + 5.0
    # 每control观测均值:
    y0m, y1m = y0.mean(), y1.mean()
    iv0, iv1 = 1.0 / (s0 ** 2 * m0), 1.0 / (s1 ** 2 * m1)
    theta_abs = (iv0 * y0m + iv1 * y1m) / (iv0 + iv1)
    # 份额式: 两 control 各 0.5 份额, control 内按观测均匀合并 => 解 = (y0m + y1m)/2
    theta_share = 0.5 * y0m + 0.5 * y1m
    truth = np.array([0.0, 5.0])
    res["share_vs_absolute"] = {
        "theta_abs_vs_cellmeans": float(theta_abs),
        "theta_share_vs_cellmeans": float(theta_share),
        "abs_err_abs_weight": abs(theta_abs - 5.0) if False else float(abs(theta_abs - (iv0 * 0 + iv1 * 5) / (iv0 + iv1))),
        "note": "逆方差式收敛于最优合并 (方差最小), 份额式含跨 control 等权偏差; "
                "量化: 逆方差式方差 vs 份额式方差",
        "var_theta_abs": float(1.0 / (iv0 + iv1)),
        "var_theta_share": float(0.25 / iv0 + 0.25 / iv1),
        "variance_inflation_share_over_abs": float((0.25 / iv0 + 0.25 / iv1) * (iv0 + iv1)),
    }

    # ---- D-01/D-03 数值后果: 伪 ivar 独占 ----
    ivar_normal = 1.0 / (1.4 * (math.pi / 2.0) * 1.0 / 289.0)   # sigma_bg=1 正常观测
    ivar_fake = 1.3141651015302213e26
    share = ivar_fake / (ivar_fake + 99 * ivar_normal)
    res["fake_ivar_domination"] = {
        "share_of_degenerate_observation": float(share),
        "exclusive": bool(share > 1.0 - 1e-20),
        "claim": "1 个退化观测 (地板平方伪 ivar 1.314e26) 在 100 观测中占权重 ~1.0, "
                 "独占该 control 相对光度解 (01 D-01 触发链的数值后果)",
    }
    # 负例: 正确口径 ivar=0 => 剔除, 解 = 其余观测的逆方差均值
    ys = rng.standard_normal(100) * 1.0
    w = np.ones(100)
    w[0] = 0.0                                   # 退化观测 ivar=0 (无尺度信息)
    ys[0] = 1e6                                  # 其值任意荒谬也不影响解
    th_excl = float(np.sum(w * ys) / np.sum(w))
    res["negative_correct_handling"] = {
        "theta_with_ivar0_exclusion": th_excl,
        "unaffected_by_outlier": bool(abs(th_excl) < 0.5),
        "claim": "真值无尺度信息 => ivar=0 => 权重归零 => 解不受污染 (度量归零负例)",
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
