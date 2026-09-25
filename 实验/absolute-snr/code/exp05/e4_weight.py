#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 D：**权重视角** —— 帧内共模相消的证明/反例 + 跨帧不消的定量。

三件事：
  D1 **定理的数值验证**：若重建算子 R 正齐次（R[a*v] = a*R[v]），则相对表示与绝对表示
     只差一个**帧内共模因子** c（同一帧所有像素同一个常数）。于是
       (a) 单帧**归一化**加权均值 sum(w*y)/sum(w) 严格逐位相同；
       (b) 权重效率损失 E = Var_w/Var_opt - 1 严格不变（尺度不变）。
     ⇒ EXP-1 实测的 ratio=1.000000 **是定理的必然结果，不是「两种表示等价」的证据**。
  D2 **跨帧反例**：不同帧的 c_k 不同 ⇒ 帧间权重比被 (c_k/c_j)^2 污染 ⇒ E > 0、
     组合 SNR 有偏。用**真实帧实测的 c_k**（e3 产物）定量。
  D3 **反例（定理条件不成立时）**：带**数据无关先验均值**的重建算子不正齐次 ⇒
     共模相消不再严格；残差中会出现**非共模**分量。

固定 seed：SEED = 20260926。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp05_common as X  # noqa: E402

SEED = X.SEED
DELTA = 64


def upsample_kriging_fixed_mean(grid: np.ndarray, delta: int, shape,
                                mean0: float = 1.0, corr_px: float = 96.0,
                                nugget_frac: float = 1e-6) -> np.ndarray:
    """**带数据无关先验均值**的类 kriging 重建（D3 的反例算子）。

    pred = m0 + k^T K^-1 (v - m0*1)：先验均值 m0 是**固定常数**（不随输入缩放）⇒
    算子**不正齐次**（R[a*v] != a*R[v]），共模相消因此不再严格。
    """
    g = np.asarray(grid, dtype=np.float64)
    ny, nx = int(shape[0]), int(shape[1])
    cy = X._cell_centers(g.shape[0], delta)
    cx = X._cell_centers(g.shape[1], delta)
    gy, gx = np.meshgrid(cy, cx, indexing="ij")
    pts = np.stack([gy.ravel(), gx.ravel()], axis=1)
    vals = g.ravel()
    ok = np.isfinite(vals)
    pts, vals = pts[ok], vals[ok]
    n = pts.shape[0]
    K = np.exp(-X._pdist(pts, pts) / corr_px) + nugget_frac * np.eye(n)
    rhs = vals - float(mean0)
    alpha = np.linalg.solve(K, rhs)
    out = np.empty((ny, nx), dtype=np.float64)
    yy = np.arange(ny, dtype=np.float64) + 0.5
    xx = np.arange(nx, dtype=np.float64) + 0.5
    for y0 in range(0, ny, 64):
        y1 = min(y0 + 64, ny)
        Y, Xg = np.meshgrid(yy[y0:y1], xx, indexing="ij")
        q = np.stack([Y.ravel(), Xg.ravel()], axis=1)
        k = np.exp(-X._pdist(q, pts) / corr_px)
        out[y0:y1, :] = (float(mean0) + k @ alpha).reshape(y1 - y0, nx)
    return out


