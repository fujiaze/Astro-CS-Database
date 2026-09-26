#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实验1：P1 稳健统计常数三腿补齐（S1 c=4.685 / S2 MAD系数 / S3 IRLS容差 / S13 1.166因子）。

独立审计路线1 standalone 脚本。纯 Python + numpy，不 import 仓库任何 Python。
seed 写死：SEED = 20260926。单次运行 CPU << 5 分钟。
运行：python3 exp1_robust_constants.py
输出：../results/exp1_robust_constants.json
"""
import json
import math
import struct
import numpy as np

SEED = 20260926          # 固定 seed（写死）
N_MC = 4000              # Monte Carlo 重复数
N_STAR = 200             # 每次重复的样本量
C_CODE = 4.685           # 代码字面量（star_matcher.cpp:21-27 族）
MAD_COEF_CODE = 0.6744897501960817  # 代码字面量（PHOTOMETRY.md:31）
TOL_CODE = 1e-6
MAX_ITER = 50

# 稠密梯形网格近似 ∫ f dPhi（hermegauss >300 节点在 numpy 会溢出，改用梯形）
_XG = np.linspace(-9.0, 9.0, 180001)
_WG = np.exp(-0.5 * _XG ** 2) / math.sqrt(2.0 * math.pi) * (_XG[1] - _XG[0])


def phi(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def phi_inv(p):
    """标准正态分位，二分到 double 精度。"""
    lo, hi = -10.0, 10.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if phi(mid) < p:
            lo = mid
        else:
            hi = mid
        if hi - lo < 1e-17:
            break
    return 0.5 * (lo + hi)


def bit_equal(a, b):
    return struct.pack("<d", a) == struct.pack("<d", b)


# ---------- V-2 复算(1)：MAD 系数 = Phi^{-1}(3/4) ----------
z34 = phi_inv(0.75)
mad_coef_rel = abs(z34 - MAD_COEF_CODE) / MAD_COEF_CODE
inv_code = 1.0 / MAD_COEF_CODE
inv_claim = 1.482602218505602
inv_rel = abs(inv_code - inv_claim) / inv_claim


# ---------- V-2 复算(2)：biweight 渐近效率 ----------
# A = ∫ psi^2 dPhi / (∫ psi' dPhi)^2,  psi(x)=x(1-(x/c)^2)^2 [|x|<c]；效率 eff = 1/A
def biweight_ARE(c):
    x, w = _XG, _WG
    mask = np.abs(x) < c
    psi = np.where(mask, x * (1 - (x / c) ** 2) ** 2, 0.0)
    num = np.sum(w * psi ** 2)
    up = 1 - (x / c) ** 2
    dpsi = np.where(mask, up * (1 - 5 * (x / c) ** 2), 0.0)
    den = np.sum(w * dpsi) ** 2
    return den / num  # 渐近效率（对均值）


def identity_ARE():
    # 自检：psi(x)=x 必须给 eff=1
    x, w = _XG, _WG
    return np.sum(w * x ** 2) / np.sum(w) ** 2


eff_at_4685 = biweight_ARE(C_CODE)
cs = np.linspace(3.0, 6.0, 301)
effs = np.array([biweight_ARE(c) for c in cs])
c_star = float(np.interp(0.95, effs, cs))
p_out = 2 * (1 - phi(C_CODE))


# ---------- IRLS（02 式-3 的独立复写） ----------
def irls_location(r, c=C_CODE, tol=TOL_CODE, max_iter=MAX_ITER):
    loc = np.median(r)
    mad = np.median(np.abs(r - loc))
    S = mad / MAD_COEF_CODE
    iters = 0
    w = np.ones_like(r)
    if S <= 0:
        return loc, 0, S, w  # S=0 退化：不迭代，location=median
    for it in range(max_iter):
        u = (r - loc) / (c * S)
        w = np.where(np.abs(u) < 1, (1 - u ** 2) ** 2, 0.0)
        sw = w.sum()
        if sw <= 0:
            break
        new_loc = np.sum(w * r) / sw
        iters = it + 1
        done = abs(new_loc - loc) < tol
        loc = new_loc
        if done:
            break
    return loc, iters, S, w


# ---------- MC：biweight location 经验效率（对样本均值） ----------
rng = np.random.default_rng(SEED)
locs_bw = np.empty(N_MC)
locs_mean = np.empty(N_MC)
for k in range(N_MC):
    r = rng.normal(0.0, 0.05, N_STAR)
    locs_mean[k] = r.mean()
    locs_bw[k] = irls_location(r)[0]
v_mean = locs_mean.var(ddof=1)
v_bw = locs_bw.var(ddof=1)
emp_eff = v_mean / v_bw

# ---------- 负例（真值无效应 => 度量归零） ----------
r_const = np.full(50, 0.123)
loc_c, it_c, S_c, _ = irls_location(r_const)
neg_const = {"S": S_c, "iters": it_c, "location": loc_c,
             "sigma_residual_zero": bool(S_c == 0.0)}


# ---------- MAD 有限样本：E[S]/sigma 与 SD(S)/sigma（S13/S2 边界） ----------
def mad_stats(n, n_rep=20000):
    x = rng.normal(0.0, 1.0, (n_rep, n))
    med = np.median(x, axis=1, keepdims=True)
    mad = np.median(np.abs(x - med), axis=1)
    S = mad / MAD_COEF_CODE
    return float(S.mean()), float(S.std(ddof=1))


mad_table = []
for n in (3, 5, 10, 20, 50, 100, 200, 1000):
    m, s = mad_stats(n)
    mad_table.append({"n": n, "E_S_over_sigma": m, "SD_S_over_sigma": s,
                      "SD_times_sqrt_n": s * math.sqrt(n)})

# ---------- IRLS 容差敏感性（S3 / V-8 负例方向） ----------
rng2 = np.random.default_rng(SEED + 1)
r_clean = rng2.normal(0.0, 0.018, 500)
out_idx = rng2.choice(500, size=100, replace=False)   # 20% 离群
r_cont = r_clean.copy()
r_cont[out_idx] += 0.8                                 # 02 式-8 的 0.8 dex 构造
tol_rows = []
loc_ref = irls_location(r_cont, tol=1e-12)[0]
for tol in (1e-2, 1e-6, 1e-9, 1e-12):
    loc, it, S, w = irls_location(r_cont, tol=tol)
    tol_rows.append({"tol": tol, "iters": it, "location": loc,
                     "delta_vs_1e-12": loc - loc_ref,
                     "n_zero_weight": int((w == 0).sum())})
loc_clean = irls_location(r_clean)[0]
loc_cont = irls_location(r_cont)[0]
robust_shift = loc_cont - loc_clean

out = {
    "seed": SEED,
    "S2_mad_coef": {
        "phi_inv_075_full": repr(z34),
        "code_constant": MAD_COEF_CODE,
        "bit_equal": bool(bit_equal(z34, MAD_COEF_CODE)),
        "rel_diff": mad_coef_rel,
        "inverse_code": inv_code, "inverse_claim": inv_claim,
        "inverse_rel_diff": inv_rel,
    },
    "S1_tukey_c": {
        "ARE_c_4685": float(eff_at_4685),
        "c_for_eff_095_scan": c_star,
        "self_check_identity_ARE": float(identity_ARE()),
        "P_outside_c": p_out,
    },
    "S1_mc_efficiency": {
        "n_mc": N_MC, "n_star": N_STAR,
        "var_mean": v_mean, "var_biweight": v_bw,
        "empirical_efficiency_vs_mean": float(emp_eff),
    },
    "negative_constant_input": neg_const,
    "S13_mad_finite_sample": mad_table,
    "S3_irls_tolerance": tol_rows,
    "S3_robust_gate_20pct_outliers": {
        "loc_clean": loc_clean, "loc_cont": loc_cont,
        "shift_dex": robust_shift, "gate_lt_0.1dex_pass": bool(abs(robust_shift) < 0.1),
    },
}
with open("../results/exp1_robust_constants.json", "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
