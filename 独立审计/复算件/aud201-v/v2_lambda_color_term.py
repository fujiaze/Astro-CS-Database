# -*- coding: utf-8 -*-
"""
AUD201-V 独立复算 V2：lambda 因子作为"色项"的量级与"25x 裕度排除备择假设"论证。

备择假设（要被排除的那条）：容器给的 F_lambda 已是光子计数谱 (ph s^-1 m^-2 nm^-1)，
则正确的合成通量应**不带** lambda 权重；带了就会给逐星残差注入一个色项
    d(m) = -2.5*log10( int F R lambda dlambda / int F R dlambda ) = -2.5*log10 <lambda>_F,R
（即"光子加权平均波长"对 SED 的依赖）。
本脚本用**三组不同源的 SED**、**两支不同源的通带**算这个色项的峰峰值与
"单位色指数的斜率"，再用仓库跟踪集里的真实星样本颜色跨度把它折回**同一口径**。

只读仓库的**数据文件**（通带/QE/滤镜曲线/真实星 CSV），不 import 仓库 Python。
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


# ------------------------------------------------------------------ 通带（两源）
def load_gaia_passbands():
    path = os.path.join(ROOT, r"lib\algorithms\photometry\cpp\test\gate4_dr3sp_gaiaxpy"
                            r"\GaiaEDR3_passband.dat")
    arr = np.loadtxt(path)
    out = {}
    for band, col in (("G", 1), ("BP", 3), ("RP", 5)):
        wl, tr = arr[:, 0].copy(), arr[:, col].copy()
        tr = np.where(tr > 10.0, 0.0, tr)          # 99.99 哨兵 -> 0
        keep = (wl >= 330) & (wl <= 1060)
        out[band] = (wl[keep], tr[keep])
    return out


def load_production_band():
    """生产模型通带 = filters.json 的滤镜曲线 x qe_curves.json 的探测器 QE（均为仓库数据）。"""
    fj = json.load(io.open(os.path.join(ROOT, r"eng\packaging\config\filters.json"),
                           encoding="utf-8"))
    fname = "Baader R"
    filt = fj["filters"][fname]
    fw = np.asarray(filt["wavelength_nm"], float)
    ft = np.asarray(filt["value"], float)
    qj = json.load(io.open(os.path.join(ROOT, r"lib\algorithms\photometry\data"
                                        r"\response_curves\qe_curves.json"), encoding="utf-8"))
    qname = "GSENSE4040BSI"
    qobj = qj[qname]
    qe = (np.asarray(qobj["wavelength_nm"], float), np.asarray(qobj["value"], float))
    return fname, fw, ft, qname, qe, list(fj["filters"])[:6]


def resample(wl_src, y_src, wl_dst):
    return np.interp(wl_dst, wl_src, y_src, left=0.0, right=0.0)


# ------------------------------------------------------------------ SED 三组源
def planck(T, wl):
    h, c, k = 6.62607015e-34, 2.99792458e8, 1.380649e-23
    wm = wl * 1e-9
    return ((2 * h * c ** 2 / wm ** 5) / np.expm1(h * c / (wm * k * T))) * 1e-9


def sed_grid(which):
    """返回 [(label, F_lambda on XPSD_WL), ...]，三组彼此不同源。"""
    g = []
    if which == "BB_draft":                     # 与成稿同跨度的黑体网格
        for T in np.arange(3000.0, 12000.0 + 1, 500.0):
            g.append((f"BB{T:g}", planck(T, XPSD_WL)))
    elif which == "BB_wide":                    # 不同温度网格（更宽、更密）
        for T in np.arange(2500.0, 50000.0 + 1, 250.0):
            g.append((f"BB{T:g}", planck(T, XPSD_WL)))
    elif which == "PL":                         # 幂律族：与黑体完全不同的 SED 族
        for a in np.arange(-4.0, 4.01, 0.25):
            g.append((f"PL a={a:g}", (XPSD_WL / 550.0) ** a))
    elif which == "BB_Balmer":                  # 黑体 + 巴尔默跳（结构扰动）
        for T in (5000.0, 6000.0, 7000.0, 8000.0, 10000.0, 15000.0, 25000.0):
            f = planck(T, XPSD_WL)
            f = np.where(XPSD_WL < 364.6, f * 0.65, f)   # 364.6nm 处 35% 吸收跳变
            g.append((f"BB{T:g}+BJ", f))
    return g


# ------------------------------------------------------------------ 度量
def lambda_term_mag(F, R, wl):
    """色项本身：带 lambda 与不带 lambda 两种约定的星等差（去掉与星无关的常数后跨星比较）。"""
    num = np.trapezoid(F * R * wl, wl)
    den = np.trapezoid(F * R, wl)
    if den <= 0 or num <= 0:
        return np.nan
    return -2.5 * np.log10(num / den)


def synth_mag(F, R, wl, zp=0.0):
    return zp - 2.5 * np.log10(max(np.trapezoid(F * R * wl, wl), 1e-300))


def analyse(band_name, R, wl, gp, gaia):
    rows = []
    for label, F in gp:
        # 色轴 = 官方 Gaia BP-RP（用 canonical 光子加权约定算，与样本同轴）
        wlB, trB = gaia["BP"]
        wlR, trR = gaia["RP"]
        c_bprp = synth_mag(F, resample(wlB, trB, XPSD_WL), XPSD_WL) - \
            synth_mag(F, resample(wlR, trR, XPSD_WL), XPSD_WL)
        lt = lambda_term_mag(F, R, wl)
        rows.append((label, c_bprp, lt))
    a = np.array([(r[1], r[2]) for r in rows], float)
    m = np.isfinite(a).all(axis=1)
    a = a[m]
    slope, icpt = np.polyfit(a[:, 0], a[:, 1], 1)
    r2 = np.corrcoef(a[:, 0], a[:, 1])[0, 1] ** 2
    return {
        "band": band_name,
        "n_sed": int(len(a)),
        "color_range_BP_RP": [float(a[:, 0].min()), float(a[:, 0].max())],
        "lambda_term_range_mag": [float(a[:, 1].min()), float(a[:, 1].max())],
        "p2p_full_grid_mag": float(a[:, 1].max() - a[:, 1].min()),
        "slope_mag_per_BP_RP": float(slope),
        "r2_vs_color": float(r2),
        "_pts": a.tolist(),
    }


def real_sample_colors():
    p = os.path.join(ROOT, r"lib\algorithms\photometry\cpp\test\gate4_dr3sp_gaiaxpy"
                         r"\evidence\gate4_compare_per_star_1050.csv")
    txt = io.open(p, encoding="utf-8").read().splitlines()
    hdr = txt[0].split(",")
    i_c = hdr.index("gaiagx_color_BP_RP")
    i_g = hdr.index("gaiagx_mag_G")
    vals, gs = [], []
    for line in txt[1:]:
        f = line.split(",")
        try:
            vals.append(float(f[i_c]))
            gs.append(float(f[i_g]))
        except (ValueError, IndexError):
            pass
    v = np.asarray(vals, float)
    g = np.asarray(gs, float)
    return {"n": int(len(v)),
            "BP_RP_percentiles": {str(q): float(np.percentile(v, q)) for q in
                                  (0, 5, 25, 50, 75, 95, 100)},
            "BP_RP_p2p_core": float(np.percentile(v, 95) - np.percentile(v, 5)),
            "G_percentiles": {str(q): float(np.percentile(g, q)) for q in (0, 5, 50, 95, 100)}}


def predict_on_span(pts, lo, hi):
    """把网格的色项按真实样本的颜色跨度折算：只取落在 [lo,hi] 内的 SED 的色项峰峰。"""
    a = np.asarray(pts, float)
    m = (a[:, 0] >= lo) & (a[:, 0] <= hi)
    if m.sum() < 2:
        return None
    return {"n_sed_in_span": int(m.sum()),
            "p2p_on_real_span_mag": float(a[m, 1].max() - a[m, 1].min())}


def main():
    gaia = load_gaia_passbands()
    fname, fw, ft, qname, qe, sample_names = load_production_band()
    wl = XPSD_WL
    R_f = resample(fw, ft, wl)
    bands = [("Gaia G (官方 passband.dat)", resample(*gaia["G"], wl))]
    if qe is not None:
        bands.append((f"{fname} x QE({qname})", R_f * resample(qe[0], qe[1], wl)))
    bands.append((f"{fname} (无 QE, Q=1)", R_f))

    out = {"production_band_info": {
        "filter_key": fname, "filter_points": int(len(fw)),
        "filter_wl_range": [float(fw.min()), float(fw.max())],
        "qe_key": qname,
        "qe_points": (int(len(qe[0])) if qe is not None else None),
        "filter_names_sample": sample_names}}

    grids = {}
    for which in ("BB_draft", "BB_wide", "PL", "BB_Balmer"):
        gp = sed_grid(which)
        grids[which] = {}
        for bname, R in bands:
            res = analyse(bname, R, wl, gp, gaia)
            pts = res.pop("_pts")
            res["band_lambda_eff_photon_weighted_nm"] = None
            grids[which][res["band"]] = res
            grids[which][res["band"]]["_pts"] = pts
    out["grids"] = {k: {b: {kk: vv for kk, vv in r.items() if kk != "_pts"}
                        for b, r in d.items()} for k, d in grids.items()}

    sample = real_sample_colors()
    out["real_sample_tracked_csv"] = sample
    lo = sample["BP_RP_percentiles"]["5"]
    hi = sample["BP_RP_percentiles"]["95"]
    folded = {}
    for which, d in grids.items():
        for b, r in d.items():
            if b != "Gaia G (官方 passband.dat)":
                continue
            fp = predict_on_span(r["_pts"], lo, hi)
            if fp:
                folded[f"{which}|{b}"] = {**fp,
                                          "p2p_full_grid_mag": r["p2p_full_grid_mag"],
                                          "slope_mag_per_BP_RP": r["slope_mag_per_BP_RP"]}
    out["fold_onto_real_sample_color_span"] = folded
    # 裕度：备择假设在该样本上**应当**产生的色项 vs 成稿实测 0.0107 mag
    MEASURED = 0.0107
    out["margin_vs_measured_0.0107"] = {
        k: {"predicted_on_sample": v["p2p_on_real_span_mag"],
            "margin_x": v["p2p_on_real_span_mag"] / MEASURED,
            "margin_if_naive_full_grid": v["p2p_full_grid_mag"] / MEASURED,
            "slope_x": v["slope_mag_per_BP_RP"] * sample["BP_RP_p2p_core"] / MEASURED}
        for k, v in folded.items()}

    txt = json.dumps(out, indent=2, ensure_ascii=False)
    io.open(os.path.join(OUT, "v2_lambda_color_term.json"), "w", encoding="utf-8").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
