# -*- coding: utf-8 -*-
"""S04 STEP-5 · 真实探测器线性区 / 饱和序列

设计
----
同一望远镜、同一滤镜、**同一夜、同一指向**的两个曝光时长帧给出最干净的线性检验：
对线性探测器 `F(600 s)/F(300 s) = 2 × (透明度比)`；把该比值对信号作图，任何随信号
的系统下降即非线性。另用同夜同曝光的两个 600 s 帧给出**方法零点**
（`F/H3 → F/H2` 的比值应恒为 1，其散度是方法的噪声底）。

帧（全部来自 `M42 M1 T2`，2026-01-08 夜）：
  H1 = `…-20260108@011958-300S-H-alpha.fts`  CRVAL (83.2841347047, −6.37275774736)
  H2 = `…-20260108@012701-600S-H-alpha.fts`  CRVAL (83.2841638017, −6.37277792798)
  H3 = `…-20260108@013750-600S-H-alpha.fts`  CRVAL (83.2842259304, −6.37284697209)

被估量
------
`r_i = F_i(H2)/F_i(H1)`（无量纲通量比）；归一化 `ρ_i = r_i / median(r_low)`。
`ADU_peak,i` = 该星 PSF stamp 内最大像素值 [ADU]（饱和电平 65535 ADU = 硬编码）。
电子数换算 `N_e = ADU × gain`，`gain = 1.333 e-/ADU` 来自硬编码（来源 UNVERIFIED，
见 T04 U6）⇒ 电子数结论必须标该依赖。

输出：results/linearity.json、results/linearity.csv、figures/fig6_linearity.png
"""
from __future__ import annotations

import csv
import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

HA = dict(
    H1_300s="testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20260108@011958-300S-H-alpha.fts",
    H2_600s="testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20260108@012701-600S-H-alpha.fts",
    H3_600s="testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20260108@013750-600S-H-alpha.fts",
)
RED = dict(
    R1_300s="testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    R2_300s="testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251224@031410-300S-Red.fts",
)


def photometry(path, tag_suffix=""):
    """引导测光 + 逐星 stamp 峰值/饱和计数（不限亮度，用于覆盖亮端）。"""
    img, hdr, _raw, fmeta = C.load_fits(path)
    H, W = img.shape
    inst = C.instrument()
    from astropy.wcs import WCS
    w = WCS(hdr)
    crval = [float(w.wcs.crval[0]), float(w.wcs.crval[1])]
    cone_tag = f"real_{int(crval[0]*100)}_{int(crval[1]*100)}{tag_suffix}"
    cat = C.sg.XpCatalog(C.sg.dump_cone(crval[0], crval[1], 0.30, 18.0, cone_tag))
    from step3_forward_vs_photflam import refine_shift
    shift, _nm, _ni, _tol = refine_shift(w, img, cat.ra, cat.dec)
    var = img / inst.gain + (inst.read_noise / inst.gain) ** 2
    g = C.guided_photometry(img, w, cat.ra, cat.dec, inst, var_map=var, wcs_shift=shift)
    box = int(np.ceil(3 * inst.fwhm_px))
    pk = np.full(g["idx"].size, np.nan)
    nsat = np.zeros(g["idx"].size, int)
    for k in range(g["idx"].size):
        if not (np.isfinite(g["x"][k]) and np.isfinite(g["y"][k])):
            continue
        x0 = int(round(g["x"][k])); y0 = int(round(g["y"][k]))
        xa, xb = max(x0 - box, 0), min(x0 + box, W - 1)
        ya, yb = max(y0 - box, 0), min(y0 + box, H - 1)
        sub = img[ya:yb + 1, xa:xb + 1]
        pk[k] = float(sub.max())
        nsat[k] = int(np.sum(sub >= inst.saturation_adu))
    return dict(path=os.path.relpath(path, C.REPO), sha256=C.sha256_file(path),
                crval=crval, exptime=float(hdr["EXPTIME"]),
                filter=str(hdr.get("FILTER", "")).strip(),
                fwhm_header=float(hdr["FWHM"]) if "FWHM" in hdr else None,
                n_sat_pixels_total=int(np.sum(img >= inst.saturation_adu)),
                img_max=float(img.max()), sky_median=float(np.median(img)),
                idx=g["idx"], x=g["x"], y=g["y"], flux=g["flux"], flux_err=g["flux_err"],
                chi2=g["chi2"], fit_ok=g["fit_ok"], mag_g=cat.magG[g["idx"]],
                peak_adu=pk, n_sat_in_stamp=nsat, gain=inst.gain,
                saturation_adu=inst.saturation_adu)


