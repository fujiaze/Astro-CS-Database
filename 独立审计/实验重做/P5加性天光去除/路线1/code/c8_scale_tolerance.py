#!/usr/bin/env python3
"""C8 -- 尺度归一容差与确定性容差 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §7 (绝对 1e-6 不可达论证)、§7a 规则 6
  (跨 worker rtol 1e-12, 实测 ΔC_max = 2.22e-15 ~ 1 ulp)、docs/plugins/algorithms_phase2/11_upm.md §4.6:
  (a) 生产标度 scale_obs = 5.26e13 (ADU·sr^-1) 处, 双精度 ULP = 2^(e-52);
      绝对容差 1e-6 远低于 ULP => 除恰好 0 外不可达 (量级论证的数值演示);
      相对判据 |dM|/scale_obs < tol 与绝对判据在 scale~1 时等价 (数值演示);
  (b) 分块求和顺序差 (FP 非结合): 1-worker 顺序 vs 4-worker 块合并,
      测 |Δ| 及其与 rtol 1e-12·scale、1 ulp 的比值 (验证 "~1 ulp" 量级声明).
"""
import json
import math
import os
import numpy as np

SEED = 20260324
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c8_scale_tolerance.json")


def ulp_at(x):
    return math.ulp(x)


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}

    # ---- (a) 绝对容差在 ULP 之下 ----
    scale = 5.26e13
    u = ulp_at(scale)
    res["absolute_tolerance_below_ulp"] = {
        "scale_obs": scale, "ulp": u,
        "ulp_over_tol": u / 1e-6,
        "claim_check": "ULP/1e-6 = %.3g => 绝对判据 |dM|<1e-6 只在 dM 恰为 0 时可满足; "
                       "相对判据 |dM|/scale < tol 才是可达判据" % (u / 1e-6),
    }
    # 相对判据在小标度下的等价性: scale=1 时 |dM|/1 < tol 等价于 |dM| < tol
    x = 1.0 + rng.standard_normal(1000) * 1e-8
    rel_ok = np.abs(x - 1.0) / 1.0 < 1e-6
    abs_ok = np.abs(x - 1.0) < 1e-6
    res["relative_equiv_at_scale1"] = {"mismatches": int(np.sum(rel_ok != abs_ok))}

    # ---- (b) FP 非结合: 分块求和顺序差 ----
    n_items, n_workers, n_trials = 4096, 4, 200
    diffs = np.empty(n_trials)
    scale_true = np.empty(n_trials)
    for t in range(n_trials):
        vals = rng.standard_normal(n_items) * 1e-3 + 299.0   # 生产量级 (百 ADU 级) 微扰
        total_seq = math.fsum([]) if False else 0.0
        s_seq = np.float64(0.0)
        for v in vals:                                        # 1 worker 顺序累加
            s_seq += v
        chunks = np.array_split(vals, n_workers)
        parts = []
        for ch in chunks:
            s = np.float64(0.0)
            for v in ch:
                s += v
            parts.append(s)
        s_par = np.float64(0.0)
        for p in parts:                                       # 4 worker 块合并 (worker 顺序)
            s_par += p
        diffs[t] = abs(float(s_seq) - float(s_par))
        scale_true[t] = abs(float(s_seq))
    res["fp_associativity"] = {
        "n_items": n_items, "n_workers": n_workers, "n_trials": n_trials,
        "max_abs_diff": float(np.max(diffs)),
        "max_diff_over_rtol_scale": float(np.max(diffs) / (1e-12 * np.max(scale_true))),
        "max_diff_in_ulp_at_scale": float(np.max(diffs) / np.max([ulp_at(s) for s in scale_true])),
        "claim_check": "跨 worker 求和顺序差 ~ ulp 量级, 远小于 rtol=1e-12 门 (规则 6)",
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
