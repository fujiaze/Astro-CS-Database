#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q7 rank_rtol=1e-10 唯一判决阈值 / 列均衡尺度不变 / H_solve 恒真门 三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §7a 规则4/规则5; docs/plugins/algorithms_phase2/11_upm.md §4.7。
    判决必须落在未正则化的列均衡数据信息矩阵 H_eq = D^-1 H_red D^-1 (D = diag(sqrt(H_ii)));
    identifiable <=> r_eff == n_free <=> kappa(H_red(列均衡)) < 1/tau, tau = rank_rtol;
    rank_rtol_effective = max(tau, max(m,n)*eps);
    H_solve = H_red + lam*P 上的门是恒真门（lam 增大 => kappa 有上界 => 恒绿）。
实验腿:
  (a) 列均衡不变量: 任意列缩放 S 下 H_eq 逐位不变（解析: D' = S D => D'^-1 S H S D'^-1 = H_eq）,
      而 kappa(H_red) 随缩放剧烈变化 => 原始 kappa 不可作判决读数;
  (b) tau 地板数值;
  (c) 恒真门数值演示: 严重秩亏 H_red 上 lam_eff = tau*mean(diag) 使 kappa(H_solve) 过门;
  (d) 判决等价性: 随机矩阵族上 r_eff==n_free <=> kappa_eq<1/tau 零错位;
  (e) 生产读数对照: M42 实测 kappa ~3.2e7 vs 1/tau = 1e10。
负例（真值无效应⇒度量归零）: 满秩良态矩阵 => n_unidentified = 0, 判决绿。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp07_rank_rtol.py
"""
import json
import os
import numpy as np

SEED = 20260926
TAU = 1e-10
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q7_rank_rtol.json")


def equilibrate(H):
    d = np.sqrt(np.diag(H))
    Dinv = np.diag(1.0 / d)
    return Dinv @ H @ Dinv, d


def judge(H, tau):
    Heq, _ = equilibrate(H)
    ev = np.linalg.eigvalsh(Heq)[::-1]
    ev = np.maximum(ev, 0.0)
    r_eff = int((ev > tau * ev[0]).sum())
    n_free = H.shape[0]
    kappa = ev[0] / ev[-1] if ev[-1] > 0 else np.inf
    return r_eff, n_free, kappa


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "tau": TAU}

    # ---- (a) 列均衡不变量 ----
    m = 24
    A = rng.normal(0.0, 1.0, (200, m))
    H = A.T @ A
    s = 10.0 ** rng.uniform(-6, 6, m)
    Hs = (A * s).T @ (A * s)
    Heq0, _ = equilibrate(H)
    HeqS, _ = equilibrate(Hs)
    rel = np.max(np.abs(HeqS - Heq0)) / np.max(np.abs(Heq0))
    kap0 = np.linalg.cond(Heq0)
    kapS = np.linalg.cond(HeqS)
    out["col_equilibration"] = {
        "max_rel_diff_Heq": float(rel),
        "kappa_Hred_original": float(np.linalg.cond(H)),
        "kappa_Hred_scaled": float(np.linalg.cond(Hs)),
        "kappa_Heq_original": float(kap0),
        "kappa_Heq_scaled": float(kapS),
        "invariant": rel < 1e-10 and abs(kapS - kap0) < 1e-8 * kap0,
        "note": "kappa(H_red) 变 12+ 个量级, kappa(H_eq) 逐位不变 => 只有列均衡口径可判"}

    # ---- (b) tau 地板 ----
    for (mm, nn) in [(49, 58), (20, 24)]:
        floor = max(TAU, max(mm, nn) * np.finfo(float).eps)
        out[f"tau_floor_m{mm}_n{nn}"] = {
            "rank_rtol_effective": floor, "tau_active": floor == TAU}

    # ---- (c) 恒真门演示 ----
    # 构造严重秩亏 H_red: 参数重复列
    A2 = rng.normal(0.0, 1.0, (300, m))
    A2[:, m - 1] = A2[:, 0]          # 精确秩亏
    H_red = A2.T @ A2
    r_eff, n_free, kap = judge(H_red, TAU)
    lam_eff = TAU * float(np.mean(np.diag(H_red)))
    H_solve = H_red + lam_eff * np.eye(m)
    kap_solve = np.linalg.cond(H_solve)
    r_eff_solve, _, _ = judge(H_solve, TAU)
    # 生产域: smoothing_lambda = 0.1 * mean(diag)（PHASE2_UPM §5 auto 档）
    lam_prod = 0.1 * float(np.mean(np.diag(H_red)))
    H_solve_prod = H_red + lam_prod * np.eye(m)
    kap_solve_prod = np.linalg.cond(H_solve_prod)
    r_eff_solve_prod, _, _ = judge(H_solve_prod, TAU)
    out["always_green_gate"] = {
        "kappa_Hred": float(kap), "r_eff_Hred": r_eff, "n_free": n_free,
        "verdict_by_Hred": kap > 1.0 / TAU,
        "lambda_eff_tau_mean_diag": lam_eff,
        "kappa_Hsolve_at_lambda_tau": float(kap_solve),
        "lambda_prod_0.1_mean_diag": lam_prod,
        "kappa_Hsolve_at_production_lambda": float(kap_solve_prod),
        "Hsolve_prod_gate_green": bool(kap_solve_prod < 1.0 / TAU),
        "r_eff_Hsolve_prod": r_eff_solve_prod,
        "r_eff_Hred_unchanged": r_eff,
        "conclusion": "lam=tau·mean(diag) 时 kappa(H_solve)~2.2e10 贴门; 生产 lam=0.1·mean(diag) 时 "
                      "kappa~O(10) 恒绿而 r_eff 仍示秩亏 => H_solve 上的门零信息（规则5 证实）"}

    # ---- (d) 判决等价性（随机族, 200 例, 含可控秩亏）----
    mism = 0
    for t in range(200):
        mm = rng.integers(6, 20)
        k = int(rng.integers(0, max(1, mm // 3)))
        B = rng.normal(0.0, 1.0, (4 * mm, mm))
        cols = rng.permutation(mm)[:k] if k else []
        if k:
            B[:, cols] = B[:, rng.permutation(mm)[:k]]   # 重复列 => 秩亏
        Ht = B.T @ B
        r, nf, kp = judge(Ht, TAU)
        v1 = (r == nf)
        v2 = (kp < 1.0 / TAU)
        mism += (v1 != v2)
    out["equivalence_family"] = {"cases": 200, "mismatches": int(mism)}

    # ---- (e) 生产读数对照 + 负例 ----
    out["production_reading"] = {
        "m42_kappa_measured": 3.2e7, "one_over_tau": 1.0 / TAU,
        "margin": (1.0 / TAU) / 3.2e7}
    A3 = rng.normal(0.0, 1.0, (400, 18))
    r3, nf3, k3 = judge(A3.T @ A3, TAU)
    out["negative_fullrank"] = {"r_eff": r3, "n_free": nf3,
                                "n_unidentified": nf3 - r3,
                                "identifiable": r3 == nf3 and k3 < 1.0 / TAU}

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps(out, ensure_ascii=False, indent=1,
                     default=lambda o: o.item() if hasattr(o, "item") else str(o))[:2600])
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
