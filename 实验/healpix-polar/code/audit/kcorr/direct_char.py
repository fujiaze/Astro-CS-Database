#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
direct_char.py - iid 高斯有限 N 基线偏置的直接定征(与 drizzle 无关的纯统计事实):
对 n=5/9/25 的 iid N(0,1) 样本, 直接测定
  (a) Var(median) / [pi sigma^2/(2n)]   —— 渐近式中位数方差公式的有限 N 比值
  (b) median over realizations of MAD-scale / sigma —— 正本估计器的尺度偏置
  (c) k_gauss(n) = Var(median) / [pi sigma_MAD^2/(2n)] —— 两者合成
写死 seed=20260816, NMC=400000。结果落 results/direct_char.json。
"""
import json
import os

import numpy as np

MAD_C = 1.482602218505602
NMC = 400000
SEED = 20260816

rng = np.random.default_rng(SEED)
rows = []
for n in (5, 9, 25):
    x = rng.normal(0.0, 1.0, size=(NMC, n))
    med = np.median(x, axis=1)
    var_med = float(med.var(ddof=1))
    mad = MAD_C * np.median(np.abs(x - med[:, None]), axis=1)
    med_mad = float(np.median(mad))
    base = np.pi / 2 * med_mad ** 2 / n
    rows.append(dict(
        n=n, var_median=var_med,
        asymptotic=np.pi / 2 / n,
        var_over_asymptotic=var_med / (np.pi / 2 / n),
        median_MAD_scale=med_mad,
        k_gauss=var_med / base,
    ))
    print(rows[-1])
out = dict(group="direct_characterization_iid_gaussian", seed=SEED, nmc=NMC, rows=rows)
p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "results", "direct_char.json")
json.dump(out, open(p, "w"), indent=1)
print("written", p)
