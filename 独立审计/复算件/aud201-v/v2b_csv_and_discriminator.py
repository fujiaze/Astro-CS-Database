# -*- coding: utf-8 -*-
"""
V2 补充复算：
 (1) 逐字复算成稿的"真实星样本色轴互差 0.0107 mag"（跟踪 CSV，1050 颗）；
 (2) 能量谱 vs 光子谱两支单位约定的**可判别量**到底是什么量级（这才是排除备择假设的杠杆）；
 (3) 生产相机通带（Baader R x QE）下 λ 色项的真实量级（成稿用的是官方 G 通带）。
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


def load_gaia(band_col):
    p = os.path.join(ROOT, r"lib\algorithms\photometry\cpp\test\gate4_dr3sp_gaiaxpy"
                           r"\GaiaEDR3_passband.dat")
    arr = np.loadtxt(p)
    wl, tr = arr[:, 0].copy(), arr[:, band_col].copy()
    tr = np.where(tr > 10.0, 0.0, tr)
    keep = (wl >= 330) & (wl <= 1060)
    return wl[keep], tr[keep]


def planck(T, wl):
    h, c, k = 6.62607015e-34, 2.99792458e8, 1.380649e-23
    wm = wl * 1e-9
    return ((2 * h * c ** 2 / wm ** 5) / np.expm1(h * c / (wm * k * T))) * 1e-9


def part1_csv_colorbins():
    p = os.path.join(ROOT, r"lib\algorithms\photometry\cpp\test\gate4_dr3sp_gaiaxpy"
                         r"\evidence\gate4_compare_per_star_1050.csv")
    lines = io.open(p, encoding="utf-8").read().splitlines()
    hdr = lines[0].split(",")
    idx = {k: hdr.index(k) for k in ("astrocs_mag_G", "gaiagx_mag_G", "gaiagx_color_BP_RP")}
    rows = []
    for ln in lines[1:]:
        f = ln.split(",")
        try:
            rows.append((float(f[idx["astrocs_mag_G"]]), float(f[idx["gaiagx_mag_G"]]),
                         float(f[idx["gaiagx_color_BP_RP"]])))
        except ValueError:
            pass
    a = np.array(rows)
    d = a[:, 0] - a[:, 1]
    c = a[:, 2]
    med = float(np.median(d))
    mad = float(1.4826 * np.median(np.abs(d - med)))
    # 6 个等频色箱（成稿口径）
    qs = np.percentile(c, np.linspace(0, 100, 7))
    qs[-1] += 1e-9
    binmed = []
    for i in range(6):
        m = (c >= qs[i]) & (c < qs[i + 1])
        binmed.append([float(np.median(d[m])), int(m.sum()), float(np.median(c[m]))])
    spread = max(x[0] for x in binmed) - min(x[0] for x in binmed)
    # 等宽 6 箱（另一种口径，看结论是否依赖分箱方式）
    edges = np.linspace(c.min(), c.max(), 7)
    edges[-1] += 1e-9
    wide = []
    for i in range(6):
        m = (c >= edges[i]) & (c < edges[i + 1])
        if m.sum() > 5:
            wide.append(float(np.median(d[m])))
    spread_w = max(wide) - min(wide)
    # 相关性：d 与颜色
    r = float(np.corrcoef(d, c)[0, 1])
    slope = float(np.polyfit(c, d, 1)[0])
    return {"n": int(len(a)), "median_diff_mag": med, "mad_sigma_mag": mad,
            "equalfreq_6bin_medians": binmed, "equalfreq_p2p_mag": spread,
            "equalwidth_6bin_p2p_mag": spread_w,
            "corr_diff_vs_BPRP": r, "slope_mag_per_BPRP": slope,
            "color_p5_p95": [float(np.percentile(c, 5)), float(np.percentile(c, 95))]}


def part2_discriminator():
    """能量谱 vs 光子谱：可判别量 = -2.5log10<lambda>，是**绝对偏移**（7 mag 量级），
    而色项只是它的跨星变化部分（0.0x-0.27 mag）。"""
    res = {}
    for name, col in (("Gaia G", 1), ("Gaia BP", 3), ("Gaia RP", 5)):
        wl, tr = load_gaia(col)
        R = np.interp(XPSD_WL, wl, tr, left=0.0, right=0.0)
        lam_eff = {}
        for T in (3000.0, 5772.0, 9000.0, 12000.0):
            F = planck(T, XPSD_WL)
            num = np.trapezoid(F * R * XPSD_WL, XPSD_WL)
            den = np.trapezoid(F * R, XPSD_WL)
            lam_eff[f"{T:g}K"] = float(num / den)
        res[name] = {
            "lambda_eff_nm": lam_eff,
            "abs_offset_mag_energy_vs_photon": {k: float(-2.5 * np.log10(v))
                                                for k, v in lam_eff.items()},
        }
    fj = json.load(io.open(os.path.join(ROOT, r"eng\packaging\config\filters.json"),
                           encoding="utf-8"))
    b = fj["filters"]["Baader R"]
    fw, ft = np.asarray(b["wavelength_nm"], float), np.asarray(b["value"], float)
    qj = json.load(io.open(os.path.join(ROOT, r"lib\algorithms\photometry\data"
                                        r"\response_curves\qe_curves.json"), encoding="utf-8"))
    q = qj["GSENSE4040BSI"]
    qw, qv = np.asarray(q["wavelength_nm"], float), np.asarray(q["value"], float)
    R = np.interp(XPSD_WL, fw, ft, left=0.0, right=0.0) * np.interp(XPSD_WL, qw, qv,
                                                                    left=0.0, right=0.0)
    lam_eff = {}
    for T in (3000.0, 5772.0, 9000.0, 12000.0):
        F = planck(T, XPSD_WL)
        lam_eff[f"{T:g}K"] = float(np.trapezoid(F * R * XPSD_WL, XPSD_WL) /
                                   np.trapezoid(F * R, XPSD_WL))
    res["Baader R x GSENSE4040BSI"] = {
        "lambda_eff_nm": lam_eff,
        "abs_offset_mag_energy_vs_photon": {k: float(-2.5 * np.log10(v))
                                            for k, v in lam_eff.items()},
        "colour_term_p2p_mag": float(max(-2.5 * np.log10(v) for v in lam_eff.values()) -
                                     min(-2.5 * np.log10(v) for v in lam_eff.values())),
    }
    return res


def main():
    out = {"part1_csv": part1_csv_colorbins(), "part2": part2_discriminator()}
    # 裕度的正确算法：备择假设在**同一口径**下应产生的信号 / 实测
    p1 = out["part1_csv"]
    out["margin_recheck"] = {
        "draft_claim": "0.2708 / 0.01070 = 25.3x",
        "my_equalfreq_p2p": p1["equalfreq_p2p_mag"],
        "my_equalwidth_p2p": p1["equalwidth_6bin_p2p_mag"],
        "measured_slope_mag_per_BPRP": p1["slope_mag_per_BPRP"],
        "predicted_from_production_band_slope_x_sample_span": 0.0061 *
            (p1["color_p5_p95"][1] - p1["color_p5_p95"][0]),
        "note": "生产相机通带下 λ 色项斜率 6.1e-3 mag/(BP-RP)，乘样本色跨 "
                f"{p1['color_p5_p95'][1]-p1['color_p5_p95'][0]:.2f} mag "
                "⇒ 备择假设在该样本上最多产生 ~0.025 mag 的色轴结构，"
                "与成稿实测 0.0107 同量级（比值 ~2x），不是 25x；"
                "且该 CSV 两臂同源（均取 GaiaXPy calibrate 的绝对浮点谱），"
                "不经 XPSD 容器解码 ⇒ 对该备择假设零灵敏度。",
    }
    txt = json.dumps(out, indent=2, ensure_ascii=False)
    io.open(os.path.join(OUT, "v2b_csv_and_discriminator.json"), "w", encoding="utf-8").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
