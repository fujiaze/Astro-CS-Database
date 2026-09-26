#!/usr/bin/env python3
"""P2 跨帧绝对 SNR（控制点方差语义）补实验：control_variance 渐近式的有限 N 偏差。

被审量：docs/science/PHASE2_UPM.md §5 / docs/algorithms/PHASE2_SAMPLER.md §5.4 的

    control_variance = k_corr x (pi/2) x sigma_bg^2 / N_retained

的 (pi/2) 渐近系数在有限 N 下的方向与幅值。文档原警告（PHASE2_UPM.md:79、
PHASE2_SAMPLER.md:267-274）："N=5 时渐近式低估 8.5%，属保守方向"。

本实验两条独立路径：
  A. 解析路径（无 MC）：对奇数 N 用样本中位数（(N+1)/2 阶序统计量）密度的
     Gauss-Legendre 数值积分，给出精确有限 N 常数 kappa_exact(N) = N*Var(median)/sigma^2。
  B. MC 路径（固定 seed）：三臂
     B1 纯公式口径（sigma 已知）：ratio_A = 实测 Var(median) / (pi/2 sigma^2 / N)；
     B2 端到端口径（生产 plug-in）：sigma_bg = 1.482602218505602 x MAD(同 patch)，
        ratio_B = E[(pi/2) sigma_bg^2 / N] / Var(median)；
     B3 结构臂：patch 样本 = 线性梯度 + 噪声（公式前提"同一正态的重复测量"被破坏）。

固定 seed：SEED = 20260601（numpy default_rng，单流顺序消耗）。
纯 Python + numpy + scipy.special.erf；不 import 仓库任何代码。
运行：python3 finiteN_control_variance.py
输出：../results/finiteN_control_variance.json（同目录打印摘要表）。
"""
import json
import math
import os
import sys

import numpy as np
from scipy.special import erf

SEED = 20260601
K_PI_HALF = math.pi / 2.0                      # kPiHalf（sampler.cpp:84 = 1.57079632679489661923）
K_MAD = 1.482602218505602                      # robust_sigma 因子（sampler 域冻结常数）
K_KCORR = 1.0                                  # 本实验隔离 (pi/2) leg：k_corr 置 1

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "finiteN_control_variance.json")

N_GRID_ODD = [5, 7, 9, 11, 13, 15, 17, 21, 25, 33, 41, 49, 65, 81, 97, 129, 161, 201]
N_GRID_EVEN = [20, 40, 100]                    # 偶数 N（中位数 = 两个中央序统计量均值），仅 MC
N_ANCHORS = [17, 65, 289]                      # 文档锚点（PHASE2_SAMPLER.md:267-268），289 为补锚
DOC_ANCHOR_RATIOS = {5: 0.9149, 17: 0.9722, 65: 0.9938, 289: 1.0030}
P5_E1_KAPPA = {5: 1.4373320348838472, 20: 1.4671338102988962, 65: 1.5687345332742153,
               289: 1.580842284191836}         # 独立审计/实验重做/P5加性天光去除/路线2 E1（seed 20250926）

N_BLOCKS = 20                                  # 分块 jackknife 估计 MC 标准误


def r_total(n: int) -> int:
    if n <= 21:
        return 4_000_000
    if n <= 65:
        return 2_000_000
    if n <= 129:
        return 1_000_000
    return 400_000


def chunk_rows(n: int) -> int:
    return int(min(400_000, max(10_000, 4.0e7 / n)))


# ---------------------------------------------------------------- 解析路径 A
def kappa_exact_odd(n: int) -> float:
    """奇数 N=2k+1：样本中位数 = X_(k+1)，密度
    f(x) = n!/(k!k!) Phi(x)^k (1-Phi(x))^k phi(x)。
    返回 kappa = n * Var(X_(k+1))（标准正态 sigma=1）。"""
    k = (n - 1) // 2
    nodes, weights = np.polynomial.legendre.leggauss(600)
    x = 12.0 * nodes                            # [-12, 12] 覆盖到 1e-32 概率尾
    w = 12.0 * weights
    c = math.factorial(n) / (math.factorial(k) ** 2)

    def Phi(t):
        return 0.5 * (1.0 + erf(t / math.sqrt(2.0)))

    phi = np.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)
    dens = c * Phi(x) ** k * (1.0 - Phi(x)) ** k * phi
    ex = float(np.sum(x * dens * w))
    ex2 = float(np.sum(x * x * dens * w))
    return n * (ex2 - ex * ex)


