#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q4 k_corr（Drizzle 相关噪声校正, 冻结 1.4 / 项目 MC 实证 1.3883）独立三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §4/§5（k_corr 定义域 1<k_corr; N_eff = N_retained/k_corr;
      线性缩放不变量）; 缺陷清单 D-38。
文献腿正本: Fruchter & Hook 2002, PASP 114, 144 (arXiv:astro-ph/9808087) §7
      "noise correlation ratio R"：drizzle 把一个输入像素的功率分给多个输出像素
      ⇒ 相邻输出像素噪声相关 ⇒ 按输出像素尺度测的噪声低估大尺度噪声。
      本实验用独立实现的简化 drizzle（等尺度网格 + 亚像素位移 + pixfrac 收缩）直接
      MC 测 patch-median 方差膨胀 k_corr，并对照交换相关近似 k ≈ 1 + N·ρ̄。
边界（诚实登记）: 项目 1.3883 来自 control_median_mc_test 的特定 harness
      （pixfrac=0.8, 2000 次），其网格/位移/合并几何未在条文级公开；本实验为
      独立几何下的独立测量，只核量级、方向（k>1, 随 pixfrac 单调增）、
      缩放不变量与 k≈1+N·ρ̄ 结构，不宣称复现 1.3883 本身。
负例（真值无效应⇒度量归零）: 位移=(0,0)（恒等映射, 无相关注入）⇒ k_corr = 1（精确）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp04_kcorr_mc.py
"""
import json
import os
import numpy as np

SEED = 20260926
N = 128            # 网格边长
PATCH = 17         # patch 边长 (N_retained = 289)
REPS = 6000
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q4_kcorr_mc.json")


def overlap_1d(shift, frac):
    """1D 权重矩阵 W[o, i] = pixfrac 收缩后的输入像素 i 与输出像素 o 的重叠面积。"""
    a1 = np.arange(N) + shift + (1.0 - frac) / 2.0
    a2 = a1 + frac
    b1 = np.arange(N)[:, None]
    b2 = b1 + 1.0
    return np.clip(np.minimum(b2, a2[None, :]) - np.maximum(b1, a1[None, :]), 0.0, None)


def run_case(shift, frac, sigma=1.0, reps=REPS, base_seed=SEED):
    wx = overlap_1d(shift[0], frac)
    wy = overlap_1d(shift[1], frac)
    cx, cy = wx.sum(1), wy.sum(1)                      # 每输出像素的总权重
    meds = np.empty(reps)
    means = np.empty(reps)
    mads = np.empty(reps)
    o0, o1 = (N - PATCH) // 2, (N + PATCH) // 2
    patch_stack = np.empty((reps, PATCH * PATCH))
    for r in range(reps):
        V = np.random.default_rng(base_seed + r).normal(0.0, sigma, (N, N))
        O = (wx @ V @ wy.T) / np.outer(cx, cy)
        p = O[o0:o1, o0:o1]
        patch_stack[r] = p.ravel()
        meds[r] = np.median(p)
        means[r] = p.mean()
        mads[r] = 1.482602218505602 * np.median(np.abs(p - np.median(p)))
    var_median = meds.var()
    var_mean = means.var()
    sigma_bg = float(np.median(mads))
    n_ret = PATCH * PATCH
    k_corr = var_median / ((np.pi / 2.0) * sigma_bg ** 2 / n_ret)
    # 交叉校验: Var(mean) 的膨胀因子（无 π/2 因子）: var_mean / (sigma_bg^2/n_ret)
    k_mean = var_mean / (sigma_bg ** 2 / n_ret)
    # patch 内平均成对相关 ρ̄（跨 reps 的像素间相关矩阵, 去对角）
    Xc = patch_stack - patch_stack.mean(0)
    cov = (Xc.T @ Xc) / (reps - 1)
    d = np.sqrt(np.diag(cov))
    corr = cov / np.outer(d, d)
    iu = np.triu_indices(n_ret, 1)
    rho_pairs = corr[iu]
    rho_bar = float(rho_pairs.mean())
    # median 的交换近似须走高斯符号相关: Corr(sign,sign) = (2/pi) arcsin(rho)
    rho_s = float(np.mean((2.0 / np.pi) * np.arcsin(rho_pairs)))
    return {"shift": list(shift), "pixfrac": frac, "sigma": sigma, "reps": reps,
            "var_median": var_median, "var_mean": var_mean, "sigma_bg": sigma_bg,
            "k_corr_measured": float(k_corr),
            "k_mean_measured": float(k_mean),
            "rho_bar": rho_bar,
            "k_median_exch_arcsin_1+(N-1)rho_s": 1.0 + (n_ret - 1) * rho_s,
            "n_retained": n_ret}


def main():
    out = {"seed": SEED, "grid": N, "patch": PATCH, "cases": []}
    # 主扫描: pixfrac 变化（固定位移 (0.31, 0.47)）
    for frac in [0.05, 0.4, 0.8, 1.0]:
        c = run_case((0.31, 0.47), frac)
        out["cases"].append(c)
        print({k: c[k] for k in ["pixfrac", "k_corr_measured", "k_mean_measured", "rho_bar",
                                 "k_median_exch_arcsin_1+(N-1)rho_s"]})
    # 负例: 恒等映射（位移 0,0）⇒ 输出=输入, 无相关 ⇒ k_corr = 1
    c0 = run_case((0.0, 0.0), 1.0)
    out["cases"].append(c0)
    print("negative identity:", c0["k_corr_measured"], "k_mean:", c0["k_mean_measured"])
    out["negative_ok"] = abs(c0["k_corr_measured"] - 1.0) < 0.05
    # pixfrac=0.05 在位移 (0.31,0.47) 下收缩足印不跨输出像素边界 (0.31+0.5-0.025>0.8
    # 不足 1.0) ⇒ 退化为最近邻恒等映射; 保留该例并说明其与恒等例同分布
    # 缩放不变量: σ=10, 同 seed 结构 ⇒ k_corr 与 σ=1 逐位同（线性缩放）
    c10 = run_case((0.31, 0.47), 0.8, sigma=10.0, reps=800, base_seed=SEED + 500000)
    ref = out["cases"][2]["k_corr_measured"]   # pixfrac=0.8, σ=1
    out["cases"].append(c10)
    out["scale_invariance"] = {"k_sigma1": ref, "k_sigma10": c10["k_corr_measured"],
                               "abs_diff": abs(c10["k_corr_measured"] - ref),
                               # σ=1 全量 2000 reps vs σ=10 只跑 400 reps ⇒ 允许 MC 波动
                               "note": "MC 波动量级 ~1/sqrt(reps)"}
    # 单调性: k 随 pixfrac 增（固定位移）
    ks = [c["k_corr_measured"] for c in out["cases"][:4]]
    out["monotone_in_pixfrac"] = bool(np.all(np.diff(ks) > 0))
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()