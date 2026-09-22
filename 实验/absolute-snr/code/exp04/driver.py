#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 统一评价驱动：同一输入、同一判据，跑完所有臂 × 所有算子。

评价粒度：
  pixel   ：真值逐像素已知（解析面 / HST 物理仿真面）——算子间比较的主粒度
  patch32 ：真值只到 P=32 patch（testdata hold-out）——与 B3 同粒度，三臂可比

臂（arm）：
  nn/bilinear/... : 各重建算子（控制点来自 Δ 网格）
  dense_P32       : 稠密口径（P=32 patch 稳健 σ 场，块常数展开到像素）
  dense_at_D      : 稠密口径降到 Δ 粒度（= 控制点场块常数展开，等价"不插值的稀疏"）
  frame_median    : 帧级标量（Δ 网格控制值的中位数；SExtractor backsig 语义）
  frame_global_mad: 帧级标量（整帧未裁剪 1.4826×MAD；**已知错误构造**，B3 同款，只作对照）
"""
from __future__ import annotations

import os
import sys
import time

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)
import sci_b_common as C      # noqa: E402
import operators as O         # noqa: E402
import exp04_common as E      # noqa: E402


def _row(face, mode, gran, D, arm, res, t_s, n_ctrl_used, extra=None):
    r = dict(face=face, mode=mode, granularity=gran, delta_px=D, arm=arm,
             n_eval=res.get("n_eval"), rmse_log_rho=res.get("rmse_log_rho"),
             level_bias_dex=res.get("level_bias_dex"), eff_loss=res.get("eff_loss"),
             recon_s=(None if t_s is None else float(t_s)),
             n_ctrl=n_ctrl_used,
             skipped=bool(res.get("skipped", False)))
    if extra:
        r.update(extra)
    return r


def run_face(face_name, img, truth_px, meta, ops, deltas, modes=("oracle", "estimated"),
             gran="pixel", img_stat=None, truth_patch=None, timeit=True, eps_ref=None,
             patch_agg_ops=True):
    """在一个数据面上跑全部臂。

    img        : 估计量输入像素（真实帧或仿真帧）
    truth_px   : 逐像素真值 σ（gran='pixel' 时必需；gran='patch32' 时可为 None）
    truth_patch: P=32 patch 真值（gran='patch32' 时必需）
    img_stat   : 统计帧级 MAD 用的像素集（默认 img；hold-out 时传族 0）
    """
    rows = []
    H, W = img.shape
    mask_px = np.isfinite(truth_px) if truth_px is not None else np.isfinite(img)
    src_stat = img if img_stat is None else img_stat

    # 帧级错误构造（整帧未裁剪 MAD）——与 Δ 无关，只算一次
    vals = src_stat[np.isfinite(src_stat)]
    frame_mad = float(C.K_MAD_TO_SIGMA * np.median(np.abs(vals - np.median(vals))))

    for D in deltas:
        ctrl_est = E.ctrl_estimated(src_stat, D)
        ctrl_or = E.ctrl_oracle(truth_px, D) if truth_px is not None else None
        n_used = int(np.isfinite(ctrl_est).sum())
        frame_med = float(np.nanmedian(ctrl_est))

        # ---- 稠密臂（与 Δ 无关，但每 Δ 重复登记以便同表比较；时间只记一次）----
        dense32 = C.sigma_field_fast(src_stat, E.P_DENSE)
        dense32_px = E.block_constant(dense32, E.P_DENSE, (H, W))

        # ---- 非算子臂 ----
        arms_fixed = [
            ("dense_P32", dense32_px, 0.0),
            ("dense_at_D", E.block_constant(ctrl_est, D, (H, W)), 0.0),
            ("frame_median", np.full((H, W), frame_med), 0.0),
            ("frame_global_mad", np.full((H, W), frame_mad), 0.0),
        ]
        for arm, field, t in arms_fixed:
            for mode in ("fixed",):
                res = _eval_gran(field, truth_px, truth_patch, mask_px, gran, D, patch_agg_ops)
                if eps_ref is not None and np.isfinite(res.get("rmse_log_rho", np.nan)):
                    res["rmse_corrected"] = float(np.sqrt(max(res["rmse_log_rho"] ** 2 - eps_ref ** 2, 0.0)))
                rows.append(_row(face_name, mode, gran, D, arm, res, t, n_used))

        # ---- 各重建算子 ----
        for mode in modes:
            ctrl = ctrl_or if (mode == "oracle" and ctrl_or is not None) else ctrl_est
            if ctrl is None:
                continue
            for op in ops:
                try:
                    out, nbad_in, t = O.run_operator(op, ctrl, D, (H, W))
                except Exception as ex:                     # 算子失败必须显式登记
                    rows.append(_row(face_name, mode, gran, D, op,
                                     dict(n_eval=0, skipped=True), None, n_used,
                                     extra=dict(error="%s: %s" % (type(ex).__name__, ex))))
                    continue
                res = _eval_gran(out, truth_px, truth_patch, mask_px, gran, D, patch_agg_ops)
                if eps_ref is not None and np.isfinite(res.get("rmse_log_rho", np.nan)):
                    res["rmse_corrected"] = float(np.sqrt(max(res["rmse_log_rho"] ** 2 - eps_ref ** 2, 0.0)))
                rows.append(_row(face_name, mode, gran, D, op, res, t if timeit else None,
                                 n_used, extra=dict(n_ctrl_nan_in=nbad_in)))
    return rows


def _eval_gran(field, truth_px, truth_patch, mask_px, gran, D, patch_agg_ops):
    H, W = field.shape
    if gran == "pixel":
        est = np.asarray(field, np.float64)
        return E.eval_field(est, truth_px, mask_px)
    # patch32：估计量按 P=32 patch 聚合（中位数），真值取 patch 真值场
    est = E.patch_agg(np.asarray(field, np.float64), E.P_DENSE)
    tp = truth_patch
    m = np.isfinite(est) & np.isfinite(tp) & (est > 0) & (tp > 0)
    return E.metrics(est, tp, m)


def row_index(rows):
    return {(r["face"], r["mode"], r["granularity"], r["delta_px"], r["arm"]): r for r in rows}


def summary_by(rows, key="arm"):
    out = {}
    for r in rows:
        out.setdefault(r[key], []).append(r)
    return out
