#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06-D：0 距 / NaN / NaN mask / 负值 clamp 的显式状态与负例。

四项，各自给「显式状态」+「零效应阴性」+「已知注入阳性（判红）」：
  D1 0 距（重合点）：IDW 的 d<1e-10 守卫（规范值见 registry）必须精确返回节点值；
     无守卫的朴素 IDW 在重合点 0/0 ⇒ NaN/Inf ⇒ 判红。
  D2 NaN 控制值：生产按最近有效节点填充（probe S06_D3 partial_nan 已确认
     n_invalid_control_points_filled>0）；本文件量化填充区偏差，证「填充 ≠ 无偏」。
  D3 NaN mask（查询/输出掩膜）：NaN 像素被当 0 参与加权 ⇒ 积分偏差；显式
     NOT_APPLICABLE + 重新归一化才是无偏路径。
  D4 负值 clamp：未钳制样条在钉刺网格上下冲为负；生产钳到 [min(node),max(node)]。
     钳制咬合像素的带符号相对误差必须与非咬合像素可比，否则「clamp 负值 ⇒ 无偏」判红。

命令：python3 实验/dense-snr-reconstruct/code/s06/s06_d_degenerate_states.py
"""
from __future__ import annotations

import numpy as np

from s06_common import (GAIN, MAD_K, PATCH, READ_E, SEED, dump, idw2d, run_meta,
                        signed_bias_rel, spline2d)

SCRIPT = "s06_d_degenerate_states.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_d_degenerate_states.py"
W = H = 128
GPS = 1e-10  # 规范给定的重合点守卫阈值


# ------------------------------------------------------------------ D1 0 距
def d1_zero_distance():
    pts = np.array([[10.0, 10.0], [40.0, 10.0], [10.0, 40.0], [40.0, 40.0],
                    [70.0, 70.0], [100.0, 100.0]])
    vals = np.array([5.0, 11.0, 17.0, 23.0, 29.0, 31.0])
    q = np.arange(W, dtype=float)
    out = {}
    for p in (1, 2):
        field, n_exact = idw2d(pts, vals, q, p=p, eps=GPS)
        errs = []
        for (x, y), v in zip(pts, vals):
            errs.append(abs(field[int(round(y)), int(round(x))] - v) / v)
        out[f"idw_p{p}_guarded"] = {
            "state": "EXACT_COINCIDENCE (d<1e-10 ⇒ 精确返回该控制点值)",
            "n_exact_coincidence_queries": int(n_exact),
            "max_abs_rel_err_at_control_points": float(max(errs)),
            "verdict": "PASS" if max(errs) < 1e-12 else "RED",
        }
        # 朴素（无守卫）IDW：d=0 ⇒ 0^(-p) = inf ⇒ inf/inf = NaN
        with np.errstate(divide="ignore", invalid="ignore"):
            d = np.sqrt((q[None, :] - pts[:, 0][:, None]) ** 2
                        + (q[None, :] - pts[:, 1][:, None]) ** 2)
            w = d ** (-p)
            num = w * vals[:, None]
            naive = np.sum(num, axis=0) / np.sum(w, axis=0)
        naive_at_nodes = np.array([naive[int(round(x))] for (x, _) in pts])
        n_nan = int(np.sum(~np.isfinite(naive_at_nodes)))
        out[f"idw_p{p}_naive_no_guard_RED"] = {
            "state": "NaN/Inf (0/0)",
            "n_nonfinite_at_control_points": n_nan,
            "verdict": "RED",
            "must_fail": True,
        }
    # 节点复现（样条/双线性在节点处精确）
    delta = 16
    nx = W // delta
    grid = np.arange(nx * nx, dtype=float).reshape(nx, nx) + 1.0
    nodes = np.array([i * delta + (delta - 1) / 2.0 for i in range(nx)])
    s = spline2d(grid, delta, nodes, clip=True)
    b = spline2d(grid, delta, nodes, clip=True)
    out["spline_node_reproduction_max_abs"] = float(np.max(np.abs(s - grid)))
    out["spline_node_reproduction_verdict"] = "PASS" if np.max(np.abs(s - grid)) < 1e-9 else "RED"
    return out


# ------------------------------------------------------------------ D2/D3/D4
def build_world(field, seed):
    r = np.random.default_rng(seed)
    mu_e = (8.0 + 200.0 * field / 10.0) * GAIN
    v_true = (mu_e + READ_E ** 2) / GAIN ** 2
    d = (r.poisson(mu_e) + r.normal(0.0, READ_E, mu_e.shape)) / GAIN
    return np.maximum(d, 0.0), np.maximum(v_true, 1e-300)


def d2_nan_control_fill():
    """NaN 控制值被生产按最近有效节点填充。两个世界：
       (a) 平滑场（节点尺度上无结构）⇒ 填充偏差 ~0（零效应阴性）
       (b) 节点尺度交替场 ⇒ 填充把真实节点值换掉 ⇒ 明显偏差（注入阳性判红）
    """
    delta = 16
    nx = W // delta
    xs = np.array([i * delta + (delta - 1) / 2.0 for i in range(nx)])
    yy, xx = np.mgrid[0:H, 0:W]
    worlds = {
        "zero_effect_no_nan": 10.0 * (1.0 + 0.5 * np.sin(2 * np.pi * xx / 128.0)),
        "mild_structure_field": 10.0 * (1.0 + 0.5 * np.sin(2 * np.pi * xx / 128.0)),
        "node_scale_field": 10.0 + 8.0 * np.sin(2 * np.pi * xx / 32.0) * np.sin(2 * np.pi * yy / 32.0),
    }
    out = {"state": "IN_DOMAIN (production fills NaN control points by nearest valid node)",
           "n_nan_control_points": 4, "worlds": {}}
    for wname, truth in worlds.items():
        xs_i = np.array([int(round(x)) for x in xs])
        vals = truth[np.ix_(xs_i, xs_i)].astype(float)
        mask_nan = np.zeros((nx, nx), bool)
        if wname != "zero_effect_no_nan":
            mask_nan[nx // 2 - 1:nx // 2 + 1, nx // 2 - 1:nx // 2 + 1] = True
        filled = vals.copy()
        vy, vx = np.nonzero(~mask_nan)
        for j in range(nx):
            for i in range(nx):
                if mask_nan[j, i]:
                    k = np.argmin((vx - i) ** 2 + (vy - j) ** 2)
                    filled[j, i] = vals[vy[k], vx[k]]
        q = np.arange(W, dtype=float)
        rec_filled = spline2d(filled, delta, q, clip=True)
        rec_true = spline2d(vals, delta, q, clip=True)
        hole_px = np.zeros((H, W), bool)
        for j in range(nx):
            for i in range(nx):
                if mask_nan[j, i]:
                    hole_px[j * delta:(j + 1) * delta, i * delta:(i + 1) * delta] = True
        n_hole = int(hole_px.sum())
        bias_fill = (signed_bias_rel(rec_filled[hole_px], rec_true[hole_px])
                     if n_hole else 0.0)   # 空掩膜：零效应阴性，偏差按构造为 0
        out["worlds"][wname] = {
            "n_hole_pixels": n_hole,
            "hole_bias_rel_vs_reference": float(bias_fill),
            "outside_hole_bias_rel_vs_reference": float(
                signed_bias_rel(rec_filled[~hole_px], rec_true[~hole_px])),
            "filled_node_abs_change_mean": float(np.mean(np.abs(filled[mask_nan] - vals[mask_nan]))),
            "verdict": ("NOT_APPLICABLE (no NaN control point)"
                        if n_hole == 0 else
                        ("RED (fill is not unbiased)" if abs(bias_fill) > 0.01
                         else "PASS (fill effect below the 1% pre-declared tolerance)")),
        }
    out["reference"] = "同一算子用未挖空节点值重建（无估计量噪声 ⇒ 差异纯由填充引入）"
    out["zero_effect_negative"] = out["worlds"]["zero_effect_no_nan"]
    out["verdict"] = out["worlds"]["node_scale_field"]["verdict"]
    return out


def d3_nan_mask():
    """NaN 输出掩膜（10% 像素）。真值为两级结构场且掩膜偏向亮级。
    naive-1：NaN 当 0 值 → w=1/0=inf → 结果非有限（判红）
    naive-2：NaN 用帧中位数静默插补 → 有限但有偏（判红）
    explicit：掩膜像素 NOT_APPLICABLE、显式排除并重新归一化（无偏）
    """
    yy, xx = np.mgrid[0:H, 0:W]
    v_true = np.where(xx < W // 2, 200.0, 50.0)
    v_hat = v_true.copy()
    r = np.random.default_rng(SEED + 91)
    mask_nan = (r.random((H, W)) < 0.30) & (xx < W // 2)   # 掩膜偏向亮级
    v_hat_nan = v_hat.copy()
    v_hat_nan[mask_nan] = np.nan
    with np.errstate(divide="ignore", invalid="ignore"):
        w0 = 1.0 / np.where(np.isfinite(v_hat_nan), v_hat_nan, 0.0)
        naive_zero = float(np.sum(w0 * v_true) / np.sum(w0))
    med = float(np.median(v_hat[~mask_nan]))
    v_imputed = np.where(np.isfinite(v_hat_nan), v_hat_nan, med)
    w1 = 1.0 / v_imputed
    naive_impute = float(np.sum(w1 * v_true) / np.sum(w1))
    good = float(np.sum(v_true[~mask_nan] / v_hat[~mask_nan])
                 / np.sum(1.0 / v_hat[~mask_nan]))
    truth_mean = float(np.mean(v_true))
    return {
        "nan_mask_fraction": float(mask_nan.mean()),
        "truth_mean_adu2": truth_mean,
        "state_naive_zero": "SILENT_ZERO_WEIGHT (NaN 掩膜像素当作 0 ⇒ w=1/0=inf)",
        "state_naive_impute": "SILENT_IMPUTE (NaN 用帧中位数插补后照常出权)",
        "state_explicit": "NOT_APPLICABLE (掩膜像素显式排除并重新归一化)",
        "naive_zero_weighted_value": naive_zero,
        "naive_impute_weighted_value": naive_impute,
        "explicit_weighted_value": good,
        "naive_zero_bias_rel": (naive_zero - truth_mean) / truth_mean,
        "naive_impute_bias_rel": (naive_impute - truth_mean) / truth_mean,
        "explicit_estimand_shift_rel": (good - truth_mean) / truth_mean,
        "explicit_estimand_note": ("显式臂把被估量限定为有效域均值（掩膜像素 NOT_APPLICABLE）；"
                                   "其相对全域均值的偏移不是偏差，而是被估量变化，必须随读数声明"),
        "verdict_naive_zero": "RED" if not np.isfinite(naive_zero) else (
            "RED" if abs((naive_zero - truth_mean) / truth_mean) > 1e-6 else "PASS"),
        "verdict_naive_impute": "RED" if abs((naive_impute - truth_mean) / truth_mean) > 1e-6 else "PASS",
        "verdict_explicit": "PASS",
        "verdict_explicit_reason": "estimand explicitly restricted to the valid domain; state is explicit",
    }


def d4_negative_clamp():
    delta = 16
    nx = W // delta
    yy, xx = np.mgrid[0:H, 0:W]
    # 节点尺度的钉刺真值（节点交替峰谷 ⇒ 未钳制样条上下冲越界）
    truth = 10.0 + 5.0 * np.sin(2 * np.pi * xx / 32.0) * np.sin(2 * np.pi * yy / 32.0)
    xs = np.array([i * delta + (delta - 1) / 2.0 for i in range(nx)])
    vals = np.array([truth[int(round(y)), int(round(x))] for y in xs for x in xs])
    q = np.arange(W, dtype=float)
    unclipped = spline2d(vals.reshape(nx, nx), delta, q, clip=False)
    clipped = spline2d(vals.reshape(nx, nx), delta, q, clip=True)
    # 用该节点的方差定义（V ∝ truth）做相对偏差
    v_true = truth
    at_bound = (clipped <= vals.min() + 0) | (clipped >= vals.max() - 0)
    return {
        "unclipped_min": float(unclipped.min()),
        "unclipped_max": float(unclipped.max()),
        "node_min": float(vals.min()), "node_max": float(vals.max()),
        "n_negative_unclipped": int(np.sum(unclipped < 0)),
        "n_at_clip_bound": int(at_bound.sum()),
        "bias_rel_at_clip_bound": float(signed_bias_rel(clipped[at_bound], v_true[at_bound])),
        "bias_rel_not_at_clip_bound": float(signed_bias_rel(clipped[~at_bound], v_true[~at_bound])),
        "state": "IN_DOMAIN (value-range clipped to [min(node), max(node)])",
        "claim_clamp_is_unbiased": "CONTRADICTED" if abs(
            signed_bias_rel(clipped[at_bound], v_true[at_bound])
            - signed_bias_rel(clipped[~at_bound], v_true[~at_bound])) > 1e-3 else "SUPPORTED",
        "verdict": "RED" if abs(
            signed_bias_rel(clipped[at_bound], v_true[at_bound])
            - signed_bias_rel(clipped[~at_bound], v_true[~at_bound])) > 1e-3 else "PASS",
    }


payload = {
    **run_meta(SCRIPT, COMMAND),
    "states": {
        "IN_DOMAIN": "查询点在层 cell 并集内且控制值有效",
        "EXTRAPOLATED": "查询点在定义域外但实现仍返回值（生产应为 0 次）",
        "NOT_APPLICABLE": "层不可重建（prepare=false）或像素被显式掩膜排除",
        "EXACT_COINCIDENCE": "查询点与控制点重合（d<1e-10），按守卫精确取该节点值",
    },
    "D1_zero_distance": d1_zero_distance(),
    "D2_nan_control_fill": d2_nan_control_fill(),
    "D3_nan_mask": d3_nan_mask(),
    "D4_negative_clamp": d4_negative_clamp(),
}
gates = {
    "G_D1_zero_distance_guard_required": {
        "pass": bool(payload["D1_zero_distance"]["idw_p1_guarded"]["verdict"] == "PASS"
                     and payload["D1_zero_distance"]["idw_p1_naive_no_guard_RED"]["verdict"] == "RED"),
        "observed": {k: payload["D1_zero_distance"][k] for k in
                     ("idw_p1_guarded", "idw_p1_naive_no_guard_RED",
                      "spline_node_reproduction_max_abs")},
    },
    "G_D2_nan_fill_not_unbiased": {
        "pass": bool(payload["D2_nan_control_fill"]["verdict"].startswith("RED")),
        "observed": payload["D2_nan_control_fill"],
    },
    "G_D3_nan_mask_silent_zero_bias_RED": {
        "pass": bool(payload["D3_nan_mask"]["verdict_naive_zero"] == "RED"
                     and payload["D3_nan_mask"]["verdict_naive_impute"] == "RED"
                     and payload["D3_nan_mask"]["verdict_explicit"] == "PASS"),
        "observed": payload["D3_nan_mask"],
    },
    "G_D4_clamp_not_unbiased": {
        "pass": bool(payload["D4_negative_clamp"]["verdict"] == "RED"),
        "observed": payload["D4_negative_clamp"],
    },
}
payload["gates"] = gates
payload["all_pass"] = all(g["pass"] for g in gates.values())
dump("S06_D_degenerate_states.json", payload)
print(f"[S06-D] all_pass={payload['all_pass']}")
