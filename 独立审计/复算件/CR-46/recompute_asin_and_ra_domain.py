#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CR-46 复算 2：asin 参数是否会因浮点舍入越过 1（wcs_tan.cpp:30 无 clamp），
以及 RA 输出域负值的影响范围。"""
import math
import random
import sys

sys.stdout.reconfigure(encoding="utf-8")
D2R = math.pi / 180.0

# ---- 1. 复算 pix2sky 的 asin 参数 = cr*sin(dec0) + eta*sr*cos(dec0)/R
#      等价形式（同式代数）： (sin(dec0) + eta*cos(dec0)) / sqrt(1+xi^2+eta^2)
rng = random.Random(7)
worst = -9.0
worst_case = None
over = 0
trials = 0
for dec0_deg in [-90, -89.9, -85, -45, -10, 0, 10, 45, 85, 89.9, 89.999999, 90]:
    dec0 = dec0_deg * D2R
    s0, c0 = math.sin(dec0), math.cos(dec0)
    for _ in range(400000):
        # 覆盖 R 从 1e-12 到 1e12（含越界与极点）
        R = 10 ** rng.uniform(-12, 12)
        ph = rng.uniform(0, 2 * math.pi)
        xi, eta = R * math.sin(ph), R * math.cos(ph)
        if R == 0:
            continue
        rho = math.atan(R)
        cr, sr = math.cos(rho), math.sin(rho)
        arg_code = cr * s0 + (eta * sr * c0) / R
        arg_alt = (s0 + eta * c0) / math.sqrt(1 + R * R)
        trials += 1
        for name, a in (("code", arg_code), ("alt", arg_alt)):
            if a > 1.0 and abs(a) > abs(worst if worst > 1 else 1.0 + 1e-18):
                pass
            if a > 1.0:
                over += 1
                if worst < a:
                    worst = a
                    worst_case = (dec0_deg, R, ph, name)
        # 极点邻域专测：eta 使点落在极点上
print(f"trials={trials}  asin 参数 > 1 的次数 = {over}")
print(f"  最大越界值 = {worst!r}  出现于 (dec0,R,phase,式) = {worst_case}")

# ---- 2. 专测：解析上恰好到极点的构造（eta = (1/sin? ) ）
#      取 dec0, 让目标点正好是北极： 应满足 arg == 1
print("\n极点构造专测（目标点=天北极，arg 理论=1）：")
for dec0_deg in (0.5, 5, 30, 45, 60, 80, 89.5, 89.9999):
    dec0 = dec0_deg * D2R
    s0, c0 = math.sin(dec0), math.cos(dec0)
    if s0 == 0:
        continue
    eta = c0 / s0          # 北极的 eta
    xi = 0.0
    R = abs(eta)
    rho = math.atan(R)
    cr, sr = math.cos(rho), math.sin(rho)
    arg = cr * s0 + (eta * sr * c0) / R
    flag = "  <-- asin 参数 > 1，C++ std::asin 返回 NaN" if arg > 1.0 else ""
    print(f"  dec0={dec0_deg:9.4f}  arg={arg!r}{flag}")

# ---- 3. RA 负值影响范围：给定像素尺度/边长，CRVAL1 多大范围内 samples 出负 RA
print("\nRA 输出域负值影响范围（samples[].ra < 0 的 CRVAL1 区间）：")
for s0_asec, npx in ((0.18, 4096), (0.18, 16384), (1.5, 4096), (6.0, 4096), (0.989, 6000)):
    diag_deg = npx * math.sqrt(2) * s0_asec / 3600.0
    print(f"  {s0_asec:6.3f}\"/px, {npx:5d}px 方场: 对角线 {diag_deg:6.3f}°"
          f" ⇒ CRVAL1 < {diag_deg/2:6.3f}° 或 > {360-diag_deg/2:6.3f}° 时西侧样本 RA 为负")
