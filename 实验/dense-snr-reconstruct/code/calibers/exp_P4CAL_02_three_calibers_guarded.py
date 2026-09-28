#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4-CAL-02：三口径（frame / cell / dense）效率对照 —— 逐像素口径 + 对比度扫描 + 守卫臂。

闭环审查 P4-M10。本文件取代同名的 patch 网格版本：度量一律算在 **逐像素**（生产 P5 逐像素定权
w = SNR²/F_ref²），避免亮源像素上的度量错配。P4-CAL-01（低对比解析场 + patch 网格）读数作为
过程记录保留在 results/calibers/exp_P4CAL_01_three_calibers.json。

口径（同一权公式 w = 1/σ̂²，只换还原粒度；E_eff 对 w 乘性缩放免疫）：
  frame 帧级标量（节点中位数；单帧内 = 生产帧级逆方差链的常数铺满）｜cell Δ=64 格级常值铺满
  dense_recon 生产样条逐像素（P4 交付物）｜guard_mask 最亮 0.5% 像素改用帧级权重
  guard_shotfloor v̂ ≥ 像素亮度隐含散粒方差（v = d/g）｜oracle_dense 逐像素真方差（上界）
判据：G1 oracle 机器零；G2 结构场 E_dense < E_frame；G3 平坦负例 E_frame = 0 且 E_dense ≥ E_frame；
G2b 亮源域反转可检出；D1 亮源抑制诊断；G4 守卫必须改善；R1 打乱变差；R2 E_prop 尺度不不变。
三类数据：① 解析合成（含平坦负例 + 对比度扫描）② HST 真实信号模板 + 物理前向 ③ 真实帧（MAD² 代理）。
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
DELTA, PATCH, SZ = 64, 8, 2048
MAD_K = 1.482602218505602
GAIN, READ_E = 1.3, 5.0
rng = np.random.default_rng(SEED)


