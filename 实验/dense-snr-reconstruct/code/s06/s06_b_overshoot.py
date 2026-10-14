#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06-B：过冲反例 y=[0,2,0,0] 的自然三次样条重测 + 形状保持算子对照。

被测量：
  M1 = max_x s(x)        [与节点同单位，此处无量纲]  分母：无（极值读数）
  M2 = M1 − max(y)       [同单位]                   过冲幅度
  M3 = min_x s(x)        [同单位]
  M4 = fraction{s(x) > max(y)}  分母：均匀采样点数（采样约定必须随读数登记）
  M5 = |{x : s(x) > max(y)}|    x 测度（无采样约定依赖）

真值/独立 oracle：
  实现 1：本仓独立自然样条求解（s06_common.natural_spline_eval，三对角 + 自然边界）
  实现 2：scipy.interpolate.CubicSpline(bc_type='natural')（版本号入 JSON）
  实现 3：解析驻点（分段三次导数的二次根）——过冲上确界的精确求法
  形状保持对照：Fritsch–Carlson 1980 单调三次 Hermite（本仓独立实现 + scipy PchipInterpolator）

命令：python3 实验/dense-snr-reconstruct/code/s06/s06_b_overshoot.py
"""
from __future__ import annotations

import json
import numpy as np

from s06_common import (dump, natural_spline_coeffs, natural_spline_eval,
                        pchip_eval, run_meta)

SCRIPT = "s06_b_overshoot.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_b_overshoot.py"

try:
    import scipy
    from scipy.interpolate import CubicSpline, PchipInterpolator
    SCIPY_VER = scipy.__version__
except Exception as e:  # pragma: no cover
    CubicSpline = PchipInterpolator = None
    SCIPY_VER = f"UNAVAILABLE: {e}"


def stationary_points(y, x):
    """分段三次 s(t)=y_i+b_i t+c_i t²+d_i t³ 的驻点（t∈[0,h]）与端点。"""
    y = np.asarray(y, float); x = np.asarray(x, float)
    b, c, d = natural_spline_coeffs(y, x)
    pts = [float(x[0]), float(x[-1])]
    for i in range(len(y) - 1):
        h = x[i + 1] - x[i]
        A, B, C = 3 * d[i], 2 * c[i], b[i]
        if abs(A) < 1e-300:
            if abs(B) > 0:
                t = -C / B
                if 0 <= t <= h:
                    pts.append(float(x[i] + t))
            continue
        disc = B * B - 4 * A * C
        if disc < 0:
            continue
        sq = np.sqrt(disc)
        for t in ((-B + sq) / (2 * A), (-B - sq) / (2 * A)):
            if 0 <= t <= h:
                pts.append(float(x[i] + t))
    return np.array(sorted(set(round(p, 12) for p in pts)))


def horner_eval(y, x, q):
    """同一自然样条的第二种数值写法（区间内 Horner 展开），用于估计求值舍入散布。"""
    y = np.asarray(y, float); x = np.asarray(x, float)
    b, c, d = natural_spline_coeffs(y, x)
    out = np.empty_like(np.asarray(q, float))
    qa = np.atleast_1d(np.asarray(q, float))
    for idx, qq in enumerate(qa):
        k = int(np.clip(np.searchsorted(x, qq) - 1, 0, len(y) - 2))
        t = qq - x[k]
        out[idx] = ((d[k] * t + c[k]) * t + b[k]) * t + y[k]
    return out.reshape(np.shape(q))


def measure(y, x, tag, N=2_000_001):
    y = np.asarray(y, float); x = np.asarray(x, float)
    q = np.linspace(x[0], x[-1], N)
    s = natural_spline_eval(y, x, q)
    xs = stationary_points(y, x)
    sv = natural_spline_eval(y, x, xs)
    k = int(np.argmax(sv))
    xstar = float(xs[k])
    smax = float(sv[k])
    smin = float(np.min(sv))
    xmin = float(xs[int(np.argmin(sv))])
    # 数值区间：
    #   (a) 代数等价写法的 float64 舍入散布（同一驻点、Horner vs b,c,d 形式）
    #   (b) 驻点邻域 δ 内的取值散布（反映极值点定位精度，而非舍入）
    alg_spread = abs(float(horner_eval(y, x, np.array([xstar]))[0]) - smax)
    neigh = {}
    for dq in (1e-15, 1e-12, 1e-9, 1e-6):
        dev = 0.0
        for sgn in (-1, 1):
            v = float(natural_spline_eval(y, x, np.array([xstar + sgn * dq]))[0])
            dev = max(dev, abs(v - smax))
        neigh[f"delta_{dq:g}"] = dev
    spread = alg_spread
    node_max = float(np.max(y)); node_min = float(np.min(y))
    frac_above = float(np.mean(s > node_max))
    # x 测度：在细网格上数超过节点最大值的步长
    dx = (x[-1] - x[0]) / (N - 1)
    x_measure = float(np.sum(s > node_max) * dx)
    out = {
        "tag": tag, "nodes_x": list(map(float, x)), "nodes_y": list(map(float, y)),
        "node_max": node_max, "node_min": node_min,
        "max_value": smax, "argmax_x": xstar,
        "max_value_interval_float64": [smax - spread, smax + spread],
        "float64_algebraic_form_spread": alg_spread,
        "neighborhood_value_spread": neigh,
        "overshoot_abs": smax - node_max,
        "overshoot_interval": [smax - spread - node_max, smax + spread - node_max],
        "min_value": smin, "argmin_x": xmin,
        "undershoot_abs": node_min - smin,
        "frac_above_node_max_uniform_grid": frac_above,
        "x_measure_above_node_max": x_measure,
        "frac_at_or_above_node_max": float(np.mean(s >= node_max)),
        "grid_N": N,
        "grid_convention": f"uniform on [{x[0]},{x[-1]}], N={N}",
        "max_over_dense_grid": float(np.max(s)),
        "argmax_over_dense_grid": float(q[int(np.argmax(s))]),
    }
    if CubicSpline is not None:
        cs = CubicSpline(x, y, bc_type="natural")
        out["scipy_natural_max"] = float(np.max(cs(q)))
        out["scipy_natural_min"] = float(np.min(cs(q)))
        out["scipy_vs_own_max_absdiff"] = abs(out["scipy_natural_max"] - smax)
        pc = PchipInterpolator(x, y)
        out["scipy_pchip_max"] = float(np.max(pc(q)))
        out["scipy_pchip_min"] = float(np.min(pc(q)))
    out["own_pchip_max"] = float(np.max(pchip_eval(x, y, q)))
    out["own_pchip_min"] = float(np.min(pchip_eval(x, y, q)))
    # 单调性检验（节点非单调段不应被"修复"成单调，但节点单调段不得出现反向过冲）
    out["pchip_no_overshoot"] = bool(out["own_pchip_max"] <= node_max + 0
                                    and out["own_pchip_min"] >= node_min - 0)
    return out


cases = {
    "y_0_2_0_0": ([0.0, 2.0, 0.0, 0.0], [0.0, 1.0, 2.0, 3.0]),
    "y_0_1_0_0_0": ([0.0, 1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 2.0, 3.0, 4.0]),
}
rows = {k: measure(y, x, k) for k, (y, x) in cases.items()}

# 生产路径交叉引用（probe_s06 的 D5 结果，若存在）
probe_ref = None
try:
    import os
    from pathlib import Path
    p = Path(os.environ.get("S06_OUT", "run/ACSDSDOC-02/S06/results")) / "probe_s06_results.json"
    if p.exists():
        pr = json.loads(p.read_text(encoding="utf-8"))
        for e in pr["experiments"]:
            if e.get("id") == "S06_D5_overshoot_production":
                probe_ref = e
except Exception:
    probe_ref = None

payload = {
    **run_meta(SCRIPT, COMMAND),
    "scipy_version": SCIPY_VER,
    "measured_quantity_definition": {
        "M1": "max_x s(x) [与节点同单位]",
        "M2": "M1 − max(node) [同单位]",
        "M3": "min_x s(x) [同单位]",
        "M4": "fraction{s > max(node)}，分母 = 采样点数（约定见 grid_convention）",
        "M5": "|{x : s(x) > max(node)}| [x 单位]",
    },
    "cases": rows,
    "shape_preservation_reference": {
        "Fritsch_Carlson_1980": {
            "cite": "Fritsch & Carlson (1980), SIAM J. Numer. Anal. 17, 238–246",
            "doi": "10.1137/0717021",
            "claim": "单调三次插值的必要条件/充分条件；按该条件构造的 Hermite 三次在节点单调段内不越界",
            "verification_level": "BIB_VERIFIED(出版方页面未取到全文；结论由本仓独立实现的 PCHIP 数值行为支持)",
        },
        "Hyman_1983": {
            "cite": "Hyman, J. M. (1983), SIAM J. Sci. Stat. Comput. 4, 645–654",
            "doi": "10.1137/0904045",
            "claim": "C² 样条插值不保证满足单调/保界条件（'will not necessarily satisfy'）；保形需额外约束或钳制",
            "verification_level": "BIB_VERIFIED + 独立数值复算（本文件）",
        },
        "conclusion": "自然样条不具普适单调/无过冲保证；形状保持性是 Fritsch–Carlson/Hyman 类算子的附加约束性质，不是自然边界的推论。",
    },
    "production_crosscheck": probe_ref,
    "measurement_note": (
        "本文件的读数是**未钳制**自然样条的性质读数。生产默认算子 "
        "natural_bicubic_spline_clip_v1 显式钳到 [min(node), max(node)]，"
        "因此产品面上不会出现 >max(node) 的值；但该上界由**显式钳制**保证，"
        "不是样条性质（见 production_crosscheck 的 clip_high 咬合计数）。"),
}

gates = {
    "G_B1_overshoot_exists": {
        "pass": bool(rows["y_0_2_0_0"]["overshoot_abs"] > 1e-3),
        "observed": rows["y_0_2_0_0"]["overshoot_abs"],
    },
    "G_B2_zero_effect_negative_flat_pattern": {
        # 零效应阴性：全等节点 [2,2,2,2] 上样条必须精确为常数（无过冲、无下冲）
        "pass": None, "observed": None,
    },
    "G_B3_shape_preserving_operator_no_overshoot": {
        "pass": bool(rows["y_0_2_0_0"]["pchip_no_overshoot"]),
        "observed": {"pchip_max": rows["y_0_2_0_0"]["own_pchip_max"],
                     "pchip_min": rows["y_0_2_0_0"]["own_pchip_min"]},
    },
    "G_B4_oracle_agreement_own_vs_scipy": {
        "pass": bool(rows["y_0_2_0_0"]["scipy_vs_own_max_absdiff"] < 1e-9),
        "observed": rows["y_0_2_0_0"]["scipy_vs_own_max_absdiff"],
    },
    "G_B5_node_reproduction": {
        "pass": bool(np.max(np.abs(natural_spline_eval(cases["y_0_2_0_0"][0],
                                                       cases["y_0_2_0_0"][1],
                                                       np.array(cases["y_0_2_0_0"][1]))
                             - np.array(cases["y_0_2_0_0"][0]))) < 1e-12),
        "observed": float(np.max(np.abs(natural_spline_eval(cases["y_0_2_0_0"][0],
                                                            cases["y_0_2_0_0"][1],
                                                            np.array(cases["y_0_2_0_0"][1]))
                                      - np.array(cases["y_0_2_0_0"][0])))),
    },
}
# 零效应阴性：常数节点
flat = measure([2.0, 2.0, 2.0, 2.0], [0.0, 1.0, 2.0, 3.0], "flat_2_2_2_2")
payload["cases"]["flat_2_2_2_2"] = flat
gates["G_B2_zero_effect_negative_flat_pattern"] = {
    "pass": bool(abs(flat["max_value"] - 2.0) < 1e-14 and abs(flat["min_value"] - 2.0) < 1e-14),
    "observed": {"max": flat["max_value"], "min": flat["min_value"]},
}
payload["gates"] = gates
payload["all_pass"] = all(g["pass"] for g in gates.values())

dump("S06_B_overshoot.json", payload)
print(f"[S06-B] all_pass={payload['all_pass']}")
for k in ("y_0_2_0_0", "y_0_1_0_0_0"):
    r = rows[k]
    print(f"  {k}: max={r['max_value']:.10f} overshoot={r['overshoot_abs']:.6e} "
          f"interval={r['max_value_interval_float64']} min={r['min_value']:.6f} "
          f"frac_above={r['frac_above_node_max_uniform_grid']:.5f} x_measure={r['x_measure_above_node_max']:.6f}")
