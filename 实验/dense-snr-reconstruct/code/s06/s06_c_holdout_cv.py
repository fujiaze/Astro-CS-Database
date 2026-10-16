#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06-C：六臂留出交叉验证 + 逐场型复核「dense 非一律最优」。

被估量：逐像素方差 V(x) [ADU²]（与 SNR 表示等价：SNR = F_ref/sqrt(V)）；
        权重 w = 1/V̂ [ADU⁻²]；真值 v_true = V_true（物理前向世界）。
度量：留出方差比 var_ratio = Var_w/Var_opt = E_eff+1（唯一口径 Var_opt = 1/Σ(1/v_true)）；
      重建偏差 bias_rel = mean((V̂−V_true)/V_true)；域外占比 out_of_domain_pct。

六臂（同一留出集合、同一 train 信息集）：
  1 natural_spline_clip  可分离自然双三次样条 + 钳到节点值域（生产默认算子）
  2 idw_p1               IDW p=1，重合点守卫 d<1e-10，同一 train 节点
  3 idw_p2               IDW p=2（规范定位：无噪/光滑极限，备选）
  4 bilinear             规则网格双线性
  5 constant             train 节点中位数（帧级常数）
  6 dense                生产 dense 口径：8×8 patch 1.4826·MAD 的平方 → cell 内平面拟合
                         → 节点方差 → 样条+钳制（估计器与算子完整链）
  （1–5 使用 train 节点上的**真值**（oracle 控制值），6 使用**生产估计器**；
    这就是 P4 报告里 E_eff_oracle 与 E_eff_dense_recon 的区分。）

留出协议 P（节点级留出）：native Δ 网格上取 (i 偶 且 j 偶) 的节点作 train 子网格
（间距 2Δ，其 cell 并集 = [−0.5, nx·Δ−0.5]，与 native 定义域相同），
在 (i 奇 或 j 奇) 的 native cell 内的像素上评分 ⇒ 无空间泄漏。
协议 Q：2×2 / 3×3 控制网格（历史域外 55.6% / 43.8%）——节点太少，节点级留出
        NOT_APPLICABLE，只报域内像素的 var_ratio/bias 与域外占比 + 静默外推对照臂。
协议 R：Δ=16 中心 3×3 节点空洞——hole 内像素不参与 train；对照
        hole_excluded（判 NOT_APPLICABLE）与 hole_nearest_fill（生产 NaN 填充行为）。
子实验 S：dense vs cell vs frame 逐场型全量对照（复核历史 E_dense=0.577 vs E_cell=0.00903）。

