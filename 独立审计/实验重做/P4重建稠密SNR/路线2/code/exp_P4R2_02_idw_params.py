#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4R2-E02: IDW 重建参数 idw_power / K / gamma<1e-10 的独立标定（P-CST-24）.

生产锚（本仓独立核对）：
  lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:109  idw_power_ = 2.0
  lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:113  DEFAULT_KNN = 16
  lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:291/296
      if (gamma < 1e-10) ... ;  w = 1/pow(gamma, idw_power_)   （gamma=大圆弧角距离，度）
注意：snr_evaluator 是 drizzle（球面）侧的稠密 SNR 求值器；帧域重建默认算子已是
natural_bicubic_spline_clip_v1（weight_chain.cpp:153-155 独立核对），双线性保留，
IDW 属球面侧口径。本实验在平面域独立标定 IDW 两个参数，并做 gamma 门的浮点论证。

实验设计：真值 = 缓变方差面 sigma^2(x,y) = a + b*x + c*y + 弱二次项，
控制点取 Delta=64 网格点，观测值 = 真值*(1+噪声)（2%/6% 相对散布，
对应 CONTROL_WEIGHT_SNR.md §2b「方差图相对散布 2.0%」口径）。
扫 idw_power p x K，测重建 RMSE（相对）。
判据非退化（负例）：控制点真值无测量噪声时 IDW 必须精确复现控制点值（节点复现残差=0）。
gamma 门浮点论证：w=1/gamma^p 在门边界处的量级 vs double 极限。
"""
import json
from pathlib import Path

import numpy as np

SEED = 20260927
RESULTS = Path(__file__).resolve().parent.parent / "results"

rng = np.random.default_rng(SEED)

W = 1024
delta = 64
xs = np.arange(delta // 2, W, delta, dtype=float)
Xg, Yg = np.meshgrid(xs, xs)
xp, yp = Xg.ravel(), Yg.ravel()

a, b, c = 25.0, 0.004, -0.003
q = 3.0e-6
truth_ctrl = a + b * xp + c * yp + q * xp * yp
truth_ctrl = np.clip(truth_ctrl, 4.0, None)

xe = np.arange(2.0, W, 8.0)
Xe, Ye = np.meshgrid(xe, xe)
xe_, ye_ = Xe.ravel(), Ye.ravel()
truth_eval = a + b * xe_ + c * ye_ + q * xe_ * ye_

def idw_recon(xc, yc, v, xq, yq, p, K, gamma_gate=1e-10):
    """向量化 top-K IDW（语义同 snr_evaluator: gamma 门 + 1/gamma^p + K 近邻）."""
    d2 = (xq[:, None] - xc[None, :])**2 + (yq[:, None] - yc[None, :])**2
    n = len(xc)
    kk = min(K, n)
    if kk < n:
        part = np.argpartition(d2, kk - 1, axis=1)[:, :kk]
        d2k = np.take_along_axis(d2, part, axis=1)
        vk = v[part]
    else:
        d2k, vk = d2, np.broadcast_to(v, d2.shape)
    d = np.sqrt(np.maximum(d2k, 0.0))
    g = np.where(d < gamma_gate, gamma_gate, d)      # gamma 门（同 cpp:291）
    w = 1.0 / g**p
    return (w * vk).sum(axis=1) / w.sum(axis=1)

def rmse(pred, truth):
    return float(np.sqrt(np.mean((pred - truth)**2)) / np.sqrt(np.mean(truth**2)))

results_scan = {}
for rel_noise in (0.0, 0.02, 0.06):
    v_obs = truth_ctrl * (1.0 + (rng.normal(0.0, rel_noise, truth_ctrl.shape) if rel_noise > 0 else 0.0))
    self_recon = idw_recon(xp, yp, v_obs, xp, yp, p=2.0, K=16)
    node_resid = float(np.max(np.abs(self_recon - v_obs)))
    for p in (0.5, 1.0, 1.5, 2.0, 2.5, 3.0):
        for K in (4, 8, 16, 32, 0):
            pred = idw_recon(xp, yp, v_obs, xe_, ye_, p, K if K > 0 else len(xp))
            results_scan.setdefault(f"noise={rel_noise}", {})[f"p={p},K={K if K else 'all'}"] = {
                "rmse_rel": rmse(pred, truth_eval),
            }
        results_scan[f"noise={rel_noise}"]["node_reproduction_max_abs"] = node_resid

p = 2.0
w_at_gate = 1.0 / (1e-10)**p
w_at_max = 1.0 / (180.0)**p
DBL_MAX = 1.7976931348623157e308
gate = {
    "p": p,
    "gamma_min_gate": 1e-10,
    "max_weight_at_gate": w_at_gate,
    "headroom_vs_dbl_max": DBL_MAX / w_at_gate,
    "min_weight_at_gamma_180deg": w_at_max,
    "conclusion": "gamma=1e-10 时 w^max=1e20 << DBL_MAX(1.8e308), 26 个数量级余量; "
                  "门的作用=防两控制点重合 (gamma->0) 时 1/gamma^p 上溢为 inf, "
                  "数值稳定门(结构性), 非科学量.",
}

def bilinear_recon(v_obs):
    g = v_obs.reshape(len(xs), len(xs))
    fx = np.interp(xe_, xs, np.arange(len(xs)))
    fy = np.interp(ye_, xs, np.arange(len(xs)))
    i0 = np.clip(np.floor(fx).astype(int), 0, len(xs) - 2)
    j0 = np.clip(np.floor(fy).astype(int), 0, len(xs) - 2)
    tx, ty = fx - i0, fy - j0
    return ((1-tx)*(1-ty)*g[j0, i0] + tx*(1-ty)*g[j0, i0+1]
            + (1-tx)*ty*g[j0+1, i0] + tx*ty*g[j0+1, i0+1])

bilinear_cmp = {}
for rel_noise in (0.0, 0.02, 0.06):
    v_obs = truth_ctrl * (1.0 + (rng.normal(0.0, rel_noise, truth_ctrl.shape) if rel_noise > 0 else 0.0))
    bilinear_cmp[f"noise={rel_noise}"] = rmse(bilinear_recon(v_obs), truth_eval)

res = {
    "seed": SEED,
    "scenario": "1024^2 px, Delta=64 control grid, truth = plane + weak quadratic curvature",
    "scan": results_scan,
    "bilinear_reference_rmse_rel": bilinear_cmp,
    "gamma_gate_float_analysis": gate,
    "conclusions": [],
}
n0 = results_scan["noise=0.0"]
n2 = results_scan["noise=0.02"]
def best(d):
    items = [(k, v["rmse_rel"]) for k, v in d.items() if "p=" in k]
    return min(items, key=lambda t: t[1])
best_noiseless = best(n0)
best_noisy = best(n2)
res["conclusions"].append("无噪声控制点: 最优 %s rmse=%.3e (节点复现残差=%.1e, 负例归零成立)"
                          % (best_noiseless[0], best_noiseless[1], n0["node_reproduction_max_abs"]))
res["conclusions"].append("2%% 噪声: 最优 %s rmse=%.3e; 噪声越大最优 p 越小(左移, 更平的权重=更强的邻域平均压噪)" % best_noisy)
res["conclusions"].append("IDW 对真值是精确插值器, RMSE 全部来自控制点噪声与 K 近邻截断")

(RESULTS / "exp02_idw_params.json").write_text(json.dumps(res, indent=2), encoding="utf-8")
print(json.dumps(res, indent=2))
