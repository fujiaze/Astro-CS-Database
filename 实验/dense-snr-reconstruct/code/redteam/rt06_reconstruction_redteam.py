#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""rt06_reconstruction_redteam.py -- P4 稠密重建算子 / 预测方差红队反例与盲复算。

定位
----
本脚本是**对抗性审稿的取证件**，不是常规实验腿。三条纪律：

1. **盲复算**：算子、判据、有效域候选判据全部按 `docs/derivations.md` 的定义与
   `docs/detail/registry/astrocs.phase1.noise-snr.md` 的冻结算子语义**独立重写**，
   不 import 本单元任何实验脚本（避免继承被审对象的实现与假设）。实现先过
   自检（节点复现、收敛阶、E_eff 恒等式）再进入反例。
2. **不改归档**：输出只打印到 stdout，**不写 `results/**`**，不改任何已归档
   数字。审稿轮不得改变被审对象的证据面。
3. **不跑重活**：只用 numpy；M16 腿只重渲一帧（与 `code/sim/` 同场景配方），
   不重跑本单元 25 个实验。

用法：python3 code/redteam/rt06_reconstruction_redteam.py
"""
import dataclasses
import json
import math
import sys
from pathlib import Path

import numpy as np

UNIT = Path(__file__).resolve().parents[2]
ROOT = UNIT.parents[1]
SYNTH = ROOT / "实验" / "shared" / "synthetic"
sys.path.insert(0, str(SYNTH))

MAD_K = 1.482602218505602          # 1/Phi^{-1}(3/4)
SEED = 20260926


# ==================================================================== 度量（derivation §7）
def E_eff(w, v):
    """E_eff = Var_w/Var_opt - 1，Var_opt = 1/sum(1/v)（文档 §7 唯一口径）。"""
    w = np.asarray(w, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    return float(np.sum(w ** 2 * v) / np.sum(w) ** 2 * np.sum(1.0 / v) - 1.0)


def J_delta(v, delta):
    """Δ-格可表示性（cell-oracle 口径）：只问「方差场在 Δ 格上能不能表示」，
    不含任何估计量噪声、不含任何算子。w = 1/v̄_cell。真值即可算，零算子依赖。"""
    v = np.asarray(v, dtype=np.float64)
    n = (v.shape[0] // delta) * delta
    m = (v.shape[1] // delta) * delta
    vc = v[:n, :m].reshape(n // delta, delta, m // delta, delta).mean(axis=(1, 3))
    w = np.repeat(np.repeat(1.0 / np.maximum(vc, 1e-300), delta, 0), delta, 1)
    return E_eff(w[:n, :m].ravel(), v[:n, :m].ravel())


def dex_rmse(a, b):
    a = np.maximum(np.asarray(a, float), 1e-12)
    b = np.maximum(np.asarray(b, float), 1e-12)
    return float(np.sqrt(np.mean((np.log10(a) - np.log10(b)) ** 2)))


# ==================================================== 算子（按冻结算子语义独立实现）
def _nat2(y, xk):
    """自然边界三次样条二阶导，沿 axis 0；y: (m,k) -> (m,k)。端点 M=0。"""
    m, k = y.shape
    M = np.zeros_like(np.asarray(y, dtype=np.float64))
    if m < 3:
        return M
    h = np.diff(xk)
    n2 = m - 2
    # 未知量 M_1..M_{m-2}；行 r（0..n2-1）对应节点 r+1：
    #   h_r·M_r + 2(h_r+h_{r+1})·M_{r+1} + h_{r+1}·M_{r+2} = rhs_r
    a = np.zeros(n2)
    a[1:] = h[1:n2]              # 下对角（第 r 行系数 h_r）
    b = 2.0 * (h[:-1] + h[1:])   # 对角
    c = np.zeros(n2)
    c[:-1] = h[1:n2]            # 上对角（第 r 行系数 h_{r+1}）
    rhs = 6.0 * ((y[2:] - y[1:-1]) / h[1:][:, None]
                 - (y[1:-1] - y[:-2]) / h[:-1][:, None])
    mu = np.zeros_like(rhs)
    dp = np.zeros_like(rhs)
    beta = b[0] * np.ones(k)
    dp[0] = rhs[0] / beta
    mu[0] = c[0] / beta
    for i in range(1, n2):
        beta = b[i] - a[i] * mu[i - 1]
        dp[i] = (rhs[i] - a[i] * dp[i - 1]) / beta
        mu[i] = c[i] / beta
    for i in range(n2 - 1, -1, -1):        # M[i+1] = dp[i] − mu[i]·M[i+2]，端点 M[0]=M[m−1]=0
        M[i + 1] = dp[i] - mu[i] * M[i + 2]
    return M


def _eval_nat(y, M, xk, xq):
    """在 xk 上的自然三次样条向 xq 求值：y/M: (m,k)，xq: (p,) -> (p,k)。"""
    m, k = y.shape
    xq = np.asarray(xq, dtype=np.float64)
    i = np.clip(np.searchsorted(xk, xq, side="right") - 1, 0, max(m - 2, 0))
    h1 = xk[i + 1] - xk[i]
    a = ((xk[i + 1] - xq) / h1)[:, None]
    b = ((xq - xk[i]) / h1)[:, None]
    h = h1[:, None]
    return a * y[i] + b * y[i + 1] + (((a ** 3 - a) * M[i] + (b ** 3 - b) * M[i + 1])
                                       * h ** 2 / 6.0)


def _eval_nat_diag(y, M, xk, yq):
    """分块配对求值：y/M 为 (m, B)，yq 长 B；第 t 点用第 t 列 -> (B,)。"""
    yq = np.asarray(yq, dtype=np.float64)
    m, B = y.shape
    rows = np.arange(B)
    i = np.clip(np.searchsorted(xk, yq, side="right") - 1, 0, max(m - 2, 0))
    h = xk[i + 1] - xk[i]
    a = (xk[i + 1] - yq) / h
    b = (yq - xk[i]) / h
    return (a * y[i, rows] + b * y[i + 1, rows]
            + ((a ** 3 - a) * M[i, rows] + (b ** 3 - b) * M[i + 1, rows]) * h ** 2 / 6.0)


def spline2d_eval(grid, xk, yk, xq, yq, block=4096):
    """可分离自然边界双三次样条，**配对**查询坐标。grid[i,j] 对应 (xk[i], yk[j])。

    两遍：先沿 x（对每个 y 节点）求到查询 x，再沿 y 求到查询 y。分块以免
    物化 P×P 的张量积中间量。
    """
    g = np.asarray(grid, dtype=np.float64)
    xk = np.asarray(xk, dtype=np.float64)
    yk = np.asarray(yk, dtype=np.float64)
    xq = np.asarray(xq, dtype=np.float64)
    yq = np.asarray(yq, dtype=np.float64)
    if xq.size != yq.size:
        raise ValueError("配对坐标长度不一致")
    Mx = _nat2(g, xk)                       # (nx, ny)
    P = xq.size
    out = np.empty(P, dtype=np.float64)
    for s in range(0, P, block):
        e = min(s + block, P)
        mid = _eval_nat(g, Mx, xk, xq[s:e])        # (B, ny)
        cols = mid.T                               # (ny, B)
        My = _nat2(cols, yk)                       # (ny, B)
        out[s:e] = _eval_nat_diag(cols, My, yk, yq[s:e])
    return out


def spline_clip_2d(grid, xk, yk, xq, yq, clip=True):
    """natural_bicubic_spline_clip_v1 = 上述样条 + 钳到有效控制值值域 [min,max]。"""
    out = spline2d_eval(grid, xk, yk, xq, yq)
    g = np.asarray(grid, dtype=np.float64)
    if clip:
        out = np.clip(out, float(np.nanmin(g)), float(np.nanmax(g)))
    return out


def _bilinear_1d(y, xk, xq):
    """一维分段线性（xq 逐点配对）。"""
    xk = np.asarray(xk, float)
    xq = np.asarray(xq, float)
    i = np.clip(np.searchsorted(xk, xq, side="right") - 1, 0, len(xk) - 2)
    t = (xq - xk[i]) / (xk[i + 1] - xk[i])
    return (1 - t) * y[i] + t * y[i + 1]


def bilinear_2d(grid, xk, yk, xq, yq):
    g = np.asarray(grid, dtype=np.float64)
    xk = np.asarray(xk, dtype=np.float64)
    yk = np.asarray(yk, dtype=np.float64)
    qx = np.asarray(xq, dtype=np.float64)
    qy = np.asarray(yq, dtype=np.float64)
    i = np.clip(np.searchsorted(xk, qx, side="right") - 1, 0, len(xk) - 2)
    j = np.clip(np.searchsorted(yk, qy, side="right") - 1, 0, len(yk) - 2)
    hx = xk[1] - xk[0]
    hy = yk[1] - yk[0]
    fx = (qx - xk[i]) / hx
    fy = (qy - yk[j]) / hy
    return ((1 - fx) * (1 - fy) * g[i, j] + fx * (1 - fy) * g[i + 1, j]
            + (1 - fx) * fy * g[i, j + 1] + fx * fy * g[i + 1, j + 1])


def nearest_2d(grid, xk, yk, xq, yq):
    g = np.asarray(grid, dtype=np.float64)
    qx = np.asarray(xq, dtype=np.float64)
    qy = np.asarray(yq, dtype=np.float64)
    i = np.abs(qx[:, None] - np.asarray(xk)[None, :]).argmin(axis=1)
    j = np.abs(qy[:, None] - np.asarray(yk)[None, :]).argmin(axis=1)
    return g[i, j]


def idw_2d(grid, xk, yk, xq, yq, p=1.0):
    g = np.asarray(grid, dtype=np.float64)
    gx, gy = np.meshgrid(np.asarray(xk), np.asarray(yk), indexing="ij")
    d2 = (np.asarray(qx)[:, None] - gx.ravel()[None, :]) ** 2 \
        + (np.asarray(qy)[:, None] - gy.ravel()[None, :]) ** 2
    w = 1.0 / np.maximum(d2, 1e-10) ** (p / 2.0)
    w /= w.sum(axis=1, keepdims=True)
    return w @ g.ravel()


# ================================================================ 盲复算自检
def selftest():
    out = {}

    # (1) 恒等式 w = SNR^2/F_ref^2 = 1/sigma_F^2
    rng = np.random.default_rng(SEED)
    snr = rng.uniform(0.1, 500.0, 20000)
    fref = 10.0 ** rng.uniform(-2, 2, 20000)
    sig = fref / snr
    out["identity_max_rel_dev"] = float(np.max(np.abs(snr ** 2 / fref ** 2 - 1.0 / sig ** 2)
                                               / np.maximum(1.0 / sig ** 2, 1e-300)))

    # (2) E_eff >= 0（Cauchy-Schwarz），随机 w、v 抽样
    neg = 0
    worst = 0.0
    for _ in range(200):
        v = 10.0 ** rng.uniform(-3, 3, 5000)
        w = 10.0 ** rng.uniform(-3, 3, 5000)
        e = E_eff(w, v)
        worst = min(worst, e)
        if e < -1e-12:
            neg += 1
    out["E_eff_min_over_200_random_fields"] = worst
    out["E_eff_negative_count"] = neg

    # (3) E_eff = 0 <=> w ∝ 1/v
    v = 10.0 ** rng.uniform(-2, 2, 5000)
    out["E_eff_shape_correct"] = E_eff(3.7 / v, v)
    out["E_eff_shape_wrong"] = E_eff(1.0 / v ** 0.8, v)

    # (4) 乘性免疫
    v = 10.0 ** rng.uniform(-2, 2, 5000)
    w = 1.0 / v ** 0.93
    out["E_eff_scale3p17_delta"] = abs(E_eff(3.17 * w, v) - E_eff(w, v))

    # (5) 节点复现（我的独立实现）
    for delta, n in ((16, 8), (32, 6), (64, 4)):
        xk = np.arange(n) * delta + (delta - 1) / 2.0
        g = np.abs(rng.normal(1.0, 0.3, (n, n))) + 0.05
        XN, YN = np.meshgrid(xk, xk, indexing="ij")
        rec = spline2d_eval(g, xk, xk, XN.ravel(), YN.ravel()).reshape(n, n)
        out["node_reproduction_delta%d" % delta] = float(np.max(np.abs(rec - g)))
        out["node_reproduction_clipped_delta%d" % delta] = float(
            np.max(np.abs(spline_clip_2d(g, xk, xk, XN.ravel(), YN.ravel()).reshape(n, n) - g)))

    # (6) 收敛阶：光滑解析场 f=sin(3t)，域 [0, π/3]（端点 f''=0 ⇒ 自然 BC 与真值
    #     一致，无端点层污染）；节点含两端。理论：样条 O(h^4)、双线性 O(h^2)。
    #     另给「含端点层」对照（端点二阶导非零），记录端点效应。
    N = 512
    L = np.pi / 3.0
    xq = np.arange(N) * L / N
    fq = np.sin(3.0 * xq)
    qs, es, eb, es_b = [], [], [], []
    for delta in (8, 16, 32, 64):
        xk = np.linspace(0.0, L, N // delta + 1)
        y = np.sin(3.0 * xk)[:, None]
        rec = _eval_nat(y, _nat2(y, xk), xk, xq)[:, 0]
        bl = _bilinear_1d(np.sin(3.0 * xk), xk, xq)
        xb = xk[:-1] + 0.5 * (xk[1] - xk[0])      # 中点：端点层对照
        yb = np.sin(3.0 * xb)[:, None]
        recb = _eval_nat(yb, _nat2(yb, xb), xb, xq)[:, 0]
        qs.append(delta)
        es.append(float(np.max(np.abs(rec - fq))))
        eb.append(float(np.max(np.abs(bl - fq))))
        es_b.append(float(np.max(np.abs(recb - fq))))
    slope = lambda e: float(np.polyfit(np.log(qs), np.log(np.array(e)), 1)[0])
    out["spline_convergence_order"] = slope(es)
    out["bilinear_convergence_order"] = slope(eb)
    out["spline_order_with_endpoint_layer"] = slope(es_b)
    out["spline_err_by_delta"] = {d: "%.3e" % e for d, e in zip(qs, es)}
    out["bilinear_err_by_delta"] = {d: "%.3e" % e for d, e in zip(qs, eb)}

    # (7) Delta^2 偏置律：二次场，cell 均值 vs cell 中心的确定性偏置。
    #     解析：cell 覆盖 [i−Δ/2, i+Δ/2)、中心 i ⇒ bias = q·Δ²/24，
    #     故 bias(Δ₂)/bias(Δ₁) = (Δ₂/Δ₁)²。判的是**标度律**（常数随口径变化，不判常数）。
    ratios = []
    prev = None
    for delta in (16, 32, 64, 128):
        bias = []
        for i in range(0, 512, delta):
            u = np.arange(-delta / 2 + 0.5, delta / 2 + 0.5)   # 对称于 0 的格内偏移
            bias.append(0.5 * np.mean((float(i) + u) ** 2) - 0.5 * float(i) ** 2)
        b = float(np.mean(bias))
        if prev is not None:
            ratios.append(b / prev)
        prev = b
    out["cellavg_minus_center_bias_ratios"] = [round(r, 6) for r in ratios]
    out["cellavg_minus_center_bias_theory_ratio"] = 4.0
    out["cellavg_bias_coefficient_over_Delta2"] = float(prev / 128.0 ** 2)
    out["cellavg_bias_analytic_coefficient"] = 1.0 / 24.0

    return out


# ================================================================ 反例构造
def _grid_truth(size, seed=SEED):
    yy, xx = np.mgrid[0:size, 0:size] / size
    return 10.0 ** (1.0 + 0.5 * np.cos(4.0 * xx) * np.sin(3.0 * yy)), xx, yy


def _moffat_point_source(size, x0, y0, fwhm, peak):
    yy, xx = np.mgrid[0:size, 0:size] / size
    a = fwhm / (2.0 * math.sqrt(2.0 ** 0.5 - 1.0))
    r2 = (xx - x0) ** 2 + (yy - y0) ** 2
    return peak * (1.0 + r2 / a ** 2) ** -2.5


def _row_digest(rows):
    keep = ["case", "n_ctrl", "node_repro_max_abs", "nonpos_frac", "below_min_frac",
            "out_of_domain_frac", "dex_rmse", "max_rel_dev", "E_eff_dense_clip",
            "E_eff_frame", "E_eff_cell_oracle", "J_delta", "sigma_ratio_p99_over_p50",
            "fail_closed"]
    return {k: r.get(k) for k in keep}


# ==================================================================== 反例构造
def _smooth_at(xx, yy):
    """平滑解析 SNR 场（与坐标无关的闭式，便于任意查询网格求值）。"""
    return 10.0 ** (1.0 + 0.5 * np.cos(4.0 * xx) * np.sin(3.0 * yy))


def _moffat_at(xx, yy, x0, y0, fwhm, peak):
    a = fwhm / (2.0 * math.sqrt(2.0 ** 0.5 - 1.0))
    r2 = (xx - x0) ** 2 + (yy - y0) ** 2
    return peak * (1.0 + r2 / a ** 2) ** -2.5


def cell_oracle_E(v2d, delta, mask=None):
    """cell-oracle（无估计量噪声、无算子）的 E_eff：只问 Δ 格能否表示该方差场。"""
    v2d = np.asarray(v2d, float)
    n = (v2d.shape[0] // delta) * delta
    m = (v2d.shape[1] // delta) * delta
    vc = v2d[:n, :m].reshape(n // delta, delta, m // delta, delta).mean(axis=(1, 3))
    full = np.zeros_like(v2d)
    full[:n, :m] = np.repeat(np.repeat(1.0 / np.maximum(vc, 1e-300), delta, 0), delta, 1)
    mask = np.ones_like(v2d, dtype=bool) if mask is None else mask
    return E_eff(full[mask], v2d[mask])


def counterexamples():
    rows = []
    size, delta = 256, 64
    node = (delta - 1) / 2.0
    q = np.arange(size, dtype=float)
    qx0, qy0 = np.meshgrid(q, q, indexing="ij")
    f_smooth = lambda X, Y: _smooth_at(np.asarray(X, float) / size, np.asarray(Y, float) / size)
    f_sharp = lambda X, Y: f_smooth(X, Y) + 50.0 * _moffat_at(
        np.asarray(X, float) / size, np.asarray(Y, float) / size, 0.5, 0.5, 3.2 / size * 4, 1.0)
    f_step = lambda X, Y: np.where(np.asarray(X, float) / size > 0.5, 100.0, 1.0)
    bg = float(np.median(f_sharp(qx0, qy0)))


    def evaluate(name, ctrl_x, ctrl_y, ctrl_v, qx=None, qy=None, f=None, note=""):
        qx = q if qx is None else np.asarray(qx, float)
        qy = q if qy is None else np.asarray(qy, float)
        f = f_smooth if f is None else f
        ctrl_x = np.asarray(ctrl_x, float)
        ctrl_y = np.asarray(ctrl_y, float)
        ctrl_v = np.asarray(ctrl_v, float)
        ux = np.unique(ctrl_x)
        uy = np.unique(ctrl_y)
        G = np.full((len(ux), len(uy)), float(np.mean(ctrl_v)))
        known = np.zeros_like(G, dtype=bool)
        li = {a: k for k, a in enumerate(ux)}
        lj = {b: k for k, b in enumerate(uy)}
        for a, b, c in zip(ctrl_x, ctrl_y, ctrl_v):
            G[li[a], lj[b]] = c
            known[li[a], lj[b]] = True
        if len(ux) >= 2 and len(uy) >= 2:
            X, Y = np.meshgrid(qx, qy, indexing="ij")
            rec = spline2d_eval(G, ux, uy, X.ravel(), Y.ravel()).reshape(qx.size, qy.size)
            rec_c = np.clip(rec, G.min(), G.max())
            XN, YN = np.meshgrid(ux, uy, indexing="ij")
            node_repro = float(np.max(np.abs(
                spline2d_eval(G, ux, uy, XN.ravel(), YN.ravel()).reshape(G.shape) - G)))
        else:
            rec_c = np.full((qx.size, qy.size), float(np.mean(G)))
            node_repro = float("nan")
        xlo, xhi = ux.min() - delta / 2.0, ux.max() + delta / 2.0
        ylo, yhi = uy.min() - delta / 2.0, uy.max() + delta / 2.0
        X, Y = np.meshgrid(qx, qy, indexing="ij")
        m = (X >= xlo) & (X <= xhi) & (Y >= ylo) & (Y <= yhi)
        oob = float((~m).mean())
        Xg, Yg = np.meshgrid(qx, qy, indexing="ij")
        tr = f(Xg, Yg)
        rec_in = rec_c[m]
        snr_true = tr[m]
        v_in = 1.0 / np.maximum(snr_true, 1e-300)
        rec_snr = 1.0 / np.maximum(rec_in, 1e-300)
        ratio = v_in / np.maximum(rec_in, 1e-300)
        rows.append({
            "case": name, "note": note, "n_ctrl": int(G.size),
            "node_repro_max_abs": node_repro,
            "nonpos_frac": float((rec_c <= 0).mean()),
            "below_min_frac": float((rec_c < G.min()).mean()),
            "out_of_domain_frac": oob,
            "dex_rmse": dex_rmse(rec_snr, 1.0 / v_in),
            "max_rel_dev": float(np.max(np.abs(ratio - 1.0))),
            "E_eff_dense_clip": E_eff(1.0 / np.maximum(rec_in, 1e-300), v_in),
            "E_eff_frame": E_eff(np.full_like(rec_in, float(np.median(G))), v_in),
            "E_eff_cell_oracle": cell_oracle_E(1.0 / np.maximum(tr, 1e-300), delta, m),
            "J_delta": cell_oracle_E(1.0 / np.maximum(tr, 1e-300), delta),
            "sigma_ratio_p99_over_p50": float(
                np.quantile(ratio, 0.99) / max(np.quantile(ratio, 0.50), 1e-300)),
            "fail_closed": bool(oob > 0 or (rec_c <= 0).any() or (rec_c < G.min()).any()),
        })

    def ctrl_values(f, XG, YG):
        """按**配对**坐标在真值场上取控制点值（返回一维）。"""
        X = np.asarray(XG, float).ravel()
        Y = np.asarray(YG, float).ravel()
        return np.asarray(f(X, Y), float).ravel()

    xs = np.arange(4) * delta + node
    XG, YG = np.meshgrid(xs, xs, indexing="ij")
    base = ctrl_values(f_smooth, XG, YG)
    evaluate("B0 正常规则格 4x4（对照）", XG.ravel(), YG.ravel(), base.ravel(),
             f=f_smooth, note="16 点规则格，平滑解析真值")

    # C1 控制点极稀疏
    for n in (2, 3):
        xs1 = np.arange(n) * delta + node
        A, B2 = np.meshgrid(xs1, xs1, indexing="ij")
        evaluate("C1 极稀疏 %dx%d（%d 点）" % (n, n, n * n), A.ravel(), B2.ravel(),
                 ctrl_values(f_smooth, A, B2).ravel(), f=f_smooth,
                 note="规则但极稀疏的控制格")
    xs1 = np.arange(3) * delta + node
    evaluate("C1 极稀疏 1x3（3 点，y 方向退化）", xs1, np.full(3, node),
             ctrl_values(f_smooth, xs1, np.full(3, node)),
             f=f_smooth, note="y 方向只有 1 个控制点 ⇒ 该方向不可插值")

    # C2 控制点共线
    evaluate("C2 控制点共线（16 点同 y）", XG.ravel(), np.full(16, node), base.ravel(),
             f=f_smooth, note="点云退化为一条直线（y 方向 1 个节点）")

    # C3 控制点全在噪声主导区
    evaluate("C3 控制点全在噪声主导区", XG.ravel(), YG.ravel(), np.full(16, bg),
             f=f_sharp,
             note="真值含 %.2f dex 峰值点源，控制点全落在天光背景"
                  % np.log10(f_sharp(qx0, qy0).max() / bg))

    # C4 控制点严重非均匀
    xs4 = np.arange(4) * delta + node
    xv = np.concatenate([np.repeat(xs4, 4), [xs4[-1]]])
    yv = np.concatenate([np.repeat(xs4, 4), [node]])
    evaluate("C4 控制点严重非均匀（16 密 + 1 孤点）", xv, yv,
             ctrl_values(f_sharp, xv, yv), f=f_sharp,
             note="右侧半区只有 1 个控制点")

    # C5 尖锐真值
    evaluate("C5 尖锐真值（Δ 内 FWHM≈3.2px 点源）", XG.ravel(), YG.ravel(), base.ravel(),
             f=f_sharp, note="点源尺度远小于 Δ=64，控制点全部落在背景上")
    evaluate("C5 尖锐真值（100:1 阶跃边缘在格内）", XG.ravel(), YG.ravel(),
             ctrl_values(f_step, XG, YG), f=f_step,
             note="阶跃边缘位于格内，无控制点落在边缘上")

    # C6 查询点越出层覆盖域
    def f_ext(X, Y):
        X = np.asarray(X, float)
        Y = np.asarray(Y, float)
        inside = (X >= 0) & (X < size) & (Y >= 0) & (Y < size)
        return np.where(inside, f_smooth(np.clip(X, 0, size - 1), np.clip(Y, 0, size - 1)),
                        float(np.median(f_smooth(qx0, qy0))))
    evaluate("C6 查询点越出层覆盖域", XG.ravel(), YG.ravel(), base.ravel(),
             qx=np.concatenate([q, [-100.0, 400.0]]),
             qy=np.concatenate([q, [128.0, 128.0]]), f=f_ext,
             note="两列查询点在层定义域外（正本要求 fail-closed 不外推）")
    return rows


# ================================================================ 预测方差
def predicted_variance_checks():
    out = {}

    # (1) 解析预测方差 vs MC 无偏性（共享链同式，逐像素）
    import noise_model as NM
    det = NM.Detector(gain_e_per_adu=1.5, read_noise_e=5.0, dark_current_e_per_s=0.0,
                      quantize=True, saturate=False)
    n = 64
    t = 300.0
    src = np.full((n, n), 120.0)
    sky = np.full((n, n), 40.0)
    v_pred = np.array([[NM.predicted_variance_adu2(
        src_e=float(src[0, 0]) * t, sky_e=float(sky[0, 0]) * t, dark_e=0.0, det=det)
        for _ in range(1)] for _ in range(1)])[0, 0]
    draws = np.array([NM.expose(src_e_per_s=src, sky_e_per_s=sky, det=det, exptime_s=t,
                                rng=np.random.default_rng(9000 + k)).adu
                      for k in range(600)])
    emp = draws.var(axis=0, ddof=1)
    ratio = emp / v_pred
    out["pred_var_predicted_adu2"] = float(v_pred)
    out["pred_var_unbiased_ratio_mean"] = float(np.mean(ratio))
    out["pred_var_unbiased_ratio_median"] = float(np.median(ratio))
    out["pred_var_unbiased_ratio_std"] = float(np.std(ratio))
    out["pred_var_MC_rel_se"] = float(np.std(ratio) / np.sqrt(2.0 * 599))

    # (2) Var_opt 的独立性前提：相关噪声下 GLS 可达方差 vs Var_opt
    rng = np.random.default_rng(SEED)
    N = 256
    v = 10.0 ** rng.uniform(-1, 1, N)
    out["Var_opt_independent_case"] = 1.0 / np.sum(1.0 / v)
    rows = []
    for rho in (0.0, 0.1, 0.3, 0.6, 0.9):
        # 交换矩阵（drizzle 型：近邻相关的对称权重化交换），再归一化对角
        idx = np.arange(N)
        d = np.abs(idx[:, None] - idx[None, :])
        E = (1.0 - rho) * (d == 0) + rho * np.exp(-d / 3.0)
        S = E * np.sqrt(v[:, None] * v[None, :])
        Si = np.linalg.inv(S)
        ones = np.ones(N)
        gls = float(1.0 / (ones @ Si @ ones))
        rows.append({"rho": rho, "gls_var": gls, "Var_opt": 1.0 / np.sum(1.0 / v),
                     "ratio_gls_over_varopt": gls * np.sum(1.0 / v)})
    out["correlated_noise_gls_vs_varopt"] = rows

    # (3) 重建量自身预测方差的独立性前提：Var(a^T x) = a^T Var a（独立）
    #     vs 相关控制噪声。K 近邻等权平均的膨胀因子 = (1-rho) + rho*K。
    out["recon_var_inflation_vs_rho_K"] = [
        {"rho": rho, "K": K, "emp_over_pred": (1 - rho) + rho * K}
        for rho in (0.0, 0.195, 0.4) for K in (4, 16, 64)]

    # (4) 稀疏区：有效邻居数下降 -> 预测方差发散
    Nq = 5
    d = np.abs(np.arange(Nq) - Nq / 2.0)
    for p in (1.0, 2.0):
        w = d ** (-p)
        w = w / w.sum()
        out["sparse_edge_weights_p%.1f" % p] = [round(float(x), 5) for x in w]
    out["sparse_edge_sum_w2_p1.0"] = float(np.sum((d ** -1.0 / np.sum(d ** -1.0)) ** 2))
    out["sparse_edge_sum_w2_p2.0"] = float(np.sum((d ** -2.0 / np.sum(d ** -2.0)) ** 2))
    out["uniform_sum_w2_same_N"] = 1.0 / Nq

    # (5) 生产方差面：residual_maker 口径 Σ=diag(sigma^2) 在相关噪声下的偏差
    #     Var_diag = sum (d_ij)^2 sigma_j^2 ; Var_true = d^T S d
    dvec = np.array([1.0 - 0.5, -0.5, -0.5])       # 排除自身的 3 帧归一化行
    s2 = np.array([4.0, 1.0, 9.0])
    S = np.sqrt(s2[:, None] * s2[None, :]) * (0.3 + 0.7 * (np.arange(3)[:, None] == np.arange(3)[None, :]))
    out["production_diag_vs_true_corr"] = {
        "Var_diag_sigma2": float(np.sum(dvec ** 2 * s2)),
        "Var_true_full_cov": float(dvec @ S @ dvec),
        "ratio": float((dvec @ S @ dvec) / np.sum(dvec ** 2 * s2)),
    }
    return out


# ================================================================ M16 有效域口径盲复算
def m16_domain_recompute():
    import m16_sampling as MS
    import noise_model as NM
    scene = MS.load_scene(SYNTH / "scenes" / "m16_sampling_overlap_common.json")
    SZ = 768
    scene["shape"] = [SZ, SZ]
    scene["frames"] = scene["frames"][:1]
    rate, valid, cmeta = MS.load_canvas(scene, verbose=False)
    base = scene["sampling"].get("base_fwhm_px")
    base_psf = (MS.measure_base_psf(rate, valid) if base is None
                else {"fwhm_px": float(base), "n_stars": 0, "method": "config_override"})
    canvas = {"rate": rate, "valid": valid, "meta": cmeta, "base_psf": base_psf}
    frame, truth, vmask, sig = MS.render_sampling_frame(scene, canvas, frame_index=0, verbose=False)
    fields = {f.name for f in dataclasses.fields(NM.Detector)}
    det = NM.Detector(**{k: v for k, v in truth["detector"].items() if k in fields})
    t_ex = float(truth["exposure_s"])
    a = NM.expose(src_e_per_s=frame.src_e / t_ex, sky_e_per_s=frame.sky_e / t_ex, det=det,
                  exptime_s=t_ex, rng=np.random.default_rng(20261010), flat=frame.flat)
    g = det.gain_e_per_adu
    lam = np.maximum(t_ex * (frame.src_e / t_ex + frame.sky_e / t_ex) + a.dark_e, 0.0)
    v_true = (lam + det.read_noise_e ** 2) / (g * g) + NM.QUANTIZATION_VARIANCE_ADU2
    snr_true = (a.src_e / g) / np.sqrt(v_true)
    good = np.isfinite(a.adu) & (a.adu < det.saturation_adu) & (a.adu > 0)
    d = np.where(good, a.adu, float(np.nanmedian(a.adu[good])))

    def patch_mad(dd, patch=8):
        n = dd.shape[0] // patch
        b = dd[:n * patch, :n * patch].reshape(n, patch, n, patch)
        med = np.median(b, axis=(1, 3))
        mad = np.median(np.abs(b - med[:, None, :, None]), axis=(1, 3))
        return (MAD_K * mad) ** 2

    def cell_nodes(vp, delta, patch=8):
        per = delta // patch
        n = vp.shape[0] // per
        idx = (np.arange(per) + 0.5) * patch
        node = (delta - 1) / 2.0
        A = np.column_stack([np.ones(per * per), np.repeat(idx, per), np.tile(idx, per)])
        Ainv = np.linalg.pinv(A)
        out = np.zeros((n, n))
        for i in range(n):
            for j in range(n):
                c = Ainv @ vp[i * per:(i + 1) * per, j * per:(j + 1) * per].ravel()
                out[i, j] = c[0] + c[1] * node + c[2] * node
        return out

    q = np.arange(SZ, dtype=float)
    rows = []
    for delta in (16, 32, 64, 128, 256):
        npl = cell_nodes(patch_mad(d), delta)
        ux = np.arange(npl.shape[0]) * delta + (delta - 1) / 2.0
        X, Y = np.meshgrid(q, q, indexing="ij")
        rec = spline2d_eval(npl, ux, ux, X.ravel(), Y.ravel()).reshape(SZ, SZ)
        rec = np.clip(rec, npl.min(), npl.max())                    # 生产默认算子含值域钳制
        cell = np.repeat(np.repeat(npl, delta, 0), delta, 1)[:SZ, :SZ]
        frame_v = np.full((SZ, SZ), float(np.median(npl)))
        w_d = 1.0 / np.maximum(rec, 1e-300)
        w_c = 1.0 / np.maximum(cell, 1e-300)
        w_f = 1.0 / frame_v
        rows.append({
            "delta": delta,
            "E_dense_clip": E_eff(w_d.ravel(), v_true.ravel()),
            "E_cell": E_eff(w_c.ravel(), v_true.ravel()),
            "E_frame": E_eff(w_f.ravel(), v_true.ravel()),
            "dense_beats_frame": bool(E_eff(w_d.ravel(), v_true.ravel())
                                      < E_eff(w_f.ravel(), v_true.ravel())),
            "J_delta_cell_oracle": J_delta(v_true, delta),
            "v_true_dr": float(v_true.max() / max(v_true.min(), 1e-300)),
            "snr_nonpos_frac": float((rec <= 0).mean()),
        })
    # Delta* = dense 首次劣于 frame 的最小 Delta（本腿族）
    star = next((r["delta"] for r in rows if not r["dense_beats_frame"]), None)
    return {"rows": rows, "delta_star_dense_vs_frame": star,
            "note": "本口径按生产默认算子（含值域钳制）复算；判据取自论文未使用的 J_Δ（cell-oracle）"}


def main():
    print("=" * 78)
    print("P4 盲复算自检（独立实现，不 import 本单元实验脚本）")
    print("=" * 78)
    st = selftest()
    for k, v in st.items():
        print("  %-42s %s" % (k, v))

    print()
    print("=" * 78)
    print("重建算子反例（真值 = 平滑解析场，除非注明）")
    print("=" * 78)
    hdr = ("case", "n_ctrl", "node_repro", "nonpos", "below_min", "oob", "dex_rmse",
           "E_dense", "E_frame", "J_delta", "sigma_ratio_p99/p50", "fail_closed")
    print("  " + " | ".join(str(h) for h in hdr))
    rows = counterexamples()
    for r in rows:
        cells = [r["case"], r["n_ctrl"], "%.1e" % r["node_repro_max_abs"],
                 "%.3f" % r["nonpos_frac"], "%.3f" % r["below_min_frac"],
                 "%.3f" % r["out_of_domain_frac"], "%.4f" % r["dex_rmse"],
                 "%.3e" % r["E_eff_dense_clip"], "%.3e" % r["E_eff_frame"],
                 "%.3e" % r["J_delta"], "%.2f" % r["sigma_ratio_p99_over_p50"],
                 r["fail_closed"]]
        print("  " + " | ".join(str(c) for c in cells))
    print("\n  明细：")
    for r in rows:
        print("   - %s :: %s" % (r["case"], r["note"]))

    print()
    print("=" * 78)
    print("预测方差")
    print("=" * 78)
    pv = predicted_variance_checks()
    print(json.dumps(pv, indent=2, ensure_ascii=False, default=float))

    print()
    print("=" * 78)
    print("M16 有效域口径盲复算（Δ* 与 J_Δ）")
    print("=" * 78)
    md = m16_domain_recompute()
    print("  %-6s %-14s %-14s %-14s %-9s %-12s %-12s" % (
        "Delta", "E_dense(clip)", "E_cell", "E_frame", "beats?", "J_delta", "v_true_dr"))
    for r in md["rows"]:
        print("  %-6d %-14.4e %-14.4e %-14.4e %-9s %-12.4e %-12.4e" % (
            r["delta"], r["E_dense_clip"], r["E_cell"], r["E_frame"],
            r["dense_beats_frame"], r["J_delta_cell_oracle"], r["v_true_dr"]))
    print("  Delta*(dense 首次劣于 frame) =", md["delta_star_dense_vs_frame"])


if __name__ == "__main__":
    main()
