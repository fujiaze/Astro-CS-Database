#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7 图表生成（纯 matplotlib，读 run/RELEASE-02/paper/data/P7/*.json）。

产出（落 run/RELEASE-02/paper/data/P7/figs/）：
    fig1_ptc.png           光子传递曲线：Var vs Mean，斜率 = 1/g，截距 = RN²/g² + 1/12
    fig2_quantization.png  量化噪声 vs 量化前 sigma（1/12 连续近似的适用域）
    fig3_sky_scan.png      天光扫描：sigma↑ / SNR↓（物理）vs 平坦（纯加性负例）
    fig4_injection.png     注入-回收：Δmag vs SNR，含零噪声负例
    fig5_sp0.png           SP-0 散差：共模/纯缩放（恒 1）vs 三波段（>1）
    fig6_m16_scenes.png    三个 M16 场景的合成帧（目视检查用）
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve()
ROOT = HERE.parents[5]                          # 仓库根
P7 = ROOT / "run" / "RELEASE-02" / "paper" / "data" / "P7"
FIG = P7 / "figs"
FIG.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT / "reverse_verify" / "experiments" / "p7_noise"))

plt.rcParams.update({"figure.dpi": 130, "savefig.dpi": 130, "font.size": 9,
                     "axes.grid": True, "grid.alpha": 0.3})


def load(name):
    p = P7 / name
    return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None


def fig1():
    d = load("noise_selftest_full.json")
    if not d:
        return
    pt = d["tests"]["A3"]["points"]
    x = np.array([p["mean_net_adu"] for p in pt])
    y = np.array([p["var_adu2"] for p in pt])
    fit = d["tests"]["A3"]["fit"]
    xs = np.linspace(0, x.max() * 1.05, 100)
    fig, ax = plt.subplots(figsize=(5.4, 3.8))
    ax.plot(x, y, "o", ms=5, label="measured (paired difference)")
    ax.plot(xs, fit["slope_meas"] * xs + fit["intercept_meas"], "-",
            label="fit: slope=%.5f (1/g=%.5f)" % (fit["slope_meas"], fit["slope_theory_1_over_g"]))
    ax.plot(xs, fit["slope_theory_1_over_g"] * xs + fit["intercept_theory"], "--",
            label="theory: 1/g=%.5f, b=%.5f" % (fit["slope_theory_1_over_g"], fit["intercept_theory"]))
    ax.set_xlabel("mean net signal [ADU]"); ax.set_ylabel("variance [ADU$^2$]")
    ax.set_title("P7-A3 photon transfer curve (Janesick 2001)\n"
                 r"recovered $g$=%.4f e-/ADU (input 1.5); $R^2$=%.6f"
                 % (d["tests"]["A3"]["implied_gain_e_per_adu"], fit["r2"]))
    ax.legend(fontsize=7); fig.tight_layout(); fig.savefig(FIG / "fig1_ptc.png"); plt.close(fig)


def fig2():
    d = load("noise_selftest_full.json")
    if not d:
        return
    a1 = d["tests"]["A1"]
    fig, axes = plt.subplots(1, 2, figsize=(8.6, 3.4))
    ax = axes[0]
    vals = [a1["method_a_residual"]["var_meas"], a1["method_a_residual"]["var_theory"],
            a1["method_b_direct_WRONG"]["var_meas"]]
    ax.bar(["residual\nmethod", "theory\n1/12", "naive direct\n(wrong)"], vals,
           color=["#2b8cbe", "#888888", "#d7301f"])
    for i, v in enumerate(vals):
        ax.text(i, v * 1.02, "%.5f" % v, ha="center", fontsize=8)
    ax.set_ylabel("variance [ADU$^2$]")
    ax.set_title("A1 quantization variance\n0.0807 is a VARIANCE, $1/\\sqrt{12}$=0.2887 is a SIGMA")
    # 右：1/sqrt(12) 适用域
    ax = axes[1]
    rng = np.random.default_rng(7)
    sig_in = np.logspace(-1.3, 0.8, 24)
    out = []
    for s in sig_in:
        x = rng.normal(0.0, s, 400000)
        out.append(float(np.var(np.round(x), ddof=1)))
    out = np.array(out)
    ax.semilogx(sig_in, out, "o-", ms=3, label="measured Var[round(x)]")
    ax.semilogx(sig_in, sig_in ** 2 + 1 / 12, "--", label=r"continuous approx $\sigma^2+1/12$")
    ax.axhline(1 / 12, color="k", lw=0.8, ls=":", label="1/12 = 0.0833")
    ax.set_xlabel(r"pre-quantization $\sigma$ [ADU]"); ax.set_ylabel("variance [ADU$^2$]")
    ax.set_title("A1 domain of validity of the 1/12 continuum approx")
    ax.legend(fontsize=7); fig.tight_layout(); fig.savefig(FIG / "fig2_quantization.png"); plt.close(fig)


