#!/usr/bin/env python3
"""C1 -- control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained 三腿之实验腿.

验证对象: docs/science/PHASE2_UPM.md §5 冻结公式
  control_variance = k_corr * (pi/2) * sigma_bg^2 / N_retained
其理论内核 = Var(median) = 1/(4 N f(m)^2) 的高斯特例 pi*sigma^2/(2N),
  声称: (a) iid + 近高斯 + N>=65 时渐近式成立;
        (b) N=5 时渐近式低估真实方差约 8.5%;
        (c) 非高斯域失效: 均匀约 1.91x、拉普拉斯约 0.335x;
        (d) 退化 patch (>=半数像素同值 => 稳健尺度 0) 无尺度信息,
            正确口径 control_ivar=0; 数值地板 1e-12 平方后发布
            cvar=1.4*(pi/2)*(1e-12)^2/289 = 7.609e-27,
            civar = 1.314e26 (01_缺陷清单 D-01 的算术) —— 本脚本独立复算.

负例(真值无效应 => 度量归零): patch 稳健尺度真值为 0 时,
  正确口径发布的方差信息量必须为 0 (ivar=0), 而地板路径给出 1e26 量级伪 ivar.

方法: 纯 numpy Monte-Carlo, 固定 seed, 单进程 < 1 min.
"""
import json
import math
import os
import numpy as np

SEED = 20260317
rng = np.random.default_rng(SEED)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c1_median_variance.json")

M_TRIALS = 40000          # MC 重复次数 (估计 Var(median) 的相对精度 ~ 1/sqrt(M) * CV)
N_LIST = [5, 9, 17, 33, 65, 129, 257]
SQRT_PI_2 = math.sqrt(math.pi / 2.0)


def mc_var_median(dist, n, m=M_TRIALS):
    """样本中位数方差的 MC 估计. dist: 无参函数, 返回 shape=(m,n) 的 iid 样本 (sigma=1 标度)."""
    x = dist(m, n)
    med = np.median(x, axis=1)
    return float(np.var(med))


def gaussian(m, n):
    return rng.standard_normal((m, n))


def uniform_sigma1(m, n):
    # U(-a,a), a = sqrt(3) => sigma = 1
    a = math.sqrt(3.0)
    return rng.uniform(-a, a, (m, n))


def laplace_sigma1(m, n):
    # Laplace(0,b), b = 1/sqrt(2) => sigma^2 = 2 b^2 = 1
    b = 1.0 / math.sqrt(2.0)
    u = rng.uniform(-0.5, 0.5, (m, n))
    return -b * np.sign(u) * np.log(1.0 - 2.0 * np.abs(u))


