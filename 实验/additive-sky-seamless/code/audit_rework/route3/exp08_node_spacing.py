#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q8 节点间距规则1/2（h <= 约束尺度/2; B_ref 表示能力）三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §7a 规则1/规则2（参考面 B_ref 只能表示尺度 ≳ 2×节点间距
      的分量; 残余接缝 ≈ 0.80×不可表示相干分量 RMS（Pearson 0.896, 项目实测）;
      尺度 ≲ 2h 时显著（1600→50 px 扫描残余 ×5.07））。
理论腿: 节点采样 + 线性(双线性)插值的传递函数 = sinc^2(f*h)（f=1/λ）,
      可表示幅度比 H = sinc^2(h/λ); 不可表示部分残余比 = 1 - H（同相投影）。
      λ = 2h => H = (2/π)^2 = 0.4053, 残余 0.5947; λ = 4h => H = 0.8106, 残余 0.189。
实验腿: 节点间距 h 的最小二乘样条拟合对相干正弦分量的残余比实测 vs sinc^2 预测;
      λ→∞（常数分量）负例 => 残余 ~1e-16（完全可表示, 度量归零）。
固定 seed: SEED = 20260926（本实验确定性, 无随机量; seed 仍写入以合规）。
纯 numpy。运行: python3 exp08_node_spacing.py
"""
import json
import os
import numpy as np

SEED = 20260926
N_NODE = 64          # 节点数（域 [0, N_NODE*h]）
OVER = 8             # 每节点间距细采样 8 点
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q8_node_spacing.json")


def main():
    h = 1.0
    xf = (np.arange(N_NODE * OVER) + 0.5) * (h / OVER)   # 细网格坐标
    nodes = np.arange(N_NODE + 2) * h                    # 节点（两端外延一格保覆盖）
    t = xf / h
    i0 = np.floor(t).astype(int)
    w1 = t - i0                                          # 细点到右节点权重
    # 设计矩阵: 双线性(此处一维线性)基
    Am = np.zeros((len(xf), len(nodes)))
    idx = np.arange(len(xf))
    np.add.at(Am, (idx, i0), 1.0 - w1)
    np.add.at(Am, (idx, i0 + 1), w1)

    rows = []
    PHASES = [0.0, np.pi / 4, np.pi / 2, 3 * np.pi / 4, np.pi, 5 * np.pi / 4,
              3 * np.pi / 2, 7 * np.pi / 4]
    for lam_h in [2.0, 4.0, 8.0, 16.0, 64.0, np.inf]:
        if np.isinf(lam_h):
            field = np.ones_like(xf)                     # 常数分量（负例: 可表示）
            phases = [0.0]
        else:
            phases = PHASES
        fracs = []
        for ph in phases:
            fld = np.ones_like(xf) if np.isinf(lam_h) else \
                np.sin(2.0 * np.pi * xf / (lam_h * h) + ph)
            coef, *_ = np.linalg.lstsq(Am, fld, rcond=None)
            fit = Am @ coef
            resid = fld - fit
            fracs.append(float(np.sqrt(np.mean(resid ** 2)) / np.sqrt(np.mean(fld ** 2))))
        frac = float(np.mean(fracs)) if not np.isinf(lam_h) else fracs[0]
        if np.isinf(lam_h):
            H_pred = 1.0
            resid_pred = 0.0
        else:
            x = h / (lam_h * h)
            H_pred = float((np.sin(np.pi * x) / (np.pi * x)) ** 2)
            resid_pred = 1.0 - H_pred
        rows.append({"lambda_over_h": lam_h, "residual_frac_measured_mean": frac,
                     "residual_frac_phase_min": float(np.min(fracs)),
                     "residual_frac_phase_max": float(np.max(fracs)),
                     "residual_frac_pred_1-sinc2_zero_phase": resid_pred,
                     "significant_by_2h_rule": (not np.isinf(lam_h)) and lam_h <= 2.0})
        print(rows[-1])

    out = {"seed": SEED, "nodes": N_NODE, "oversample": OVER, "rows": rows,
           "negative_ok": rows[-1]["residual_frac_measured_mean"] < 1e-10,
           "doc_claim_0.80RMS": {
               "doc_value": 0.80,
               "this_geometry_marginal_2h_phase_range": [rows[0]["residual_frac_phase_min"], rows[0]["residual_frac_phase_max"]],
               "this_geometry_marginal_2h_mean": rows[0]["residual_frac_measured_mean"],
               "note": "0.80 为项目特定几何的经验回归值; 本独立几何的边际残余 0.59, 同量级、同方向（显著残余）, 但数值不可迁移"},
           "rule1_check": {
               "claim": "节点间距 <= 目标可表示尺度/2",
               "at_lambda_2h_residual_mean": rows[0]["residual_frac_measured_mean"],
               "at_lambda_2h_phase_range": [rows[0]["residual_frac_phase_min"],
                                            rows[0]["residual_frac_phase_max"]],
               "at_lambda_4h_residual_mean": rows[1]["residual_frac_measured_mean"],
               "ratio_means": rows[0]["residual_frac_measured_mean"] / rows[1]["residual_frac_measured_mean"],
               "note": "尺度从 4h 降到 2h（越过规则1 界限）残余比升 5.3x => '尺度≲2h 显著'方向证实"}}
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()