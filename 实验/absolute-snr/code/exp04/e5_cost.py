#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 代价：重建计算量（含 N 标度）与存储量。

输出：results/exp04_e5_cost.json
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

OPS = list(O.OPERATORS.keys())
# 全局核算子（tps/rbf）在 N=4096 上单次 >200 s，按预算上限截断并显式登记
BUDGET_S = 60.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e5_cost.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    H = 1024
    img = E.rng(21).normal(0.0, 1.0, size=(H, H))
    deltas = [256, 64] if a.quick else [256, 128, 64, 32, 16]
    rows = []
    for D in deltas:
        ctrl = C.sigma_field_fast(img, D)
        N = int(np.isfinite(ctrl).sum())
        for op in OPS:
            t1 = time.perf_counter()
            try:
                out, _, _ = O.run_operator(op, ctrl, D, (H, H))
                dt = time.perf_counter() - t1
                rows.append(dict(delta_px=D, n_ctrl=N, op=op, recon_s=float(dt),
                                 per_px_us=float(dt / (H * H) * 1e6),
                                 finite=bool(np.isfinite(out).all()), measured=True))
            except Exception as ex:
                rows.append(dict(delta_px=D, n_ctrl=N, op=op, recon_s=None, measured=False,
                                 error="%s: %s" % (type(ex).__name__, ex)))
            if time.perf_counter() - t1 > BUDGET_S:
                print("  [budget] %s D=%d 超单点预算 %.0fs，后续 Δ 不再测该算子"
                      % (op, D, BUDGET_S), flush=True)
            print("  D=%3d N=%5d %-18s %s" % (D, N, op,
                  ("%.3f s" % rows[-1]["recon_s"]) if rows[-1]["measured"] else "SKIP"), flush=True)
    # 输出尺寸标度（固定 Δ=64，H=512/1024/2048）
    size_rows = []
    for H2 in (512, 1024, 2048):
        img2 = E.rng(22).normal(0.0, 1.0, size=(H2, H2))
        ctrl = C.sigma_field_fast(img2, 64)
        for op in ("bilinear", "spline_natural", "photutils_zoom3", "gpr_rbf"):
            t1 = time.perf_counter()
            O.run_operator(op, ctrl, 64, (H2, H2))
            size_rows.append(dict(H=H2, op=op, recon_s=float(time.perf_counter() - t1)))
    # N 标度拟合（每个算子单独拟合 log t vs log N）
    fit = {}
    for op in OPS:
        rr = [r for r in rows if r["op"] == op and r["measured"]]
        if len(rr) >= 3:
            x = np.log10([r["n_ctrl"] for r in rr]); y = np.log10([r["recon_s"] for r in rr])
            sl = float(np.polyfit(x, y, 1)[0])
            fit[op] = dict(n_points=len(rr), loglog_slope_vs_N=sl,
                           note="t ∝ N^%.2f（1024² 输出、Δ 扫描）" % sl)
        else:
            fit[op] = dict(n_points=len(rr), loglog_slope_vs_N=None,
                           note="测量点不足（成本预算截断），未拟合")
    storage = []
    n_px_prod = E.FRAME_PX ** 2
    storage.append(dict(kind="dense", delta_px=None, n_ctrl=n_px_prod,
                        bytes_per_frame=E.storage_bytes("dense"),
                        MiB=E.storage_bytes("dense") / 1048576.0,
                        over_budget=E.storage_bytes("dense") / E.BUDGET_BYTES))
    storage.append(dict(kind="frame", delta_px=None, n_ctrl=1, bytes_per_frame=E.storage_bytes("frame"),
                        MiB=E.storage_bytes("frame") / 1048576.0,
                        over_budget=E.storage_bytes("frame") / E.BUDGET_BYTES))
    for D in E.DELTA_GRID:
        b = E.storage_bytes("sparse", D)
        storage.append(dict(kind="sparse", delta_px=D, n_ctrl=E.n_ctrl(E.FRAME_PX, D),
                            bytes_per_frame=b, MiB=b / 1048576.0, over_budget=b / E.BUDGET_BYTES))
    obj = dict(experiment="SCI-B / EXP-04 代价：重建计算量与存储量",
               frozen_config=dict(H=H, deltas=deltas, budget_s=BUDGET_S,
                                  frame_px=E.FRAME_PX, budget_bytes=E.BUDGET_BYTES,
                                  bytes_ctrl_sparse=E.BYTES_CTRL_SPARSE,
                                  bytes_px_dense=E.BYTES_PX_DENSE, seed_base=E.SEED_BASE),
               rows=rows, size_scaling=size_rows, n_scaling_fit=fit, storage=storage,
               generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
