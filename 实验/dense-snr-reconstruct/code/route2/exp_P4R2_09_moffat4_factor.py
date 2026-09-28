#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4R2-E09: Moffat4 FWHM/sigma 因子的两个 sigma 约定（P4-B01 订正版）。

生产锚：docs/science/PSF.md §5/§16  MOFFAT4_FWHM_FACTOR = 1.230310；FWHM = 2*alpha*sqrt(2^{1/beta}-1)。

一手定义（beta=4，闭式与数值双证，不复用被审代码结论）：
  I(r) = A (1 + r^2/alpha^2)^{-4}
  半高： (1+r^2/alpha^2)^{-4} = 1/2  =>  r_half = alpha*sqrt(2^{1/4}-1)
  FWHM  = 2*alpha*sqrt(2^{1/4}-1) = 0.8699588841*alpha
  <r^2> = int r^3 I dr / int r I dr = alpha^2/2   （int rI=alpha^2/6, int r^3 I=alpha^4/12）
  => sigma   := sqrt(<r^2>) = alpha/sqrt(2)   [模型参数口径；生产 Q = 0.5*r^2/sigma^2 的 sigma]
  => sigma_g := alpha/2                       [同二阶矩高斯逐轴口径；他域]

两个约定各自的 FWHM/sigma（恒差 sqrt(2)，禁止混用）：
  模型参数口径 : 2*sqrt(2)*sqrt(2^{1/4}-1) = 1.2303076525901024  （生产唯一口径）
  逐轴高斯口径 : 4*sqrt(2^{1/4}-1)         = 1.7399178387        （生产禁用）

判据（能红能绿）：
  正例：生产参数化 (1+r^2/(2*sigma^2))^{-4} 下 sigma=FWHM/1.230310 时，剖面在 r=FWHM/2 处 = 0.5
       （残差量级 = 实现常量的圆整量 ~1e-6）；
  负例：同一检验代入 1.7399178 -> r=FWHM/2 处约 0.2770（反解 FWHM 短 29.3%）⇒ 必须判红；
  负例 2（轮廓族）：高斯同口径 = 2*sqrt(2 ln2)/sqrt(2) = 1.6651092223 != Moffat4 值。
