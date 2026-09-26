#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4R2-E09: Moffat4 FWHM/sigma = 1.230310 的解析复算（P-CST-05 方案 B：改理论推导腿）.

生产锚：docs/science/PSF.md:76-80  MOFFAT4_FWHM_FACTOR = 1.230310；FWHM = 2*alpha*sqrt(2^{1/beta}-1)。
解析推导（beta=4）：
  I(r) = A (1 + r^2/alpha^2)^{-4}
  总通量 = pi*alpha^2*A/(beta-1) = pi*alpha^2*A/3
  <r^2> = alpha^2/(beta-2) = alpha^2/2  =>  sigma = alpha/sqrt(2)
  半高： (1+r^2/alpha^2)^{-4} = 1/2 => r_half = alpha*sqrt(2^{1/4}-1)
  FWHM = 2*alpha*sqrt(2^{1/4}-1)
  =>  FWHM/sigma = 2*sqrt(2)*sqrt(2^{1/4}-1) = 1.230310...
实验腿：2D 数值积分（极坐标 Simpson）复核 sigma 与半高半径，验证比值；
负例：真值无效应——对高斯轮廓同一计算必须给出 2*sqrt(2*ln2)=2.35482（判据能区分两种轮廓，
常数随轮廓族变化，非普适）。
"""
import json
from pathlib import Path

import numpy as np

RESULTS = Path(__file__).resolve().parent.parent / "results"
REGISTERED = 1.230310

# 解析值
analytic = 2.0 * np.sqrt(2.0) * np.sqrt(2.0 ** 0.25 - 1.0)

# 数值积分：sigma = sqrt(<r^2>)，<r^2> = int r^3 f(r) dr / int r f(r) dr（圆对称，dA = 2pi r dr）
r = np.linspace(0, 60, 400001)
f = (1.0 + (r / 1.0) ** 2) ** -4.0          # alpha=1
num = np.trapz(r**3 * f, r)
den = np.trapz(r * f, r)
sigma_num = np.sqrt(num / den)
r_half = np.interp(0.5, f[::-1], r[::-1])   # f 单调降，插值半高半径
fwhm_num = 2.0 * r_half
ratio_num = float(fwhm_num / sigma_num)
rel_err = abs(ratio_num - REGISTERED) / REGISTERED

# 高斯对照（负例：判据必须能区分）
g = np.exp(-0.5 * r**2)
num_g = np.trapz(r**3 * g, r)
den_g = np.trapz(r * g, r)
sigma_g = np.sqrt(num_g / den_g)
r_half_g = np.sqrt(2.0 * np.log(2.0))
ratio_gauss = float(2.0 * r_half_g / sigma_g)

res = {
    "analytic_value_beta4": float(analytic),
    "numeric_value": ratio_num,
    "registered": REGISTERED,
    "rel_err_numeric_vs_registered": rel_err,
    "verdict": "PASS" if rel_err < 1e-4 else "FAIL",
    "gaussian_control_fwhm_over_sigma": ratio_gauss,
    "gaussian_theory_2sqrt_2ln2": 2.0 * np.sqrt(2.0 * np.log(2.0)),
    "gaussian_theory_2d_rms_convention": 2.0 * np.sqrt(2.0 * np.log(2.0)) / np.sqrt(2.0),
    "conclusions": [
        "解析：FWHM/sigma = 2*sqrt(2)*sqrt(2^{1/4}-1) = %.9f，与登记值 1.230310 相对差 %.2e；" % (analytic, abs(analytic-REGISTERED)/REGISTERED),
        "数值积分复核：%.9f（相对差 %.2e）⇒ P-CST-05 可走方案 B（理论推导腿），实验单元不再是必需；" % (ratio_num, rel_err),
        "负例：同一计算作用于高斯轮廓得 %.6f = 2*sqrt(2 ln2)/sqrt(2)（同 2D rms 半径口径；若按逐轴 sigma 口径则为 2.3548200450）"
    "——常数随轮廓族与 sigma 口径双重变化，判据能区分（非恒真）。" % ratio_gauss,
    ],
}
(RESULTS / "exp09_moffat4_factor.json").write_text(json.dumps(res, indent=2), encoding="utf-8")
print(json.dumps(res, indent=2))
