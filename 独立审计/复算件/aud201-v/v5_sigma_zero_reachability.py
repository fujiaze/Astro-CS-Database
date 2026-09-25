# -*- coding: utf-8 -*-
"""
V5 独立复算：sigma_residual 取 0 的**可达**条件，与现行判据是否放行。

按生产实现（star_matcher.cpp 的 medianOf / S=0 分支 / Tukey 入选规则 / MAD）
在我自己目录里重写一份等价标量逻辑（不 import 仓库代码、不编译仓库源），
再逐条喂构造样本。现行唯一消费它的判据（module_adapters.cpp:5747-5748）
只卡上界 sigma_residual_dex > 1.0 dex。
"""
import io
import json
import os
import random
import sys

sys.stdout.reconfigure(encoding="utf-8")
OUT = os.path.dirname(os.path.abspath(__file__))
MAD_SCALE = 0.6744897501960817
TUKEY_C = 4.685
IRLS_MAX_ITER = 50
IRLS_CONVERGE = 1e-6
GATE_UPPER_DEX = 1.0


def median_of(vals):
    """与 star_matcher.cpp:165-170 同式：奇数取中，偶数取两中平均。"""
    v = sorted(vals)
    n = len(v)
    if n == 0:
        return 0.0
    return v[n // 2] if n % 2 == 1 else 0.5 * (v[n // 2 - 1] + v[n // 2])


def fit(r_consistent):
    """复刻 star_matcher.cpp:540-626 的位置/内点/sigma 三步（同一分支顺序）。"""
    n = len(r_consistent)
    loc = median_of(r_consistent)
    mad = median_of([abs(x - loc) for x in r_consistent])
    S = (mad / MAD_SCALE) if mad > 0.0 else 0.0
    iters = 0
    if S > 0.0:
        prev = loc
        for it in range(IRLS_MAX_ITER):
            iters = it + 1
            cS = TUKEY_C * S
            sw = swr = 0.0
            for x in r_consistent:
                u = (x - loc) / cS
                w = 0.0 if abs(u) >= 1.0 else (1.0 - u * u) ** 2
                sw += w
                swr += w * x
            if sw <= 0.0:
                break
            new = swr / sw
            diff = abs(new - prev)
            loc = new
            if diff < IRLS_CONVERGE:
                break
            prev = new
    cS = TUKEY_C * S if S > 0.0 else 1.0
    inliers = [x for x in r_consistent
               if (S <= 0.0) or (abs((x - loc) / cS) < 1.0)]
    sigma = 0.0
    if len(inliers) >= 2:
        mad_in = median_of([abs(x - loc) for x in inliers])
        sigma = (mad_in / MAD_SCALE) if mad_in > 0.0 else 0.0
    return {"n": n, "n_inliers": len(inliers), "location": loc, "S": S,
            "sigma_residual_dex": sigma,
            "sigma_mag": 2.5 * sigma,
            "outlier_rate": (1 - len(inliers) / n) if n else None,
            "gate_passes_upper_only": sigma <= GATE_UPPER_DEX}


def main():
    res = {}
    # --- 构造 1：多数票相同（最小可达零离散度）---
    cases = {}
    for tag, rs in (
            ("3star_2identical_plus_arbitrary", [0.30, 0.30, 0.30 + 7.7]),
            ("3star_all_identical", [0.30, 0.30, 0.30]),
            ("4star_2identical_minority_tie", [0.30, 0.30, 0.31, 9.9]),
            ("4star_3identical", [0.30, 0.30, 0.30, 9.9]),
            ("9star_production_fixture", [-7.666286] * 9),
            ("realistic_20star_scatter", [0.30 + 0.02 * (i % 5) for i in range(20)])):
        cases[tag] = fit(rs)
    res["constructed"] = cases

    # --- 反证：|r_inliers| < 2 分支是否可达（生产门要求 n>=3）---
    rng = random.Random(20260925)
    min_inl = 99
    trials = 200000
    worst_sigma_zero_example = None
    n_sigma_zero = 0
    for _ in range(trials):
        n = rng.randint(3, 12)
        rs = [rng.uniform(-6, 6) if rng.random() > 0.5 else round(rng.uniform(-6, 6), 3)
              for _ in range(n)]
        if rng.random() < 0.35:                      # 制造重复值
            j = rng.randrange(n)
            rs[rng.randrange(n)] = rs[j]
        o = fit(rs)
        min_inl = min(min_inl, o["n_inliers"])
        if o["sigma_residual_dex"] == 0.0:
            n_sigma_zero += 1
            if worst_sigma_zero_example is None or \
               (max(o["n"], 0) and abs(max(rs) - min(rs)) >
                abs(max(worst_sigma_zero_example["rs"]) - min(worst_sigma_zero_example["rs"]))):
                worst_sigma_zero_example = {"rs": rs, **o}
    res["search_for_inliers_lt_2"] = {
        "trials": trials, "min_observed_n_inliers": min_inl,
        "conclusion": ("|r_inliers|<2 分支在 n>=3 的随机+重复值搜索中从未出现"
                       if min_inl >= 2 else "找到了 |r_inliers|<2 的样本"),
        "sigma_zero_hits": n_sigma_zero,
        "widest_spread_still_reporting_zero": (
            {"rs": worst_sigma_zero_example["rs"],
             "n": worst_sigma_zero_example["n"],
             "n_inliers": worst_sigma_zero_example["n_inliers"],
             "spread_dex": max(worst_sigma_zero_example["rs"]) - min(worst_sigma_zero_example["rs"]),
             "outlier_rate": worst_sigma_zero_example["outlier_rate"]}
            if worst_sigma_zero_example else None)}
    txt = json.dumps(res, indent=2, ensure_ascii=False)
    io.open(os.path.join(OUT, "v5_sigma_zero_reachability.json"), "w",
            encoding="utf-8").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