def main():
    res = {"seed": SEED, "m_trials": M_TRIALS, "n_list": N_LIST}

    # ---- (0) N=5 的真值: 阶统计量密度精确积分 (无 MC 误差) ----
    # median of 5 iid N(0,1): f_med(x) = 5!/(2!2!) * Phi(x)^2 * (1-Phi(x))^2 * phi(x)
    from math import erf, sqrt, pi, exp
    def phi(x): return exp(-0.5 * x * x) / sqrt(2.0 * pi)
    def Phi(x): return 0.5 * (1.0 + erf(x / sqrt(2.0)))
    xs = np.linspace(-8.0, 8.0, 200001)
    phiv = np.vectorize(phi); Phiv = np.vectorize(Phi)
    pdf = 30.0 * Phiv(xs) ** 2 * (1.0 - Phiv(xs)) ** 2 * phiv(xs)
    cdf = np.concatenate([[0.0], np.cumsum((pdf[1:] + pdf[:-1]) * 0.5 * np.diff(xs))])
    ex = np.concatenate([[0.0], np.cumsum((xs[1:] * pdf[1:] + xs[:-1] * pdf[:-1]) * 0.5 * np.diff(xs))])
    ex2 = np.concatenate([[0.0], np.cumsum((xs[1:] ** 2 * pdf[1:] + xs[:-1] ** 2 * pdf[:-1]) * 0.5 * np.diff(xs))])
    var_exact5 = ex2[-1] - ex[-1] ** 2
    res["gaussian_N5_exact"] = {
        "var_exact_quadrature": float(var_exact5),
        "asymptotic": math.pi / 10.0,
        "asymptotic_over_true": float((math.pi / 10.0) / var_exact5),
        "true_over_asymptotic": float(var_exact5 / (math.pi / 10.0)),
        "verdict": "渐近式在 N=5 高估 Var(median) ~9.5% (方差高估 => ivar/权重低估 => 保守方向); "
                   "科学文档'渐近式低估 8.5%'的'低估'一词方向有误",
    }

    # ---- (1) 高斯: ratio = Var_MC(median) / (pi sigma^2 / (2N)) ----
    gauss = {}
    for n in N_LIST:
        v = mc_var_median(gaussian, n)
        asy = math.pi / (2.0 * n)
        gauss[str(n)] = {"var_mc": v, "asymptotic": asy, "ratio": v / asy,
                         "rel_err_mc": 1.0 / math.sqrt(M_TRIALS)}  # 量级参考
    res["gaussian"] = gauss
    # 渐近式相对 MC 真值的高估幅度 = asymptotic/var_mc - 1
    res["gaussian_N5_overestimate"] = gauss["5"]["asymptotic"] / gauss["5"]["var_mc"] - 1.0
    res["gaussian_N65_ratio"] = gauss["65"]["ratio"]

    # ---- (2) 非高斯: 大 N (N=257) 的 ratio 对闭式预测 ----
    # 均匀: Var(median) = 3 sigma^2/N   => ratio = 6/pi  ~= 1.9099
    # 拉普拉斯: Var(median) = sigma^2/(2N) => ratio = 1/pi ~= 0.3183
    ng = {}
    for name, dist, pred in (("uniform", uniform_sigma1, 6.0 / math.pi),
                             ("laplace", laplace_sigma1, 1.0 / math.pi)):
        v = mc_var_median(dist, 257, m=M_TRIALS)
        asy = math.pi / (2.0 * 257)
        ng[name] = {"var_mc_N257": v, "ratio_mc": v / asy, "ratio_closed_form": pred}
    # 文档引用的实测档 N=5 (ALG-P2-SMP-001 §5.4 的口径): 补 N=5 的 ratio
    for name, dist in (("uniform", uniform_sigma1), ("laplace", laplace_sigma1)):
        v = mc_var_median(dist, 5, m=M_TRIALS)
        asy = math.pi / 10.0
        ng[name]["ratio_mc_N5"] = v / asy
    res["non_gaussian"] = ng

    # ---- (3) 退化 patch: 地板平方发布 vs 正确口径 ----
    # D-01 闭式复算 (独立): cvar = 1.4*(pi/2)*(1e-12)^2 / 289
    cvar_floor = 1.4 * (math.pi / 2.0) * (1e-12 ** 2) / 289.0
    civar_floor = 1.0 / cvar_floor
    res["degenerate_floor_publish"] = {
        "cvar_closed_form": cvar_floor,
        "cvar_doc_value": 7.609e-27,
        "civar_closed_form": civar_floor,
        "civar_doc_value": 1.314e26,
        "abs_rel_err_vs_doc": abs(cvar_floor - 7.609e-27) / 7.609e-27,
    }
    # 负例: 真实尺度信息 = 0 (半数像素同值, 稳健尺度 0) => 权重占比
    # 正确口径: ivar = 0 => 权重份额 0 (度量归零)
    # 地板口径: ivar = 1.314e26, 对照正常观测 sigma_bg = 1 ADU/sr =>
    #   正常 cvar = 1.4*(pi/2)*1/289 = 7.609e-3, ivar = 131.4
    ivar_normal = 1.0 / (1.4 * (math.pi / 2.0) * 1.0 / 289.0)
    share_buggy = civar_floor / (civar_floor + ivar_normal)
    share_correct = 0.0 / (0.0 + ivar_normal)
    res["degenerate_weight_share"] = {
        "ivar_normal_sigma1": ivar_normal,
        "share_buggy_floor": share_buggy,      # 应 ≈ 1.0 (独占)
        "share_correct_ivar0": share_correct,  # 应 = 0 (真值无效应 => 归零)
    }

    # ---- (4) N_eff = N/k_corr 缩放不变性 (方差随 k_corr 线性) ----
    n, k = 65, 1.4
    v0 = mc_var_median(gaussian, n, m=20000)
    res["kcorr_scaling"] = {
        "var_indep_median_N65": v0,
        "prediction_x_kcorr": k * v0,
        "note": "control_variance 对 k_corr 线性缩放属定义面(相关样本方差放大), "
                "k_corr 本身的取值由 C2 实验独立测定",
    }

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
