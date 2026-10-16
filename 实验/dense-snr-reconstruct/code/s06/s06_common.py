#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4 / S06 共享定义：重建算子、场型、度量。被 s06_* 脚本 import。

口径（全部来自 docs/science 与 docs/detail/registry，行号见 S06 报告）：
  w = SNR²/F_ref² = 1/σ_F²         [ADU^-2]，SNR 与 F_ref 同源（同一帧）
  E_eff = Var_w/Var_opt − 1        唯一口径，Var_opt = 1/Σ(1/v_true)
  控制网格 cell_center_v1：node_i = grid_origin + i*Δ + (Δ−1)/2
  定义域 = cell 并集 [gox−0.5, gox+nx·Δ−0.5]（最外半 cell 为端点节点常数延拓）

本模块只做数值，无 I/O 副作用；结果落盘由各脚本负责。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

import numpy as np

# ---------------------------------------------------------------- 常数
F_REF = 100.0          # 参考通量 [ADU]，逐帧固定（冻结参考星等档 m_ref）
MAD_K = 1.4826022185056023   # 1/Φ⁻¹(3/4)
GAIN = 1.3             # e-/ADU
READ_E = 5.0           # 读出噪声 [e-]
SEED = 20261020        # 本轮固定 seed（与 T07 的 20261016–20 同族，独立）
DELTA = 64             # 生产控制格 Δ = hips.tile_width/8 = 512/8
PATCH = 8              # 生产 8×8 patch MAD²


# ---------------------------------------------------------------- 度量
def E_eff(w, v):
    """E_eff = Var_w/Var_opt − 1（唯一口径；v = 被加权量的真方差）。"""
    w = np.asarray(w, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    return float(np.sum(w ** 2 * v) / np.sum(w) ** 2 * np.sum(1.0 / v) - 1.0)


def var_ratio(w, v):
    """留出方差比 Var_w/Var_opt = E_eff + 1（≥1，1 = 最优）。"""
    return E_eff(w, v) + 1.0


def signed_bias_rel(est, true):
    est = np.asarray(est, float)
    true = np.asarray(true, float)
    return float(np.mean((est - true) / true))


def rms_rel(est, true):
    est = np.asarray(est, float)
    true = np.asarray(true, float)
    return float(np.sqrt(np.mean(((est - true) / true) ** 2)))


def dex_rmse(a, b):
    a = np.maximum(np.asarray(a, float), 1e-300)
    b = np.maximum(np.asarray(b, float), 1e-300)
    return float(np.sqrt(np.mean((np.log10(a) - np.log10(b)) ** 2)))


# ---------------------------------------------------------------- 1D 自然三次样条
def natural_spline_coeffs(y, x):
    """自然边界三次样条系数；返回 (b,c,d)，区间 i: y_i + b_i t + c_i t² + d_i t³。"""
    y = np.asarray(y, float)
    x = np.asarray(x, float)
    n = len(y)
    h = np.diff(x)
    al = np.zeros(n)
    al[1:-1] = 3 * ((y[2:] - y[1:-1]) / h[1:] - (y[1:-1] - y[:-2]) / h[:-1])
    l = np.ones(n); mu = np.zeros(n); z = np.zeros(n)
    for i in range(1, n - 1):
        l[i] = 2 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1]
        mu[i] = h[i] / l[i]
        z[i] = (al[i] - h[i - 1] * z[i - 1]) / l[i]
    b = np.zeros(n - 1); c = np.zeros(n); d = np.zeros(n - 1)
    for j in range(n - 2, -1, -1):
        c[j] = z[j] - mu[j] * c[j + 1]
        b[j] = (y[j + 1] - y[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3
        d[j] = (c[j + 1] - c[j]) / (3 * h[j])
    return b, c, d


def natural_spline_eval(y, x, q):
    """在 q 处求自然三次样条（q 超出节点跨度时按边界区间三次式外推 =
    朴素实现的静默外推行为）。"""
    y = np.asarray(y, float); x = np.asarray(x, float); q = np.asarray(q, float)
    b, c, d = natural_spline_coeffs(y, x)
    k = np.clip(np.searchsorted(x, q) - 1, 0, len(y) - 2)
    t = q - x[k]
    return y[k] + b[k] * t + c[k] * t ** 2 + d[k] * t ** 3


def pchip_slopes(x, y):
    """Fritsch–Carlson (1980) 单调三次 Hermite 斜率（加权调和平均）。"""
    x = np.asarray(x, float); y = np.asarray(y, float)
    n = len(y)
    h = np.diff(x)
    delta = np.diff(y) / h
    m = np.zeros(n)
    if n == 2:
        m[:] = delta[0]
        return m
    for i in range(1, n - 1):
        if delta[i - 1] * delta[i] <= 0:
            m[i] = 0.0
        else:
            w1 = 2 * h[i] + h[i - 1]
            w2 = h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i])
    # 端点：单侧三点公式 + 单调性限制（Fritsch–Carlson 的端点处理）
    def end_slope(h0, h1, d0, d1):
        s = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
        if s * d0 <= 0:
            return 0.0
        if d0 * d1 < 0 and abs(s) > abs(3 * d0):
            return 3 * d0
        return s
    m[0] = end_slope(h[0], h[1], delta[0], delta[1])
    m[-1] = end_slope(h[-1], h[-2], delta[-1], delta[-2])
    return m