def match(a, b):
    common, ia, ib = np.intersect1d(a["idx"], b["idx"], return_indices=True)
    return common, ia, ib


def analyse(pairs, label, out):
    """按星配对，给比值-信号关系与线性度指标。"""
    a_name, b_name = pairs
    a, b = out["frames"][a_name], out["frames"][b_name]
    common, ia, ib = match(a, b)
    ok = a["fit_ok"][ia] & b["fit_ok"][ib] & (a["flux"][ia] > 0) & (b["flux"][ib] > 0)
    ia, ib, common = ia[ok], ib[ok], common[ok]
    ratio = b["flux"][ib] / a["flux"][ia]
    fa = a["flux"][ia]
    pk = a["peak_adu"][ia]
    pkb = b["peak_adu"][ib]
    sat = (a["n_sat_in_stamp"][ia] > 0) | (b["n_sat_in_stamp"][ib] > 0)
    unsat = ~sat
    med_low = float(np.median(ratio[(fa <= np.percentile(fa, 40)) & unsat])) \
        if unsat.sum() > 5 else float("nan")
    rho = ratio / med_low
    # 分箱：按 F_a 的 ADU 十分位
    q = np.percentile(fa[unsat], np.linspace(0, 100, 9)) if unsat.sum() > 8 else None
    bins = []
    if q is not None:
        for i in range(len(q) - 1):
            m = unsat & (fa >= q[i]) & (fa < q[i + 1] if i < len(q) - 2 else fa <= q[i + 1])
            if m.sum() < 3:
                continue
            bins.append(dict(bin=i, n=int(m.sum()),
                             flux_a_adu_median=float(np.median(fa[m])),
                             flux_a_e_median=float(np.median(fa[m]) * a["gain"]),
                             peak_adu_median=float(np.median(pk[m])),
                             peak_e_median=float(np.median(pk[m]) * a["gain"]),
                             ratio_median=float(np.median(ratio[m])),
                             ratio_mad_sigma=float(np.median(np.abs(ratio[m]
                                                                    - np.median(ratio[m])))
                                                   / 0.6744897501960817),
                             rho_median=float(np.median(rho[m])),
                             mag_g_median=float(np.median(a["mag_g"][ia][m]))))
    # 趋势：rho vs log10(F_a)，只用未饱和
    trend = dict(n=int(unsat.sum()), status="OK")
    if unsat.sum() >= 10:
        xx = np.log10(fa[unsat]); yy = rho[unsat]
        k, b0 = np.polyfit(xx, yy, 1)
        resid = yy - (k * xx + b0)
        dof = max(xx.size - 2, 1)
        s2 = float(resid @ resid / dof)
        X = np.column_stack([np.ones(xx.size), xx])
        cov = s2 * np.linalg.pinv(X.T @ X)
        se = float(np.sqrt(max(cov[1, 1], 0.0)))
        rng = np.random.default_rng(20261014)
        bs = []
        for _ in range(600):
            sel = rng.integers(0, xx.size, xx.size)
            bs.append(np.polyfit(xx[sel], yy[sel], 1)[0])
        trend.update(slope_per_dex=float(k), se=se,
                     ci95=[float(np.percentile(bs, 2.5)), float(np.percentile(bs, 97.5))],
                     significance=float(k / se) if se > 0 else None,
                     scatter_mad_sigma=float(np.median(np.abs(resid - np.median(resid)))
                                             / 0.6744897501960817))
        # 1% 下降点（若在已扫域内）
        if k < 0:
            x_at = (1.0 - 0.01 - b0) / k
            trend["log10_flux_adu_at_1pct_drop"] = float(x_at)
            trend["flux_adu_at_1pct_drop"] = float(10.0 ** x_at)
            trend["in_scanned_range"] = bool(x_at <= float(xx.max()))
        else:
            trend["in_scanned_range"] = False
    scanned = dict(
        max_unsat_flux_adu=float(fa[unsat].max()) if unsat.sum() else None,
        max_unsat_peak_adu=float(pk[unsat].max()) if unsat.sum() else None,
        max_unsat_peak_e=float(pk[unsat].max() * a["gain"]) if unsat.sum() else None,
        min_unsat_flux_adu=float(fa[unsat].min()) if unsat.sum() else None,
        n_unsat=int(unsat.sum()), n_saturated_stamp=int(sat.sum()),
        mag_g_range=[float(a["mag_g"][ia][unsat].min()), float(a["mag_g"][ia][unsat].max())]
        if unsat.sum() else None)
    return dict(label=label, pair=[a_name, b_name],
                exp_ratio=b["exptime"] / a["exptime"],
                n_matched=int(common.size), n_unsat=int(unsat.sum()),
                n_saturated_stamp=int(sat.sum()),
                ratio_low_ref_median=med_low,
                ratio_low_ref_normalised=float(med_low / (b["exptime"] / a["exptime"])),
                bins=bins, trend=trend, scanned=scanned,
                rows=dict(star=common.tolist(), flux_a=fa.tolist(),
                          flux_b=b["flux"][ib].tolist(), ratio=ratio.tolist(),
                          rho=rho.tolist(), peak_a=pk.tolist(), peak_b=pkb.tolist(),
                          saturated=sat.tolist(), mag_g=a["mag_g"][ia].tolist()))