def part1_theorem(rng) -> Dict[str, Any]:
    shape = (512, 512)
    grid = np.exp(rng.normal(0.0, 0.35, size=(8, 8)))
    ops = {"nearest": X.upsample_nearest, "bilinear": X.upsample_bilinear,
           "kriging_like": X.upsample_kriging_like}
    eq = {k: X.scale_equivariance(v, grid, 7.5, DELTA, shape) for k, v in ops.items()}
    # 帧内共模：w_rel = w_abs / c^2
    c = 2.9
    var_true = (1.0 / X.upsample_bilinear(grid, DELTA, shape)) ** 2
    var_true = np.where(np.isfinite(var_true) & (var_true > 0), var_true, np.nan)
    w_abs = 1.0 / var_true
    w_rel = w_abs / (c * c)
    y = rng.normal(0.0, 1.0, size=shape) * np.sqrt(var_true) + 5.0
    m = np.isfinite(var_true)
    mean_abs = float(np.sum(w_abs[m] * y[m]) / np.sum(w_abs[m]))
    mean_rel = float(np.sum(w_rel[m] * y[m]) / np.sum(w_rel[m]))
    e_abs = X.weight_efficiency(w_abs[m], var_true[m])
    e_rel = X.weight_efficiency(w_rel[m], var_true[m])
    return {"operator_scale_equivariance_max_rel": eq,
            "single_frame_weighted_mean_abs": mean_abs,
            "single_frame_weighted_mean_rel": mean_rel,
            "single_frame_mean_rel_diff": abs(mean_rel / mean_abs - 1.0),
            "weight_efficiency_abs": e_abs, "weight_efficiency_rel": e_rel,
            "weight_efficiency_rel_diff": abs(e_rel - e_abs),
            "c_used": c}


def cross_frame_E(c: np.ndarray, sigma: np.ndarray) -> float:
    """K 帧、帧间 c_k 不同时的权重效率损失 E（闭式，用 w_k 正比于 1/(c_k^2 sigma_k^2)）。"""
    c = np.asarray(c, float)
    sigma = np.asarray(sigma, float)
    w = 1.0 / (c * c * sigma * sigma)
    var = sigma * sigma
    var_w = float(np.sum(w * w * var)) / float(np.sum(w)) ** 2
    var_opt = 1.0 / float(np.sum(1.0 / var))
    return var_w / var_opt - 1.0


def cross_frame_E_analytic2(c1: float, c2: float) -> float:
    """K=2、等 sigma 的闭式：E = 2(c1^4+c2^4)/(c1^2+c2^2)^2 - 1。"""
    return 2.0 * (c1 ** 4 + c2 ** 4) / (c1 * c1 + c2 * c2) ** 2 - 1.0


def part2_cross_frame(real_json: Path, rng) -> Dict[str, Any]:
    cases: List[Dict[str, Any]] = []
    measured: Dict[str, List[float]] = {}
    if real_json.exists():
        d = json.loads(real_json.read_text(encoding="utf-8"))
        for p in d.get("panels", []):
            measured[p["panel"]] = [float(x) for x in p["c_sigma_per_frame"]]
    if not measured:
        measured = {"synthetic_M5_like": [2.937, 2.940, 2.704],
                    "synthetic_M1_like": [1.175, 1.206],
                    "negative_control_equal_c": [1.5, 1.5, 1.5]}
    for name, cs in measured.items():
        c = np.asarray(cs, float)
        sigma = np.ones_like(c)
        e = cross_frame_E(c, sigma)
        # 组合 SNR^2 = sum SNR_k^2：相对表示的组合电平偏差（拆成"共模"与"跨帧离散"两部分）
        snr_true = np.ones_like(c)
        snr_rel = snr_true / c
        bias = float(np.sqrt(np.sum(snr_true ** 2)) / np.sqrt(np.sum(snr_rel ** 2)) - 1.0)
        c_mean = float(np.mean(c))
        snr_common = snr_true / c_mean
        bias_common = float(np.sqrt(np.sum(snr_true ** 2))
                            / np.sqrt(np.sum(snr_common ** 2)) - 1.0)
        bias_xframe = float((1.0 + bias) / (1.0 + bias_common) - 1.0)
        # 两两反例（K=2 等 sigma）与闭式对拍
        pairs = []
        for i in range(len(c)):
            for j in range(i + 1, len(c)):
                e_num = cross_frame_E(np.array([c[i], c[j]]), np.ones(2))
                e_ana = cross_frame_E_analytic2(float(c[i]), float(c[j]))
                pairs.append({"i": i, "j": j, "c_i": float(c[i]), "c_j": float(c[j]),
                              "E_numeric": e_num, "E_analytic": e_ana,
                              "abs_dev": abs(e_num - e_ana),
                              "weight_ratio_distortion": float((c[j] / c[i]) ** 2)})
        cases.append({"case": name, "c": cs, "E": e, "combined_snr_rel_dev": bias,
                      "combined_snr_dev_common_only": bias_common,
                      "combined_snr_dev_cross_frame_only": bias_xframe,
                      "c_spread_p95_over_p05": float(np.percentile(c, 95)
                                                     / np.percentile(c, 5)),
                      "pairs": pairs})
    return {"measured_c_source": str(real_json) if real_json.exists() else "builtin",
            "cases": cases}