命令：python3 实验/dense-snr-reconstruct/code/s06/s06_c_holdout_cv.py
"""
from __future__ import annotations

import numpy as np

from s06_common import (DELTA, F_REF, GAIN, MAD_K, PATCH, READ_E, SEED, E_eff,
                        bilinear2d, dex_rmse, domain_box, dump, idw2d, make_fields,
                        out_of_domain_mask, patch_mad_var, rms_rel, run_meta,
                        signed_bias_rel, spline2d, var_ratio)

SCRIPT = "s06_c_holdout_cv.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_c_holdout_cv.py"

W = H = 192
BG_ADU = 8.0
AMP_ADU = 200.0
rng = np.random.default_rng(SEED)


# ------------------------------------------------------------------ 世界
def build_world(field, seed):
    r = np.random.default_rng(seed)
    mu_e = (BG_ADU + AMP_ADU * field / 10.0) * GAIN
    d = (r.poisson(mu_e) + r.normal(0.0, READ_E, mu_e.shape)) / GAIN
    v_true = (mu_e + READ_E ** 2) / GAIN ** 2
    return np.maximum(d, 0.0), np.maximum(v_true, 1e-300)


def cell_nodes_from_pixels(d, delta, patch, cells_ij):
    """生产控制值配方：每 cell 内 patch MAD² → cell 内平面拟合到 cell 中心节点。

    cells_ij: 允许使用的 cell 索引列表 [(i,j), ...]（其余 cell 的像素不进估计 ⇒ 无泄漏）。
    返回 dict {(i,j): node_value}。
    """
    per = delta // patch
    out = {}
    for (i, j) in cells_ij:
        y0, x0 = j * delta, i * delta
        blk = d[y0:y0 + delta, x0:x0 + delta]
        if blk.shape != (delta, delta):
            continue
        b = blk.reshape(per, patch, per, patch)
        med = np.median(b, axis=(1, 3))
        mad = np.median(np.abs(b - med[:, None, :, None]), axis=(1, 3))
        vp = (MAD_K * mad) ** 2
        idx = (np.arange(per) + 0.5) * patch
        X, Y = np.meshgrid(idx, idx)
        A = np.column_stack([np.ones(per * per), X.ravel(), Y.ravel()])
        coef = np.linalg.pinv(A) @ vp.ravel()
        node = (delta - 1) / 2.0
        out[(i, j)] = float(coef[0] + coef[1] * node + coef[2] * node)
    return out


def native_grid_values_from_nodes(node_map, nx, ny):
    g = np.full((ny, nx), np.nan)
    for (i, j), v in node_map.items():
        g[j, i] = v
    return g


def arms_on_coarse(train_vals, train_pts, q, delta_c, nx_c, p1, p2, node0):
    """在粗网格上构造五条 oracle-节点臂 + 逐像素输出（q = arange(W)）。

    node0 = 粗网格首节点坐标（train 子网格不是 native 的 (delta-1)/2 偏移）。"""
    grid = np.asarray(train_vals, float).reshape(nx_c, nx_c)
    out = {}
    out["natural_spline_clip"] = spline2d(grid, delta_c, q, clip=True, node0=node0)
    out["bilinear"] = bilinear2d(grid, delta_c, q, node0=node0)
    n1, _ = idw2d(train_pts, np.asarray(train_vals, float), q, p=p1)
    n2, _ = idw2d(train_pts, np.asarray(train_vals, float), q, p=p2)
    out["idw_p1"] = n1
    out["idw_p2"] = n2
    out["constant"] = np.full((len(q), len(q)), float(np.median(train_vals)))
    return out


def metrics(est_field, v_true, test_mask, w_extra=None):
    m = np.asarray(test_mask, bool)
    v = v_true[m]
    est = np.asarray(est_field, float)[m]
    good = np.isfinite(est) & (est > 0)
    out = {"n_test": int(m.sum()), "n_test_valid": int(good.sum()),
           "frac_nonpositive_or_nan": float(1.0 - good.mean())}
    if good.sum() < 4:
        out.update(var_ratio=None, E_eff=None, bias_rel=None, rms_rel=None, dex_rmse=None)
        return out
    vv = v[good]
    w = 1.0 / est[good]
    out["var_ratio"] = var_ratio(w, vv)
    out["E_eff"] = E_eff(w, vv)
    out["bias_rel"] = signed_bias_rel(est[good], vv)
    out["rms_rel"] = rms_rel(est[good], vv)
    out["dex_rmse"] = dex_rmse(est[good], vv)
    return out


records = []
fields = make_fields(W, H)

# ----------------------------------------------------------- 协议 P：节点级留出
for fname, field in fields.items():
    d, v_true = build_world(field, seed=SEED + 1000 + len(fname))
    for delta in (8, 16, 32, 64):
        nx = (W - 1) // delta + 1
        ny = (H - 1) // delta + 1
        even_i = [i for i in range(nx) if i % 2 == 0]
        even_j = [j for j in range(ny) if j % 2 == 0]
        train_cells = [(i, j) for j in even_j for i in even_i]
        if len(even_i) < 2 or len(even_j) < 2:
            records.append({"protocol": "P", "field": fname, "delta": delta,
                            "status": "NOT_APPLICABLE",
                            "reason": "coarse train grid has <2 nodes per axis"})
            continue
        nx_c, ny_c = len(even_i), len(even_j)
        delta_c = 2 * delta
        xs = np.array([i * delta + (delta - 1) / 2.0 for i in even_i])
        ys = np.array([j * delta + (delta - 1) / 2.0 for j in even_j])
        train_pts = np.array([(x, y) for y in ys for x in xs])
        # oracle 节点值 = 真值在 train 节点处
        train_vals_true = np.array([v_true[int(round(y)), int(round(x))] for (x, y) in train_pts])
        # 生产估计器节点值（只用 train cell 的像素）
        est_nodes = cell_nodes_from_pixels(d, delta, PATCH, train_cells)
        train_vals_est = np.array([est_nodes.get((i, j), np.nan)
                                   for j in even_j for i in even_i])
        q = np.arange(W, dtype=float)
        arms = arms_on_coarse(train_vals_true, train_pts, q, delta_c, nx_c, 1, 2,
                              node0=float(xs.min()))
        if np.all(np.isfinite(train_vals_est)):
            grid_est = train_vals_est.reshape(ny_c, nx_c)
            arms["dense"] = spline2d(grid_est, delta_c, q, clip=True, node0=float(xs.min()))
        else:
            arms["dense"] = np.full((W, H), np.nan)

        # test mask：native cell 的 (i 奇 或 j 奇)，且限制在 train 节点张成区间的**内部**
        # （train 节点之外是外推段，不属于插值 CV 的评分域；域外/边界另行由协议 Q 覆盖）
        test = np.zeros((H, W), bool)
        for j in range(ny):
            for i in range(nx):
                if (i % 2 == 1) or (j % 2 == 1):
                    test[j * delta:(j + 1) * delta, i * delta:(i + 1) * delta] = True
        ood = out_of_domain_mask(nx_c, ny_c, delta_c, delta_c, W, H)
        yy_i, xx_i = np.mgrid[0:H, 0:W]
        test = test & (~ood) & (xx_i >= xs.min()) & (xx_i <= xs.max()) \
                         & (yy_i >= ys.min()) & (yy_i <= ys.max())

        rec = {"protocol": "P", "field": fname, "delta": delta, "status": "PASS",
               "native_grid": [nx, ny], "coarse_train_grid": [nx_c, ny_c],
               "cv_quality_warning": ("coarse train grid has only %d nodes per axis; "
                                      "holes in the train span are extrapolation, not interpolation"
                                      % nx_c) if nx_c < 4 else None,
               "train_node_span_px": [float(xs.min()), float(xs.max())],
               "coarse_delta": delta_c, "n_train_nodes": int(len(train_pts)),
               "n_test_pixels": int(test.sum()),
               "out_of_domain_pct": float(ood.mean() * 100.0),
               "arm_node_source": "1-5: oracle node values; 6: production estimator (patch MAD^2 + cell plane fit)",
               "arms": {}}
        for aname, afield in arms.items():
            rec["arms"][aname] = metrics(afield, v_true, test)
        records.append(rec)

# ----------------------------------------------------------- 协议 Q：2×2 / 3×3
for fname in ("constant", "slow_linear", "nebula", "blobs"):
    field = fields[fname]
    d, v_true = build_world(field, seed=SEED + 2000 + len(fname))
    for (nx, ny, dx, dy, tag) in ((2, 2, 64.0, 64.0, "2x2"), (3, 3, 48.0, 48.0, "3x3")):
        xs = np.arange(nx) * dx + (dx - 1) / 2.0
        ys = np.arange(ny) * dy + (dy - 1) / 2.0
        pts = np.array([(x, y) for y in ys for x in xs])
        vals_true = np.array([v_true[int(round(y)), int(round(x))] for (x, y) in pts])
        ood = out_of_domain_mask(nx, ny, dx, dy, W, H)
        in_dom = ~ood
        q = np.arange(W, dtype=float)
        arms = {}
        arms["natural_spline_clip"] = spline2d(vals_true.reshape(ny, nx), dx, q,
                                               clip=True, gox=0.0, goy=0.0)
        arms["bilinear"] = bilinear2d(vals_true.reshape(ny, nx), dx, q)
        arms["idw_p1"] = idw2d(pts, vals_true, q, p=1)[0]
        arms["idw_p2"] = idw2d(pts, vals_true, q, p=2)[0]
        arms["constant"] = np.full((W, H), float(np.median(vals_true)))
        rec = {"protocol": "Q", "field": fname, "delta": None, "config": tag,
               "status": "PASS", "native_grid": [nx, ny],
               "out_of_domain_pct": float(ood.mean() * 100.0),
               "n_out_of_domain_pixels": int(ood.sum()),
               "node_level_holdout": "NOT_APPLICABLE (too few nodes)",
               "arms": {}}
        for aname, afield in arms.items():
            rec["arms"][aname] = metrics(afield, v_true, in_dom)
        # 静默自然外推对照：同一算子（含值域钳制）在**域外**像素上不做
        # fail-closed、直接取边界三次式外推值（alter 实现/朴素消费方的行为）
        naive = spline2d(vals_true.reshape(ny, nx), dx, q, clip=True)
        naive_unclipped = spline2d(vals_true.reshape(ny, nx), dx, q, clip=False)
        rec["naive_extrapolation_out_of_domain"] = {
            "clip_on": metrics(naive, v_true, ood),
            "clip_off": metrics(naive_unclipped, v_true, ood),
            "domain_aware_state": "NOT_APPLICABLE (eval fail-closed; no value, no weight)",
            "distance_strata_bias_rel_clip_on": {},
        }
        # 离域边界的距离分层（x 向与 y 向取较大者，单位 px）
        lo_x, hi_x, lo_y, hi_y = domain_box(nx, ny, dx, dy)
        yy, xx = np.mgrid[0:H, 0:W]
        dist = np.maximum(np.maximum(lo_x - xx, xx - hi_x), np.maximum(lo_y - yy, yy - hi_y))
        strata = {}
        for ring in ((0.5, 8.0), (8.0, 32.0), (32.0, 128.0)):
            m = ood & (dist > ring[0]) & (dist <= ring[1])
            if m.sum() >= 16:
                strata[f"{ring[0]:g}-{ring[1]:g}px"] = {
                    "n": int(m.sum()),
                    "bias_rel": signed_bias_rel(naive[m], v_true[m]),
                    "rms_rel": rms_rel(naive[m], v_true[m]),
                }
        rec["naive_extrapolation_out_of_domain"]["distance_strata_bias_rel_clip_on"] = strata
        records.append(rec)

# ----------------------------------------------------------- 协议 R：中心空洞
for fname in ("slow_linear", "nebula"):
    field = fields[fname]
    d, v_true = build_world(field, seed=SEED + 3000 + len(fname))
    delta = 16
    nx = (W - 1) // delta + 1
    ny = (H - 1) // delta + 1
    hole = [(i, j) for j in range(ny // 2 - 1, ny // 2 + 2)
                   for i in range(nx // 2 - 1, nx // 2 + 2)]
    xs = np.array([i * delta + (delta - 1) / 2.0 for i in range(nx)])
    ys = np.array([j * delta + (delta - 1) / 2.0 for j in range(ny)])
    all_cells = [(i, j) for j in range(ny) for i in range(nx)]
    train_cells = [c for c in all_cells if c not in hole]
    est = cell_nodes_from_pixels(d, delta, PATCH, train_cells)
    # 空洞填充臂：最近有效节点（生产 NaN 填充行为）
    hole_px = np.zeros((H, W), bool)
    for (i, j) in hole:
        hole_px[j * delta:(j + 1) * delta, i * delta:(i + 1) * delta] = True
    filled = native_grid_values_from_nodes(est, nx, ny)
    # 最近有效节点填充
    ii, jj = np.meshgrid(np.arange(nx), np.arange(ny))
    valid = np.isfinite(filled)
    if valid.any():
        vy, vx = np.nonzero(valid)
        for j in range(ny):
            for i in range(nx):
                if not np.isfinite(filled[j, i]):
                    k = np.argmin((vx - i) ** 2 + (vy - j) ** 2)
                    filled[j, i] = filled[vy[k], vx[k]]
    q = np.arange(W, dtype=float)
    arm_fill = spline2d(filled, delta, q, clip=True)
    # 排除臂：空洞内像素不计分（显式 NOT_APPLICABLE）
    rec = {"protocol": "R", "field": fname, "delta": delta, "status": "PASS",
           "hole_cells": [[int(i), int(j)] for (i, j) in hole],
           "n_hole_pixels": int(hole_px.sum()),
           "production_fill_state": "n_invalid_control_points_filled > 0（最近有效节点填充，见 probe S06_D3）",
           "arms": {
               "natural_spline_clip_excluded_hole": {
                   **metrics(arm_fill, v_true, ~hole_px),
                   "metrics_scope": "outside_hole（空洞按显式 NOT_APPLICABLE 处理，空洞内不计分）",
                   "hole_state": "NOT_APPLICABLE（空洞像素没有重建值）"},
               "hole_nearest_fill": {
                   **metrics(arm_fill, v_true, hole_px),
                   "metrics_scope": "inside_hole（生产把 NaN 节点按最近有效节点静默填充）",
                   "hole_state": "IN_DOMAIN（填充值被当作有效重建值）"},
           }}
    rec["hole_fill_bias_detected_RED"] = bool(
        rec["arms"]["hole_nearest_fill"]["bias_rel"] is not None
        and abs(rec["arms"]["hole_nearest_fill"]["bias_rel"]) > 0.01)
    records.append(rec)

# ------------------------------------------------- 子实验 S：dense vs cell vs frame
s_rows = []
for fname, field in fields.items():
    d, v_true = build_world(field, seed=SEED + 4000 + len(fname))
    delta = DELTA
    nx = (W - 1) // delta + 1
    ny = (H - 1) // delta + 1
    cells = [(i, j) for j in range(ny) for i in range(nx)]
    est = cell_nodes_from_pixels(d, delta, PATCH, cells)
    npl = native_grid_values_from_nodes(est, nx, ny)
    q = np.arange(W, dtype=float)
    rec_field = spline2d(npl, delta, q, clip=True)
    cell_field = np.repeat(np.repeat(npl, delta, 0), delta, 1)[:H, :W]
    frame_field = np.full((H, W), float(np.median(npl)))
    shuf = spline2d(rng.permutation(npl.ravel()).reshape(npl.shape), delta, q, clip=True)
    m = np.ones((H, W), bool)
    row = {"field": fname, "delta": delta, "n_nodes": [nx, ny],
           "v_true_dynamic_range": float(v_true.max() / v_true.min()),
           "E_eff_frame": E_eff(1.0 / frame_field[m], v_true[m]),
           "E_eff_cell": E_eff(1.0 / cell_field[m], v_true[m]),
           "E_eff_dense": E_eff(1.0 / rec_field[m], v_true[m]),
           "E_eff_dense_shuffled_RED": E_eff(1.0 / shuf[m], v_true[m]),
           "dex_rmse_frame": dex_rmse(frame_field, v_true),
           "dex_rmse_cell": dex_rmse(cell_field, v_true),
           "dex_rmse_dense": dex_rmse(rec_field, v_true),
           "n_at_node_value_bounds": int(np.sum(rec_field >= npl.max()) + np.sum(rec_field <= npl.min())),
           "n_at_node_value_bounds_note": "上界计数：包含真实相等，不是咬合次数"}
    row["dense_best"] = bool(row["E_eff_dense"] < min(row["E_eff_frame"], row["E_eff_cell"]))
    row["cell_best"] = bool(row["E_eff_cell"] < min(row["E_eff_frame"], row["E_eff_dense"]))
    row["frame_best"] = bool(row["E_eff_frame"] < min(row["E_eff_cell"], row["E_eff_dense"]))
    s_rows.append(row)

payload = {
    **run_meta(SCRIPT, COMMAND),
    "world": {
        "mu_e_expression": "(BG_ADU + AMP_ADU*field/10) * GAIN   [e-]",
        "detector": "d = (Poisson(mu_e) + N(0, read_noise_e)) / GAIN   [ADU]",
        "v_true_expression": "(mu_e + read_noise_e^2) / GAIN^2   [ADU^2]",
        "BG_ADU": BG_ADU, "AMP_ADU": AMP_ADU,
        "estimator": "8x8 patch, v = (1.4826*MAD)^2, then in-cell plane fit at cell center",
    },
    "protocols": {
        "P": "node-level holdout: train = (i even & j even) nodes at spacing 2Δ; score on pixels in cells with (i odd | j odd)",
        "Q": "2x2 / 3x3 control grids: node holdout NOT_APPLICABLE; in-domain metrics + out_of_domain_pct + naive silent-extrapolation arm",
        "R": "Δ=16 with a central 3x3-node hole: hole_excluded vs production nearest-fill",
        "S": "full-data dense vs cell vs frame per field type (re-check of historical E_dense=0.577 vs E_cell=0.00903)",
    },
    "records": records,
    "subexperiment_S_dense_vs_cell_vs_frame": s_rows,
}

# ---------------------------------------------------------------- 判据
p_records = [r for r in records if r.get("protocol") == "P" and r.get("status") == "PASS"]
q_records = [r for r in records if r.get("protocol") == "Q"]
r_records = [r for r in records if r.get("protocol") == "R"]

gates = {}

# G-C1 恒定场零效应阴性：帧级常数臂必须恰好最优（E=0），稠密/样条不得凭空占优
flat = [r for r in p_records if r["field"] == "constant" and r["delta"] == 16]
if flat:
    a = flat[0]["arms"]
    gates["G_C1_flat_zero_effect_frame_exactly_optimal"] = {
        "observed": {"var_ratio_constant": a["constant"]["var_ratio"],
                     "var_ratio_spline": a["natural_spline_clip"]["var_ratio"],
                     "var_ratio_dense": a["dense"]["var_ratio"]},
        "pass": bool(abs(a["constant"]["var_ratio"] - 1.0) < 1e-9
                     and a["natural_spline_clip"]["var_ratio"] >= 1.0 - 1e-9
                     and a["dense"]["var_ratio"] >= 1.0 - 1e-9),
        "meaning": "常真值场上等权/常数臂最优；样条与稠密口径不得低于 1（幽灵占优）",
    }

# G-C2 注入阳性：打乱控制点值必须使留出方差比变差（结构场）
#     对照用 **oracle 节点值**（排除估计器噪声），保证是纯算子/注入对照。
shuffle_rows = []
for r in p_records:
    if r["field"] not in ("slow_linear", "strong_gradient", "edge", "nebula", "blobs"):
        continue
    if r["delta"] != 16:
        continue
    shuffle_rows.append({"field": r["field"],
                         "var_ratio_spline": r["arms"]["natural_spline_clip"]["var_ratio"]})
    r["shuffle_control_oracle_nodes"] = None  # 由下方显式重算填充（见 dump 前的补充）
shuf_better = []
for r in p_records:
    if r["field"] not in ("slow_linear", "strong_gradient", "edge", "nebula", "blobs"):
        continue
    if r["delta"] != 16:
        continue
    field = fields[r["field"]]
    d, v_true = build_world(field, seed=SEED + 1000 + len(r["field"]))
    delta = 16
    nx = (W - 1) // delta + 1
    ny = (H - 1) // delta + 1
    even_i = [i for i in range(nx) if i % 2 == 0]
    even_j = [j for j in range(ny) if j % 2 == 0]
    xs = np.array([i * delta + (delta - 1) / 2.0 for i in even_i])
    ys = np.array([j * delta + (delta - 1) / 2.0 for j in even_j])
    vals = np.array([v_true[int(round(y)), int(round(x))] for y in ys for x in xs])
    nx_c, ny_c = len(even_i), len(even_j)
    q = np.arange(W, dtype=float)
    good = spline2d(vals.reshape(ny_c, nx_c), 2 * delta, q, clip=True,
                    node0=float(xs.min()))
    shuf_vals = rng.permutation(vals).reshape(ny_c, nx_c)
    shuf = spline2d(shuf_vals, 2 * delta, q, clip=True, node0=float(xs.min()))
    test = np.zeros((H, W), bool)
    for j in range(ny):
        for i in range(nx):
            if (i % 2 == 1) or (j % 2 == 1):
                test[j * delta:(j + 1) * delta, i * delta:(i + 1) * delta] = True
    ood = out_of_domain_mask(nx_c, ny_c, 2 * delta, 2 * delta, W, H)
    test = test & (~ood)
    # 空间注入对照的评分点 = **留出节点本身**（(i 奇 且 j 奇) 处）：对「控制点空间
    # 位置被破坏」最敏感。好臂在留出节点上是插值误差，置换臂在那里输出一个随机节点值。
    odd_i = [i for i in range(nx) if i % 2 == 1 and even_i[0] < i < even_i[-1]]
    odd_j = [j for j in range(ny) if j % 2 == 1 and even_j[0] < j < even_j[-1]]
    ho_pts = [(i * delta + (delta - 1) / 2.0, j * delta + (delta - 1) / 2.0)
              for j in odd_j for i in odd_i]
    ho_xi = np.array([int(round(p[0])) for p in ho_pts])
    ho_yi = np.array([int(round(p[1])) for p in ho_pts])
    ho_mask = np.zeros((H, W), bool)
    ho_mask[ho_yi, ho_xi] = True
    r["shuffle_control_oracle_nodes"] = {
        "score_set": "held-out interior nodes (i odd & j odd, strictly inside the train node span)",
        "n_score_points": int(len(ho_pts)),
        "good": metrics(good, v_true, ho_mask),
        "shuffled": metrics(shuf, v_true, ho_mask),
        "good_at_test_pixels": metrics(good, v_true, test),
        "shuffled_at_test_pixels": metrics(shuf, v_true, test),
    }
    gv = r["shuffle_control_oracle_nodes"]["good"]
    sv = r["shuffle_control_oracle_nodes"]["shuffled"]
    shuf_better.append({
        "field": r["field"],
        "var_ratio_good": gv["var_ratio"], "var_ratio_shuffled": sv["var_ratio"],
        "ratio_var_ratio_shuffled_over_good": (sv["var_ratio"] / gv["var_ratio"]) if gv["var_ratio"] else None,
        "dex_rmse_good": gv["dex_rmse"], "dex_rmse_shuffled": sv["dex_rmse"],
        "ratio_dex_shuffled_over_good": (sv["dex_rmse"] / gv["dex_rmse"]) if gv["dex_rmse"] else None})
# 预声明分层：oracle 臂自身的 dex_rmse ≤ 0.05（≈12% 重建误差）⇒ 该场在 Δ 格上可表示，
# 注入对照必须判红（比值 > 3）；否则该场本身不可由 Δ 格表示，算子内禀插值误差主导，
# 注入对照判别力弱 —— 两种情形都如实报告，不调宽可表示域的门。
representable = [s for s in shuf_better if s["dex_rmse_good"] is not None and s["dex_rmse_good"] <= 0.05]
weak_control = [s for s in shuf_better if s["dex_rmse_good"] is None or s["dex_rmse_good"] > 0.05]
gates["G_C2_shuffled_nodes_worse_RED"] = {
    "observed": {
        "rows": shuf_better,
        "representable_fields_ratio_gt_3": {s["field"]: s["ratio_dex_shuffled_over_good"] for s in representable},
        "weak_control_fields": {s["field"]: {"ratio_dex": s["ratio_dex_shuffled_over_good"],
                                            "oracle_dex_rmse": s["dex_rmse_good"]} for s in weak_control},
    },
    "criterion": ("在留出节点上，oracle 节点值随机置换后 dex_rmse 至少劣化 3 倍；"
                  "分层：oracle 臂 dex_rmse ≤ 0.05 的场（Δ 格可表示）必须满足"),
    "pass": bool(representable and all(s["ratio_dex_shuffled_over_good"] > 3.0 for s in representable)),
    "meaning": "控制点空间位置的注入损坏在可表示场上必须使空间水平误差变差 3 倍以上（已知注入阳性判红）；"
               "在 Δ 格不可表示的场上该对照判别力弱，如实登记为弱对照而非删除",
}
findings_metric_discrimination = {
    "id": "F_C1_var_ratio_not_sensitive_to_node_permutation",
    "observation": {s["field"]: {"ratio_var_ratio": s["ratio_var_ratio_shuffled_over_good"],
                                 "ratio_dex": s["ratio_dex_shuffled_over_good"]}
                    for s in shuf_better},
    "conclusion": ("留出方差比 var_ratio = E_eff+1 是**分布型**判据（只依赖 w 与 v 的联合分布），"
                   "对控制点的空间置换不敏感（5 场中若干场劣化 <5%、个别场反而更优）；"
                   "空间损坏必须用 dex_rmse/rms_rel 这类**水平型**判据才能判红 ⇒ "
                   "var_ratio 与 dex 判据必须成对报告（与 T07 §3.2、H8 的乘性免疫结论一致）。"),
}

# G-C3 域外可检测性：2×2 / 3×3 域外占比必须与生产探针一致且 > 0
q22 = [r for r in q_records if r.get("config") == "2x2"]
q33 = [r for r in q_records if r.get("config") == "3x3"]
gates["G_C3_out_of_domain_pct_matches_production"] = {
    "observed": {"2x2": [round(r["out_of_domain_pct"], 4) for r in q22],
                 "3x3": [round(r["out_of_domain_pct"], 4) for r in q33],
                 "production_probe": {"2x2": 55.55555555555556, "3x3": 43.75}},
    "pass": bool(q22 and q33
                 and all(abs(r["out_of_domain_pct"] - 55.55555555555556) < 1e-6 for r in q22)
                 and all(abs(r["out_of_domain_pct"] - 43.75) < 1e-6 for r in q33)),
    "meaning": "2×2/3×3 域外占比与生产 eval 状态机逐位一致（55.56% / 43.75%）",
}

# G-C4 静默自然外推在域外像素上必须产生可判红的偏差（域内 vs 域外分层）
sil = []
for r in q_records:
    ov = r["naive_extrapolation_out_of_domain"]
    sil.append({"field": r["field"], "config": r["config"],
                "out_of_domain_pct": r["out_of_domain_pct"],
                "in_domain_bias_rel_spline": r["arms"]["natural_spline_clip"]["bias_rel"],
                "naive_ood_bias_rel_clip_on": ov["clip_on"]["bias_rel"],
                "naive_ood_bias_rel_clip_off": ov["clip_off"]["bias_rel"],
                "naive_ood_rms_rel_clip_on": ov["clip_on"]["rms_rel"],
                "strata_abs_bias_rel": {k: abs(v["bias_rel"])
                                        for k, v in sorted(ov["distance_strata_bias_rel_clip_on"].items())}})
structured = [s for s in sil if s["field"] != "constant"]
constant = [s for s in sil if s["field"] == "constant"]
# 预声明判据：结构场域外 |bias_rel| > 2%（距离分层读数一并落盘，不单独设单调门）
gates["G_C4_silent_extrapolation_biased_on_out_of_domain_RED"] = {
    "observed": sil,
    "criterion": "结构场域外 |bias_rel| > 2%（域外份额 43.75–55.56%）；恒定场为预声明零效应子例",
    "zero_effect_subcase": [{"field": s["field"], "config": s["config"],
                             "naive_ood_bias_rel": s["naive_ood_bias_rel_clip_on"]} for s in constant],
    "pass": bool(structured and all(abs(s["naive_ood_bias_rel_clip_on"]) > 0.02 for s in structured)),
    "meaning": "静默自然外推在 43.75–55.56% 的视场像素上给出 >2% 系统偏差 ⇒ 可被判红；"
               "域内路径对这些像素一律 NOT_APPLICABLE（无值、无权重）",
}

# G-C5 空洞最近填充引入偏差 ⇒ 判红（不得把填充当无偏）
hole_bias = [{"field": r["field"],
              "hole_bias_rel": r["arms"]["hole_nearest_fill"]["bias_rel"],
              "hole_rms_rel": r["arms"]["hole_nearest_fill"]["rms_rel"],
              "detected": r["hole_fill_bias_detected_RED"]} for r in r_records]
gates["G_C5_hole_nearest_fill_is_biased_RED"] = {
    "observed": hole_bias,
    "pass": bool(hole_bias and all(h["detected"] for h in hole_bias)),
    "meaning": "生产把 NaN 节点按最近有效节点填充：空洞区重建偏差超 1% 即判红",
}

# G-C6 dense 非一律最优（逐场型）
not_always = [r["field"] for r in s_rows if not r["dense_best"]]
gates["G_C6_dense_not_always_best"] = {
    "observed": {"dense_best_fields": [r["field"] for r in s_rows if r["dense_best"]],
                 "cell_or_frame_best_fields": not_always,
                 "rows": {r["field"]: {"E_frame": r["E_eff_frame"], "E_cell": r["E_eff_cell"],
                                       "E_dense": r["E_eff_dense"]} for r in s_rows}},
    "pass": bool(len(not_always) >= 1),
    "meaning": "至少一个场型上 dense 不是最优 ⇒ 否定「dense 一律最优」",
}

payload["gates"] = gates
payload["findings"] = [findings_metric_discrimination]
payload["all_pass"] = all(g["pass"] for g in gates.values())

dump("S06_C_holdout_cv.json", payload)
print(f"[S06-C] all_pass={payload['all_pass']}  records={len(records)}")
for r in s_rows:
    print(f"  S {r['field']:<16} E_frame={r['E_eff_frame']:.4g} E_cell={r['E_eff_cell']:.4g} "
          f"E_dense={r['E_eff_dense']:.4g} dense_best={r['dense_best']} cell_best={r['cell_best']}")