def fig3():
    d = load("exp3_sky_scan.json")
    if not d:
        return
    scenes = list(d["scenes"].keys())
    fig, axes = plt.subplots(1, 3, figsize=(12.4, 3.6))
    for ax, sc in zip(axes, scenes):
        v = d["scenes"][sc]
        s = v["scan"]
        B = np.array([r["sky_e_per_pix"] for r in s])
        ax.loglog(B, [r["sigma_meas_adu"] for r in s], "o-", label=r"$\sigma$ measured (physical)")
        ax.loglog(B, [r["sigma_pred_adu"] for r in s], "--", label=r"$\sigma$ analytic")
        ax.loglog(B, [r["sigma_additive_adu"] for r in s], "s-", color="#d7301f",
                  label=r"$\sigma$ additive (NEGATIVE)")
        ax2 = ax.twinx(); ax2.grid(False)
        ax2.loglog(B, [r["snr_scalar_meas"] for r in s], "^-", color="#238b45",
                   label="SNR measured")
        ax2.loglog(B, [r["snr_additive_meas"] for r in s], "v--", color="#fd8d3c",
                   label="SNR additive (NEGATIVE)")
        ax2.set_ylabel("SNR", color="#238b45")
        ax.set_xlabel(r"sky level $B$ [e$^-$ pix$^{-1}$]")
        ax.set_ylabel(r"$\sigma_{pix}$ [ADU]")
        c = v["criteria"]
        ax.set_title("%s\nS1=%s S2=%.3f S3=%.3f/%.3f S4=%.1e"
                     % (sc, c["S1_sigma_strictly_monotone"], c["S2_median_abs_rel_dev_sigma"],
                        c["S3_snr_ratio_meas_scalar"], c["S3_snr_ratio_pred"],
                        c["S4_additive_max_abs_sigma_ratio_dev"]), fontsize=7)
        h1, l1 = ax.get_legend_handles_labels(); h2, l2 = ax2.get_legend_handles_labels()
        ax.legend(h1 + h2, l1 + l2, fontsize=6, loc="upper left")
    fig.suptitle("P7-EXP3 sky-level scan: B$\\uparrow$ $\\Rightarrow$ Poisson variance$\\uparrow$ $\\Rightarrow$ SNR$\\downarrow$ "
                 "(physical) vs EXACTLY FLAT (pure-additive negative control)", fontsize=9)
    fig.tight_layout(rect=[0, 0, 1, 0.93]); fig.savefig(FIG / "fig3_sky_scan.png"); plt.close(fig)


def fig4():
    d = load("exp1_injection_recovery.json")
    if not d:
        return
    fig, axes = plt.subplots(1, 2, figsize=(9.4, 3.8))
    ax = axes[0]
    for lab, col, mk in (("sky_x1.0", "#2b8cbe", "o"), ("sky_x20.0", "#d7301f", "s"),
                         ("NEG_zero_noise", "#238b45", "^")):
        a = d["arms"].get(lab)
        if not a:
            continue
        snr = np.array([x["snr_psf"] for x in a["stars"]], float)
        dm = np.array([x["dmag_psf"] for x in a["stars"]], float)
        ok = np.isfinite(snr) & np.isfinite(dm)
        ax.semilogx(snr[ok], dm[ok], mk, ms=4, color=col, alpha=0.8, label=lab)
    sn = np.logspace(0.7, 2.6, 50)
    ax.semilogx(sn, 1.0857 / sn, "k--", lw=1, label=r"theory $\pm1.0857/$SNR")
    ax.semilogx(sn, -1.0857 / sn, "k--", lw=1)
    ax.axhline(0, color="k", lw=0.6)
    ax.set_xlabel("SNR (PSF-domain)"); ax.set_ylabel(r"$\Delta$mag = recovered $-$ true")
    ax.set_ylim(-0.6, 0.6); ax.set_title("P7-EXP1 injection-recovery")
    ax.legend(fontsize=7)
    ax = axes[1]
    c = d["criteria"]
    names = ["R1 zero-noise\nnull (max|dF/F|)", "R2 bright\n|$\\Delta$mag|", "R4 seeing\nspread [mag]",
             "R5 additive\nSNR ratio dev"]
    vals = [c["R1_zero_noise_max_abs_rel_dev"], c["R2_bright_median_abs_dmag"],
            c["R4_seeing_spread_mag"], abs(c["R5_additive_snr_ratio_sky20_over_sky1"] - 1)]
    thr = [1e-12, 0.05, 0.05, 1e-12]
    xp = np.arange(4)
    ax.bar(xp - 0.18, np.maximum(vals, 1e-18), 0.36, label="measured", color="#2b8cbe")
    ax.bar(xp + 0.18, thr, 0.36, label="threshold", color="#bbbbbb")
    ax.set_yscale("log"); ax.set_xticks(xp); ax.set_xticklabels(names, fontsize=6.5)
    ax.set_ylabel("value (log)"); ax.legend(fontsize=7)
    ax.set_title("criteria: measured vs threshold")
    fig.tight_layout(); fig.savefig(FIG / "fig4_injection.png"); plt.close(fig)