# ---------------------------------------------------------------- MC 路径 B
def mc_arm(n: int, rng: np.random.Generator, slope: float = 0.0):
    """返回 dict：Var(median)、E[sigma_bg^2]、分块标准误、结构参数。
    slope=0 为 iid 高斯；slope>0 时样本 = slope*(i-(n-1)/2) + 噪声（sigma=1）。"""
    R = r_total(n)
    ch = chunk_rows(n)
    nb = N_BLOCKS
    per = R // nb
    sums = {"med": 0.0, "med2": 0.0, "mad2": 0.0, "cnt": 0}
    block_med_var = []
    block_mad2 = []
    for b in range(nb):
        rows = per if b < nb - 1 else R - per * (nb - 1)
        meds = np.empty(rows)
        mads = np.empty(rows)
        done = 0
        while done < rows:
            m = min(ch, rows - done)
            x = rng.standard_normal((m, n))
            if slope != 0.0:
                x = x + slope * (np.arange(n) - (n - 1) / 2.0)
            med = np.median(x, axis=1)
            mad = np.median(np.abs(x - med[:, None]), axis=1) * K_MAD
            meds[done:done + m] = med
            mads[done:done + m] = mad
            done += m
        sums["med"] += meds.sum(); sums["med2"] += (meds ** 2).sum()
        sums["mad2"] += (mads ** 2).sum(); sums["cnt"] += rows
        block_med_var.append(meds.var(ddof=1))
        block_mad2.append((mads ** 2).mean())
    var_med = sums["med2"] / sums["cnt"] - (sums["med"] / sums["cnt"]) ** 2
    c_mad2 = sums["mad2"] / sums["cnt"]
    se_var = float(np.std(block_med_var, ddof=1) / math.sqrt(nb))
    se_mad2 = float(np.std(block_mad2, ddof=1) / math.sqrt(nb))
    return {"var_med": float(var_med), "se_var": se_var,
            "c_mad2": float(c_mad2), "se_mad2": se_mad2, "R": R,
            "block_med_var": block_med_var, "block_mad2": block_mad2}


