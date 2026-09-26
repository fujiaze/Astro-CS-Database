#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q10 科学权重源 control_ivar vs uniform vs snr^2 三臂对照 + 链条贯穿用例（路线3）。

正本: docs/science/PHASE2_UPM.md §5（w_UPM = quality×control_reliability×control_ivar;
      禁 production 乘 star SNR / snr^2）;
      docs/plugins/algorithms_phase2/11_upm.md §4.4（w_ki = 1/sigma^2 ∝ SNR^2 —— 与正本
      禁令条文冲突, 见报告 Q10 的条文互斥登记）;
      缺陷清单 D-06（snr^2 的分子是局部背景电平=天光本身, 方向反）; PHASE2_UPM §16.1.3
      （实测伪影漏入: ivar 0.0245 < uniform 0.0651 < SNR^2 0.3203）。
上游接口（链条）: control_ivar = 1/(k_corr*(pi/2)*sigma_bg^2/N_retained)（Q3/Q4 公式,
      sigma_bg 由 patch MAD×1.4826 实测 —— 即 P2/P4 交给本模块的量）。
下游接口（链条）: 校正场 delta_f 施加后做子集拼叠, 接缝读数 rel_step 过 Q1 的门
      （max|rel_step| <= 1e-2, PHASE2_UPM §9a）。
