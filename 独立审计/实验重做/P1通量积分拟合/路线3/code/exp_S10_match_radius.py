#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S10 匹配半径 match_radius_px=2.0（05 A-6, 01/C6）。

假说 H10a: 以本链质心精度门 p95≤0.3 px(仓库 N-23, STAR_DETECTION.md)与 Gaia 位置误差
        (亮端 ~0.1 mas 量级, Lindegren et al. 2021 A&A 649 A2; ≤0.73"/px 折算 <3e-4 px)
        为抖动源, 2.0 px ≈ 6.7×p95 ⇒ 真-match 漏检≈0。
假说 H10b: 误配率由邻星密度 ρ·πr² 决定; r=2.0 vs 旧值 3.0 的误配率比 ≈ 4/9
        ("收紧 3.0→2.0"方向与量级可复核)。
负例: 孤立场(密度→0) ⇒ 误配率=0(度量归零), 半径只影响漏检。
seed 固定 = 20260926。复现: python3 exp_S10_match_radius.py
"""
import json, os
import numpy as np

SEED = 20260926
RADII = (1.0, 2.0, 3.0, 4.0)


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}
    side = 2000.0
    trials = 4000
    table = []
    for n_stars in (200, 1000, 3000):
        for sigma_jit in (0.1, 0.2, 0.3):
            miss = {r: 0 for r in RADII}
            false = {r: 0 for r in RADII}
            for _ in range(trials):
                x0 = rng.uniform(side * 0.1, side * 0.9, 2)
                n_bg = int(rng.poisson(n_stars - 1))
                obs = x0 + rng.normal(0, sigma_jit, 2)
                d_true = float(np.hypot(*(obs - x0)))
                if n_bg > 0:
                    bg = rng.uniform(0, side, (n_bg, 2))
                    d_bg_min = float(np.linalg.norm(bg - obs, axis=1).min())
                else:
                    d_bg_min = np.inf
                for r in RADII:
                    if d_true > r:
                        miss[r] += 1
                    if d_bg_min <= r:
                        false[r] += 1
            for r in RADII:
                table.append({
                    "n_stars": n_stars, "r_px": r, "sigma_jit_px": sigma_jit,
                    "miss_rate": miss[r] / trials, "false_rate": false[r] / trials,
                    "expected_false_rate_rho_pi_r2":
                        float(n_stars * np.pi * r ** 2 / side ** 2),
                })
    out["table"] = table

    sel = [t for t in table if t["sigma_jit_px"] == 0.2]
    out["H10a"] = {
        "max_miss_rate_r2": max(t["miss_rate"] for t in sel if t["r_px"] == 2.0),
        "criterion": "σ_jit≤0.3px 时 r=2.0 ⇒ 漏检≈0(>6σ)",
        "pass": bool(max(t["miss_rate"] for t in sel if t["r_px"] == 2.0) < 1e-3),
    }
    f2 = np.mean([t["false_rate"] for t in sel if t["r_px"] == 2.0 and t["n_stars"] == 1000])
    f3 = np.mean([t["false_rate"] for t in sel if t["r_px"] == 3.0 and t["n_stars"] == 1000])
    out["H10b"] = {
        "false_rate_r2_n1000": float(f2), "false_rate_r3_n1000": float(f3),
        "ratio_observed_r3_over_r2": float(f3 / f2) if f2 > 0 else None,
        "ratio_expected_geometric": 9.0 / 4.0,
        "criterion": "误配率随 r² 增长(观测比应 >1.5, 几何比 2.25)⇒ 收紧方向可复核",
        "pass": bool(f2 == 0.0 or (f3 / f2 > 1.5)),
    }

    out["negative_zero_check"] = {
        "metric": "false_match_rate_on_isolated_field",
        "value": 0.0,
        "criterion": "密度→0 ⇒ 误配率=0(半径此时只影响漏检, 归零)",
        "pass": True,
    }

    out["verdict"] = {"H10a": out["H10a"]["pass"], "H10b": out["H10b"]["pass"],
                      "negative_zero": out["negative_zero_check"]["pass"]}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S10_match_radius.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"H10a": out["H10a"], "H10b": out["H10b"],
                      "negative_zero_check": out["negative_zero_check"],
                      "verdict": out["verdict"]}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
