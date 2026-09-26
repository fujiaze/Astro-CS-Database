#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q6 chi2_red 分母必须取 dof = n_obs - r_eff 的三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §7a 规则4; docs/plugins/algorithms_phase2/11_upm.md §4.7。
文献腿: Andrae, Schulze-Hartung & Melchior 2010 (arXiv:1012.3754) 式(8)(9)（已逐字核验）:
    P_eff = tr(H) = rank(X)                        (8)
    K = N - P_eff >= N - P                          (9)
理论腿（线性模型标准结果）: y = X beta + eps, eps~N(0, sigma^2 I), X 秩 r:
    E[chi2] = sigma^2 * (n - r)  —— 残差自由度由【秩】而非参数个数决定。
实验腿: MC 验证 E[chi2/(n-r_eff)] = 1 而 E[chi2/(n-n_params)] = (n-r)/(n-p) > 1
    （即 n_obs - n_params 作分母会系统性【高估】chi2_red;
      正本 §7a 括注"秩亏时后者系统性低估 chi2_red"字面方向存疑, 见报告）。
负例: 满秩 (r = p) 时两个分母相同 => 两种口径逐位一致（度量差=0）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp06_dof_rankeff.py
"""
import json
import os
import numpy as np

SEED = 20260926
N_OBS, P = 60, 10
R = 7                    # 设计矩阵秩（3 列精确线性相关）
REPS = 4000
SIGMA = 1.0
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q6_dof_rankeff.json")


def make_design(rng, rank_deficient=True):
    A = rng.normal(0.0, 1.0, (N_OBS, P))
    if rank_deficient:
        A[:, 7] = A[:, 0] + A[:, 1]
        A[:, 8] = 2.0 * A[:, 2] - A[:, 3]
        A[:, 9] = A[:, 4] + A[:, 5]
    return A


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "n_obs": N_OBS, "n_params": P, "rank": R, "reps": REPS}

    # 式(8) 数值核验: P_eff = tr(H) = rank(X)
    A0 = make_design(np.random.default_rng(SEED + 1))
    H = A0 @ np.linalg.pinv(A0)
    out["eq8_check"] = {"trace_hat": float(np.trace(H)), "rank": int(np.linalg.matrix_rank(A0)),
                        "abs_diff": abs(float(np.trace(H)) - R)}

    for tag, rank_def in [("rank_deficient", True), ("full_rank_negative", False)]:
        chi2 = np.empty(REPS)
        for rep in range(REPS):
            A = make_design(rng, rank_def)
            beta = rng.normal(0.0, 1.0, P) if rank_def else None
            if rank_def:
                beta[7] = beta[8] = beta[9] = 0.0      # 零空间分量任意, 取 0
                y = A @ beta + SIGMA * rng.normal(0.0, 1.0, N_OBS)
            else:
                y = A @ rng.normal(0.0, 1.0, P) + SIGMA * rng.normal(0.0, 1.0, N_OBS)
            sol, *_ = np.linalg.lstsq(A, y, rcond=None)
            r = y - A @ sol
            chi2[rep] = r @ r
        r_eff = R if rank_def else P
        row = {
            "case": tag,
            "mean_chi2_over_sigma2": float(chi2.mean()),
            "theory_n_minus_r": N_OBS - r_eff,
            "ratio_dof_n_minus_r_eff": float(chi2.mean() / (N_OBS - r_eff)),
            "ratio_dof_n_minus_params": float(chi2.mean() / (N_OBS - P)),
            "chi2red_bias_nparams_vs_neff":
                float((N_OBS - r_eff) / (N_OBS - P)),
        }
        out[tag] = row
        print(row)

    out["negative_ok"] = abs(out["full_rank_negative"]["ratio_dof_n_minus_r_eff"]
                             - out["full_rank_negative"]["ratio_dof_n_minus_params"]) < 1e-12
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
