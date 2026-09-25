# -*- coding: utf-8 -*-
"""
V3 独立复算：现行"按星等分箱的 median(m_syn-G)"锚对**色项型**缺陷有没有判别力。

构造两组样本（同边缘分布、同缺陷、同判据口径）：
  A 相关组：BP-RP 与 G 有主序式相关（真实球/盘星的 CMD 性质）
  B 解耦组：把 A 的颜色与星等逐星打乱配对 —— 边缘分布完全不变，corr(c,G)=0
缺陷：b(c) = -2.5*log10<lambda>_F,R(c)，用我自己算的黑体->(BP-RP, λ项) 曲线插值
      （两套：官方 G 通带 / 生产相机通带）
读出比例 = 分箱后各箱 median 的峰峰 / 缺陷本身的峰峰
"""
import io
import json
import os
import sys

import numpy as np

sys.stdout.reconfigure(encoding="utf-8")
ROOT = r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.dirname(os.path.abspath(__file__))
XPSD_WL = np.arange(336.0, 1020.0 + 1e-9, 2.0)


def planck(T, wl):
    h, c, k = 6.62607015e-34, 2.99792458e8, 1.380649e-23
    wm = wl * 1e-9
    return ((2 * h * c ** 2 / wm ** 5) / np.expm1(h * c / (wm * k * T))) * 1e-9


def resample_band(path, col, lo=330.0, hi=1060.0):
    arr = np.loadtxt(path)
    wl, tr = arr[:, 0].copy(), arr[:, col].copy()
    tr = np.where(tr > 10.0, 0.0, tr)
    keep = (wl >= lo) & (wl <= hi)
    return wl[keep], tr[keep]


def build_defect_curves():
    """返回 (color_axis, lam_term_Gband, lam_term_prodband)，全部由我自己算。"""
    pb = os.path.join(ROOT, r"lib\algorithms\photometry\cpp\test\gate4_dr3sp_gaiaxpy"
                            r"\GaiaEDR3_passband.dat")
    wG, tG = resample_band(pb, 1)
    wB, tB = resample_band(pb, 3)
    wR, tR = resample_band(pb, 5)
    RG = np.interp(XPSD_WL, wG, tG, left=0, right=0)
    RB = np.interp(XPSD_WL, wB, tB, left=0, right=0)
    RR = np.interp(XPSD_WL, wR, tR, left=0, right=0)
    fj = json.load(io.open(os.path.join(ROOT, r"eng\packaging\config\filters.json"),
                           encoding="utf-8"))
    b = fj["filters"]["Baader R"]
    qj = json.load(io.open(os.path.join(ROOT, r"lib\algorithms\photometry\data"
                                        r"\response_curves\qe_curves.json"), encoding="utf-8"))
    q = qj["GSENSE4040BSI"]
    RP_ = (np.interp(XPSD_WL, np.asarray(b["wavelength_nm"], float),
                     np.asarray(b["value"], float), left=0, right=0) *
           np.interp(XPSD_WL, np.asarray(q["wavelength_nm"], float),
                     np.asarray(q["value"], float), left=0, right=0))

    def m25(F, R):
        return -2.5 * np.log10(max(np.trapezoid(F * R * XPSD_WL, XPSD_WL), 1e-300))

    cols, ltG, ltP = [], [], []
    # 更密的黑体网格 + 巴尔默跳扰动，给缺陷曲线一个物理形状
    Ts = list(np.arange(2600.0, 20000.0, 200.0)) + list(np.arange(20000.0, 40000.0, 2000.0))
    for T in Ts:
        F = planck(T, XPSD_WL)
        cols.append(m25(F, RB) - m25(F, RR))
        ltG.append(-2.5 * np.log10(np.trapezoid(F * RG * XPSD_WL, XPSD_WL) /
                                   np.trapezoid(F * RG, XPSD_WL)))
        ltP.append(-2.5 * np.log10(np.trapezoid(F * RP_ * XPSD_WL, XPSD_WL) /
                                   np.trapezoid(F * RP_, XPSD_WL)))
    for T in (5000.0, 7000.0, 9000.0, 12000.0, 18000.0):   # 带巴尔默跳的 A/F 型
        F = planck(T, XPSD_WL)
        F = np.where(XPSD_WL < 364.6, F * 0.65, F)
        cols.append(m25(F, RB) - m25(F, RR))
        ltG.append(-2.5 * np.log10(np.trapezoid(F * RG * XPSD_WL, XPSD_WL) /
                                   np.trapezoid(F * RG, XPSD_WL)))
        ltP.append(-2.5 * np.log10(np.trapezoid(F * RP_ * XPSD_WL, XPSD_WL) /
                                   np.trapezoid(F * RP_, XPSD_WL)))
    a = np.array([cols, ltG, ltP], float)
    a = a[:, np.argsort(a[0])]
    return a


