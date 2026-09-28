#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4-FIX-02: 控制点估计量偏差（P4-M01）+ Δ 上界守卫（P4-M02）+ 节点相位（P4-M04）。

A 段 · 控制点估计量偏差（按生产同法复现）
  生产链（P2 侧，只读核对）：variance 平面场由 patch 稳健方差（1.4826·MAD）经
  var = a + b·x + c·y 平面拟合给出；P4 需要的却是 **cell 平均方差**。三段偏差：
    (1) 节点平面值 vs 节点真值         —— 估计量统计偏差（patch 噪声 + 平面拟合）
    (2) 节点真值   vs cell 平均真值     —— 确定性 cell 聚合差
    (3) 节点平面值 vs cell 平均真值     —— P4 实际吃到的合并偏差
  负例：平坦方差场 ⇒ 三段偏差在 MC 噪声内归零（判绿）。
B 段 · Δ 上界守卫（p4_delta_guard.delta_guard，按输入几何 + 误差预算 + 内存/几何一致性）
  正例 Δ=64 平滑域 PASS；负例 Δ=256 高对比域 RED（route3 存档 E∈[1.52,1.89] 佐证）。
C 段 · 节点相位（生产 cell_center_v1 = i·Δ+(Δ−1)/2，Δ=64 ⇒ 31.5）
  四档相位：生产 31.5 / route1 (Δ−1)//2=31 / route3 Δ//2=32 / 角点 0，同 fixture 取数。
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from p4_delta_guard import delta_guard  # noqa: E402

SEED = 20260928
UNIT = Path(__file__).resolve().parents[2]
RESULTS = UNIT / "results" / "fix"
RESULTS.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(SEED)
res = {"seed": SEED}


def E_eff(w, v):
    var_w = np.sum(w ** 2 * v) / np.sum(w) ** 2
    return float(var_w * np.sum(1.0 / v) - 1.0)


# ------------------------------------------------- A: 控制点估计量偏差
N, G, SIGMA_SLOW, DELTA = 512, 1.0, 5.0, 64
ALPHA = 3.4424  # FWHM≈3 px 的 Moffat4 α（FWHM = 2α√(2^{1/4}−1)）


def var_field(structured=True, amp=0.0):
    yy, xx = np.mgrid[0:N, 0:N]
    if not structured:
        return np.full((N, N), SIGMA_SLOW ** 2)
    src = amp * (1.0 + ((xx - N / 2) ** 2 + (yy - N / 2) ** 2) / ALPHA ** 2) ** -4
    grad = 0.25 * SIGMA_SLOW ** 2 * np.sin(2 * np.pi * xx / (4 * DELTA))
    return np.clip(SIGMA_SLOW ** 2 + src / G + grad, 1e-6, None)


def patch_estimates(v, patch=8):
    xs, ys, vs = [], [], []
    for y0 in range(0, N - patch + 1, patch):
        for x0 in range(0, N - patch + 1, patch):
            mu = float(np.mean(v[y0:y0 + patch, x0:x0 + patch]))   # 该 patch 的真方差
            s = np.sqrt(max(mu, 1e-12))
            # 生产同法：在 patch 内像素样本上取稳健 σ 估计（1.4826·MAD）再平方得方差估计。
            # 注意：MAD 必须作用于 **patch 的像素样本**（方差 = 真值），
            # 不是作用于「估计量自身的抽样分布」——后者会引入 ~126 倍的系统性标度错（见 P4 订正报告）。
            smp = rng.normal(0.0, s, patch * patch)
            m = float(np.median(smp))
            mad = 1.482602218505602 * float(np.median(np.abs(smp - m)))
            xs.append(x0 + (patch - 1) / 2.0)
            ys.append(y0 + (patch - 1) / 2.0)
            vs.append(mad ** 2)
    return np.array(xs), np.array(ys), np.array(vs)


def plane_fit(xs, ys, vs):
    A = np.column_stack([np.ones_like(xs), xs, ys])
    coef, *_ = np.linalg.lstsq(A, vs, rcond=None)
    return coef


def plane_eval(coef, x, y):
    return coef[0] + coef[1] * x + coef[2] * y


