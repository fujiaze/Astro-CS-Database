#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实验5：Simpson 奇区间缺陷与 n_int>=1 守卫（S14）、m_cut 初值常数 6.0/1.5/2.0（S16）、
身份指纹容差 1e-9 的豁免论证（S17）。

seed 写死：SEED = 20260930。纯 Python + numpy。
运行：python3 exp5_integration_gates.py
输出：../results/exp5_integration_gates.json
"""
import json
import math
import numpy as np

SEED = 20260930


# ---------- S14：复化 Simpson ----------
def simpson_even(y, h):
    """偶数区间复化 Simpson（正确版）。"""
    n = len(y) - 1
    if n % 2 != 0:
        raise ValueError("even intervals required")
    if n == 0:
        return 0.0
    s = y[0] + y[-1] + 4 * np.sum(y[1:-1:2]) + 2 * np.sum(y[2:-1:2])
    return s * h / 3.0


def simpson_buggy_n3(y, h):
    """复现 01/B11：奇区间分支 n_int==3 时多算一项 2*y[0]*h/3（n_13=0 仍计端点权重）。

    生产实现形态：n=3 区间时用 3/8 尾巴（4 点、3 区间）本身正确，
    但退化段权重初始化 n_13=0 时仍按"两端点各一权重"额外计入 2*y[0]*h/3。
    B11 的读数：h=1, y=[1,1,1,1] 真值 3、代码 3.666667（+22.2%）；y=[0,1,2,3] 恰为真值（y0=0 ⇒ 幽灵项=0）。
    """
    return simpson_38_tail(y, h) + 2 * y[0] * h / 3.0   # 正确 3/8 积分 + 幽灵端点项


def simpson_38_tail(y, h):
    """正确版：3 区间尾巴用 3/8 规则。"""
    body = simpson_even(y[:-3], h)
    yy = y[-4:]
    tail = (yy[0] + 3 * yy[1] + 3 * yy[2] + yy[3]) * 3 * h / 8.0
    return body + tail


simp_rows = []
# B11 原案：h=1, y=[1,1,1,1] 真值 3
for tag, y, h in (("const_ones", np.array([1., 1, 1, 1]), 1.0),
                  ("linear_0123", np.array([0., 1, 2, 3]), 1.0)):
    exact = float(np.trapezoid(y, dx=h))
    try:
        even_res = simpson_even(y, h)
    except ValueError:
        even_res = None  # 3 区间（奇）：偶区间规则本就不适用，正确做法是 3/8 尾巴
    simp_rows.append({
        "case": tag, "exact": exact,
        "even_rule_result": even_res,
        "buggy_n3_result": simpson_buggy_n3(y, h),
        "buggy_rel_err": simpson_buggy_n3(y, h) / exact - 1.0,
        "correct_38tail_result": simpson_38_tail(y, h),
    })

# 平滑通带上的收敛阶（正确版）：常数被积式负例 → 误差 0
band = lambda lam: np.exp(-0.5 * ((lam - 643.0) / 36.0) ** 2)   # Baader R 形态示意
conv_rows = []
for npts in (9, 17, 33, 65, 129):
    lam = np.linspace(500.0, 800.0, npts)
    y = band(lam)
    h = lam[1] - lam[0]
    exact = np.trapezoid(y, dx=h)
    approx = simpson_even(y, h)
    conv_rows.append({"n_points": npts, "rel_err": float(abs(approx / exact - 1.0))})
# 常数被积式负例
lam = np.linspace(500, 800, 33)
neg_const = float(simpson_even(np.ones_like(lam), lam[1] - lam[0]) - 300.0)

# n_int 守卫演示：wl_count ∈ {1,2,3,4} 的退化网格
def integrate_like_production(lam):
    """带 3/8 奇尾巴的复化 Simpson（与生产系数族一致，N-19）。"""
    y = band(lam)
    h = lam[1] - lam[0]
    n = len(y) - 1
    if n % 2 == 0:
        return simpson_even(y, h)
    return simpson_38_tail(y, h)

guard_rows = []
for wl_count in (1, 2, 3, 4):
    lam = np.linspace(600.0, 700.0, wl_count) if wl_count > 1 else np.array([650.0])
    try:
        v = integrate_like_production(lam)
        guard_rows.append({"wl_count": wl_count, "can_integrate": wl_count >= 3,
                           "value_or_zero": float(v),
                           "guard_should_reject": wl_count < 3})
    except Exception:
        guard_rows.append({"wl_count": wl_count, "can_integrate": False,
                           "value_or_zero": None, "guard_should_reject": True})

# ---------- S16：m_cut 初值常数（割线迭代初值不影响收敛值） ----------
# 模型：N(m) = rho * A * 10^(a*(m - m0))；解 N(m_cut) = N_target
rho, A, a, m0 = 820.0, 1.0, 0.36, 16.0
N_target = 1500.0


def N_of(m):
    return rho * A * 10 ** (a * (m - m0))


def secant(f, x0, x1, tol=1e-12, itmax=100, lo=0.0, hi=30.0, max_step=3.0):
    """阻尼割线法：步长限幅，防指数模型发散（不影响收敛值，只影响步数）。"""
    it = 0
    for it in range(1, itmax + 1):
        f0, f1 = f(x0), f(x1)
        if f1 == f0:
            break
        step = f1 * (x1 - x0) / (f1 - f0)
        if abs(step) > max_step:
            step = math.copysign(max_step, step)
        x2 = x1 - step
        x2 = min(max(x2, lo), hi)   # 域内截断：域外指数溢出
        if abs(x2 - x1) < tol:
            return x2, it
        x0, x1 = x1, x2
    return x1, it


m_cut_rows = []
for (init0, init1) in ((6.0, 6.5), (1.5, 2.0), (2.0, 3.0), (0.5, 1.0)):
    m, it = secant(lambda m: N_of(m) - N_target, init0, init1)
    m_cut_rows.append({"init": [init0, init1], "m_cut_converged": m,
                       "iterations": it})
m_exact = m0 + math.log10(N_target / (rho * A)) / a
m_cut_rows.append({"init": "analytic", "m_cut_converged": m_exact, "iterations": 0})

# ---------- S17：身份指纹容差 1e-9（数值卫生豁免论证） ----------
rng = np.random.default_rng(SEED)
# 模拟 73 点曲线（Baader R 点数量级）
n_pts = 73
wl = np.linspace(572.0, 716.0, n_pts)
val = np.clip(0.9 * np.exp(-0.5 * ((wl - 643.0) / 36.0) ** 2) + rng.normal(0, 1e-4, n_pts), 0, 1)
wl_sum = float(np.sum(wl)); val_sum = float(np.sum(val))
val_sumsq = float(np.sum(val ** 2))


def fp(vals):
    return {"wl_sum": float(np.sum(vals[0])), "val_sum": float(np.sum(vals[1])),
            "val_sumsq": float(np.sum(vals[1] ** 2))}


base = fp((wl, val))
# 纯 double 舍入噪声量级：对同一数组做一次逐元素 round-trip（+0 再减 0 无意义，改为重排累加）
val_rev = val[::-1].copy()
reorder = fp((wl[::-1].copy(), val_rev))
# 单点 1e-12 相对扰动（±16 ulp）：能否被 1e-9 指纹发现？
val_p = val.copy(); val_p[10] *= (1 + 1e-12)
pert = fp((wl, val_p))
# 单点 1e-6 相对扰动：能否发现？
val_q = val.copy(); val_q[10] *= (1 + 1e-6)
pert2 = fp((wl, val_q))


def max_rel(fp1, fp2):
    keys = ("wl_sum", "val_sum", "val_sumsq")
    return max(abs(fp2[k] - fp1[k]) / abs(fp1[k]) for k in keys if fp1[k] != 0)


fp_rows = {
    "base": base,
    "summation_order_noise_rel": max_rel(base, reorder),
    "single_point_1e-12_rel": max_rel(base, pert),
    "single_point_1e-6_rel": max_rel(base, pert2),
    "tolerance_code": 1e-9,
    "detects_1e-12": max_rel(base, pert) > 1e-9,
    "detects_1e-6": max_rel(base, pert2) > 1e-9,
    "double_eps": 2.220446049250313e-16,
}

out = {
    "seed": SEED,
    "S14_simpson_b11": simp_rows,
    "S14_convergence": conv_rows,
    "S14_negative_constant_integrand_abs_err": neg_const,
    "S14_n_int_guard": guard_rows,
    "S16_m_cut_secant": m_cut_rows,
    "S16_m_cut_analytic": m_exact,
    "S17_fingerprint_tolerance": fp_rows,
}
with open("../results/exp5_integration_gates.json", "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
