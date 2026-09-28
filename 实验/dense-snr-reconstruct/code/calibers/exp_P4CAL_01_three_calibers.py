#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4-CAL-01：三口径（frame / cell / dense）效率对照实验 —— 闭环审查 P4-M10。

审查 M10 的两条补救：
  ① 补 dense 口径对照实验（同 fixture：陡峭结构场 + 平坦负例 + 真实信号模板前向 + 真实帧）；
  ② frame 口径需说明其与生产"帧级逆方差链"的关系（单帧内 = 常数铺满，多帧才有标量差异）。

口径定义（全部用**同一权公式** w = SNR²/F_ref² = 1/σ̂²，只换还原粒度）：
  frame        帧级标量：σ̂² = 节点值中位数（生产帧级口径在单帧内等价于常数铺满）；
  cell         Δ=64 格级：节点值在 cell 内常值铺满（"最近/块状"还原）；
  dense_recon  稠密：生产算子（可分自然三次样条）把同一批节点还原到逐像素（= P4 交付物）；
  oracle_dense 稠密上界：逐像素真方差（合成/前向类有真值；真值权重 ⇒ E 必须机器零）。
判据（非退化）：
  G1 oracle_dense E_eff ≤ 1e-12（度量自检：真值权重无损失）；
  G2 结构场 E_frame > E_cell > E_dense_recon ≥ 0（粒度越细越优，严格序）；
  G3 平坦负例：真值无结构时 E_frame = 0（机器零）且 E_dense_recon ≥ E_frame（稠密**不得**凭空占优）；
  R1 红臂：节点打乱后的稠密场 E_eff 必须 > 真实稠密场；
  R2 红臂：被否口径 E_prop = Var_w/(1/Σw) − 1 尺度不不变、可为负 ⇒ 判无效。
三类数据：
  ① 解析合成（含"真值无效应⇒归零"负例）；
  ② HST 真实信号模板 + 物理前向（泊松散粒 + 读出噪声，真值方差逐像素已知）；
  ③ 真实帧（HST M16 f502n）—— 无绝对真值，参照量取局部 MAD² 代理，只作一致性读数。
