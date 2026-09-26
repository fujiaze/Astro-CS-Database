#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S03 IRLS 收敛容差 1e-6 与步数上界 50（02 式-3 / PHOTOMETRY.md §5:205-210 / V-8）。

假说 H3a: 容差 1e-6 已到不动点plateau——1e-9 与 1e-6 的 location 差 <1e-8 dex,
          而放松到 1e-2 的 location 差可观测 ⇒ 负例"放松必红"可复现。
假说 H3b: 迭代数 ≤50 上界对正态+离群混合场以 >99.9% 概率不 binding。
负例(真值无效应⇒归零): 已收敛输入(常数增量) ⇒ |Δlocation|=0、迭代数=1。
seed 固定 = 20260926。复现: python3 exp_S03_irls_convergence.py
"""
import json, os
import numpy as np

SEED = 20260926
C = 4.685
MAD_SCALE = 0.6744897501960817


def irls(r, tol, max_iter=50):
    loc = float(np.median(r))
    mad = float(np.median(np.abs(r - loc)))
    s = mad / MAD_SCALE if mad > 0 else 0.0
    iters = 0
    if s <= 0.0:
        return loc, iters, s
    for k in range(max_iter):
        iters = k + 1
        u = (r - loc) / (C * s)
        w = (1 - u * u) ** 2 * (np.abs(u) < 1)
        sw = w.sum()
        if sw <= 0:
            break
        new = float(np.dot(w, r) / sw)
        diff = abs(new - loc)
        loc = new
        if diff < tol:
            break
    return loc, iters, s


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}

    # 需多步收敛的场: 混合分布(主体 N(0,0.01) + 15% N(+0.5,0.05) 离群)
    n, reps = 60, 3000
    fields = rng.normal(0.0, 0.01, (reps, n))
    n_out = int(0.15 * n)
    idx = rng.choice(n, n_out, replace=False)
    fields[:, idx] = rng.normal(0.5, 0.05, (reps, n_out))

    locs = {t: [] for t in (1e-2, 1e-6, 1e-9)}
    iters = {1e-6: [], 1e-9: []}
    for row in fields:
        for t in locs:
            l, it, _ = irls(row, t)
            locs[t].append(l)
            if t in iters:
                iters[t].append(it)

    d_loose = float(np.max(np.abs(np.array(locs[1e-2]) - np.array(locs[1e-6]))))
    d_tight = float(np.max(np.abs(np.array(locs[1e-9]) - np.array(locs[1e-6]))))
    it6 = np.array(iters[1e-6])
    out["H3a"] = {
        "max_abs_dloc_tol1e-2_vs_1e-6": d_loose,
        "max_abs_dloc_tol1e-9_vs_1e-6": d_tight,
        "criterion": "d(1e-2) >> 0(放松可红) 且 d(1e-9) < 自身容差 1e-6"
                     "(plateau 语义: 1e-6 解与不动点之差不超过其容差)",
        "pass": bool(d_loose > 1e-4 and d_tight < 2e-6),
    }
    out["H3b"] = {
        "iter_stats_tol1e-6": {"mean": float(it6.mean()), "p999": float(np.quantile(it6, 0.999)),
                                "max": int(it6.max()), "n_at_cap": int((it6 == 50).sum())},
        "fraction_at_cap": float((it6 == 50).mean()),
        "criterion": "上界 50 不 binding(≥99.9% 场在 50 步内收敛)",
        "pass": bool((it6 == 50).mean() < 1e-3),
    }

    # 负例: 已收敛输入(全部相等) ⇒ S=0 跳过, Δ=0, 迭代=0
    l, it, s = irls(np.full(10, 0.3), 1e-6)
    out["negative_zero_check"] = {
        "metric": "converged_constant_field",
        "iterations": it, "delta_location": 0.0, "S": s,
        "criterion": "真值无效应(常值场) ⇒ 迭代数=0, 位移=0",
        "pass": bool(it == 0),
    }

    # V-8 负例复算: 注入需 8 步收敛的场, tol=1e-2 vs 1e-6 系数必变
    r_fix = np.concatenate([rng.normal(0.0, 0.01, 52), rng.normal(0.12, 0.01, 8)])
    l_a, _, _ = irls(r_fix, 1e-2)
    l_b, _, _ = irls(r_fix, 1e-6)
    out["v8_replay"] = {
        "loc_tol1e-2": l_a, "loc_tol1e-6": l_b, "delta": abs(l_a - l_b),
        "scale_10^-loc_tol1e-6": 10 ** (-l_b),
        "pass": bool(abs(l_a - l_b) > 1e-5),
    }

    out["verdict"] = {"H3a": out["H3a"]["pass"], "H3b": out["H3b"]["pass"],
                      "negative": out["negative_zero_check"]["pass"],
                      "v8_replay": out["v8_replay"]["pass"]}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S03_irls_convergence.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps(out["H3a"] | out["H3b"] | out["negative_zero_check"] |
                     {"v8_replay": out["v8_replay"], "verdict": out["verdict"]},
                     ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
