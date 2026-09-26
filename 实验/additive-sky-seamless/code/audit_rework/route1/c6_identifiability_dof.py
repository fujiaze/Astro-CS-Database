#!/usr/bin/env python3
"""C6 -- 可辨识性唯一判据 (tau=rank_rtol, 列均衡 H_red) 与 dof 实验腿. v2

验证对象: docs/science/PHASE2_UPM.md §7a 规则 4/5, docs/plugins/algorithms_phase2/11_upm.md §4.7:
  (a) 判决落在未正则化、列均衡数据信息矩阵 H_eq = D^-1 H_red D^-1 上;
      参数列缩放与全局权重缩放下判决/r_eff/kappa 不变;
  (b) H_solve = H_red + lambda·P 上设门是恒真门;
  (c) E[chi2] = n_obs - r_eff (Andrae et al. 2010, arXiv:1012.3754); 并数值检验
      分母取 n_obs - n_params 在秩亏时对 chi2_red 的偏差方向;
  (d) 负例/正例: gauge 自由 (秩亏) 判红; gauge 固定 (满秩) 判绿.
fixture: UPM 型联合参数化 y_{f,c,i} = s_c + delta_{f,c} + eps (lambda=0, 无跨 cell 耦合),
  每 (f,c) 6 次观测, ivar 随机. gauge 退化: 每 cell 内 (s_c, delta_{1c..Fc}) 只约束和
  => 每 cell 恰 1 个简并方向 (与 PHASE2_UPM.md §16.2 "smoothing_lambda=0 时 per-(frame,cell)
  自由加性场恰好定解"一致); gauge 固定 = 参考帧分量系数退出行列剔除.
"""
import json
import math
import os
import numpy as np

SEED = 20260322
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c6_identifiability_dof.json")
TAU = 1e-10
N_FRAMES, N_CELLS, N_OBS_PC = 3, 8, 6


def design(gauge_fixed):
    n_s = N_CELLS
    n_d = N_FRAMES * N_CELLS
    cols = n_s + n_d
    drop = set()
    if gauge_fixed:
        for c in range(N_CELLS):
            drop.add(n_s + 0 * N_CELLS + c)      # 参考帧 (f=0) 分量系数 gauge 固定
    keep = [i for i in range(cols) if i not in drop]
    rows = []
    for f in range(N_FRAMES):
        for c in range(N_CELLS):
            for _ in range(N_OBS_PC):
                r = np.zeros(cols)
                r[c] = 1.0                       # s_c
                r[n_s + f * N_CELLS + c] = 1.0   # delta_{f,c}
                rows.append(r)
    return np.array(rows)[:, keep], len(keep), keep


def assess(H_red, tau=TAU):
    d = np.sqrt(np.diag(H_red))
    H_eq = np.diag(1.0 / d) @ H_red @ np.diag(1.0 / d)
    ev = np.linalg.eigvalsh(H_eq)[::-1]
    ev = np.clip(ev, 0.0, None)
    lam1 = ev[0]
    r_eff = int(np.sum(ev > tau * lam1))
    kappa_full = (lam1 / ev[-1]) if ev[-1] > 0 else math.inf
    return r_eff, kappa_full, ev


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "tau": TAU, "n_frames": N_FRAMES, "n_cells": N_CELLS,
           "n_obs_per_cell_frame": N_OBS_PC}

    # ---- (a)(d) 判决 ----
    Ag, cols_g, keep_g = design(gauge_fixed=False)
    Af, cols_f, keep_f = design(gauge_fixed=True)
    ivar = rng.uniform(0.5, 2.0, Ag.shape[0])
    Hg = (Ag * ivar[:, None]).T @ Ag
    Hf = (Af * ivar[:, None]).T @ Af
    r_g, k_g, _ = assess(Hg)
    r_f, k_f, _ = assess(Hf)
    res["verdicts"] = {
        "gauge_free": {"n_params": cols_g, "r_eff": r_g, "n_free": cols_g,
                       "note": "n_free = 未 gauge 固定的满秩基准列数" + str(cols_g),
                       "identifiable": bool(r_g == cols_g),
                       "kappa_Hred": None if math.isinf(k_g) else k_g,
                       "expected": "每 cell 1 简并 => r_eff = N_CELLS*(1+N_FRAMES-1) 判红"},
        "gauge_fixed": {"n_params": cols_f, "r_eff": r_f, "n_free": cols_f,
                        "identifiable": bool(r_f == cols_f),
                        "kappa_Hred": None if math.isinf(k_f) else k_f,
                        "expected": "满秩判绿"},
    }

    # ---- 缩放不变性 ----
    S = np.diag(np.exp(rng.uniform(-3, 3, Hf.shape[0])))
    r_s, k_s, _ = assess(S @ Hf @ S)
    res["column_scaling_invariance"] = {"r_eff_invariant": bool(r_s == r_f),
                                        "kappa_rel_diff": abs(k_s - k_f) / k_f}
    r_c, k_c, _ = assess(7.3 * Hf)
    res["global_weight_invariance"] = {"r_eff_invariant": bool(r_c == r_f),
                                       "kappa_identical": bool(k_c == k_f)}

    # ---- (b) H_solve 恒真门 ----
    lam_eff = TAU * float(np.mean(np.diag(Hg)))
    row = {}
    for mult in (1.0, 1e3, 1e6):
        Hs = Hg + mult * lam_eff * np.eye(cols_g)
        r_s2, k_s2, _ = assess(Hs)
        row[f"lambda_x{mult:.0e}"] = {
            "kappa_Hsolve": None if math.isinf(k_s2) else k_s2,
            "identifiable_on_Hsolve": bool(r_s2 == cols_g),
            "true_Hred_r_eff": r_g}
    res["always_green_gate_on_Hsolve"] = {
        "detail": row,
        "claim_check": "H_red 实际 r_eff < n_params (gauge 不可辨识), 但 H_solve 上的门恒绿",
    }

    # ---- (c) dof ----
    n_obs = Ag.shape[0]
    nmc = 3000
    chi2s = np.empty(nmc)
    Aw_sqrt = np.sqrt(ivar)
    pinv = np.linalg.pinv(Ag * Aw_sqrt[:, None])
    for t in range(nmc):
        y = (rng.standard_normal(n_obs) / Aw_sqrt)
        theta = pinv @ (y * Aw_sqrt)
        resid = y - Ag @ theta
        chi2s[t] = np.sum(ivar * resid ** 2)
    chi2_mean = float(np.mean(chi2s))
    dof_reff = n_obs - r_g
    dof_nparams = n_obs - cols_g
    res["dof_test"] = {
        "n_obs": n_obs, "r_eff": r_g, "n_params": cols_g,
        "E_chi2_mc": chi2_mean, "pred_dof_reff": dof_reff,
        "chi2_red_with_reff": chi2_mean / dof_reff,
        "chi2_red_with_nparams": chi2_mean / dof_nparams,
        "ratio_nparams_over_reff": (chi2_mean / dof_nparams) / (chi2_mean / dof_reff),
        "finding": "n_obs-n_params 分母更小 => chi2_red 被高估 (偏红方向), "
                   "不是 11_upm.md §4.7 声称的'系统性低估'; "
                   "'掩盖未被约束方向数'的部分成立 (dof 差 = r_eff 与 n_params 之差)",
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