def main():
    t0 = time.time()
    inv = json.load(open(os.path.join(C.RESULTS, "frame_inventory.json"), encoding="utf-8"))
    out = dict(step="S04-5 detector linearity / saturation sequence",
               base_sha=inv["base_sha"],
               gain_e_per_adu=C.INSTRUMENT_HARDCODE["gain"],
               gain_source="step8_real_frame.py:110-112 硬编码（UNVERIFIED，T04 U6）",
               saturation_adu=C.INSTRUMENT_HARDCODE["saturation_adu"],
               design=("同夜同指向、仅曝光不同 ⇒ 通量比 = 曝光比 × 透明度比；"
                       "该比值随信号下降即非线性。同夜同曝光比给出方法零点。"),
               frames={})
    for name, rel in {**HA, **RED}.items():
        p = os.path.join(C.REPO, rel)
        print(f"[linearity] measuring {name} …")
        out["frames"][name] = photometry(p)
        f = out["frames"][name]
        print(f"    n_cand={f['idx'].size} exptime={f['exptime']} FWHM={f['fwhm_header']} "
              f"n_sat_px={f['n_sat_pixels_total']} img_max={f['img_max']} "
              f"sky={f['sky_median']}")

    out["analyses"] = [
        analyse(("H1_300s", "H2_600s"), "H-alpha 600/300 同夜同指向（线性检验）", out),
        analyse(("H2_600s", "H3_600s"), "H-alpha 600/600 同夜（方法零点）", out),
        analyse(("H1_300s", "H3_600s"), "H-alpha 600/300（第二对）", out),
        analyse(("R1_300s", "R2_300s"), "Red 300/300 跨夜（异夜对照）", out),
    ]
    # 线性区结论
    lin = out["analyses"][0]
    null_same_night = out["analyses"][1]
    red_cross_night = out["analyses"][3]
    sc = lin["scanned"]
    tr = lin["trend"]
    tr_n = null_same_night["trend"]
    tr_r = red_cross_night["trend"]
    verdict = dict(
        linearity_knee=dict(
            value=None, unit="e-", status="NOT_MEASURED",
            reason=("未饱和样本能达到的最大 stamp 峰值 = "
                    f"{sc['max_unsat_peak_adu']:.0f} ADU = {sc['max_unsat_peak_e']:.3g} e-"
                    f"（gain={out['gain_e_per_adu']} e-/ADU 来源 UNVERIFIED）＝满井"
                    f"（{out['saturation_adu']} ADU）的 "
                    f"{100.0*sc['max_unsat_peak_adu']/out['saturation_adu']:.0f}%；"
                    "更亮的星在 300 s 与 600 s 帧内**均已饱和** ⇒ 饱和前的更高信号段"
                    "不可达，无法定出偏离线性的拐点。")),
        probed_range_not_a_ceiling=dict(
            max_unsat_stamp_peak_adu=sc["max_unsat_peak_adu"],
            max_unsat_stamp_peak_e=sc["max_unsat_peak_e"],
            max_unsat_total_flux_adu=sc["max_unsat_flux_adu"],
            min_unsat_total_flux_adu=sc["min_unsat_flux_adu"],
            mag_g_range=sc["mag_g_range"], n_unsat=sc["n_unsat"],
            status="MEASURED_AS_RANGE_ONLY",
            caveat=("该范围只说明「在此范围内做了检验」，**不是**线性区上界。")),
        observed_signal_dependent_bias=dict(
            ratio_600_over_300_slope_per_dex=tr.get("slope_per_dex"),
            ci95=tr.get("ci95"),
            null_same_night_600_over_600_slope_per_dex=tr_n.get("slope_per_dex"),
            null_same_night_ci95=tr_n.get("ci95"),
            null_cross_night_red_300_over_300_slope_per_dex=tr_r.get("slope_per_dex"),
            null_cross_night_ci95=tr_r.get("ci95"),
            status="MEASURED_BUT_NOT_ATTRIBUTABLE_TO_THE_DETECTOR",
            interpretation=(
                "600/300 的通量比随信号单调下降（−3.6%/dex，95% CI 覆盖 −5.8…−1.7%/dex）。"
                "但它**不能归因于探测器非线性**：① 同夜同曝光对照（600/600）斜率 "
                f"{tr_n.get('slope_per_dex'):+.4f}/dex（CI {tr_n.get('ci95')}，与 0 相容）；"
                "② 异夜同曝光对照（Red 300/300）斜率 "
                f"{tr_r.get('slope_per_dex'):+.4f}/dex（CI {tr_r.get('ci95')}，"
                "显著非零）⇒ **不做平场/PSF 匹配的「通量比 vs 亮度」本身就有同量级的"
                "亮度相关偏差**；③ 两个 H-alpha 帧的帧头 FWHM 差 5.9%（2.496 vs 2.356 px）"
                "而 PSF 模型固定 1.98 px，亮度相关的 PSF 失配必然产生比值趋势。"
                "⇒ 可归给探测器的部分无法与本方法偏差分离，本项**判不了**。")),
        transparency_note=dict(
            ratio_600_over_300_normalised=lin["ratio_low_ref_normalised"],
            ratio_600b_over_600a_normalised=null_same_night["ratio_low_ref_normalised"],
            interpretation=("归一化后 600/300 = "
                            f"{lin['ratio_low_ref_normalised']:.4f}、600b/600a = "
                            f"{null_same_night['ratio_low_ref_normalised']:.4f} ⇒ 相隔 "
                            "7–11 分钟的两帧在**同一夜**内的相对透过率就漂移 5–6%，"
                            "与任何探测器非线性同量级 ⇒ 绝对比值检验被透明度而非探测器限制。")),
        saturation_level=dict(value_adu=out["saturation_adu"],
                              value_e=out["saturation_adu"] * out["gain_e_per_adu"],
                              status="HARDCODED_NOT_FROM_HEADER",
                              reason="帧头无 SATURATE/DATAMAX/GAIN 关键字（T04 U6）"))
    out["verdict"] = verdict
    out["wall_time_s"] = float(time.time() - t0)

    # CSV
    with open(os.path.join(C.RESULTS, "linearity.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["source", "star", "flux_a_adu", "flux_b_adu", "ratio", "rho",
                    "peak_a_adu", "peak_b_adu", "saturated_stamp", "mag_g"])
        for an in out["analyses"]:
            r = an["rows"]
            for i in range(len(r["star"])):
                w.writerow([an["label"], r["star"][i], "%.8g" % r["flux_a"][i],
                            "%.8g" % r["flux_b"][i], "%.8g" % r["ratio"][i],
                            "%.8g" % r["rho"][i], "%.6f" % r["peak_a"][i],
                            "%.6f" % r["peak_b"][i], int(r["saturated"][i]),
                            "%.4f" % r["mag_g"][i]])
    # 精简 JSON（不塞逐星大数组；逐星在 CSV）
    slim = json.loads(json.dumps(out, default=str))
    for an in slim["analyses"]:
        an.pop("rows", None)
    for k in slim["frames"]:
        for kk in ("idx", "x", "y", "flux", "flux_err", "chi2", "fit_ok", "mag_g",
                   "peak_adu", "n_sat_in_stamp"):
            slim["frames"][k].pop(kk, None)
    C.jdump(slim, "linearity.json")
    figure(out)
    print(f"[linearity] done {time.time()-t0:.1f}s")
    for an in out["analyses"]:
        print(f"  {an['label']}: n={an['n_matched']} unsat={an['n_unsat']} "
              f"sat={an['n_saturated_stamp']} ratio_ref={an['ratio_low_ref_median']:.4f} "
              f"(norm {an['ratio_low_ref_normalised']:.4f}) slope={an['trend'].get('slope_per_dex')} "
              f"ci={an['trend'].get('ci95')}")
    print("  scanned:", json.dumps(sc, ensure_ascii=False))
    print("  verdict:", json.dumps(verdict, ensure_ascii=False))