"""
import json
from pathlib import Path

import numpy as np

# 结果目录锚定本单元 results/route2（P4-B04：旧版 parent.parent 解析到 code/results，
# 使存档与脚本脱钩；单脚本重跑还会 FileNotFoundError）。
UNIT = Path(__file__).resolve().parents[2]
RESULTS = UNIT / "results" / "route2"
RESULTS.mkdir(parents=True, exist_ok=True)
REGISTERED = 1.230310

# ---- 解析值 ----------------------------------------------------------------
half_root = np.sqrt(2.0 ** 0.25 - 1.0)
analytic_model_q = 2.0 * np.sqrt(2.0) * half_root      # FWHM/sigma（模型参数口径）
analytic_peraxis = 4.0 * half_root                     # FWHM/sigma_g（逐轴高斯口径）
fwhm_over_alpha = 2.0 * half_root

# ---- 数值积分：sigma = sqrt(<r^2>)，r_half 用二分求根（不依赖插值网格） ----
r = np.linspace(0.0, 200.0, 2_000_001)
f = (1.0 + r ** 2) ** -4.0                             # alpha = 1
num = np.trapz(r ** 3 * f, r)
den = np.trapz(r * f, r)
sigma_num = float(np.sqrt(num / den))
lo, hi = 0.0, 5.0
for _ in range(200):
    mid = 0.5 * (lo + hi)
    g = lambda x: float((1.0 + x * x) ** -4.0) - 0.5
    if g(lo) * g(mid) <= 0.0:
        hi = mid
    else:
        lo = mid
r_half_num = 0.5 * (lo + hi)
ratio_num = 2.0 * r_half_num / sigma_num               # 同口径（模型参数）数值复核
rel_err = abs(ratio_num - REGISTERED) / REGISTERED


# ---- 生产参数化往返：Q = 0.5*r^2/sigma^2, I = (1+Q)^{-4} -------------------
def profile_at(rr, sigma):
    return (1.0 + rr ** 2 / (2.0 * sigma ** 2)) ** -4.0


def roundtrip(C, fwhm_true=3.0):
    sigma = fwhm_true / C
    val = float(profile_at(0.5 * fwhm_true, sigma))
    lo2, hi2 = 0.0, 10.0 * fwhm_true
    for _ in range(200):
        mid = 0.5 * (lo2 + hi2)
        h = lambda x: float(profile_at(x, sigma)) - 0.5
        if h(lo2) * h(mid) <= 0.0:
            hi2 = mid
        else:
            lo2 = mid
    implied = 2.0 * 0.5 * (lo2 + hi2)
    return {
        "C": C,
        "sigma_px": sigma,
        "profile_value_at_r=FWHM/2": val,
        "half_max_residual": abs(val - 0.5),
        "implied_FWHM_px": implied,
        "implied_FWHM_rel_err": abs(implied - fwhm_true) / fwhm_true,
        "verdict": "PASS" if abs(val - 0.5) <= 2e-6 else "RED",
    }


rt_prod = roundtrip(REGISTERED)                        # 正例：生产口径
rt_bad = roundtrip(1.7399178)                          # 负例：他域口径代入生产参数化
rt_exact = roundtrip(analytic_model_q)                 # 闭式精确值（极限对照）

# ---- 高斯对照（负例 2：常数随轮廓族变化） ---------------------------------
g = np.exp(-0.5 * r ** 2)
sigma_g_num = float(np.sqrt(np.trapz(r ** 3 * g, r) / np.trapz(r * g, r)))
ratio_gauss = float(2.0 * np.sqrt(2.0 * np.log(2.0)) / sigma_g_num)

res = {
    "seed": None,   # 解析 + 数值积分，无随机源（SEEDS.md 同口径）
    "convention_definitions": {
        "sigma_model_q": "sqrt(<r^2>) = alpha/sqrt(2); 生产 Q = 0.5*r^2/sigma^2 的参数（唯一生产口径）",
        "sigma_g_peraxis": "alpha/2; 同二阶矩高斯逐轴口径（他域，生产禁用）",
        "r2_over_alpha2": 0.5,
        "r_half_over_alpha": float(r_half_num),
        "fwhm_over_alpha": float(fwhm_over_alpha),
    },
    "analytic_value_beta4": float(analytic_model_q),
    "fwhm_over_sigma_model_q_analytic": float(analytic_model_q),
    "fwhm_over_sigma_model_q_numeric": float(ratio_num),
    "fwhm_over_sigma_peraxis_gauss_g": float(analytic_peraxis),
    "numeric_value": float(ratio_num),
    "registered": REGISTERED,
    "rel_err_numeric_vs_registered": rel_err,
    "verdict": "PASS" if rel_err < 1e-4 else "FAIL",
    "production_roundtrip_positive": rt_prod,
    "production_roundtrip_negative_peraxis": rt_bad,
    "production_roundtrip_exact_closed_form": rt_exact,
    "gaussian_control_fwhm_over_sigma": ratio_gauss,
    "gaussian_theory_2sqrt_2ln2": 2.0 * np.sqrt(2.0 * np.log(2.0)),
    "gaussian_theory_2d_rms_convention": 2.0 * np.sqrt(2.0 * np.log(2.0)) / np.sqrt(2.0),
    "conclusions": [
        "一手定义：beta=4 时 <r^2>=alpha^2/2, FWHM=2*alpha*sqrt(2^{1/4}-1)；"
        "sigma=sqrt(<r^2>)=alpha/sqrt(2)（生产口径）⇒ FWHM/sigma = %.9f" % analytic_model_q,
        "数值积分复核（同口径）：%.9f，与登记值 1.230310 相对差 %.2e" % (ratio_num, rel_err),
        "他域口径 sigma_g=alpha/2 ⇒ FWHM/sigma_g = %.9f（与生产口径恒差 sqrt(2)=%.6f，生产禁用）"
        % (analytic_peraxis, np.sqrt(2.0)),
        "生产参数化往返（正例）：sigma=FWHM/1.230310 时 r=FWHM/2 处剖面 = %.7f（残差 %.2e，判 %s）"
        % (rt_prod["profile_value_at_r=FWHM/2"], rt_prod["half_max_residual"], rt_prod["verdict"]),
        "生产参数化往返（负例）：代入 1.7399178 时剖面 = %.7f、反解 FWHM 短 %.1f%%（判 %s）——"
        "判据能红能绿，非恒真" % (rt_bad["profile_value_at_r=FWHM/2"],
                                rt_bad["implied_FWHM_rel_err"] * 100.0, rt_bad["verdict"]),
        "负例 2（轮廓族）：同一计算作用于高斯轮廓得 %.6f = 2*sqrt(2 ln2)/sqrt(2)（同 rms 半径口径；"
        "逐轴口径为 2.3548200450）——常数随轮廓族与 sigma 口径双重变化（非普适）" % ratio_gauss,
    ],
}
(RESULTS / "exp09_moffat4_factor.json").write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
print(json.dumps(res, indent=2, ensure_ascii=False))