def cell_nodes(delta):
    idx = np.arange(N // delta) * delta + (delta - 1) // 2
    return [(ix, iy) for iy in idx for ix in idx]


A = {}
for tag, structured, amp in [("structured_field", True, 4000.0), ("flat_control", False, 0.0)]:
    v = var_field(structured, amp)
    xs, ys, vs = patch_estimates(v)
    coef = plane_fit(xs, ys, vs)
    nodes = cell_nodes(DELTA)
    nodev = np.array([v[iy, ix] for ix, iy in nodes], dtype=float)
    planev = np.array([plane_eval(coef, ix, iy) for ix, iy in nodes], dtype=float)
    cellv = np.array([float(np.mean(v[max(iy - DELTA // 2, 0):iy + DELTA // 2 + 1,
                                     max(ix - DELTA // 2, 0):ix + DELTA // 2 + 1]))
                      for ix, iy in nodes])
    dex = lambda a, b: float(np.median(np.abs(np.log10(np.maximum(a, 1e-12) / np.maximum(b, 1e-12)))))
    A[tag] = {
        "n_nodes": int(len(nodes)),
        "bias_node_plane_vs_node_truth": {"E_eff": E_eff(1.0 / planev, nodev), "dex": dex(planev, nodev)},
        "bias_node_truth_vs_cell_mean": {"E_eff": E_eff(1.0 / nodev, cellv), "dex": dex(nodev, cellv)},
        "bias_combined_plane_vs_cell_mean": {"E_eff": E_eff(1.0 / planev, cellv), "dex": dex(planev, cellv)},
    }
A["rule"] = "平坦场三段偏差在 MC 噪声内归零（判绿）；结构场合并偏差非零并按量级登记"
A["pass"] = bool(A["flat_control"]["bias_combined_plane_vs_cell_mean"]["dex"] < 0.02
                 and A["structured_field"]["bias_combined_plane_vs_cell_mean"]["dex"] > 0.0)
res["A_control_point_estimator_bias"] = A

# ------------------------------------------------- B: Δ 上界守卫
guard_cases = {}
for tag, delta, ell, q, vlev in [("smooth_delta64", 64, 128.0, 3.84e-5 * 25.0, 25.0),
                                 ("smooth_delta128", 128, 128.0, 3.84e-5 * 25.0, 25.0),
                                 ("highcontrast_delta256", 256, 128.0, 0.40, 25.0),
                                 ("flat_delta512", 512, 128.0, 0.0, 25.0)]:
    ok, dmax, why = delta_guard(delta, ell, q, vlev, 4096, 4096)
    guard_cases[tag] = {"ok": ok, "delta_max": dmax, "why": why}
res["B_delta_guard"] = {
    "cases": guard_cases,
    "archive_evidence": "results/route3/exp03_sparse_dense_reconstruction.json::domains.highcontrast.256 "
                        "五算子 E=1.515-1.894（完全失效）",
    "pass": bool(guard_cases["smooth_delta64"]["ok"] and not guard_cases["highcontrast_delta256"]["ok"]
                 and not guard_cases["flat_delta512"]["ok"]),
}


# ------------------------------------------------- C: 节点相位
FRAME = 512
yy, xx = np.mgrid[0:FRAME, 0:FRAME]
truth = 2.0 * np.exp(0.5 * (0.6 * np.sin(2 * np.pi * xx / 97.0) + 0.4 * np.sin(2 * np.pi * yy / 41.0 + 1.1)))


def spline1d_coeffs(vals):
    m = len(vals)
    Aa = np.zeros((m, m))
    bb = np.zeros(m)
    Aa[0, 0] = 1.0
    for i in range(1, m - 1):
        Aa[i, i - 1] = 1.0
        Aa[i, i] = 4.0
        Aa[i, i + 1] = 1.0
        bb[i] = 6.0 * (vals[i - 1] - 2.0 * vals[i] + vals[i + 1])
    Aa[-1, -1] = 1.0
    return np.linalg.solve(Aa, bb)


def spline1d_eval(vals, c, x, delta, origin):
    m = len(vals)
    t = (x - origin) / delta
    i = np.clip(np.floor(t).astype(int), 0, m - 2)
    f = t - i
    cpad = np.concatenate([[0.0], c, [0.0]])
    return ((1.0 - f) * vals[i] + f * vals[i + 1]
            + ((1.0 - f) ** 3 - (1.0 - f)) * cpad[i + 1] / 6.0
            + (f ** 3 - f) * cpad[i + 2] / 6.0)


def spline2d_eval(g, delta, origin, qx, qy):
    m, n = g.shape
    cx = [spline1d_coeffs(g[i, :]) for i in range(m)]
    tmp = np.array([spline1d_eval(g[i, :], cx[i], qx, delta, origin) for i in range(m)])  # (m, nx)
    cy = [spline1d_coeffs(tmp[:, j]) for j in range(tmp.shape[1])]
    out = np.array([spline1d_eval(tmp[:, j], cy[j], qy, delta, origin) for j in range(tmp.shape[1])])
    return out.T


q = np.arange(0, FRAME, 4).astype(float)
tq = truth[np.ix_(q.astype(int), q.astype(int))]
phase_cases = {"production_cell_center_31p5": (DELTA - 1) / 2.0,
               "route1_floor_31": (DELTA - 1) // 2,
               "route3_32": DELTA // 2,
               "corner_0": 0.0}
phase_out = {}
for tag, origin in phase_cases.items():
    idx = np.arange(FRAME // DELTA) * DELTA + (DELTA - 1) // 2
    g = truth[np.ix_(idx, idx)]
    rec = spline2d_eval(g, DELTA, float(origin), q, q)
    dex = float(np.sqrt(np.mean((np.log10(np.maximum(rec, 1e-9)) - np.log10(tq)) ** 2)))
    phase_out[tag] = {"node_origin_px": float(origin), "rmse_dex": dex,
                      "E_eff": E_eff(1.0 / rec.ravel() ** 2, tq.ravel() ** 2)}
res["C_node_phase"] = {
    "cases": phase_out,
    "production_convention": "cell_center_v1: 节点 i·Δ+(Δ−1)/2（Δ=64 ⇒ 31.5，半整数）",
    "evidence": "weight_chain.cpp:473-486 与 sparse_snr_layer.schema.json node_placement",
    "pass": bool(phase_out["production_cell_center_31p5"]["rmse_dex"] < phase_out["corner_0"]["rmse_dex"]),
}
res["all_pass"] = bool(A["pass"] and res["B_delta_guard"]["pass"] and res["C_node_phase"]["pass"])
(RESULTS / "fix02_boundaries_estimator_phase.json").write_text(
    json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
print(json.dumps(res, indent=2, ensure_ascii=False))