def E_eff(w, v):
    w = np.asarray(w, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    return float(np.sum(w ** 2 * v) / np.sum(w) ** 2 * np.sum(1.0 / v) - 1.0)


def E_prop(w, v):
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


def blocky_px(nodes, delta, size):
    return np.repeat(np.repeat(nodes, delta, 0), delta, 1)[:size, :size]


def shoot(mu_e, shape):
    d = (rng.poisson(mu_e) + rng.normal(0.0, READ_E, shape)) / GAIN
    v = (np.asarray(mu_e, dtype=np.float64) + READ_E ** 2) / GAIN ** 2
    return d, np.maximum(v, 1e-300)


def calibers_px(v_true, d_img, tag, expect):
    size = v_true.shape[0]
    vp = patch_mad_var(d_img)
    npl = cell_nodes(vp)
    q = np.arange(size, dtype=np.float64)
    rec = spline2d(npl, DELTA, q)
    cell = blocky_px(npl, DELTA, size)
    shuf = spline2d(rng.permutation(npl.ravel()).reshape(npl.shape), DELTA, q)
    frame_sigma2 = float(np.median(npl))
    w_frame = np.full(rec.shape, 1.0 / frame_sigma2)
    thr = float(np.percentile(d_img, 99.5))
    w_mask = np.where(d_img >= thr, 1.0 / frame_sigma2, 1.0 / rec)
    v_floor = np.maximum(rec, d_img / GAIN)
    v_clamp = np.maximum(np.clip(rec, 0.25 * float(npl.min()), 4.0 * float(npl.max())), d_img / GAIN)
    node_px = blocky_px(npl, DELTA, size)
    bright = d_img >= float(np.percentile(d_img, 99.0))
    out = {
        "tag": tag,
        "E_eff_frame": E_eff(w_frame, v_true),
        "E_eff_cell": E_eff(1.0 / cell, v_true),
        "E_eff_dense_recon": E_eff(1.0 / rec, v_true),
        "E_eff_dense_guard_mask": E_eff(w_mask, v_true),
        "E_eff_dense_guard_shotfloor": E_eff(1.0 / v_floor, v_true),
        "E_eff_dense_guard_clamp": E_eff(1.0 / v_clamp, v_true),
        "E_eff_oracle_dense": E_eff(1.0 / v_true, v_true),
        "E_eff_dense_shuffled_RED": E_eff(1.0 / shuf, v_true),
        "spline_rec_min": float(rec.min()),
        "spline_rec_nonpos_frac": float((rec <= 0).mean()),
        "spline_rec_below_node_min_frac": float((rec < float(npl.min())).mean()),
        "dex_rmse_dense_recon": dex_rmse(rec, v_true),
        "dex_rmse_frame": dex_rmse(np.full(rec.shape, frame_sigma2), v_true),
        "node_field_vs_truth_at_bright_pixels": float(np.median(node_px[bright] / v_true[bright])) if bright.any() else float("nan"),
        "v_true_dynamic_range": float(v_true.max() / v_true.min()),
        "p5_stack_var_ratio": {"frame": E_eff(w_frame, v_true) + 1.0, "cell": E_eff(1.0 / cell, v_true) + 1.0,
                               "dense_recon": E_eff(1.0 / rec, v_true) + 1.0,
                               "dense_guard_shotfloor": E_eff(1.0 / v_floor, v_true) + 1.0},
        "E_prop_invalid_RED": {"dense_w": E_prop(1.0 / rec, v_true), "dense_w_x3p17": E_prop(3.17 / rec, v_true),
                               "note": "被否口径：乘性缩放后读数改变，且可为负"},
        "n_nodes": [int(npl.shape[0]), int(npl.shape[1])],
        "frame_sigma2_median": frame_sigma2,
    }
    if expect == "structured":
        out["gates"] = {
            "G1_oracle_dense_zero": out["E_eff_oracle_dense"] <= 1e-12,
            "G2_dense_beats_frame": out["E_eff_dense_recon"] < out["E_eff_frame"],
            "R1_shuffled_worse": out["E_eff_dense_shuffled_RED"] > out["E_eff_dense_recon"],
            "R2_prop_not_scale_invariant": abs(out["E_prop_invalid_RED"]["dense_w_x3p17"] - out["E_prop_invalid_RED"]["dense_w"]) > 1e-6,
        }
    elif expect == "flat":
        out["gates"] = {
            "G1_oracle_dense_zero": out["E_eff_oracle_dense"] <= 1e-12,
            "G3_flat_frame_zero": abs(out["E_eff_frame"]) <= 1e-12,
            "G3_flat_dense_no_fake_advantage": out["E_eff_dense_recon"] >= out["E_eff_frame"],
            "G3_flat_spline_no_undershoot": out["spline_rec_nonpos_frac"] == 0.0,
            "R2_prop_not_scale_invariant": abs(out["E_prop_invalid_RED"]["dense_w_x3p17"] - out["E_prop_invalid_RED"]["dense_w"]) > 1e-6,
        }
    else:
        out["gates"] = {
            "G1_oracle_dense_zero": out["E_eff_oracle_dense"] <= 1e-12,
            "G2b_inversion_dense_worse_than_frame": out["E_eff_dense_recon"] > out["E_eff_frame"],
            "D1_spline_undershoot_detected": bool(out["spline_rec_min"] <= 0.0 or out["spline_rec_nonpos_frac"] > 0.0),
            "G4_guard_shotfloor_improves": out["E_eff_dense_guard_shotfloor"] < out["E_eff_dense_recon"],
            "G4_guard_clamp_improves": out["E_eff_dense_guard_clamp"] < out["E_eff_dense_recon"],
            "R1_shuffled_worse": out["E_eff_dense_shuffled_RED"] > out["E_eff_dense_recon"],
            "R2_prop_not_scale_invariant": abs(out["E_prop_invalid_RED"]["dense_w_x3p17"] - out["E_prop_invalid_RED"]["dense_w"]) > 1e-6,
        }
    out["all_pass"] = all(out["gates"].values())
    return out


res = {"seed": SEED, "delta": DELTA, "patch": PATCH, "gain_e_per_adu": GAIN, "read_noise_e": READ_E,
       "metric": "E_eff = Var_w/Var_opt - 1，逐像素（v = σ_true²；唯一口径 P4-B02）", "size": SZ,
       "calibers": {"frame": "帧级标量（节点中位数）", "cell": "Δ=64 格级常值铺满",
                    "dense_recon": "生产样条逐像素（P4 交付物）", "guard_mask": "最亮 0.5% 像素改用帧级权重",
                    "guard_shotfloor": "v̂ ≥ 像素亮度隐含散粒方差（v = d/g）", "oracle_dense": "逐像素真方差（上界）"}}

yy, xx = np.mgrid[0:SZ, 0:SZ] / SZ
shape_field = (np.exp(-((xx - 0.35) ** 2 + (yy - 0.4) ** 2) / 0.02)
               + 0.25 * np.exp(-((xx - 0.72) ** 2 + (yy - 0.62) ** 2) / 0.01)
               + 0.06 * (1.0 + np.sin(10 * np.pi * xx) * np.sin(10 * np.pi * yy)) * np.exp(-((yy - 0.25) / 0.3) ** 2))
BG_ADU = 8.0
mu_s = (BG_ADU + 900.0 * shape_field) * GAIN
d_s, v_s = shoot(mu_s, mu_s.shape)
res["A_analytic_structured"] = calibers_px(v_s, d_s, "analytic_structured", "structured")
mu_f = np.full((SZ, SZ), 15.0) * GAIN
d_f, v_f = shoot(mu_f, mu_f.shape)
res["A2_flat_negative_control"] = calibers_px(v_f, d_f, "flat_negative_control", "flat")

sweep = []
for amp in (0.5, 20.0, 900.0, 6000.0, 40000.0, 3.0e5):
    mu_k = (BG_ADU + amp * shape_field) * GAIN
    d_k, v_k = shoot(mu_k, mu_k.shape)
    r = calibers_px(v_k, d_k, "contrast_amp=%g" % amp, "structured")
    sweep.append({"amp_adu": amp, "v_dynamic_range": r["v_true_dynamic_range"],
                  "E_eff_frame": r["E_eff_frame"], "E_eff_cell": r["E_eff_cell"],
                  "E_eff_dense_recon": r["E_eff_dense_recon"],
                  "E_eff_dense_guard_shotfloor": r["E_eff_dense_guard_shotfloor"],
                  "dense_beats_frame": r["E_eff_dense_recon"] < r["E_eff_frame"]})
wins = [r for r in sweep if r["dense_beats_frame"]]
loss = [r for r in sweep if not r["dense_beats_frame"]]
res["A3_contrast_sweep"] = {
    "rows": sweep,
    "band_note": ("实测为**有限窗口**：稠密口径只在 v 跨幅 %.3g–%.3g 之间优于帧级常数口径；低端受常数权重的"
                  "Jensen 惩罚限制，高端受重建自身噪声/样条欠冲限制。原判据「随对比度单调变好」被本扫描否证，"
                  "在此登记为已证伪假设。") % (min(r["v_dynamic_range"] for r in wins), max(r["v_dynamic_range"] for r in wins))
                 if wins else "本次扫描未观察到稠密口径优于帧级口径的档位",
    "dense_wins_vranges": [r["v_dynamic_range"] for r in wins],
    "dense_loses_vranges": [r["v_dynamic_range"] for r in loss],
    "observation_cell_is_best_rows": "%d/%d" % (sum(1 for r in sweep if r["E_eff_cell"] < min(r["E_eff_frame"], r["E_eff_dense_recon"])), len(sweep)),
    "gates": {"G5_band_two_sided": bool(len(wins) > 0 and len(loss) > 0)},
    "all_pass": bool(len(wins) > 0 and len(loss) > 0),
}

with fits.open(FRAME, memmap=True) as h:
    d = np.array(h[0].data)
    r0 = (d.shape[0] - SZ) // 2
    c0 = (d.shape[1] - SZ) // 2
    rate = np.clip(d[r0:r0 + SZ, c0:c0 + SZ].astype(np.float64), 0.0, None)
del d
t_e = 5.0e4 / max(np.percentile(rate, 99.9), 1e-9)
mu_h = np.clip(rate * t_e, 0.0, None)
d_h, v_h = shoot(mu_h, mu_h.shape)
res["B_hst_template_physical_forward"] = calibers_px(v_h, d_h, "hst_template_compact_sources", "compact")
res["B_hst_template_physical_forward"]["exposure_scale_e"] = float(t_e)
res["B_hst_template_physical_forward"]["p999_counts_e"] = float(np.percentile(mu_h, 99.9))

with fits.open(FRAME, memmap=True) as h:
    d3 = np.array(h[0].data)[r0:r0 + SZ, c0:c0 + SZ].astype(np.float64)
vp3 = patch_mad_var(d3)
npl3 = cell_nodes(vp3)
q3 = np.arange(SZ, dtype=np.float64)
rec3 = spline2d(npl3, DELTA, q3)
cell3 = blocky_px(npl3, DELTA, SZ)
ref3 = np.repeat(np.repeat(vp3, PATCH, 0), PATCH, 1)[:SZ, :SZ]
thr3 = float(np.percentile(d3, 99.5))
w_frame3 = np.full(rec3.shape, 1.0 / float(np.median(npl3)))
w_mask3 = np.where(d3 >= thr3, 1.0 / float(np.median(npl3)), 1.0 / rec3)
res["C_real_frame_proxy_reference"] = {
    "frame": str(FRAME.relative_to(UNIT.parents[1])), "crop": [SZ, SZ],
    "reference": "局部 MAD² 代理（铺到逐像素；含散粒噪声 + 真实小尺度结构，非绝对真方差）",
    "E_eff_frame": E_eff(w_frame3, ref3), "E_eff_cell": E_eff(1.0 / cell3, ref3),
    "E_eff_dense_recon": E_eff(1.0 / rec3, ref3),
    "E_eff_dense_guard_mask": E_eff(w_mask3, ref3),
    "E_eff_dense_shuffled_RED": E_eff(1.0 / spline2d(rng.permutation(npl3.ravel()).reshape(npl3.shape), DELTA, q3), ref3),
    "dex_rmse_dense_recon": dex_rmse(rec3, ref3),
    "note": "真实帧无绝对真值 ⇒ 只作一致性/相对序读数；散粒地板守卫需要源项标定（本类不适用）",
    "gates": {"G2b_inversion_detected": E_eff(1.0 / rec3, ref3) > E_eff(w_frame3, ref3),
              "G4_guard_mask_improves": E_eff(w_mask3, ref3) < E_eff(1.0 / rec3, ref3)},
}
res["C_real_frame_proxy_reference"]["all_pass"] = all(res["C_real_frame_proxy_reference"]["gates"].values())
res["all_pass"] = bool(res["A_analytic_structured"]["all_pass"]
                       and res["A2_flat_negative_control"]["all_pass"]
                       and res["A3_contrast_sweep"]["all_pass"]
                       and res["B_hst_template_physical_forward"]["all_pass"]
                       and res["C_real_frame_proxy_reference"]["all_pass"])
(RESULTS / "exp_P4CAL_02_three_calibers_guarded.json").write_text(
    json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
print(json.dumps(res, indent=2, ensure_ascii=False))