def part3_counterexample(rng) -> Dict[str, Any]:
    shape = (256, 256)
    grid = np.exp(rng.normal(0.0, 0.35, size=(4, 4)))
    ops = {"kriging_like": X.upsample_kriging_like,
           "kriging_fixed_mean_1.0": lambda g, d, s: upsample_kriging_fixed_mean(g, d, s, 1.0),
           "kriging_fixed_mean_5.0": lambda g, d, s: upsample_kriging_fixed_mean(g, d, s, 5.0)}
    eq = {k: X.scale_equivariance(v, grid, 3.0, DELTA, shape) for k, v in ops.items()}
    # 非共模残差：R[alpha*v]/alpha - R[v] 的空间标准差（相对）
    resid = {}
    for k, v in ops.items():
        a = v(grid, DELTA, shape)
        b = v(grid * 3.0, DELTA, shape) / 3.0
        m = np.isfinite(a) & np.isfinite(b) & (np.abs(a) > 1e-12)
        resid[k] = {"max_rel": float(np.max(np.abs(a[m] - b[m]) / np.abs(a[m]))),
                    "rms_rel": float(np.sqrt(np.mean(((a[m] - b[m]) / a[m]) ** 2)))}
    return {"scale_equivariance_max_rel": eq, "non_common_mode_residual": resid}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e4_weight.json"))
    ap.add_argument("--real-json", default=str(X.RESULTS / "exp05_e3_real.json"))
    a = ap.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED + 55000)
    out: Dict[str, Any] = {"meta": {
        "seed": SEED, "delta_px": DELTA,
        "weight_definition": "w = SNR^2/F_ref^2 = 1/sigma_F^2（07_noise_snr.md 4.1；严格只在通量型 SNR 下成立）",
        "efficiency_definition": "E = Var_w/Var_opt - 1，Var_w = sum(w^2 var)/(sum w)^2，Var_opt = 1/sum(1/var)",
    }}
    out["D1_theorem"] = part1_theorem(rng)
    out["D2_cross_frame"] = part2_cross_frame(Path(a.real_json), rng)
    out["D3_counterexample"] = part3_counterexample(rng)
    d1 = out["D1_theorem"]
    print("D1 算子尺度等变性 max rel:", {k: "%.2e" % v for k, v in
                                          d1["operator_scale_equivariance_max_rel"].items()})
    print("D1 单帧归一化加权均值 相对差 = %.3e；E 相对差 = %.3e"
          % (d1["single_frame_mean_rel_diff"], d1["weight_efficiency_rel_diff"]))
    for c in out["D2_cross_frame"]["cases"]:
        print("D2 %-26s c=%s E=%.4f combSNRdev=%+.4f (common %+.4f / xframe %+.4f)"
              % (c["case"], ["%.3f" % x for x in c["c"]], c["E"],
                 c["combined_snr_rel_dev"], c["combined_snr_dev_common_only"],
                 c["combined_snr_dev_cross_frame_only"]))
    print("D3 非共模残差:", {k: "%.3e" % v["max_rel"] for k, v in
                             out["D3_counterexample"]["non_common_mode_residual"].items()})
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
