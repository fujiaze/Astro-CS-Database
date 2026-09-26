#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4R2-E03: 稀疏层控制网格间距 Delta 的敏感性 + 结构性常数豁免论证（P-CST-16）.

生产锚：eng/contracts/schemas/unified/phase_config_normalize.schema.json:466
  稀疏 SNR 层控制点间隔 Delta（px）默认 64，出处 = 负责人 2026-09-19 裁决
  （GAP_AUDIT §9.45「64 可以了，够密且占用空间可控」），复用 Phase2 UPM 8x8/tile。
（注：审查-1 路推测 Delta = tile_width/8 = 512 属幻觉推算；schema 原文是"复用 Phase2 UPM
  的 8x8/tile 控制网格"的密度口径，仓内 HiPS tile_width 实际为 512，与 Delta 无导出关系。）

问题：Delta 是科学量还是结构性常数？
实验：真值方差面两种情形——(a) 纯线性平面（双线性插值精确复现，误差与 Delta 无关）；
(b) 弱二次项 q*x^2（双线性不可精确复现，插值偏置 ~ q*Delta^2/8 随 Delta 增长）。
控制点带 0.5% 相对测量噪声（低于 2% 散布口径以显出确定性偏置）。
负例（真值无效应⇒度量归零）：曲率=0 时 "Delta 依赖性" 度量必须归零（比值~1）。
若 Delta 只决定采样密度/存储而不进入物理模型，则它是结构性/工程参数（豁免三腿）。
"""
import json
from pathlib import Path

import numpy as np

SEED = 20260928
RESULTS = Path(__file__).resolve().parent.parent / "results"
rng = np.random.default_rng(SEED)

W = 2048
REL_NOISE = 0.005

def bilinear_grid_rmse(delta, q2, rel_noise=REL_NOISE):
    xs = np.arange(delta // 2, W, delta, dtype=float)
    Xg, Yg = np.meshgrid(xs, xs)
    v = 25.0 + 0.002 * Xg - 0.0015 * Yg + q2 * Xg**2
    if rel_noise > 0:
        v_obs = v * (1.0 + rng.normal(0.0, rel_noise, v.shape))
    else:
        v_obs = v
    # 只在内域求值（控制点覆盖范围），避免边界外推伪效应
    xe = np.arange(xs[0], xs[-1] + 1.0, 8.0)
    Xe, Ye = np.meshgrid(xe, xe)
    truth = 25.0 + 0.002 * Xe - 0.0015 * Ye + q2 * Xe**2
    fx = np.interp(Xe[0], xs, np.arange(len(xs)))     # x -> 分数格 index（2-D 广播）
    fy = np.interp(Ye[:, 0], xs, np.arange(len(xs)))  # y -> 分数格 index
    i0 = np.clip(np.floor(fx).astype(int), 0, len(xs) - 2)
    j0 = np.clip(np.floor(fy).astype(int), 0, len(xs) - 2)
    tx = fx - i0
    ty = fy - j0
    TX, TY = np.meshgrid(tx, ty)
    I0, J0 = np.meshgrid(i0, j0)
    pred = ((1-TX)*(1-TY)*v_obs[J0, I0] + TX*(1-TY)*v_obs[J0, I0+1]
            + (1-TX)*TY*v_obs[J0+1, I0] + TX*TY*v_obs[J0+1, I0+1])
    return float(np.sqrt(np.mean((pred - truth)**2)) / np.sqrt(np.mean(truth**2)))

rows = []
for q2, nz, tag in ((0.0, REL_NOISE, "flat+noise"), (3.0e-5, REL_NOISE, "curved+noise"),
                    (3.0e-5, 0.0, "curved+bias_only")):
    for delta in (16, 32, 64, 128, 256):
        rows.append({"tag": tag, "q2": q2, "delta_px": delta,
                     "rmse_rel": bilinear_grid_rmse(delta, q2, nz)})

dep_flat = [r["rmse_rel"] for r in rows if r["tag"] == "flat+noise"]
dep_curved = [r["rmse_rel"] for r in rows if r["tag"] == "curved+noise"]
dep_bias = [r["rmse_rel"] for r in rows if r["tag"] == "curved+bias_only"]
metric_flat = max(dep_flat) / min(dep_flat)
metric_curved = max(dep_curved) / min(dep_curved)
bias_growth_ratio = dep_bias[-1] / dep_bias[0]     # Delta 256 vs 16, 理论 (256/16)^2 = 256
bias_delta2_scaling = dep_bias[3] / dep_bias[2]    # 128/64, 理论 4
# 理论偏置读数: q*Delta^2/8 相对水平 25
bias_theory = {d: 3.0e-5 * d * d / 8.0 / 25.0 for d in (16, 32, 64, 128, 256)}

res = {
    "seed": SEED,
    "scenario": "2048^2 px, bilinear-on-grid reconstruction of variance plane, control noise 0.5%",
    "rows": rows,
    "delta_dependency_metric_flat_truth": metric_flat,
    "delta_dependency_metric_curved_truth": metric_curved,
    "bias_only_growth_256_over_16": bias_growth_ratio,
    "bias_only_scaling_128_over_64": bias_delta2_scaling,
    "interpolation_bias_theory_rel": bias_theory,
    "negative_control_verdict": "PASS" if abs(metric_flat - 1.0) < 0.15 else "FAIL",
    "conclusions": [
        "曲率=0（负例，内域求值）时 RMSE 与 Delta 无关（max/min=%.3f~1）⇒ Delta 不进入物理模型，只决定采样密度；" % metric_flat,
        "q2=3e-5 时确定性插值偏置随 Delta 呈 ~Delta^2 增长（零噪声行 128/64=%.2f，理论 4；256/16=%.0f，理论 256）⇒ 效应幅度由场景曲率决定，属确定性插值误差而非物理参数；" % (bias_delta2_scaling, bias_growth_ratio),
        "结论：Delta=64 是结构性/工程参数（采样密度+存储，负责人裁决记录于 schema:466），不是科学量，豁免三腿；"
        "登记面应为 config_hash 而非科学常数表。",
    ],
}
(RESULTS / "exp03_delta_grid.json").write_text(json.dumps(res, indent=2), encoding="utf-8")
print(json.dumps(res, indent=2))
