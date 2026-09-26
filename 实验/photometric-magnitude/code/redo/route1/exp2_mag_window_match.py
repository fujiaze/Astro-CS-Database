#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实验2：P1 预过滤窗 mag_tolerance=3.0（S4）、星等窗 mag_min/max（S7）、匹配半径 match_radius_px=2.0（S8），
并实现一条贯穿上下游接口的最小链条用例（P1 上游输入 -> P1 产物 -> P2 消费口径）。

seed 写死：SEED = 20260927。纯 Python + numpy。
运行：python3 exp2_mag_window_match.py
输出：../results/exp2_mag_window_match.json
"""
import json
import math
import numpy as np

SEED = 20260927
MAD_COEF = 0.6744897501960817
C_TUKEY = 4.685
MAG_TOL_CODE = 3.0          # PHOTOMETRY.md:34
MATCH_RADIUS_CODE = 2.0     # star_matcher 默认实参（01/C6）
SIGMA_R = 0.018             # dex，逐星残差量级（取 M42 实测 sigma_mag~0.045 的 dex 值 0.018）


def irls_location(r, c=C_TUKEY, tol=1e-6, max_iter=50):
    loc = np.median(r)
    mad = np.median(np.abs(r - loc))
    S = mad / MAD_COEF
    if S <= 0:
        return loc, 0, S, np.ones_like(r)
    w = np.ones_like(r)
    for it in range(max_iter):
        u = (r - loc) / (c * S)
        w = np.where(np.abs(u) < 1, (1 - u ** 2) ** 2, 0.0)
        sw = w.sum()
        if sw <= 0:
            break
        new = np.sum(w * r) / sw
        it += 1
        done = abs(new - loc) < tol
        loc = new
        if done:
            break
    return loc, it, S, w


def prefilter(r, mag_tol):
    """式-2 预过滤：|delta - median(delta)| > mag_tol 拒绝；delta = -2.5*r + const（推导见报告 S4 节）。"""
    delta = -2.5 * r                       # 常数项在 median 差中消去
    keep = np.abs(delta - np.median(delta)) <= mag_tol
    return keep


# ---------- S4：mag_tolerance 对污染场的防护 ----------
rng = np.random.default_rng(SEED)
rows = []
for f in (0.0, 0.02, 0.05, 0.10):
    for d_dex in (0.8, 1.2):
        n = 2000
        r = rng.normal(0.0, SIGMA_R, n)
        nout = int(round(f * n))
        if nout:
            oi = rng.choice(n, size=nout, replace=False)
            r[oi] += d_dex
        keep = prefilter(r, MAG_TOL_CODE)
        loc_kept, _, _, _ = irls_location(r[keep])
        loc_all, _, _, _ = irls_location(r)
        rows.append({
            "contam_frac": f, "shift_dex": d_dex,
            "n_rejected_by_prefilter": int((~keep).sum()),
            "zp_bias_with_prefilter_dex": loc_kept,
            "zp_bias_without_prefilter_dex": loc_all,
            "protection_gain_dex": loc_all - loc_kept,
        })

# 负例：无污染 ⇒ 预过滤拒绝数=0 且对 location 无效应（不依赖窗宽取值）
neg = []
for tol in (1.0, 2.0, 3.0, 5.0):
    r = rng.normal(0.0, SIGMA_R, 2000)
    keep = prefilter(r, tol)
    loc = irls_location(r[keep])[0]
    neg.append({"tol": tol, "n_rejected": int((~keep).sum()), "location_dex": loc})

# ---------- S7：星等窗 mag_min/mag_max 的星族效应（颜色项模型） ----------
# 模型：F_syn 的通带失配误差 = 颜色一次项：log10 F_syn_model = log10 F_syn_true + beta*(color)
# 星族：color 与亮度相关（亮端偏红），G ~ N(11, 2.5) 截断 [6,16]
rng2 = np.random.default_rng(SEED + 1)
n = 20000
G = np.clip(rng2.normal(11.0, 2.6, n), 6.0, 16.0)
color = 0.3 + 0.08 * (14.0 - G) + rng2.normal(0, 0.15, n)   # 亮端更红
beta = 0.06                                   # dex/mag-color：通带一次颜色项
r_color_err = beta * (color - color.mean())   # 若用同族中位色归一，窗改变 → ZP 平移 + 残差变化
zp_rows = []
for lo, hi in ((6.0, 16.0), (6.0, 12.0), (12.0, 16.0), (10.0, 15.0)):
    m = (G >= lo) & (G <= hi)
    rr = r_color_err[m] + rng2.normal(0, SIGMA_R, m.sum())
    zp = np.median(rr)
    mad = np.median(np.abs(rr - zp))
    zp_rows.append({"mag_min": lo, "mag_max": hi, "n": int(m.sum()),
                    "zp_offset_dex": zp,
                    "sigma_residual_dex": mad / MAD_COEF})
# 窗间 ZP 平移（同帧不同窗 ⇒ ZP_syn 系统差）
zp_full = zp_rows[0]["zp_offset_dex"]
for row in zp_rows:
    row["zp_shift_vs_full_dex"] = row["zp_offset_dex"] - zp_full
    row["zp_shift_vs_full_mag"] = 2.5 * (row["zp_offset_dex"] - zp_full)

# ---------- S8：匹配半径 match_radius_px ----------
rng3 = np.random.default_rng(SEED + 2)
match_rows = []
for sigma_j in (0.0, 0.2, 0.5, 1.0):
    for radius in (0.5, 1.0, 2.0, 3.0, 5.0):
        n_frames = 200
        n_star = 500
        area = 1500.0 * 1000.0                       # px^2
        correct = 0; ambiguous = 0; total = 0
        for _ in range(n_frames):
            gx = rng3.uniform(0, 1500, n_star); gy = rng3.uniform(0, 1000, n_star)
            ox = gx + rng3.normal(0, sigma_j, n_star)
            oy = gy + rng3.normal(0, sigma_j, n_star)
            for i in range(n_star):
                total += 1
                d2 = (gx - ox[i]) ** 2 + (gy - oy[i]) ** 2
                within = d2 <= radius ** 2
                k = int(within.sum())
                if k == 1:
                    correct += 1
                elif k > 1:
                    ambiguous += 1
        match_rows.append({"sigma_jitter_px": sigma_j, "radius_px": radius,
                           "correct_rate": correct / total,
                           "ambiguous_rate": ambiguous / total})

# 负例：零抖动、稀疏场 ⇒ 半径敏感性趋零（最小半径即可全对）
r0 = [row for row in match_rows if row["sigma_jitter_px"] == 0.0]
sens = max(row["correct_rate"] for row in r0) - min(row["correct_rate"] for row in r0)

# 理论对照：稀疏场 false-match 概率 ≈ 1-exp(-rho*pi*r^2)
rho = 500 / (1500 * 1000)
theory = {r: 1 - math.exp(-rho * math.pi * r * r) for r in (1.0, 2.0, 3.0, 5.0)}

# ---------- 贯穿上下游接口的最小链条用例 ----------
# 上游（PSF/星表侧给什么）：F_instr [ADU]（PSF 拟合域通量）、F_syn [W m^-2 nm]（式-1 合成通量）
# P1 产出：location [dex]、scale=10^-location [[F_syn 单位]/ADU]、sigma_residual [dex]
# 下游（P2 链拿去做什么）：零点统计标准误 sigma_kappa,stat = 1.253 * sigma_residual / sqrt(N)
rng4 = np.random.default_rng(SEED + 3)
n_up = 800
F_syn = np.full(n_up, 1.0e-11)                       # [W m^-2 nm]，同族常数（示意）
true_scale = 3.7e6                                   # [F_syn 单位]/ADU 的真值（示意）
F_instr = F_syn / true_scale * 10 ** rng4.normal(0, SIGMA_R, n_up)
loc_chain, it_chain, S_chain, _ = irls_location(np.log10(F_instr / F_syn))
scale_chain = 10 ** (-loc_chain)
sigma_res_chain = np.median(np.abs(np.log10(F_instr / F_syn) - loc_chain)) / MAD_COEF
sigma_kappa_stat = 1.253 * sigma_res_chain / math.sqrt(n_up)
chain_case = {
    "n_stars": n_up,
    "upstream_units": {"F_instr": "ADU", "F_syn": "W m^-2 nm"},
    "true_scale_Fsyn_per_ADU": true_scale,
    "recovered_scale": scale_chain,
    "recovered_rel_err": abs(scale_chain - true_scale) / true_scale,
    "location_dex": loc_chain, "irls_iters": it_chain,
    "sigma_residual_dex": sigma_res_chain,
    "downstream_sigma_kappa_stat_dex": sigma_kappa_stat,
    "downstream_formula": "sigma_kappa_stat = 1.253*sigma_residual/sqrt(N)  (02 式-4 消费口径)",
}

out = {
    "seed": SEED,
    "S4_mag_tolerance": rows,
    "S4_negative_no_contamination": neg,
    "S7_mag_window": zp_rows,
    "S8_match_radius": match_rows,
    "S8_zero_jitter_sensitivity": sens,
    "S8_false_match_theory": theory,
    "chain_case": chain_case,
}
with open("../results/exp2_mag_window_match.json", "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
