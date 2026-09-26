#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-11: P-CST-24 (IDW reconstruction parameters idw_power=2.0, K=16,
degeneracy threshold gamma < 1e-10)

A literature leg  : Shepard, D. 1968, "A two-dimensional interpolation function
                    for irregularly-spaced data", Proc. 23rd ACM National
                    Conference, 517-524 (inverse-distance weighting).
B experiment leg  : sparse control points on the Delta = 64 cell-centre grid of
                    a 512^2 frame (8x8 = 64 points, the P2 -> P4 interface),
                    sampled from a known smooth SNR field; IDW reconstruction at
                    NON-node test points (cell corners), scan power p in
                    {1, 2, 3} and neighbourhood K in {4, 8, 16, 32, 64}.
                    NEGATIVE CONTROL: constant field => reconstruction error
                    exactly 0 for every (p, K) (true-zero-effect => metric zero).
                    Degeneracy guard: all control points at the query location
                    (distance 0) => weighted mean of coincident values; the
                    gamma < 1e-10 threshold separates "all distances degenerate"
                    from normal operation.
C seed             : SEED = 20260926.
D outputs          : results/exp11_idw_reconstruction.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp11_idw_reconstruction.json"
DELTA = 64
N = 512

def true_snr_field(x, y):
    """smooth SNR field: ridge + gradient + plateau (typical frame-depth field)."""
    return (18.0 + 0.006 * x - 0.004 * y
            + 22.0 * np.exp(-(((x - 300.0) ** 2) + ((y - 220.0) ** 2)) / (2.0 * 90.0 ** 2))
            + 10.0 * np.exp(-(((x - 120.0) ** 2) + ((y - 380.0) ** 2)) / (2.0 * 60.0 ** 2)))

def idw(nodes, vals, q, power, K):
    d = np.hypot(nodes[:, 0] - q[0], nodes[:, 1] - q[1])
    if K < len(d):
        idx = np.argpartition(d, K)[:K]
        d, v = d[idx], vals[idx]
    else:
        v = vals
    if np.any(d < 1e-10):
        return float(v[d < 1e-10].mean())
    w = 1.0 / d ** power
    return float(np.sum(w * v) / np.sum(w))

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "delta": DELTA}

    # control points: cell centres of the 8x8 grid (P2 interface, cell_center_v1)
    idx = np.arange(8)
    cx = idx * DELTA + (DELTA - 1) / 2.0
    gx, gy = np.meshgrid(cx, cx)
    nodes = np.column_stack([gx.ravel(), gy.ravel()]).astype(float)
    vals = true_snr_field(nodes[:, 0], nodes[:, 1])

    # test points: cell corners (never nodes)
    jc = rng.uniform(0.0, DELTA - 1.0, size=(400, 2))
    test = (jc + (rng.integers(0, 8, size=(400, 2)) * DELTA)).clip(0, N - 1)
    truth = true_snr_field(test[:, 0], test[:, 1])

    grid = []
    for p in (1.0, 2.0, 3.0):
        for K in (4, 8, 16, 32, 64):
            est = np.array([idw(nodes, vals, q, p, K) for q in test])
            rmse = float(np.sqrt(np.mean((est - truth) ** 2)))
            grid.append({"power": p, "K": K, "rmse": rmse,
                         "rel_rmse": float(rmse / truth.std())})
    res["rmse_grid"] = grid

    # noisy control points (sigma_c = 2, typical control-point scatter):
    # larger K averages the noise down -> the K trade-off becomes visible
    vals_noisy = vals + 2.0 * rng.standard_normal(vals.shape)
    grid_noisy = []
    for p in (1.0, 2.0, 3.0):
        for K in (4, 8, 16, 32, 64):
            est = np.array([idw(nodes, vals_noisy, q, p, K) for q in test])
            rmse = float(np.sqrt(np.mean((est - truth) ** 2)))
            grid_noisy.append({"power": p, "K": K, "rmse": rmse,
                               "rel_rmse": float(rmse / truth.std())})
    res["rmse_grid_noisy_control_points_sigma2"] = grid_noisy

    # negative control: constant field
    vals_const = np.full(len(nodes), 25.0)
    err_const = []
    for p in (1.0, 2.0, 3.0):
        for K in (4, 8, 16, 32, 64):
            est = np.array([idw(nodes, vals_const, q, p, K) for q in test])
            err_const.append(float(np.max(np.abs(est - 25.0))))
    res["negative_control_constant_field_max_abs_err"] = float(np.max(err_const))

    # degeneracy guard: query coincides with a node
    q0 = nodes[3]
    est0 = idw(nodes, vals, q0, 2.0, 16)
    res["node_coincidence_recovery_abs_err"] = float(abs(est0 - vals[3]))

    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