def pchip_eval(x, y, q):
    """单调三次 Hermite（Fritsch–Carlson）求值；域外按端点常数延拓。"""
    x = np.asarray(x, float); y = np.asarray(y, float); q = np.asarray(q, float)
    m = pchip_slopes(x, y)
    n = len(y)
    k = np.clip(np.searchsorted(x, q) - 1, 0, n - 2)
    h = x[k + 1] - x[k]
    t = (q - x[k]) / h
    t = np.clip(t, 0.0, 1.0)  # 域外：端点常数延拓
    h00 = 2 * t ** 3 - 3 * t ** 2 + 1
    h10 = t ** 3 - 2 * t ** 2 + t
    h01 = -2 * t ** 3 + 3 * t ** 2
    h11 = t ** 3 - t ** 2
    return h00 * y[k] + h10 * h * m[k] + h01 * y[k + 1] + h11 * h * m[k + 1]


# ---------------------------------------------------------------- 2D 可分离重建
def spline2d(grid, delta, q, clip=True, gox=0.0, goy=0.0, node0=None):
    """可分离自然双三次样条（生产默认算子语义）。

    grid[m,n] = 节点值；节点坐标 = node0 + i*delta（默认 node0 = gox + (delta-1)/2，
    即 native 网格的 cell_center_v1 约定）。做留出交叉验证时 train 子网格的首节点
    不是 (delta-1)/2，必须显式传 node0（否则节点坐标错位、样条被构建在错误位置上）。
    clip=True 时把结果钳到 [min(grid), max(grid)]（= 生产 natural_bicubic_spline_clip_v1）。
    q: 1D 查询坐标数组（x 与 y 用同一组，输出 |q|×|q|）。
    域外（越出 cell 并集）不做 fail-closed，按边界三次式外推 —— 这是**朴素消费方**的
    行为，用于对照与可检测性演示；生产 eval 会拒绝并给 out_of_domain。
    """
    grid = np.asarray(grid, float)
    m = grid.shape[0]
    n0 = (gox + (delta - 1) / 2.0) if node0 is None else node0
    knots = n0 + np.arange(m) * delta
    q = np.asarray(q, float)

    def coeffs(mat, xq):
        n, k = mat.shape
        h = np.diff(xq)
        al = np.zeros((n, k))
        al[1:-1] = 3 * ((mat[2:] - mat[1:-1]) / h[1:, None]
                        - (mat[1:-1] - mat[:-2]) / h[:-1, None])
        l, mu, z = np.ones((n, k)), np.zeros((n, k)), np.zeros((n, k))
        for i in range(1, n - 1):
            l[i] = 2 * (xq[i + 1] - xq[i - 1]) - h[i - 1] * mu[i - 1]
            mu[i] = h[i] / l[i]
            z[i] = (al[i] - h[i - 1] * z[i - 1]) / l[i]
        b, c, d = np.zeros((n - 1, k)), np.zeros((n, k)), np.zeros((n - 1, k))
        for j in range(n - 2, -1, -1):
            c[j] = z[j] - mu[j] * c[j + 1]
            b[j] = (mat[j + 1] - mat[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3
            d[j] = (c[j + 1] - c[j]) / (3 * h[j])
        return b, c, d

    b0, c0, d0 = coeffs(grid, knots)
    k = np.clip(np.searchsorted(knots, q) - 1, 0, m - 2)
    dx = (q[:, None] - knots[k][:, None])
    mid = grid[k, :] + b0[k, :] * dx + c0[k, :] * dx ** 2 + d0[k, :] * dx ** 3
    b1, c1, d1 = coeffs(mid.T.copy(), knots)
    dx2 = (q[:, None] - knots[k][:, None])
    out = mid.T[k, :] + b1[k, :] * dx2 + c1[k, :] * dx2 ** 2 + d1[k, :] * dx2 ** 3
    # out[a,b] = f(x=q[a], y=q[b])；统一返回 [y, x] 下标（与 v_true / 掩膜同约定）
    out = out.T
    if clip:
        out = np.clip(out, float(grid.min()), float(grid.max()))
    return out


def bilinear2d(grid, delta, q, gox=0.0, node0=None):
    """规则网格双线性（生产 bilinear_regular_grid_v1 语义；域外端点常数延拓、不钳制）。

    node0 同 spline2d：train 子网格必须显式给首节点坐标。"""
    grid = np.asarray(grid, float)
    m = grid.shape[0]
    n0 = (gox + (delta - 1) / 2.0) if node0 is None else node0
    knots = n0 + np.arange(m) * delta
    q = np.asarray(q, float)

    def interp1(mat, xq):
        k = np.clip(np.searchsorted(xq, q) - 1, 0, len(xq) - 2)
        t = (q - xq[k]) / (xq[k + 1] - xq[k])
        t = np.clip(t, 0.0, 1.0)
        return mat[k, :] * (1 - t)[:, None] + mat[k + 1, :] * t[:, None]

    mid = interp1(grid, knots)          # (len(q), m)
    b1 = mid.T                          # (m, len(q)) -> 沿 y 的查询维
    out = interp1(b1, knots)            # [x, y]
    return out.T                        # 统一返回 [y, x]


def idw2d(pts_xy, vals, q, p, eps=1e-10):
    """IDW：q(x) = Σ w_i q_i / Σ w_i，w_i = d_i^(−p)；d<eps ⇒ 精确返回该控制点值。

    返回 (field, n_exact_coincidence)。"""
    pts = np.asarray(pts_xy, float)
    vals = np.asarray(vals, float)
    q = np.asarray(q, float)
    QX, QY = np.meshgrid(q, q)          # 注意 meshgrid 默认 'xy'
    flat_x = QX.ravel(); flat_y = QY.ravel()
    px = pts[:, 0][:, None]; py = pts[:, 1][:, None]
    d = np.sqrt((flat_x[None, :] - px) ** 2 + (flat_y[None, :] - py) ** 2)
    out = np.empty(flat_x.size, float)
    n_exact = 0
    for j in range(flat_x.size):
        dj = d[:, j]
        hit = np.nonzero(dj < eps)[0]
        if hit.size:
            out[j] = vals[hit[0]]
            n_exact += 1
        else:
            w = dj ** (-p)
            out[j] = float(np.sum(w * vals) / np.sum(w))
    return out.reshape(QX.shape), n_exact


# ---------------------------------------------------------------- 控制网格
def grid_nodes(nx, ny, delta_x, delta_y, gox=0.0, goy=0.0):
    xs = gox + np.arange(nx) * delta_x + (delta_x - 1) / 2.0
    ys = goy + np.arange(ny) * delta_y + (delta_y - 1) / 2.0
    X, Y = np.meshgrid(xs, ys)          # X[j,i], Y[j,i]
    return X, Y


def domain_box(nx, ny, dx, dy, gox=0.0, goy=0.0):
    return (gox - 0.5, gox + nx * dx - 0.5, goy - 0.5, goy + ny * dy - 0.5)


def out_of_domain_mask(nx, ny, dx, dy, W, H, gox=0.0, goy=0.0, tol=1e-6):
    """生产定义域语义下的域外查询布尔掩膜（W×H 视场）。

    生产判定（weight_chain.cpp:762-771）：in-domain ⇔
      gox − 0.5 ≤ x ≤ gox + nx·dx − 0.5（y 同，容差 1e-6）
    """
    lo_x, hi_x, lo_y, hi_y = domain_box(nx, ny, dx, dy, gox, goy)
    xs = np.arange(W, dtype=float)
    ys = np.arange(H, dtype=float)
    inx = (xs >= lo_x - tol) & (xs <= hi_x + tol)
    iny = (ys >= lo_y - tol) & (ys <= hi_y + tol)
    return ~(iny[:, None] & inx[None, :])


# ---------------------------------------------------------------- 场型
def make_fields(W, H):
    """S06 场型：常数 / 缓变 / 强梯度 / 边缘 / 多尺度结构 / 亮斑。"""
    x = np.arange(W, dtype=float)[None, :]
    y = np.arange(H, dtype=float)[:, None]
    u = x / W
    v = y / H
    f = {}
    f["constant"] = np.full((H, W), 10.0)
    f["slow_linear"] = 10.0 * (1.0 + 0.8 * (x - W / 2.0) / W) * np.ones((H, W))
    f["strong_gradient"] = 10.0 * 10 ** (2.0 * (u - 0.5)) * np.ones((H, W))
    f["edge"] = np.where(x < W / 2.0, 10.0, 40.0) * np.ones((H, W))
    f["nebula"] = 10.0 * np.maximum(
        1.0 + 0.45 * np.sin(3 * np.pi * u) * np.cos(2 * np.pi * v)
        + 0.25 * np.sin(11 * np.pi * u + 0.7) * np.sin(7 * np.pi * v + 1.3)
        + 0.12 * np.sin(23 * np.pi * u + 2.1) * np.cos(19 * np.pi * v + 0.3), 1e-6)
    blobs = np.ones((H, W))
    for (cx, cy, amp, r) in ((0.22, 0.30, 30.0, 0.06), (0.68, 0.75, 12.0, 0.10),
                             (0.45, 0.55, 60.0, 0.03)):
        blobs += amp * np.exp(-(((u - cx) ** 2 + (v - cy) ** 2)) / (2 * r * r))
    f["blobs"] = 10.0 * blobs
    return f


def patch_mad_var(d, patch=PATCH):
    n = d.shape[0] // patch
    b = d[:n * patch, :n * patch].reshape(n, patch, n, patch)
    med = np.median(b, axis=(1, 3))
    mad = np.median(np.abs(b - med[:, None, :, None]), axis=(1, 3))
    return (MAD_K * mad) ** 2


# ---------------------------------------------------------------- 输出
def sha256_text(s: str) -> str:
    return hashlib.sha256(s.encode("utf-8")).hexdigest()


def sha256_file(p) -> str:
    p = Path(p)
    if not p.exists():
        return ""
    h = hashlib.sha256()
    h.update(p.read_bytes())
    return h.hexdigest()


def out_dir() -> Path:
    d = Path(os.environ.get("S06_OUT", "run/ACSDSDOC-02/S06/results"))
    d.mkdir(parents=True, exist_ok=True)
    return d


def run_meta(script: str, command: str, extra=None):
    """每个数值 JSON 的固定元信息（物理定义/单位/分母/样本量/输入/seed/SHA）。"""
    meta = {
        "paper_id": "P4",
        "task_id": "S06",
        "script": script,
        "script_sha256": sha256_file(Path(__file__).resolve().parent / script),
        "command": command,
        "seed": SEED,
        "f_ref_adu": F_REF,
        "gain_e_per_adu": GAIN,
        "read_noise_e": READ_E,
        "delta_px": DELTA,
        "patch_px": PATCH,
        "unit_of_snr": "dimensionless [1] (flux-type unweighted SNR = F_ref/sigma_F)",
        "unit_of_variance": "ADU^2",
        "unit_of_weight": "ADU^-2",
        "e_eff_denominator": "Var_opt = 1/sum(1/v_true) per pixel (P4-B02 sole definition)",
    }
    if extra:
        meta.update(extra)
    return meta


def dump(name: str, payload: dict):
    p = out_dir() / name
    p.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[S06] wrote {p}")
    return p
