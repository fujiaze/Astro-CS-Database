#!/usr/bin/env python3
"""C2 -- k_corr (Drizzle 相关校正) 三腿之实验腿: 纯合成物理前向 MC. v2

验证对象: docs/science/PHASE2_UPM.md §4/§5
  control_variance = k_corr * (pi/2) * sigma_bg^2 / N_retained, 定义域 1 < k_corr,
  冻结默认 k_corr = 1.4 (项目自产 MC, pixfrac=0.8, 实证 1.3883).
物理依据: drizzle 重采样使输出像素间共享源像素 => 正相关 => N_eff = N/k_corr <= N.

v2 修正 (v1 的两条结构性预测在单帧等尺度域方向标反, 此处按 drizzle 几何重写):
  - 相关性来源 = 多个输出像素共享同一源像素的贡献. 共享程度由
    (输出/源像素尺度比 s, pixfrac, 亚像素偏移) 共同决定:
    * s = 1 (输出与源同尺度): pixfrac 增大 => 单个源像素的缩小方块覆盖更多
      输出像素 => 共享更强 => k_corr 更大;
    * s = 0.5 (输出更细, HST 型): pixfrac 减小 => 输出像素块变小但落进同一
      源像素的相邻输出块增多 => 共享更强 => k_corr 更大 (文献方向).
  - 负例(null): s = 1, pixfrac = 1, 整像素对齐 (偏移 0.5) => 输出/源一一对应,
    无共享 => k_corr ≈ 1 (真值无相关 => 校正归一).
  - 强制 k_corr = 1 的低估幅度: 主案直接量化.

方法: 单帧面积重叠 drizzle 最小前向 (线性、通量守恒), iid N(0,1) 源噪声,
  输出网格 KxK patch median 的 2000 次 MC. 固定 seed, < 5 min.
"""
import json
import math
import os
import numpy as np

SEED = 20260318
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c2_kcorr_drizzle.json")
SRC = 64
N_MC = 2000


def drizzle_matrix(x0, y0, pixfrac, out_scale, n_out):
    """返回 (W, n_src): W[j, i] = 输出像素 j 从源像素 i 收到的归一化权重.
    源像素 i (单位方格, 覆盖 [i, i+1)) 经 pixfrac 缩小后中心平移 (x0, y0);
    输出像素 j 覆盖 [j*out_scale, (j+1)*out_scale), n_out = 输出每边像素数."""
    half = pixfrac / 2.0
    n_src = int(round(n_out * out_scale))
    n_src = max(n_src, int(n_out * out_scale) + 1)  # 覆盖足够源域
    W = np.zeros((n_out * n_out, n_src * n_src))
    for jy in range(n_out):
        oy0, oy1 = jy * out_scale, (jy + 1) * out_scale
        for jx in range(n_out):
            j = jy * n_out + jx
            ox0, ox1 = jx * out_scale, (jx + 1) * out_scale
            iy_lo = max(0, int(math.floor(y0 + oy0 - half)) - 1)
            iy_hi = min(n_src, int(math.ceil(y0 + oy1 + half)) + 1)
            ix_lo = max(0, int(math.floor(x0 + ox0 - half)) - 1)
            ix_hi = min(n_src, int(math.ceil(x0 + ox1 + half)) + 1)
            for iy in range(iy_lo, iy_hi):
                sy0, sy1 = iy + y0 - half, iy + y0 + half
                wy = min(sy1, oy1) - max(sy0, oy0)
                if wy <= 0:
                    continue
                for ix in range(ix_lo, ix_hi):
                    sx0, sx1 = ix + x0 - half, ix + x0 + half
                    wx = min(sx1, ox1) - max(sx0, ox0)
                    if wx > 0:
                        W[j, iy * n_src + ix] = wx * wy
    Wn = W / W.sum(axis=1, keepdims=True)
    return Wn, n_src