def main():
    rng = np.random.default_rng(SEED)
    res = {
        "seed": SEED, "k_pi_half": K_PI_HALF, "k_mad": K_MAD, "k_kcorr": K_KCORR,
        "numpy": np.__version__, "python": sys.version.split()[0],
        "conventions": {
            "kappa(N)": "N * Var(median) / sigma^2；渐近极限 = pi/2 = 1.5707963",
            "ratio_A(pure)": "实测 Var(median) / ((pi/2) sigma^2 / N) = kappa / (pi/2)",
            "pure_over_true": "(pi/2) sigma^2/N / 实测 Var(median)（>1 = 公式高估方差）",
            "ratio_B(end-to-end)": "E[(pi/2) (1.4826*MAD)^2 / N] / 实测 Var(median)",
            "c_mad2": "E[(1.4826*MAD)^2] / sigma^2（MAD plug-in 尺度估计量的平方偏置）",
        },
        "exact": [], "mc_iid": [], "structure": [], "negative": {}, "cross_check": {},
    }

    # ---- A 解析：奇数 N
    exact_ns = sorted(set(N_GRID_ODD + N_ANCHORS))
    for n in exact_ns:
        kap = kappa_exact_odd(n)
        res["exact"].append({"N": n, "kappa_exact": kap,
                             "ratio_A": kap / K_PI_HALF,
                             "pure_over_true": K_PI_HALF / kap})
    exact_map = {r["N"]: r for r in res["exact"]}

    # ---- B MC：iid
    for n in sorted(set(N_GRID_ODD + N_GRID_EVEN + [5] + N_ANCHORS)):
        a = mc_arm(n, rng)
        kap = a["var_med"] * n
        row = {"N": n, "R": a["R"], "var_med": a["var_med"], "se_var": a["se_var"],
               "kappa_mc": kap, "kappa_se": a["se_var"] * n,
               "ratio_A": kap / K_PI_HALF, "ratio_A_se": a["se_var"] * n / K_PI_HALF,
               "pure_over_true": K_PI_HALF / kap,
               "c_mad2": a["c_mad2"], "c_mad2_se": a["se_mad2"],
               "ratio_B": (K_PI_HALF * a["c_mad2"]) / kap,
               "ratio_B_se": (K_PI_HALF / kap) * a["se_mad2"],
               "factorization_gap": (K_PI_HALF * a["c_mad2"]) / kap - (K_PI_HALF / kap) * a["c_mad2"]}
        res["mc_iid"].append(row)

    # ---- 交叉核对：解析 vs MC；本文 vs 文档锚点 vs P5 E1
    mc_map = {r["N"]: r for r in res["mc_iid"]}
    cc = []
    for n in sorted(set(list(exact_map.keys()) + [20])):
        e = exact_map.get(n)
        m = mc_map.get(n)
        if e and m:
            cc.append({"N": n, "kappa_exact": e["kappa_exact"], "kappa_mc": m["kappa_mc"],
                       "z": (e["kappa_exact"] - m["kappa_mc"]) / m["kappa_se"]})
        elif m:
            cc.append({"N": n, "kappa_exact": None, "kappa_mc": m["kappa_mc"], "z": None})
    res["cross_check"]["exact_vs_mc"] = cc
    res["cross_check"]["doc_anchor_ratioA"] = [
        {"N": n, "mc_ratio_A": mc_map[n]["ratio_A"], "doc_claim": DOC_ANCHOR_RATIOS[n],
         "exact_ratio_A": exact_map[n]["ratio_A"] if n in exact_map else None}
        for n in sorted(DOC_ANCHOR_RATIOS) if n in mc_map]
    res["cross_check"]["p5_e1_kappa"] = [
        {"N": n, "p5_e1": P5_E1_KAPPA[n], "this_mc": mc_map[n]["kappa_mc"],
         "rel_diff": (mc_map[n]["kappa_mc"] - P5_E1_KAPPA[n]) / P5_E1_KAPPA[n]}
        for n in sorted(P5_E1_KAPPA) if n in mc_map]

    # ---- B3 结构臂（前提破坏：像素集含线性结构信号）
    for n in (5, 17):
        for b in (0.0, 0.05, 0.1, 0.3, 1.0):
            a = mc_arm(n, rng, slope=b)
            kap = a["var_med"] * n
            res["structure"].append({
                "N": n, "slope_per_sample": b, "R": a["R"],
                "var_med": a["var_med"], "kappa_struct": kap,
                "c_mad2": a["c_mad2"],
                "ratio_B": (K_PI_HALF * a["c_mad2"]) / kap,
                "sigma_bg_over_sigma": math.sqrt(a["c_mad2"])})

    # ---- 负例（真值无效应 => 归零）
    x0 = np.zeros((1000, 65))
    med0 = np.median(x0, axis=1)
    res["negative"]["sigma0_var_med"] = float(med0.var())
    res["negative"]["sigma0_published_cvar_mean"] = 0.0  # sigma_bg=0 => cvar=(pi/2)*0/N=0

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)

    # ---- 摘要打印
    print("== 解析路径（奇数 N，精确）==")
    print(" N    kappa_exact  ratio_A(=实测/公式)  pure_over_true(公式/真值)")
    for r in res["exact"]:
        print(f" {r['N']:4d}  {r['kappa_exact']:.6f}   {r['ratio_A']:.6f}   {r['pure_over_true']:+.6f}")
    print()
    print("== MC 路径（iid 高斯，sigma=1）==")
    print(" N    R        kappa_mc     ratio_A       pure_over_true  c_mad2     ratio_B(end-to-end)")
    for r in res["mc_iid"]:
        print(f" {r['N']:4d}  {r['R']:7d}  {r['kappa_mc']:.6f}  {r['ratio_A']:.6f}"
              f"  {r['pure_over_true']:+.6f}  {r['c_mad2']:.6f}  {r['ratio_B']:+.6f}")
    print()
    print("== 结构臂 ==")
    for r in res["structure"]:
        print(f" N={r['N']:3d} b={r['slope_per_sample']:.2f}  sigma_bg/sigma={r['sigma_bg_over_sigma']:.4f}"
              f"  kappa_struct={r['kappa_struct']:.4f}  ratio_B={r['ratio_B']:+.4f}")
    print()
    print("== 交叉核对（解析 vs MC）==")
    for r in res["cross_check"]["exact_vs_mc"]:
        z = "n/a" if r["z"] is None else f"{r['z']:+.2f}"
        ke = "n/a" if r["kappa_exact"] is None else f"{r['kappa_exact']:.6f}"
        print(f" N={r['N']:4d}  exact={ke}  mc={r['kappa_mc']:.6f}  z={z}")
    print()
    print("== 与文档锚点 / P5 E1 对比 ==")
    for r in res["cross_check"]["doc_anchor_ratioA"]:
        print(f" doc anchor N={r['N']:3d}: mc={r['mc_ratio_A']:.6f} doc={r['doc_claim']}"
              f" exact={r['exact_ratio_A']}")
    for r in res["cross_check"]["p5_e1_kappa"]:
        print(f" P5 E1   N={r['N']:3d}: p5={r['p5_e1']:.6f} this={r['this_mc']:.6f}"
              f" rel={r['rel_diff']:+.4%}")
    print()
    print("negative:", res["negative"])
    print("written:", OUT)


if __name__ == "__main__":
    main()
