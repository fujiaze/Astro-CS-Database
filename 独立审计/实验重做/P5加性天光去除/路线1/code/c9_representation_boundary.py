#!/usr/bin/env python3
"""C9 -- B_ref 表示能力边界 (无接缝 <=> 公共面可表示) 实验腿. v2 (1D 化, 场沿 y 相干)

验证对象: docs/science/PHASE2_UPM.md §16.2 (7a 适用域边界):
  帧间天光差含 "B_ref 不可表示且沿某方向相干" 的分量时, 残余接缝 ≈ 0.80 x 该分量 RMS
  (Pearson 0.8964); 可表示尺度 ≈ 2x 节点间距 (≈256 px; 实验网格 0.0355 deg / 0.989"
  = 129.2 px). fixture: 8x8 control cell 双线性 B_ref, 帧间差 = 沿 y 正弦相干分量.
  因场沿 x 不变, x 方向双线性恒等 => 问题严格 1D 化 (控制点 = y 向 cell 均值,
  双线性插值 = 节点中心间线性内插).
负例: 常数分量 (lambda -> inf) 完全可表示 => 残余接缝 -> 0.
"""
import json
import math
import os
import numpy as np

SEED = 20260325
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c9_representation_boundary.json")
IMG = 1024
GRID = 8
H = IMG / GRID


def control_points(comp):
    """B_ref 控制点值 = 每 cell 内像素均值 (8 个)."""
    edges = np.linspace(0, IMG, GRID + 1).astype(int)
    return np.array([comp[edges[i]:edges[i + 1]].mean() for i in range(GRID)])


def bilinear_eval(cy):
    """节点中心 (cell 中点) 间线性内插; 域外取端值 (双线性域外行为)."""
    centers = (np.arange(GRID) + 0.5) * H
    yy = np.arange(IMG) + 0.5
    return np.interp(yy, centers, cy)


def seam_step(field_1d):
    """以 y 中线为帧足迹边界 (边沿 y 方向), 两侧 ±2px 均值差 = 电平台阶."""
    mid = IMG // 2
    return float(field_1d[mid + 1:mid + 3].mean() - field_1d[mid - 2:mid].mean())


def seam_steps_multi(resid_1d, n_cuts=16):
    """多个边界位置 (跨相位) 的电平台阶; 返回台阶 RMS.
    单一边界的读数依赖分量在该处的相位 (可为 0), 公平口径 = 跨相位 RMS."""
    cuts = (np.arange(n_cuts) + 0.5) * IMG / n_cuts
    steps = []
    for cpos in cuts:
        c = int(round(cpos))
        steps.append(resid_1d[c + 1:c + 3].mean() - resid_1d[c - 2:c].mean())
    return float(np.sqrt(np.mean(np.array(steps) ** 2)))


def main():
    res = {"seed": SEED, "img_px": IMG, "grid": GRID, "node_spacing_px": H,
           "fixture": "y-coherent sinusoid, B_ref = 8x8 bilinear (x-invariant => 1D)"}
    yy = np.arange(IMG) + 0.5
    lambdas = [50.0, 100.0, 200.0, 400.0, 512.0, 800.0, 1600.0, 3200.0]
    rows = []
    for lam in lambdas:
        comp = np.sin(2 * np.pi * yy / lam)
        comp /= np.sqrt(np.mean(comp ** 2))            # RMS = 1
        rep = bilinear_eval(control_points(comp))
        resid = comp - rep
        rows.append({"lambda_px": lam, "resid_rms": float(np.sqrt(np.mean(resid ** 2))),
                     "seam_step": seam_step(resid),
                     "seam_step_rms_over_cuts": seam_steps_multi(resid)})
    res["scan"] = rows
    rms = np.array([r["resid_rms"] for r in rows])
    seam = np.array([r["seam_step_rms_over_cuts"] for r in rows])
    a = float(np.sum(seam * rms) / np.sum(rms * rms))
    res["proportionality"] = {"slope_seam_over_rms": a,
                              "doc_claim_slope": 0.7976,
                              "pearson_r": float(np.corrcoef(seam, rms)[0, 1]),
                              "doc_claim_r": 0.8964}
    below = [r["lambda_px"] for r in rows if r["resid_rms"] < 0.1]
    res["cutoff_scale_px"] = min(below) if below else None
    res["representation_scale_2h"] = 2 * H
    # 负例: 常数分量完全可表示
    c0 = control_points(np.ones(IMG))
    resid0 = np.ones(IMG) - bilinear_eval(c0)
    res["negative_constant_component"] = {
        "resid_rms": float(np.sqrt(np.mean(resid0 ** 2))),
        "seam_step": seam_step(resid0),
        "claim": "真值可表示 => 残余接缝归零 (度量归零负例)",
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