依赖：numpy（计算）+ astropy（仅 FITS 读取）。
"""
import json
from pathlib import Path

import numpy as np
from astropy.io import fits

SEED = 20260928
UNIT = Path(__file__).resolve().parents[2]
RESULTS = UNIT / "results" / "calibers"
RESULTS.mkdir(parents=True, exist_ok=True)
FRAME = UNIT.parents[1] / "testdata" / "HST_M16" / "hlsp_heritage_hst_wfc3-uvis_m16_f502n_v1_drz.fits"
DELTA, PATCH = 64, 8
MAD_K = 1.482602218505602
GAIN, READ_E = 1.3, 5.0          # e-/ADU、读出噪声 e-
rng = np.random.default_rng(SEED)


def E_eff(w, v):
    """唯一口径（P4-B02）：E_eff = Var_w/Var_opt − 1，Var_opt = 1/Σ(1/v)，恒 ≥ 0。"""
    w = np.asarray(w, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    return float(np.sum(w ** 2 * v) / np.sum(w) ** 2 * np.sum(1.0 / v) - 1.0)


def E_prop(w, v):
    """被否口径（负例）：Var_opt = 1/Σw ⇒ 无下界语义、且不满足尺度不变。"""
    w = np.asarray(w, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    return float(np.sum(w ** 2 * v) / np.sum(w) - 1.0)


def dex_rmse(a, b):
    a = np.maximum(np.asarray(a, float), 1e-300)
    b = np.maximum(np.asarray(b, float), 1e-300)
    return float(np.sqrt(np.mean((np.log10(a) - np.log10(b)) ** 2)))


def patch_mad_var(d, patch=PATCH):
    n = d.shape[0] // patch
    b = d[:n * patch, :n * patch].reshape(n, patch, n, patch)
    med = np.median(b, axis=(1, 3))
    mad = np.median(np.abs(b - med[:, None, :, None]), axis=(1, 3))
    return (MAD_K * mad) ** 2


def cell_nodes(vp, delta=DELTA, patch=PATCH):
    """生产配方：cell 内 8×8 patch 稳健方差 → var = a+b·x+c·y 平面拟合 → 节点值。"""
    per = delta // patch
    n = vp.shape[0] // per
    idx = (np.arange(per) + 0.5) * patch
    node = (delta - 1) / 2.0
    X, Y = np.meshgrid(idx, idx)
    Ainv = np.linalg.pinv(np.column_stack([np.ones(per * per), X.ravel(), Y.ravel()]))
    out = np.zeros((n, n))
    for i in range(n):
        for j in range(n):
            coef = Ainv @ vp[i * per:(i + 1) * per, j * per:(j + 1) * per].ravel()
            out[i, j] = coef[0] + coef[1] * node + coef[2] * node
    return out


def spline2d(grid, delta, q):
    """可分自然三次样条（与生产/route1/route3 同族算子），在查询坐标 q 处求值。"""
    m = grid.shape[0]
    knots = np.arange(m) * delta + (delta - 1) / 2.0

    def coeffs(y, xq):
        n, k = y.shape
        h = np.diff(xq)
        al = np.zeros((n, k))
        al[1:-1] = 3 * ((y[2:] - y[1:-1]) / h[1:, None] - (y[1:-1] - y[:-2]) / h[:-1, None])
        l, mu, z = np.ones((n, k)), np.zeros((n, k)), np.zeros((n, k))
        for i in range(1, n - 1):
            l[i] = 2 * (xq[i + 1] - xq[i - 1]) - h[i - 1] * mu[i - 1]
            mu[i] = h[i] / l[i]
            z[i] = (al[i] - h[i - 1] * z[i - 1]) / l[i]
        b, c, d = np.zeros((n, k)), np.zeros((n, k)), np.zeros((n, k))
        for j in range(n - 2, -1, -1):
            c[j] = z[j] - mu[j] * c[j + 1]
            b[j] = (y[j + 1] - y[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3
            d[j] = (c[j + 1] - c[j]) / (3 * h[j])
        return b, c, d

    b0, c0, d0 = coeffs(grid.copy(), knots)
    k = np.clip(np.searchsorted(knots, q) - 1, 0, m - 2)
    dx = (q - knots[k])[:, None]
    mid = grid[k, :] + b0[k, :] * dx + c0[k, :] * dx ** 2 + d0[k, :] * dx ** 3
    b1, c1, d1 = coeffs(mid.T.copy(), knots)
    return mid.T[k, :] + b1[k, :] * dx + c1[k, :] * dx ** 2 + d1[k, :] * dx ** 3


def blocky(nodes, delta, patch, size):
    """格级还原：节点值在 cell 内常值铺满，再降到 patch 网格。"""
    per = delta // patch
    fine = np.repeat(np.repeat(nodes, per, 0), per, 1)
    q = np.arange(size // patch) * patch + (patch - 1) / 2.0
    return fine[:size // patch, :size // patch]


def calibers(v_true, d_img, size, tag):
    """在同一 fixture 上产出四个口径 + 两条红臂的读数。"""
    vp = patch_mad_var(d_img)
    npl = cell_nodes(vp)
    q = np.arange(size // PATCH) * PATCH + (PATCH - 1) / 2.0
    rec = spline2d(npl, DELTA, q)
    cell = blocky(npl, DELTA, PATCH, size)
    vf = np.repeat(np.repeat(v_true.reshape(size // PATCH, PATCH, size // PATCH, PATCH)
                             .mean(axis=(1, 3)), PATCH, 0), PATCH, 1)
    vf = vf[:rec.shape[0], :rec.shape[1]]
    sh = spline2d(rng.permutation(npl.ravel()).reshape(npl.shape), DELTA, q)
    frame_sigma2 = float(np.median(npl))
    out = {
        "E_eff_frame": E_eff(np.full(rec.shape, 1.0 / frame_sigma2), vf),
        "E_eff_cell": E_eff(1.0 / cell, vf),
        "E_eff_dense_recon": E_eff(1.0 / rec, vf),
        "E_eff_oracle_dense": E_eff(1.0 / vf, vf),
        "E_eff_dense_shuffled_RED": E_eff(1.0 / sh, vf),
        "dex_rmse_dense_recon": dex_rmse(rec, vf),
        "dex_rmse_cell": dex_rmse(cell, vf),
        "p5_stack_var_ratio": {
            "frame": E_eff(np.full(rec.shape, 1.0 / frame_sigma2), vf) + 1.0,
            "cell": E_eff(1.0 / cell, vf) + 1.0,
            "dense_recon": E_eff(1.0 / rec, vf) + 1.0,
            "oracle_dense": 1.0,
        },
        "E_prop_invalid_RED": {
            "dense_w": E_prop(1.0 / rec, vf),
            "dense_w_x3p17": E_prop(3.17 / rec, vf),
            "note": "被否口径：乘性缩放后读数改变（不尺度不变），且可为负",
        },
        "n_nodes": [int(npl.shape[0]), int(npl.shape[1])],
        "frame_sigma2_median": frame_sigma2,
    }
    out["gates"] = {
        "G1_oracle_dense_zero": out["E_eff_oracle_dense"] <= 1e-12,
        "G2_monotone_frame_gt_cell_gt_dense": (out["E_eff_frame"] > out["E_eff_cell"] > out["E_eff_dense_recon"] >= 0.0),
        "R1_shuffled_worse": out["E_eff_dense_shuffled_RED"] > out["E_eff_dense_recon"],
        "R2_prop_not_scale_invariant": abs(out["E_prop_invalid_RED"]["dense_w_x3p17"]
                                           - out["E_prop_invalid_RED"]["dense_w"]) > 1e-6,
    }
    out["all_pass"] = all(out["gates"].values())
    out["tag"] = tag
    return out


res = {"seed": SEED, "delta": DELTA, "patch": PATCH, "gain_e_per_adu": GAIN,
       "read_noise_e": READ_E, "metric": "E_eff = Var_w/Var_opt - 1（v = σ_true²，唯一口径 P4-B02）",
       "calibers": {"frame": "帧级标量（节点中位数；单帧内 = 生产帧级逆方差链的常数铺满）",
                    "cell": "Δ=64 格级常值铺满", "dense_recon": "生产样条逐像素（P4 交付物）",
                    "oracle_dense": "逐像素真方差（上界，E 必须机器零）"}}

# ---------------- ① 解析合成：结构场 + 平坦负例
SZ = 2048
yy, xx = np.mgrid[0:SZ, 0:SZ] / SZ
struct = (12.0 + 8.0 * np.exp(-((xx - 0.35) ** 2 + (yy - 0.4) ** 2) / 0.01)
          + 6.0 * np.exp(-((xx - 0.7) ** 2 + (yy - 0.65) ** 2) / 0.002)
          + 3.0 * (1.0 + np.sin(16 * np.pi * xx) * np.sin(16 * np.pi * yy)) * np.exp(-((yy - 0.2) / 0.25) ** 2))
flat = np.full((SZ, SZ), 15.0)
syn = {}
for tag, S in (("structured", struct), ("flat_negative_control", flat)):
    mu_e = S * GAIN
    d_img = (rng.poisson(mu_e) + rng.normal(0.0, READ_E, S.shape)) / GAIN
    v_true = (mu_e + READ_E ** 2) / GAIN ** 2
    r = calibers(v_true, d_img, SZ, tag)
    if tag == "flat_negative_control":
        r["gates"]["G3_flat_no_fake_advantage"] = (abs(r["E_eff_frame"]) <= 1e-12
                                                  and r["E_eff_dense_recon"] >= r["E_eff_frame"])
        r["gates"].pop("G2_monotone_frame_gt_cell_gt_dense", None)
        r["all_pass"] = all(r["gates"].values())
    syn[tag] = r
res["A_analytic_synthetic"] = syn

# ---------------- ② HST 真实信号模板 + 物理前向
with fits.open(FRAME, memmap=True) as h:
    d = np.array(h[0].data)
    r0 = (d.shape[0] - SZ) // 2
    c0 = (d.shape[1] - SZ) // 2
    tmpl_rate = np.clip(d[r0:r0 + SZ, c0:c0 + SZ].astype(np.float64), 0.0, None)
del d
t_e = 5.0e4 / max(np.percentile(tmpl_rate, 99.9), 1e-9)      # 曝光标定：p99.9 ≈ 5e4 e-
mu_e2 = np.clip(tmpl_rate * t_e, 0.0, None)
d2 = (rng.poisson(mu_e2) + rng.normal(0.0, READ_E, mu_e2.shape)) / GAIN
v_true2 = (mu_e2 + READ_E ** 2) / GAIN ** 2
res["B_hst_template_physical_forward"] = calibers(v_true2, d2, SZ, "hst_template")
res["B_hst_template_physical_forward"]["exposure_scale_e"] = float(t_e)
res["B_hst_template_physical_forward"]["median_counts_e"] = float(np.median(mu_e2))
res["B_hst_template_physical_forward"]["p999_counts_e"] = float(np.percentile(mu_e2, 99.9))

# ---------------- ③ 真实帧（受限：无绝对真值）
with fits.open(FRAME, memmap=True) as h:
    d3 = np.array(h[0].data)[r0:r0 + SZ, c0:c0 + SZ].astype(np.float64)
vp3 = patch_mad_var(d3)
npl3 = cell_nodes(vp3)
q3 = np.arange(SZ // PATCH) * PATCH + (PATCH - 1) / 2.0
rec3 = spline2d(npl3, DELTA, q3)
cell3 = blocky(npl3, DELTA, PATCH, SZ)
ref3 = vp3[:rec3.shape[0], :rec3.shape[1]]
res["C_real_frame_proxy_reference"] = {
    "frame": str(FRAME.relative_to(UNIT.parents[1])), "crop": [SZ, SZ],
    "reference": "局部 MAD² 代理（含散粒噪声 + 真实小尺度结构，非绝对真方差）",
    "E_eff_frame": E_eff(np.full(rec3.shape, 1.0 / float(np.median(npl3))), ref3),
    "E_eff_cell": E_eff(1.0 / cell3, ref3),
    "E_eff_dense_recon": E_eff(1.0 / rec3, ref3),
    "E_eff_dense_shuffled_RED": E_eff(1.0 / spline2d(rng.permutation(npl3.ravel()).reshape(npl3.shape), DELTA, q3), ref3),
    "dex_rmse_dense_recon": dex_rmse(rec3, ref3),
    "note": "真实帧无绝对真值 ⇒ 本类只作一致性/相对序读数，不参与 G1/G2 判定",
    "relative_order_frame_gt_dense": bool(E_eff(np.full(rec3.shape, 1.0 / float(np.median(npl3))), ref3)
                                          > E_eff(1.0 / rec3, ref3)),
}
res["all_pass"] = bool(res["A_analytic_synthetic"]["structured"]["all_pass"]
                       and res["A_analytic_synthetic"]["flat_negative_control"]["all_pass"]
                       and res["B_hst_template_physical_forward"]["all_pass"])
(RESULTS / "exp_P4CAL_01_three_calibers.json").write_text(
    json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
print(json.dumps(res, indent=2, ensure_ascii=False))
