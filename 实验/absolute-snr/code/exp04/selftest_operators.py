#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 算子自检（Oracle）：把每个算子放到解析可判的场景上，红/绿分明。

判据（全部非退化，且都允许翻红）：
  S1 平坦场精确性：真值无空间效应 ⇒ 任一算子输出必须逐像素等于该常数（atol=1e-9）。
  S2 线性场精确性：真值 σ 为坐标的线性函数 ⇒ 一阶精确算子（bilinear/bicubic_cc/
     spline_natural/photutils_zoom1/…）必须精确；nn 允许有界块状误差（登记其量级）。
  S3 一维对拍：spline_natural 在控制点行上必须与 scipy CubicSpline(bc_type='natural')
     逐点一致（rtol=1e-10）。
  S4 控制点处插值性：精确插值类算子（spline_natural/tps/photutils_zoom3 等）在
     控制点所在像素处应接近控制值（登记偏差，不设硬门）。
  S5 NaN 契约：nan_policy=nearest_valid 下不得输出 NaN（除非全无效）。
"""
from __future__ import annotations

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import operators as O  # noqa: E402


def _mk(D=32, H=256):
    ny = nx = H // D
    return D, H, ny, nx


def main():
    D, H, ny, nx = _mk()
    shape = (H, H)
    rows = []
    yy, xx = np.mgrid[0:H, 0:H]

    # S1 平坦场
    flat_ctrl = np.full((ny, nx), 3.75)
    s1_bad = []
    REGULARIZED = {"gpr_rbf", "gpr_matern32", "gpr_exp"}     # 带 nugget 的 GPR
    for name in O.OPERATORS:
        out, _, _ = O.run_operator(name, flat_ctrl, D, shape)
        err = float(np.nanmax(np.abs(out - 3.75)))
        tol = 1e-4 if name in REGULARIZED else 1e-9
        rows.append(dict(gate="S1_flat_exact", op=name, value=err, tol=tol, ok=bool(err <= tol)))
        if err > tol:
            s1_bad.append((name, err))

    # S2 线性场（内部区，排除两端 D px 的常数外推带）：
    #    线性精确算子必须逐像素精确；非精确算子只登记量级（不设硬门）。
    LIN_EXACT = {"bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
                 "photutils_zoom1", "photutils_zoom3", "tps", "tps_smooth"}
    # 用 16x16 控制网格（H=512）：scipy.ndimage.zoom 的样条预滤波是 IIR，8x8 网格上
    # 边界影响会传到网格中部，会把"预滤波边界效应"误记成"插值不精确"
    H2, ny2, nx2 = 512, 16, 16
    lin_true = 1.0 + 2e-3 * np.mgrid[0:H2, 0:H2][1]
    lin_ctrl = lin_true.reshape(ny2, D, nx2, D).transpose(0, 2, 1, 3).reshape(ny2, nx2, -1).mean(axis=2)
    #    排除最外两圈 cell：4/6 抽头模板（CC / spline zoom）在最外 cell 内触及边界，
    #    边界策略（clamp/reflect）本身就不是线性外推，属边界行为而非插值精度（见 §边界行为）
    #    4D 起评价：scipy.ndimage.zoom(order=3) 的样条预滤波是 IIR，边界效应按几何级数
    #    衰减（实测 3e-2@边缘 -> 3.6e-3 -> 9.6e-4 -> 2.6e-4 -> 6.9e-5@4D），
    #    该衰减本身是 photutils 默认插值器的真实性质，单列 LIN_NEAR 登记
    interior = np.zeros((H2, H2), bool)
    interior[4 * D:H2 - 4 * D, 4 * D:H2 - 4 * D] = True
    LIN_NEAR = {"photutils_zoom3": 1e-4}
    s2 = {}
    for name in O.OPERATORS:
        out, _, _ = O.run_operator(name, lin_ctrl, D, (H2, H2))
        err = float(np.nanmax(np.abs(out - lin_true)[interior]) / np.ptp(lin_true))
        s2[name] = err
        tol = LIN_NEAR.get(name, 1e-6 if name in LIN_EXACT else 0.2)
        rows.append(dict(gate="S2_linear_rel_maxerr_interior", op=name, value=err, tol=tol,
                         ok=bool(err <= tol)))

    # S3 一维对拍：自然边界三次样条
    from scipy.interpolate import CubicSpline
    rng = np.random.default_rng(7)
    v = rng.normal(size=nx)
    cs = CubicSpline(np.arange(nx, dtype=float), v, bc_type="natural")
    pos = O._axis_positions(H, nx, D)
    M = O._nat_second_deriv(v[None, :], axis=1)
    got = O._nat_eval(v[None, :], M, pos, axis=1)[0]
    want = cs(pos)
    s3 = float(np.max(np.abs(got - want)))
    rows.append(dict(gate="S3_natural_spline_vs_scipy", op="spline_natural", value=s3,
                     ok=bool(s3 <= 1e-10)))

    # S4 控制点处插值性
    rng2 = np.random.default_rng(11)
    c = 10.0 ** rng2.normal(0, 0.2, size=(ny, nx))
    cen_y = (np.arange(ny) * D + D // 2)
    cen_x = (np.arange(nx) * D + D // 2)
    for name in O.OPERATORS:
        out, _, _ = O.run_operator(name, c, D, shape)
        sub = out[np.ix_(cen_y, cen_x)]
        err = float(np.nanmedian(np.abs(np.log10(np.abs(sub) / c))))
        rows.append(dict(gate="S4_interp_at_nodes_dex", op=name, value=err, ok=bool(err < 0.5)))

    # S5 NaN 契约
    c2 = c.copy()
    c2[3:5, 4:7] = np.nan
    s5 = []
    for name in O.OPERATORS:
        out, nbad, _ = O.run_operator(name, c2, D, shape)
        n_nan = int(np.isnan(out).sum())
        s5.append((name, nbad, n_nan))
        rows.append(dict(gate="S5_no_nan_out", op=name, value=float(n_nan),
                         ok=bool(n_nan == 0 and nbad == 6)))

    # S6 生产算子等价：op_bilinear 必须与逐行镜像生产 C++ 语义的 op_bilinear_prod 逐位一致
    #    （生产出处 lib/algorithms/integration/v6/src/weight_chain.cpp:136 reconstruct_sparse_snr
    #     的 eval_bilinear：域内 clamp 到 [0, n-1] 后索引夹取 [0, n-2]；域外 fail-closed 不外推）
    rng3 = np.random.default_rng(23)
    s6 = {}
    for D6, n6 in ((16, 32), (32, 16), (64, 8)):
        c6 = 10.0 ** rng3.normal(0, 0.25, size=(n6, n6))
        sh6 = (D6 * n6, D6 * n6)
        a6, _, _ = O.run_operator("bilinear", c6, D6, sh6)
        b6, _, _ = O.run_operator("bilinear_prod", c6, D6, sh6)
        e6 = float(np.max(np.abs(a6 - b6)))
        s6["D%d" % D6] = e6
        rows.append(dict(gate="S6_production_bilinear_equiv", op="bilinear_vs_bilinear_prod_D%d" % D6,
                         value=e6, tol=1e-12, ok=bool(e6 <= 1e-12)))

    # S7 值域钳制契约：所有 *_clip 算子的输出必须落在有效控制值值域内，且不产生非物理 sigma<=0
    rng4 = np.random.default_rng(29)
    c7 = 10.0 ** rng4.normal(0, 0.35, size=(ny, nx))
    for name in O.OPERATORS:
        if not name.endswith("_clip"):
            continue
        out, _, _ = O.run_operator(name, c7, D, shape)
        lo, hi = float(np.nanmin(c7)), float(np.nanmax(c7))
        ok7 = bool(out.min() >= lo - 1e-12 and out.max() <= hi + 1e-12 and (out > 0).all())
        rows.append(dict(gate="S7_clip_contract", op=name,
                         value=float(max(lo - out.min(), out.max() - hi)), ok=ok7))

    ok_all = all(r["ok"] for r in rows)
    bad = [r for r in rows if not r["ok"]]
    print("S1 flat bad:", s1_bad)
    print("S2 rel maxerr:", {k: round(v, 9) for k, v in sorted(s2.items(), key=lambda kv: -kv[1])})
    print("S3 natural spline vs scipy:", s3)
    print("S5:", s5)
    print("S6 bilinear vs production semantics (max abs diff):", s6)
    # S4/S7 明细也落盘（审稿指出：只有汇总无法复核单算子数值）
    print("S4 interp at nodes (dex):",
          {r["op"]: round(r["value"], 6) for r in rows if r["gate"].startswith("S4")})
    print("S7 clip contract (signed range deviation; <=0 = within range):",
          {r["op"]: r["value"] for r in rows if r["gate"].startswith("S7")})
    print("TOTAL rows=%d  FAIL=%d" % (len(rows), len(bad)))
    for r in bad:
        print("  FAIL", r)
    print("ALL_PASS =", ok_all)
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