def fig5():
    d = load("exp4_scatter_sp0.json")
    if not d:
        return
    arms = d["arms"]; c = d["criteria"]
    fig, axes = plt.subplots(1, 2, figsize=(9.4, 3.6))
    ax = axes[0]
    labs = ["A common-mode\n(NEGATIVE)", "B scale-only\n(NEGATIVE)", "C three bands\n(REAL)"]
    rt = [arms["A_common_mode"]["truth_layer"]["R_SP0_median"],
          arms["B_scale_only"]["truth_layer"]["R_SP0_median"],
          arms["C_three_bands"]["truth_layer"]["R_SP0_median"]]
    ds = [arms["A_common_mode"]["truth_layer"]["D_shape_median"],
          arms["B_scale_only"]["truth_layer"]["D_shape_median"],
          arms["C_three_bands"]["truth_layer"]["D_shape_median"]]
    ax.bar(labs, np.array(rt) - 1.0, color=["#238b45", "#238b45", "#d7301f"])
    for i, (r, dd) in enumerate(zip(rt, ds)):
        ax.text(i, max(r - 1.0, 1e-18) * 1.3 + 1e-9, "R=%.9f\nD=%.3e" % (r, dd),
                ha="center", fontsize=7)
    ax.set_ylabel(r"$R_{SP0}-1$"); ax.set_yscale("symlog", linthresh=1e-12)
    ax.set_title("P7-EXP4 SP-0 (truth layer)")
    ax = axes[1]
    rm = [arms["A_common_mode"]["measured_layer"]["R_SP0_median"],
          arms["B_scale_only"]["measured_layer"]["R_SP0_median"],
          arms["C_three_bands"]["measured_layer"]["R_SP0_median"]]
    ax.bar(labs, np.array(rm) - 1.0, color=["#238b45", "#238b45", "#d7301f"])
    for i, r in enumerate(rm):
        ax.text(i, max(r - 1.0, 1e-9) * 1.3, "R=%.5f" % r, ha="center", fontsize=7)
    ax.set_ylabel(r"$R_{SP0}-1$ (measured from frames)"); ax.set_yscale("symlog", linthresh=1e-6)
    ax.set_title("SP-0 measured layer (paired-difference variance)")
    fig.suptitle("SP-0 measures inter-frame SHAPE difference, not LEVEL difference", fontsize=9)
    fig.tight_layout(rect=[0, 0, 1, 0.92]); fig.savefig(FIG / "fig5_sp0.png"); plt.close(fig)


def fig6():
    from astropy.io import fits
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "p7lib", str(ROOT / "reverse_verify" / "experiments" / "p7_noise" / "p7lib.py"))
    P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
    fr_dir = ROOT / "run" / "reverse_verify" / "m16_scene" / "frames"
    items = [("m16_nebula_core", "m16_nebula_core_f00.fits", "bright nebula core (Ha)"),
             ("m16_starfield", "m16_starfield_f00.fits", "general star field"),
             ("m16_dark_lowsnr", "m16_dark_lowsnr_f00.fits", "faint / low-SNR (SII)")]
    fig, axes_all = plt.subplots(2, 3, figsize=(12.4, 8.0))
    axes = axes_all[0]; axes2 = axes_all[1]
    for ax, (sid, fn, title) in zip(axes, items):
        p = fr_dir / sid / fn
        if not p.exists():
            ax.set_axis_off(); ax.set_title("missing: %s" % p.name, fontsize=7); continue
        with fits.open(p, memmap=True) as f:
            d = np.asarray(f[0].data, dtype=np.float32)
        v = d[d > 0]
        lo, hi = np.percentile(v, 5), np.percentile(v, 99.5)
        ax.imshow(d, origin="lower", cmap="gray", vmin=lo, vmax=hi, interpolation="nearest")
        ax.set_title("%s\n%s  [linear 5-99.5%%]" % (title, fn), fontsize=7); ax.set_axis_off()
        # 第二行：asinh 拉伸（天文标准显示），揭示被噪声掩盖的真实结构
        ax2 = axes2[items.index((sid, fn, title))]
        p1, p50, p99, p999 = (float(np.percentile(v, q)) for q in (1, 50, 99, 99.9))
        lw = max(0.5 * (p99 - p1), 1e-6)
        ax2.imshow(d, origin="lower", cmap="gray", interpolation="nearest",
                   norm=matplotlib.colors.AsinhNorm(linear_width=lw, vmin=p1, vmax=p999))
        ax2.set_title("asinh stretch (vmin=p1, vmax=p99.9, a=%.0f ADU)" % lw,
                      fontsize=7); ax2.set_axis_off()
    fig.suptitle("P7 M16 synthetic scenes (real HST base + physical noise chain, 1024$^2$ crops)\n"
                 "top: linear stretch | bottom: asinh stretch (same data) - the faint real structure is present, "
                 "just buried under the physical noise (that is the point of scenario (3))", fontsize=8)
    fig.tight_layout(rect=[0, 0, 1, 0.92]); fig.savefig(FIG / "fig6_m16_scenes.png"); plt.close(fig)


def main():
    for f in (fig1, fig2, fig3, fig4, fig5, fig6):
        try:
            f(); print("[figs] %s ok" % f.__name__)
        except Exception as e:
            print("[figs] %s FAILED: %s: %s" % (f.__name__, type(e).__name__, e))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