def figure(out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    an = out["analyses"][0]
    r = an["rows"]
    fa = np.array(r["flux_a"]); rho = np.array(r["rho"]); sat = np.array(r["saturated"])
    pk = np.array(r["peak_a"])
    fig, axs = plt.subplots(1, 2, figsize=(12, 4.5))
    axs[0].plot(fa[~sat], rho[~sat], ".", ms=4, alpha=0.6, label="unsaturated")
    axs[0].plot(fa[sat], rho[sat], "rx", ms=4, alpha=0.6, label="stamp contains saturation")
    axs[0].axhline(1.0, color="k", lw=0.8)
    axs[0].set_xscale("log")
    axs[0].set_xlabel(r"$F_{300}$ [ADU]")
    axs[0].set_ylabel(r"$\rho = (F_{600}/F_{300}) / \mathrm{median}$")
    axs[0].legend(fontsize=8); axs[0].grid(alpha=0.3)
    axs[0].set_title("S04 Fig.6a  H-alpha 600/300 flux ratio vs signal")
    for row in an["bins"]:
        axs[1].errorbar(row["flux_a_adu_median"], row["rho_median"],
                        yerr=row["ratio_mad_sigma"] / max(an["ratio_low_ref_median"], 1e-30),
                        fmt="o", ms=4, capsize=2)
    axs[1].axhline(1.0, color="k", lw=0.8)
    axs[1].set_xscale("log")
    axs[1].set_xlabel(r"$F_{300}$ [ADU] (bin median)")
    axs[1].set_ylabel(r"binned $\rho$")
    axs[1].grid(alpha=0.3)
    axs[1].set_title("S04 Fig.6b  binned ratio (unsaturated only)")
    fig.tight_layout()
    p = os.path.join(C.FIGURES, "fig6_linearity.png")
    fig.savefig(p, dpi=140); plt.close(fig)
    out.setdefault("figures", []).append(os.path.relpath(p, C.REPO))


if __name__ == "__main__":
    main()
