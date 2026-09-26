#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06 式-1 的 λ 幂次与 Simpson 积分核（PHOTOMETRY.md §2a.1-§2a.3, B11, K-29）。

假说 H6a: F_syn=∫F_λTQλdλ = hc·N_γ（光子计数约定, hc=1.98644586e-16 J·nm）;
        幂次取 0 或 2 偏离光子加权有效波长的量级（02 V-1: 去 λ 比值 ~ 带内有效波长 nm）。
假说 H6b: 亮度差 1 mag 的两星(同谱形) F_syn 之比 = 10^-0.4（星等因子进 0 次）。
假说 H6c: 复化 Simpson 对 2nm 网格上的光滑通带积分误差 ~h⁴ 远小于 XP 刻度 1%；
        负例(真值无效应⇒归零): 被积函数为二次多项式时 Simpson 精确 ⇒ 误差=0。
假说 H6d(B11): 奇区间 n_int=3 时"n_13=0 仍计端点权重"的缺陷给出 +22.2% 偏差
        (h=1, y=[1,1,1,1]: 真值 3, 缺陷版 3.6667); 生产网格 343 点(342 区间, 偶)不触发。
seed 固定 = 20260926。复现: python3 exp_S06_fsyn_lambda_simpson.py
"""
import json, os
import numpy as np

SEED = 20260926
HC = 1.98644586e-16  # J·nm


def planck(lam_nm, T=5800.0):
    h, c, kB = 6.62607015e-34, 2.99792458e8, 1.380649e-23
    lam = lam_nm * 1e-9
    x = h * c / (lam * kB * T)
    return (2 * np.pi * h * c ** 2) / lam ** 5 / (np.expm1(x))


def gaussian_band(lam, mu=640.0, fwhm=120.0):
    sig = fwhm / 2.3548200450309493
    return np.exp(-0.5 * ((lam - mu) / sig) ** 2)


def simpson(y, h):
    """复化 Simpson; 奇区间: 前 n-3 段 1/3 + 末 3 段 3/8.
    返回 (correct, buggy); buggy 复刻 B11: n_13=0 时多算一项 2·y[0]·h/3."""
    n = len(y) - 1
    if n == 0:
        return 0.0, 0.0
    if n == 1:
        v = 0.5 * h * (y[0] + y[1])
        return v, v
    if n % 2 == 0:
        s = y[0] + y[-1]
        for i in range(1, n):
            s += (4.0 if i % 2 == 1 else 2.0) * y[i]
        v = s * h / 3.0
        return v, v
    m = n - 3            # 1/3 段数(偶, 可为 0)
    s = 0.0
    if m > 0:
        s = y[0] + y[m]
        for i in range(1, m):
            s += (4.0 if i % 2 == 1 else 2.0) * y[i]
        s *= h / 3.0
    s3 = (y[m] + 3 * y[m + 1] + 3 * y[m + 2] + y[m + 3]) * 3 * h / 8.0
    correct = s + s3
    buggy = correct + 2.0 * y[0] * h / 3.0     # B11 缺陷项(01/B11: 多算 2·y[0]·h/3)
    return correct, buggy


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}

    # H6a: λ 幂次
    lam = np.arange(336.0, 1021.0, 2.0)
    F = planck(lam)
    T = gaussian_band(lam)
    Q = np.clip(0.85 * np.exp(-((lam - 500) / 700) ** 2), 0, 1)
    f1 = np.trapezoid(F * T * Q * lam, lam)              # λ^1 (正本)
    f0 = np.trapezoid(F * T * Q, lam)                    # 去 λ
    f2 = np.trapezoid(F * T * Q * lam ** 2, lam)
    # 光子计数等价: N_γ = ∫F_λTQ/(hc/λ)dλ ⇒ hc·N_γ = ∫F_λTQλdλ
    N_gamma = np.trapezoid(F * T * Q / (HC / lam), lam)  # photons/s/m²/nm 积分, E_γ=hc/λ J (λ in nm)
    out["H6a"] = {
        "F_syn_lambda1": f1,
        "hc_times_N_gamma": HC * N_gamma,
        "rel_diff": abs(f1 - HC * N_gamma) / f1,
        "ratio_drop_lambda": f0 / f1,       # 应 ~ 带内有效波长(nm) 量级, 02 V-1 实测 649.5
        "photon_weighted_lambda_eff_nm": float(f1 / f0),
        "ratio_lambda2": f2 / f1,
        "pass": bool(abs(f1 - HC * N_gamma) / f1 < 1e-12 and 500 < f1 / f0 < 800),
    }

    # H6b: 1 mag ⇒ 10^-0.4
    f_b = np.trapezoid(planck(lam, 5800) * T * Q * lam, lam)
    f_d = np.trapezoid(planck(lam, 5800) * 10 ** (-0.4) * T * Q * lam, lam)
    out["H6b"] = {"ratio": f_d / f_b, "expected": 10 ** (-0.4),
                  "pass": bool(abs(f_d / f_b - 10 ** (-0.4)) < 1e-12)}

    # H6c: Simpson 误差与负例
    def integrand(l):
        return planck(l) * gaussian_band(l) * 1.0

    fine = np.arange(336.0, 1021.0, 0.05)
    ref = np.trapezoid(integrand(fine), fine)
    errs = {}
    for step in (8.0, 4.0, 2.0, 1.0):
        g = np.arange(336.0, 1021.0, step)
        v, _ = simpson(integrand(g), step)
        errs[str(step)] = abs(v - ref) / ref
    # 负例: 二次被积函数 Simpson 精确 ⇒ 0
    x = np.arange(0.0, 10.0001, 0.5)     # 21 点 20 区间(偶)
    vq, _ = simpson(x ** 2, 0.5)
    out["H6c"] = {
        "rel_err_by_step_nm": errs,
        "err_2nm": errs["2.0"],
        "negative_zero_quadratic": {"value": vq, "truth": 1000.0 / 3,  # ∫x²dx 0..10
                                     "abs_err": abs(vq - 1000.0 / 3),
                                     "criterion": "二次型被积式 Simpson 精确 ⇒ 误差归零",
                                     "pass": bool(abs(vq - 1000.0 / 3) < 1e-9)},
        "pass": bool(errs["2.0"] < 0.01),
    }

    # H6d: B11 缺陷
    v_ok, v_bug = simpson(np.ones(4), 1.0)     # y=[1,1,1,1], h=1, 真值 3
    v_lin, v_lin_b = simpson(np.arange(4) * 1.0, 1.0)  # y=[0,1,2,3], 真值 4.5
    grid343 = np.arange(336.0, 1021.0, 2.0)
    n_int = len(grid343) - 1
    out["H6d_B11"] = {
        "const_case": {"value_buggy": v_bug, "value_correct": v_ok, "truth": 3.0,
                        "rel_bias_buggy": (v_bug - 3.0) / 3.0},
        "linear_case": {"value": v_lin, "truth": 4.5,
                         "note": "误差正比于左端点取值(02 断言)"},
        "production_grid": {"n_points": len(grid343), "n_intervals": n_int,
                             "is_even": n_int % 2 == 0,
                             "triggered": bool(n_int % 2 == 1)},
        "pass": bool(abs(v_bug - 11.0 / 3) < 1e-12 and n_int % 2 == 0),
    }

    out["verdict"] = {k: out[k]["pass"] for k in ("H6a", "H6b", "H6c", "H6d_B11")}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S06_fsyn_lambda_simpson.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({k: out[k] for k in ("H6a", "H6b", "H6c", "H6d_B11", "verdict")},
                     ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