def bin_medians(x, y, nbin=6, equal_pop=True):
    if equal_pop:
        qs = np.percentile(x, np.linspace(0, 100, nbin + 1))
        qs[-1] += 1e-9
    else:
        qs = np.linspace(x.min(), x.max(), nbin + 1)
        qs[-1] += 1e-9
    meds = []
    for i in range(nbin):
        m = (x >= qs[i]) & (x < qs[i + 1])
        if m.sum() >= 3:
            meds.append(float(np.median(y[m])))
    return meds


def run(n=3000, noise=0.0, seed=1234):
    a = build_defect_curves()
    cax, ltG, ltP = a
    rng = np.random.default_rng(seed)
    # 颜色：类现场主序分布（0.2..2.6，红端多）
    color = 0.2 + 2.4 * np.sqrt(rng.random(n))
    # 相关组：主序式 G = 2.5 + 3.4*color + 散射
    mag = 2.5 + 3.4 * color + rng.normal(0, 0.6, n) + 10.0
    # 解耦组：同样的边缘分布，逐星打乱配对
    color_dec = rng.permutation(color)
    mag_dec = rng.permutation(mag)

    out = {"defect_curve_endpoints": {
        "G_band_lam_term_over_color_axis_mag": float(ltG.max() - ltG.min()),
        "prod_band_lam_term_over_color_axis_mag": float(ltP.max() - ltP.min()),
        "color_axis_range": [float(cax.min()), float(cax.max())]}}
    for tag, defect in (("Gband_defect", ltG), ("prodband_defect", ltP)):
        b_corr = np.interp(color, cax, defect)
        b_dec = np.interp(color_dec, cax, defect)
        for arm, col_v, mag_v, b in (("correlated", color, mag, b_corr),
                                     ("decoupled", color_dec, mag_dec, b_dec)):
            y = b + (rng.normal(0, noise, n) if noise else 0.0)
            p2p_defect = float(b.max() - b.min())
            mb = bin_medians(mag_v, y)             # 现行判据：按星等分箱
            cb = bin_medians(col_v, y)             # 正确轴：按色轴分箱
            read_m = float(max(mb) - min(mb))
            read_c = float(max(cb) - min(cb))
            out.setdefault(tag, {})[arm] = {
                "corr_color_vs_mag": float(np.corrcoef(col_v, mag_v)[0, 1]),
                "defect_p2p_mag": p2p_defect,
                "magbin_medians": [round(x, 5) for x in mb],
                "magbin_readout_p2p_mag": read_m,
                "magbin_readout_fraction": read_m / p2p_defect,
                "colorbin_medians": [round(x, 5) for x in cb],
                "colorbin_readout_p2p_mag": read_c,
                "colorbin_readout_fraction": read_c / p2p_defect,
            }
    return out


def main():
    res = {
        "noiseless": run(noise=0.0),
        "noise_0.03mag": run(noise=0.03),
        "n_200": run(n=200, noise=0.0),
        "cmd_slope_1.0_weaker_corr": None,
    }
    # 相关强度扫描：判据读出比例是否只由 CMD 相关强度决定
    a = build_defect_curves()
    cax, ltG, ltP = a
    rng = np.random.default_rng(99)
    scan = []
    for slope in (0.0, 0.5, 1.5, 3.4, 6.0):
        color = 0.2 + 2.4 * np.sqrt(rng.random(3000))
        mag = 10.0 + slope * color + rng.normal(0, 0.6, 3000)
        b = np.interp(color, cax, ltG)
        mb = bin_medians(mag, b)
        scan.append({"cmd_slope": slope,
                     "corr": float(np.corrcoef(color, mag)[0, 1]),
                     "magbin_fraction": (max(mb) - min(mb)) / float(b.max() - b.min())})
    res["cmd_slope_1.0_weaker_corr"] = scan
    txt = json.dumps(res, indent=2, ensure_ascii=False)
    io.open(os.path.join(OUT, "v3_binning_axis.json"), "w", encoding="utf-8").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
