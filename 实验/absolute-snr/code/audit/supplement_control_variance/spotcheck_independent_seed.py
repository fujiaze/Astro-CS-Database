#!/usr/bin/env python3
"""独立 seed 复核：主实验中 |z| 最大的两个点（N=201 z=+3.0、N=5 z=+2.3）与偶 N 锚（N=20/100）。
三个独立 seed 重跑 kappa_mc 与 c_mad2，检验主结果是否 seed 偶然。
固定 seed：20260602 / 20260603 / 20260604。运行：python3 spotcheck_independent_seed.py"""
import json
import math
import os

import numpy as np

K_PI_HALF = math.pi / 2.0
K_MAD = 1.482602218505602
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "spotcheck_independent_seed.json")

CASES = [  # (N, R_total)
    (5, 4_000_000), (20, 4_000_000), (100, 1_000_000), (201, 1_000_000),
]
SEEDS = [20260602, 20260603, 20260604]


def run(n, R, seed):
    rng = np.random.default_rng(seed)
    ch = int(min(200_000, max(10_000, 4.0e7 / n)))
    s1 = s2 = sm = cnt = 0
    done = 0
    while done < R:
        m = min(ch, R - done)
        x = rng.standard_normal((m, n))
        med = np.median(x, axis=1)
        mad = np.median(np.abs(x - med[:, None]), axis=1) * K_MAD
        s1 += med.sum(); s2 += (med ** 2).sum(); sm += (mad ** 2).sum(); cnt += m
        done += m
    var_med = s2 / cnt - (s1 / cnt) ** 2
    return var_med * n, sm / cnt


def main():
    res = {"seeds": SEEDS, "cases": []}
    for n, R in CASES:
        rows = []
        for sd in SEEDS:
            kap, cm = run(n, R, sd)
            rows.append({"seed": sd, "kappa_mc": kap, "c_mad2": cm,
                         "ratio_A": kap / K_PI_HALF,
                         "ratio_B": K_PI_HALF * cm / kap})
        mean_k = sum(r["kappa_mc"] for r in rows) / len(rows)
        sd_k = float(np.std([r["kappa_mc"] for r in rows], ddof=1))
        res["cases"].append({"N": n, "R": R, "rows": rows,
                             "kappa_mean": mean_k, "kappa_sd_across_seeds": sd_k,
                             "ratio_A_mean": mean_k / K_PI_HALF})
        print(f"N={n:4d} R={R}: kappa mean={mean_k:.6f} sd={sd_k:.6f} "
              f"ratio_A={mean_k / K_PI_HALF:.6f} "
              f"ratio_B={[round(r['ratio_B'], 5) for r in rows]}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print("written:", OUT)


if __name__ == "__main__":
    main()
