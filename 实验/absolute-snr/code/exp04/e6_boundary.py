#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 边界行为：无效区（NaN）、剧烈变化区、图像边界。

判据：
  B1 无效区：注入 NaN 控制点块 ⇒ 输出是否有限、NaN 泄漏数、误差传播半径（dex 阈 0.05）。
  B2 剧烈变化区：阶跃 σ 场 ⇒ 超调（超出控制值值域的幅度，以阶跃幅度为单位）与振铃。
  B3 图像边界：最外 Δ 带的相对误差 vs 内部区。
  B4 nan_policy 对比：nearest_valid（SExtractor 语义）vs frame_median。

输出：results/exp04_e6_boundary.json
"""
from __future__ import annotations

import argparse
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

OPS = ["nn", "bilinear", "bilinear_prod", "bicubic_cc", "spline_natural", "sextractor_spline",
       "photutils_zoom3", "photutils_zoom1", "idw", "gpr_rbf", "gpr_matern32", "gpr_exp"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e6_boundary.json"))
    a = ap.parse_args()
    t0 = time.time()
    D, H = 64, E.CROP
    ny = nx = H // D

    # ---------- B1 无效区 ----------
    sigma = E.synth_sigma_face(64.0, 0.15, H, seed_off=801)
    truth = sigma
    ctrl_ref = E.ctrl_oracle(truth, D)
    ctrl_bad = ctrl_ref.copy()
    ctrl_bad[3:6, 4:7] = np.nan           # 中心 3x3 cell 无效块
    ctrl_bad[0, :] = np.nan               # 顶行无效
    yy, xx = np.mgrid[0:ny, 0:nx]
    invalid = ~np.isfinite(ctrl_bad)
    b1 = []
    for op in OPS:
        for policy in ("nearest_valid", "frame_median"):
            try:
                out, nbad, _ = O.run_operator(op, ctrl_bad, D, (H, H), nan_policy=policy)
            except Exception as ex:
                b1.append(dict(op=op, nan_policy=policy, error="%s: %s" % (type(ex).__name__, ex)))
                continue
            ref, _, _ = O.run_operator(op, ctrl_ref, D, (H, H))
            finite = bool(np.isfinite(out).all())
            dev = np.abs(np.log10(np.abs(out) / np.abs(ref)))
            bad_px = np.isfinite(dev) & (dev > 0.05)
            # 无效区之外的污染足迹（真正需要登记的传播范围）
            inv_px = E.block_constant(invalid.astype(float), D, (H, H)) > 0
            outside = bad_px & ~inv_px
            # 传播半径：坏像素到最近无效 cell 的最大切比雪夫距离（cell 单位）
            radius = 0.0
            if bad_px.any():
                by, bx = np.nonzero(bad_px)
                cy = np.clip(by // D, 0, ny - 1); cx = np.clip(bx // D, 0, nx - 1)
                iy, ix = np.nonzero(invalid)
                if iy.size:
                    d = np.maximum(np.abs(cy[:, None] - iy[None, :]),
                                   np.abs(cx[:, None] - ix[None, :])).min(axis=1)
                    radius = float(d.max())
            b1.append(dict(op=op, nan_policy=policy, n_nan_ctrl_in=nbad, finite=finite,
                           nan_leak=int((~np.isfinite(out)).sum()),
                           dev_px=int(bad_px.sum()), dev_px_outside_invalid=int(outside.sum()),
                           prop_radius_cell=radius,
                           max_dev_outside_dex=(float(np.nanmax(dev[outside])) if outside.any() else 0.0),
                           max_dev_dex=float(np.nanmax(dev)) if np.isfinite(dev).any() else None))

    # ---------- B2 剧烈变化区（阶跃） ----------
    step = np.where(np.arange(nx)[None, :] < nx // 2, 1.0, 3.0) * np.ones((ny, 1))
    b2 = []
    for op in OPS:
        try:
            out, _, _ = O.run_operator(op, step, D, (H, H))
        except Exception as ex:
            b2.append(dict(op=op, error="%s: %s" % (type(ex).__name__, ex))); continue
        lo, hi = float(np.min(step)), float(np.max(step))
        amp = hi - lo
        overshoot = float(max(out.max() - hi, lo - out.min()) / amp)
        # 振铃：远离阶跃面（>2 cell）处相对理想台阶的偏差峰值
        col = np.arange(H)
        far = np.abs(col - (nx // 2) * D) > 2 * D
        ideal = np.where(col < (nx // 2) * D, lo, hi)      # (W,)
        ring = float(np.nanmax(np.abs(out[:, far] - ideal[far][None, :])) / amp)
        b2.append(dict(op=op, overshoot_frac_amp=overshoot, ringing_frac_amp=ring,
                       out_min=float(out.min()), out_max=float(out.max())))

    # ---------- B3 图像边界 ----------
    b3 = []
    interior = np.zeros((H, H), bool); interior[D:H - D, D:H - D] = True
    border = ~interior
    for op in OPS:
        try:
            out, _, _ = O.run_operator(op, ctrl_ref, D, (H, H))
        except Exception as ex:
            b3.append(dict(op=op, error="%s: %s" % (type(ex).__name__, ex))); continue
        d = np.abs(np.log10(np.abs(out) / truth))
        b3.append(dict(op=op,
                       rmse_interior_dex=float(np.sqrt(np.nanmean(d[interior] ** 2))),
                       rmse_border_dex=float(np.sqrt(np.nanmean(d[border] ** 2))),
                       bias_border_dex=float(np.nanmedian(np.log10(np.abs(out[border]) / truth[border])))))
    obj = dict(experiment="SCI-B / EXP-04 边界行为", frozen_config=dict(
        delta_px=D, crop=H, nan_policy_default="nearest_valid", seed_base=E.SEED_BASE),
        B1_invalid=b1, B2_step=b2, B3_border=b3, generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
