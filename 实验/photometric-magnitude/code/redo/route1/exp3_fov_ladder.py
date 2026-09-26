#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实验3：FOV 半径三常数 1.2/1.0/10.0（S5）、自适应星等阶梯 {12..16}/2000/5（S6）、
max_stars=5000 星数上限（S11）。

seed 写死：SEED = 20260928。纯 Python + numpy。
运行：python3 exp3_fov_ladder.py
输出：../results/exp3_fov_ladder.json
"""
import json
import math
import numpy as np

SEED = 20260928

# ---------- S5：FOV 几何与钳位 ----------
# fov_natural = pixel_scale_deg * sqrt(W^2+H^2)/2 * 1.2（05 A-4b）
configs = [
    # (名称, pixel_scale [arcsec/px], W, H)  常见相机+望远镜组合的示意域
    ("long-focal small-sensor", 0.30, 9576, 6388),   # 长焦+全幅
    ("rc-astrograph aps-c", 0.90, 6248, 4168),
    ("fast-newton qhy600", 1.20, 9576, 6388),
    ("guidescope small-cam", 3.00, 3000, 2000),
    ("telephoto dslr", 4.50, 6000, 4000),
    ("ultra-wide lens", 10.0, 6000, 4000),
]
fov_rows = []
for name, scale_as, W, H in configs:
    scale_deg = scale_as / 3600.0
    natural = scale_deg * math.sqrt(W * W + H * H) / 2.0 * 1.2
    clamped = min(max(natural, 1.0), 10.0)
    area_ratio = (clamped / natural) ** 2 if natural > 0 else float("inf")
    fov_rows.append({
        "config": name, "pixel_scale_arcsec": scale_as, "W": W, "H": H,
        "fov_natural_deg": natural, "fov_clamped_deg": clamped,
        "was_clamped": abs(clamped - natural) > 1e-12,
        "star_count_ratio_vs_natural": area_ratio,   # 均匀天区：星数 ∝ 面积
    })

# 缓冲 1.2 vs 1.0 的星数增益（均匀密度）：面积比 1.44
buffer_gain = 1.2 ** 2
# 均匀天区 + Poisson：fov 内星数期望与波动（1 度锥、Gaia 平均密度量级）
rng = np.random.default_rng(SEED)
rho_mean_deg2 = 820.0     # Gaia G<16 全天平均 ~3.4e7/41253 deg^2（见报告推导），取整
n_draw = rng.poisson(rho_mean_deg2 * math.pi * 1.0 ** 2, size=2000)
cone_rows = {"fov_deg": 1.0, "rho_per_deg2": rho_mean_deg2,
             "mean_n": float(n_draw.mean()), "sd_n": float(n_draw.std(ddof=1)),
             "p_n_below_3": float((n_draw < 3).mean())}

# 负例/危害量化（01/B12）：CD 奇异 ⇒ pixel_scale=0 ⇒ fov 走钳位 1.0 继续搜
# 量化：位置错配（随机 Gaia 点当匹配）下 location/sigma 是什么样
rng2 = np.random.default_rng(SEED + 1)
n_bad = 500
r_bad = rng2.normal(0.0, 1.5, n_bad)          # 错配残差 ~ dex 量级宽分布
loc_bad = np.median(r_bad)
mad_bad = np.median(np.abs(r_bad - loc_bad))
S_bad = mad_bad / 0.6744897501960817
keep_bad = np.abs(-2.5 * r_bad - np.median(-2.5 * r_bad)) <= 3.0   # 式-2 预过滤
loc_bad_fit = irls = None
r_keep = r_bad[keep_bad]
loc_bad_fit = np.median(r_keep)
for _ in range(50):
    u = (r_keep - loc_bad_fit) / (4.685 * (np.median(np.abs(r_keep - loc_bad_fit)) / 0.6744897501960817))
    w = np.where(np.abs(u) < 1, (1 - u ** 2) ** 2, 0.0)
    if w.sum() <= 0:
        break
    new = np.sum(w * r_keep) / w.sum()
    if abs(new - loc_bad_fit) < 1e-6:
        loc_bad_fit = new
        break
    loc_bad_fit = new
b12_hazard = {
    "scenario": "cd singular -> pixel_scale=0 -> fov clamped 1.0 -> search proceeds on positionally wrong stars",
    "n_mismatch_pairs": n_bad,
    "prefilter_keep_frac": float(keep_bad.mean()),
    "location_from_mismatch_dex": loc_bad_fit,
    "scale_bias_dex": loc_bad_fit,
    "note": "错配定位下 location 系统偏移 ~O(0.1 dex)，且 |r_consistent|>=3 门可被满足 ⇒ 拟合照常出标度",
}

# ---------- S6：自适应星等阶梯 ----------
# 累积计数模型（对数线性）：N(<G) per deg^2 = rho16 * 10^(a*(G-16))，a=0.36（Gaia 亮端典型对数斜率）
rho16 = 820.0
a = 0.36


def n_cum(mag_max, fov_deg=1.0):
    return rho16 * 10 ** (a * (mag_max - 16.0)) * fov_deg


ladder = [12.0, 13.0, 14.0, 15.0, 16.0]
early_stop = 2000
max_rounds = 5
ladder_rows = []
rng3 = np.random.default_rng(SEED + 2)
for dens_factor in (0.2, 1.0, 5.0, 25.0):
    fov = 1.0
    queries = 0
    n_gaia = 0
    for i, mag_max in enumerate(ladder):
        lam = n_cum(mag_max, fov) * dens_factor
        n_gaia = int(rng3.poisson(max(lam, 0.0)))
        queries += 1
        if n_gaia >= early_stop or i == max_rounds - 1:
            break
    sigma_star_mag = 0.045      # 逐星定标散度（sigma_mag 典型）
    sigma_zp = sigma_star_mag / math.sqrt(max(n_gaia, 1))
    ladder_rows.append({
        "sky_density_factor": dens_factor,
        "rounds_used": queries,
        "mag_max_reached": ladder[queries - 1],
        "n_gaia": n_gaia,
        "early_stop_hit": n_gaia >= early_stop,
        "sigma_zp_stat_mag": sigma_zp,
        "queries_saved_vs_always_max": max_rounds - queries,
        "note": "拥挤场 n_gaia 可远超 10000 ⇒ 代码注释宣称的'上界 10000'从未实现（01/C5）",
    })

# 负例：空天区（零密度）⇒ 全档走完 n_gaia=0 ⇒ NO_DATA 分支（度量归零面）
lam0 = 0.0
neg_empty = {"n_gaia": int(rng3.poisson(lam0)), "expected_NO_DATA": True}

# 早停阈 2000 的统计含义：sigma_zp(2000) 相对 sigma_star
sigma_zp_at_2000 = 0.045 / math.sqrt(2000)

# ---------- S11：max_stars=5000 上限的边际收益 ----------
cap_rows = []
for n in (100, 300, 1000, 3000, 5000, 10000, 20000):
    cap_rows.append({"n_stars": n, "sigma_zp_stat_mag": 0.045 / math.sqrt(n),
                     "gain_vs_5000": (0.045 / math.sqrt(5000)) / (0.045 / math.sqrt(n))})

out = {
    "seed": SEED,
    "S5_fov_configs": fov_rows,
    "S5_buffer_gain_area": buffer_gain,
    "S5_cone_poisson": cone_rows,
    "S5_b12_hazard": b12_hazard,
    "S6_ladder_sweep": ladder_rows,
    "S6_negative_empty_field": neg_empty,
    "S6_sigma_zp_at_early_stop_mag": sigma_zp_at_2000,
    "S11_cap_curve": cap_rows,
}
with open("../results/exp3_fov_ladder.json", "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
