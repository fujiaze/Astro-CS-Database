#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4-Δ 上界守卫（P4-M02 订正）：按输入几何 + 内存预算导出的自适应上界。

背景（SCI-704 审查件 P4-M02）：生产 Δ 只受 schema 约束 Δ ≥ 1，无上界守卫；
route3 存档实测高对比域 Δ=256 时五个算子 E ∈ [1.52, 1.89]（完全失效域）。

守卫的两个独立上界（取小）：
  ① 几何/可分辨上界 Δ_geom = ℓ（场相关长度；Δ/ℓ ≲ 1 是本单元「可分辨域」判据。
     更保守可取 ℓ/2 = Nyquist 采样：每相关长度两个控制点）。
  ② 误差预算上界 Δ_err：确定性插值偏置律 bias ≈ q·Δ²/8（route2 exp03 实测
     q·Δ²/8/25 = 3.84e-5·(Δ/16)²），令相对偏置 ≤ tol_rel ⇒ Δ_err = sqrt(8·tol_rel·v̄/q̄)。
  ③ 内存预算上界 Δ_mem：重建工作面 O(W·H) 与 Δ 无关，但**控制格**与**平面拟合样本**
     随 Δ 反比增长；守卫按"控制点数 ≥ 1"与"patch 样本 ≥ min_samples"的几何一致性给出
     Δ ≤ min(W,H)/4（保证每维至少 4 个控制点）。
判据：Δ ≤ min(Δ_geom, Δ_err, Δ_mem) ⇒ PASS（可重建）；否则 RED（fail-closed，须重验）。
"""
import math


def delta_guard(delta, ell_px, q_field, v_level, w_px, h_px,
                tol_rel=0.05, nyquist=True, min_cells_per_axis=4):
    """返回 (ok, delta_max, reasons dict)。q_field: 场曲率量纲 [v/px²]（v 为方差电平）。"""
    d_geom = (ell_px / 2.0) if nyquist else ell_px
    if q_field > 0.0:
        d_err = math.sqrt(8.0 * tol_rel * max(v_level, 1e-30) / q_field)
    else:
        d_err = float("inf")
    d_mem = min(w_px, h_px) / float(min_cells_per_axis)
    d_max = min(d_geom, d_err, d_mem)
    reasons = {"delta_geom": d_geom, "delta_err": d_err, "delta_mem": d_mem,
               "delta_max": d_max, "delta": delta}
    ok = bool(delta <= d_max)
    reasons["verdict"] = "PASS" if ok else "RED"
    return ok, d_max, reasons


def _selftest():
    cases = [
        # (delta, ell, q, v, W, H, expect)
        (64, 128.0, 3.84e-5 * 25.0, 25.0, 4096, 4096, True),    # 平滑域 Δ=64：可重建
        (256, 128.0, 0.4, 25.0, 4096, 4096, False),              # 高对比 Δ=256：判红
        (16, 128.0, 0.0, 25.0, 512, 512, True),                  # 平坦场：几何上界内
        (300, 128.0, 0.0, 25.0, 512, 512, False),                # 超过 W/4 ⇒ 判红
    ]
    for d, ell, q, v, w, h, exp in cases:
        ok, dmax, _ = delta_guard(d, ell, q, v, w, h)
        assert ok == exp, (d, ok, exp, dmax)
    return True


if __name__ == "__main__":
    assert _selftest()
    print("p4_delta_guard self-test PASS")