def mc_kcorr(x0, y0, pixfrac, out_scale, n_out, K, n_mc, rng):
    """对中央 KxK 窗口内有覆盖 (行和 > 0) 的输出像素测 patch median 方差.
    零权重输出像素 = drizzle 权重图里的零覆盖, 真实流程会剔除, 不进 median."""
    Wn, n_src = drizzle_matrix(x0, y0, pixfrac, out_scale, n_out)
    wsum = Wn.sum(axis=1)
    var_out = (Wn ** 2).sum(axis=1)
    sigma_out2 = float(np.median(var_out[wsum > 0]))
    c0 = n_out // 2 - K // 2
    idx = [(jy * n_out + jx) for jy in range(c0, c0 + K) for jx in range(c0, c0 + K)
           if wsum[jy * n_out + jx] > 0]
    n_used = len(idx)
    if n_used < 2:
        raise RuntimeError("patch 无覆盖")
    Wp = Wn[np.array(idx)]
    meds = np.empty(n_mc)
    for t in range(n_mc):
        x = rng.standard_normal(n_src * n_src)
        meds[t] = np.median(Wp @ x)
    v_med = float(np.var(meds))
    asy = (math.pi / 2.0) * sigma_out2 / n_used
    return {"k_corr": v_med / asy, "var_median": v_med, "sigma_out2": sigma_out2,
            "n_used": n_used, "coverage_fraction": n_used / (K * K)}


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "n_mc": N_MC, "src_px": SRC,
           "model": "single-frame area-overlap drizzle, flux-conserving, iid N(0,1) source noise"}
    cases = {}
    # 主案: s=1, pixfrac=0.8, 亚像素偏移, K=17 (N=289 = 生产 N_retained 上限)
    cases["s1.0_pf0.8_K17"] = mc_kcorr(0.37, 0.21, 0.8, 1.0, 48, 17, N_MC, rng)
    # HST 型: 输出比源细一倍 (s=0.5)
    cases["s0.5_pf0.8_K17"] = mc_kcorr(0.37, 0.21, 0.8, 0.5, 48, 17, 1000, rng)
    # pixfrac 方向: s=1 域 (pf 0.4/1.0) 与 s=0.5 域 (pf 0.4)
    cases["s1.0_pf0.4_K17"] = mc_kcorr(0.37, 0.21, 0.4, 1.0, 48, 17, 1000, rng)
    cases["s1.0_pf1.0_K17"] = mc_kcorr(0.37, 0.21, 1.0, 1.0, 48, 17, 1000, rng)
    cases["s0.5_pf0.4_K17"] = mc_kcorr(0.37, 0.21, 0.4, 0.5, 48, 17, 1000, rng)
    # 负例: 一一对应无共享 => k_corr ≈ 1 (MC 加密到 4000 收紧方差)
    cases["s1.0_pf1.0_aligned_K17"] = mc_kcorr(0.5, 0.5, 1.0, 1.0, 48, 17, 4000, rng)
    # N 依赖 (主案几何, K=5/9)
    cases["s1.0_pf0.8_K9"] = mc_kcorr(0.37, 0.21, 0.8, 1.0, 48, 9, 1000, rng)
    cases["s1.0_pf0.8_K5"] = mc_kcorr(0.37, 0.21, 0.8, 1.0, 48, 5, 1000, rng)
    res["cases"] = cases

    main_case = cases["s1.0_pf0.8_K17"]
    var_k1 = main_case["sigma_out2"] * math.pi / 2.0 / 289.0
    res["ignore_correlation_underestimate"] = {
        "var_true": main_case["var_median"],
        "var_kcorr1_prediction": var_k1,
        "fraction_underestimated": 1.0 - var_k1 / main_case["var_median"],
    }
    res["structural_checks"] = {
        "main_case_k_corr_gt_1": bool(main_case["k_corr"] > 1.0),
        "null_case_near_1": bool(abs(cases["s1.0_pf1.0_aligned_K17"]["k_corr"] - 1.0) < 0.03),
        "s1_monotone_in_pixfrac": bool(cases["s1.0_pf0.4_K17"]["k_corr"]
                                       < cases["s1.0_pf0.8_K17"]["k_corr"]
                                       < cases["s1.0_pf1.0_K17"]["k_corr"]),
        "s0p5_finer_pf0.4_ge_s1_pf0.4": bool(cases["s0.5_pf0.4_K17"]["k_corr"]
                                             > cases["s1.0_pf0.4_K17"]["k_corr"]),
        "frozen_default_1p4_same_order": bool(0.5 < main_case["k_corr"] < 3.0),
    }
    res["relation_to_project_mc"] = {
        "project_mc_value": 1.3883,
        "project_frozen_default": 1.4,
        "this_fixture_s1_pf0.8": main_case["k_corr"],
        "claim": "同 pixfrac=0.8、同尺度域内, 本独立 fixture 得到同量级 k_corr; "
                 "量值依赖 fixture 几何, 不构成对 1.3883 的复现声明, 只构成量级与方向佐证",
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