负例（真值无效应⇒度量归零）: 无帧差无污染 => 三臂接缝读数均 ≈0（远低于门）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp10_weight_arms.py
"""
import json
import os
import numpy as np

SEED = 20260926
NC = 32                 # cell 网格 32x32
CELLPX = 16             # 每 cell 16x16 像素 (N=256)
F = 5
OFFS = np.array([0.0, 4.8, -3.1, 2.2, 12.0])
SIG_FR = np.array([1.0, 1.0, 1.0, 1.0, 2.0])   # 帧5: 更差噪声
K_CORR, N_RET = 1.4, CELLPX * CELLPX
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q10_weight_arms.json")


def true_sky(x, y):
    return 300.0 + 30.0 * np.sin(2.0 * np.pi * x / NC) + 15.0 * np.cos(2.0 * np.pi * y / 24.0)


def pollution(x, y):
    r2 = (x - 22.0) ** 2 + (y - 10.0) ** 2
    return 40.0 * np.exp(-r2 / (2.0 * 4.0 ** 2))


def sample_cells(rng, with_pollution, with_offsets, sigma_fr=SIG_FR):
    """逐 cell 逐帧: patch median + 上游接口量 (sigma_bg, control_ivar)。"""
    ii, jj = np.meshgrid(np.arange(NC), np.arange(NC), indexing="ij")
    B = true_sky(ii, jj)
    P = pollution(ii, jj) if with_pollution else np.zeros_like(B)
    y = np.zeros((F, NC, NC))
    sbg = np.zeros((F, NC, NC))
    for f in range(F):
        off = OFFS[f] if with_offsets else 0.0
        for a in range(NC):
            for b in range(NC):
                pix = B[a, b] + off + (P[a, b] if f == F - 1 else 0.0)                     + rng.normal(0.0, sigma_fr[f], (CELLPX, CELLPX))
                y[f, a, b] = np.median(pix)
                sbg[f, a, b] = 1.482602218505602 * np.median(np.abs(pix - np.median(pix)))
    ivar = 1.0 / (K_CORR * (np.pi / 2.0) * sbg ** 2 / N_RET)   # 上游接口量
    return y, sbg, ivar, B, P


def fit_delta_and_plane(y, w, iters=3):
    """两步迭代: delta_f = 逐帧一阶平面拟合 (y_f - B); B = Σw(y-delta)/Σw。gauge: 帧0。"""
    B = np.sum(w * y, axis=0) / np.sum(w, axis=0)
    deltas = np.zeros_like(y)
    a, b = np.meshgrid(np.arange(NC), np.arange(NC), indexing="ij")
    A = np.stack([np.ones(NC * NC), a.ravel() / NC, b.ravel() / NC], 1)
    for _ in range(iters):
        for f in range(F):
            z = (y[f] - B).ravel()
            sol, *_ = np.linalg.lstsq(A, z, rcond=None)
            deltas[f] = (A @ sol).reshape(NC, NC)
        B = np.sum(w * (y - deltas), axis=0) / np.sum(w, axis=0)
    return B, deltas


def seam_readout(y, deltas, B_true):
    """下游接口: 校正后子集拼叠, x=16 处子集切换 (左: 帧0-2, 右: 帧3-4)。
    读数 = 门口径 rel_step 的【天光梯度项扣除版】: 平滑天光沿法向的确定性项
    (2d*grad/bg, PHASE2_UPM §9a 门公式中含 72.6% 裕度的已知项) 被减去,
    余量即"帧间不连续"贡献 —— 本实验关心的量。"""
    corr = y - deltas
    mosaic = np.where((np.arange(NC) < 16)[None, :],
                      corr[:3].mean(0), corr[3:].mean(0))
    d = 2
    left = mosaic[:, 16 - d]
    right = mosaic[:, 16 + d]
    sky_term = B_true[:, 16 + d] - B_true[:, 16 - d]      # 已知确定性天光差
    seam = np.median((right - left) - sky_term)
    bg = np.median(np.abs(np.concatenate([left, right])))
    return seam / bg


def main():
    out = {"seed": SEED, "arms": {}, "negative": {}, "snr_direction_check": {}}

    # ---- 主对照: 有帧差 + 有污染 ----
    rng = np.random.default_rng(SEED)
    y, sbg, ivar, B, P = sample_cells(rng, True, True)
    ii, jj = np.meshgrid(np.arange(NC), np.arange(NC), indexing="ij")
    # D-06 的错误定义: snr = 【该帧自己的】局部背景电平 / sigma_bg => w = snr^2
    level_f = np.stack([B + OFFS[f] + (P if f == F - 1 else 0.0) for f in range(F)])
    arms = {
        "ivar": np.broadcast_to(ivar, (F, NC, NC)).copy(),
        "uniform": np.ones((F, NC, NC)),
        "snr2": (level_f / sbg) ** 2,
    }
    truth_err = {}
    for name, w in arms.items():
        Bhat, deltas = fit_delta_and_plane(y, w)
        truth_err[name] = {
            "mean_abs_err_B": float(np.mean(np.abs(Bhat - B))),
            "pollution_leak_bias": float(np.mean((Bhat - B)[P > 5.0])),
            "max_offset_err": float(np.max(np.abs(
                [np.mean(deltas[f]) - OFFS[f] for f in range(F)]))),
            "seam_rel_step": float(seam_readout(y, deltas, B)),
        }
        print(name, truth_err[name])
    out["arms"] = truth_err
    out["ordering_matches_doc_16.1.3"] = (
        truth_err["ivar"]["pollution_leak_bias"] < truth_err["uniform"]["pollution_leak_bias"]
        < truth_err["snr2"]["pollution_leak_bias"])

    # ---- 负例: 无帧差无污染 => 三臂接缝归零 ----
    rng2 = np.random.default_rng(SEED + 1)
    y0, sbg0, ivar0, B0, P0 = sample_cells(rng2, False, False)
    bg0 = np.broadcast_to(B0, (F, NC, NC)).copy()
    neg = {}
    for name, w in {"ivar": np.broadcast_to(ivar0, (F, NC, NC)).copy(),
                    "uniform": np.ones((F, NC, NC)),
                    "snr2": (bg0 / sbg0) ** 2}.items():
        Bh, dd = fit_delta_and_plane(y0, w)
        neg[name] = {"seam_rel_step": float(seam_readout(y0, dd, B0)),
                     "mean_abs_err_B": float(np.mean(np.abs(Bh - B0)))}
    out["negative"] = neg
    out["negative_ok"] = all(abs(v["seam_rel_step"]) < 1e-3 for v in neg.values())

    # ---- D-06 方向命题: 光子噪声域 sigma=sqrt(sky) => snr2 随天光增, ivar 随天光减 ----
    sky = np.array([100.0, 300.0, 900.0, 2700.0])
    sig = np.sqrt(sky)
    snr2 = (sky / sig) ** 2
    out["snr_direction_check"] = {
        "sky": sky.tolist(), "sigma": sig.tolist(),
        "snr2_proportional": snr2.tolist(),
        "ivar_proportional": (1.0 / sig ** 2).tolist(),
        "snr2_increases_with_sky": bool(np.all(np.diff(snr2) > 0)),
        "ivar_decreases_with_sky": bool(np.all(np.diff(1.0 / sig ** 2) < 0)),
        "claim": "D-06 证实: snr^2(以电平为分子)随天光增大而增大, 与真实统计权重(ivar)方向相反"}

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps({"ordering_ok": out["ordering_matches_doc_16.1.3"],
                      "negative_ok": out["negative_ok"],
                      "direction": out["snr_direction_check"]}, ensure_ascii=False, indent=1))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()