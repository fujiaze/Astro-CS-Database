#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q2 方差比诊断量"对电平阶跃原理性失明"命题的定义域分解（路线3）。

正本: docs/plugins/algorithms_phase2/11_upm.md §4.1
    "方差比对电平阶跃原理性失明（阶跃不改变方差），方差比的引用面 = 诊断量本身"
命题分解（本实验验证）:
  定义 A  VR_side = s²_left / s²_right          （两侧各自样本方差之比）
  定义 B  VR_pool = s²_pooled / mean(s²_side)   （合并窗口方差 / 组内方差均值）
理论（解析推导腿）: 左 X~N(μ,σ²), 右 Y~N(μ+Δ,σ²) 独立:
  E[s²_left] = E[s²_right] = σ²                  ⇒ VR_side → 1（对 Δ 确定性盲）
  E[s²_pooled] = σ² + Δ²/4                       ⇒ VR_pool → 1 + Δ²/(4σ²)（对 Δ 敏感）
即"阶跃不改变方差"只对【逐侧】方差定义成立；对【合并窗口】方差定义不成立。
负例（真值无效应⇒度量归零）: Δ=0 ⇒ 两种定义的 VR 都 → 1（偏差=纯抽样噪声）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp02_variance_ratio.py
"""
import json
import os
import numpy as np

SEED = 20260926
N_SIDE = 256
REPS = 20000
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q2_variance_ratio.json")


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "n_side": N_SIDE, "reps": REPS, "rows": []}
    for ds in [0.0, 1.0, 2.0, 4.0]:
        vr_side = np.empty(REPS)
        vr_pool = np.empty(REPS)
        for r in range(REPS):
            x = rng.normal(0.0, 1.0, N_SIDE)
            y = rng.normal(ds, 1.0, N_SIDE)
            s2l, s2r = x.var(ddof=1), y.var(ddof=1)
            vr_side[r] = s2l / s2r
            vr_pool[r] = np.concatenate([x, y]).var(ddof=1) / (0.5 * (s2l + s2r))
        pred_pool = 1.0 + ds ** 2 / 4.0
        row = {
            "delta_over_sigma": ds,
            "VR_side_mean": float(vr_side.mean()),
            "VR_pool_mean": float(vr_pool.mean()),
            "VR_pool_analytic": pred_pool,
            "VR_pool_abs_err": abs(vr_pool.mean() - pred_pool),
            "side_blind_vs_pool_sensitive":
                bool(abs(vr_side.mean() - 1.0) < abs(vr_pool.mean() - 1.0) / 2.0),
        }
        out["rows"].append(row)
        print(row)
    # 结论判定
    r0 = out["rows"][0]
    out["negative_ok"] = (abs(r0["VR_side_mean"] - 1) < 0.02 and abs(r0["VR_pool_mean"] - 1) < 0.02)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
