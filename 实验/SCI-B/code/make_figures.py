#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 图件：天光扫描曲线 / 三口径适用域 / 负例与判据审查。"""
from __future__ import annotations
import json, os, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C


def fig_sky():
    j = json.load(open(os.path.join(C.RESULTS, "b1_sky_scan.json"), encoding="utf-8"))
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, key, tag in [(axes[0], "sky_scan_bright", "F=3000 e-"), (axes[1], "sky_scan_faint", "F=100 e-")]:
        rows = j[key]
        B = np.array([r["sky_e_per_px"] for r in rows]) + 1e-3
        sd = np.array([r["snr_def"] for r in rows])
        se = np.array([r["snr_emp"] for r in rows])
        lo = np.array([r["snr_emp_ci95"][0] for r in rows]); hi = np.array([r["snr_emp_ci95"][1] for r in rows])
        trad = np.array([r["snr_arm_trad"] for r in rows])
        prn = np.array([r["snr_prod_empirical_rn"] for r in rows])
        ax.loglog(B, sd, "k-", lw=2, label="definition $F_{signal}/\\sigma_F$")
        ax.fill_between(B, lo, hi, color="0.7", alpha=.5, label="MC 95% CI")
        ax.loglog(B, se, "o", ms=3, color="0.2", label="MC empirical")
        ax.loglog(B, prn, "--", color="tab:red", label="prod: empirical $\\sigma$ + RN (double count)")
        ax.loglog(B, trad, ":", color="tab:orange", label="traditional (sky in signal)")
        ax.set_xlabel("sky level [e-/px]"); ax.set_ylabel("frame SNR")
        ax.set_title("sky scan, " + tag); ax.grid(alpha=.3, which="both")
    axes[0].legend(fontsize=7)
    fig.tight_layout(); fig.savefig(os.path.join(C.FIGS, "fig1_sky_scan.png"), dpi=140)
    plt.close(fig)


def fig_domain():
    j = json.load(open(os.path.join(C.RESULTS, "b3_domain_map.json"), encoding="utf-8"))
    fig, axes = plt.subplots(1, 3, figsize=(14, 4.2))
    # (a) 纯合成：Δ/ℓ vs RMSE（按 s_field 分组）
    ax = axes[0]
    for s in [0.0, 0.03, 0.10, 0.30]:
        for arm, ls in [("sparse", "-"), ("frame_median", "--")]:
            xs, ys = [], []
            for res in j["faces"]["synthetic_grf"]:
                if res["meta"]["s_field_true_log10"] != s: continue
                ell = res["meta"]["ell_nominal_px"]
                for r in res["rows"]:
                    if r["arm"] == arm:
                        xs.append(r["delta_px"] / ell); ys.append(r["rmse_log_rho"])
            o = np.argsort(xs)
            ax.loglog(np.array(xs)[o], np.array(ys)[o], ls, marker=".", ms=4,
                      label="%s s=%.2f" % (arm.replace("_median", ""), s))
    ax.set_xlabel(r"$\Delta/\ell$ (constructed)"); ax.set_ylabel(r"RMSE($\log_{10}\rho$)")
    ax.set_title("synthetic GRF: sparse vs frame"); ax.grid(alpha=.3, which="both")
    ax.legend(fontsize=6, ncol=2)
    # (b) 真实两域 Δ=64 柱状
    ax = axes[1]
    faces = [("M42 M1", j["faces"]["testdata_m42"][0]), ("M42 M2", j["faces"]["testdata_m42"][1]),
             ("M42 M4", j["faces"]["testdata_m42"][2]), ("HST M16", j["faces"]["hst_m16"])]
    arms = ["dense", "sparse", "frame_median"]
    w = 0.25
    for i, arm in enumerate(arms):
        vals = []
        for _, res in faces:
            v = next((r["rmse_log_rho"] for r in res["rows"] if r["delta_px"] == 64 and r["arm"] == arm), np.nan)
            vals.append(v)
        ax.bar(np.arange(len(faces)) + (i - 1) * w, vals, w, label=arm)
    ax.set_xticks(range(len(faces))); ax.set_xticklabels([f[0] for f in faces], fontsize=8)
    ax.set_ylabel(r"RMSE($\log_{10}\rho$) at $\Delta$=64"); ax.set_title("ground vs HST (real data)")
    ax.legend(fontsize=7); ax.grid(alpha=.3, axis="y")
    # (c) 存储代价
    ax = axes[2]
    cost = j["faces"]["hst_m16"]["cost"]
    names = ["dense", "sparse64", "sparse256", "frame"]
    vals = [cost["dense_MiB_4096"], cost["sparse_bytes_4096"]["64"] / 1048576,
            cost["sparse_bytes_4096"]["256"] / 1048576, cost["frame_bytes"] / 1048576]
    ax.bar(names, vals, color=["tab:red", "tab:green", "tab:green", "tab:blue"])
    ax.axhline(1.0, color="k", ls="--", lw=1, label="1 MiB/frame budget")
    ax.set_yscale("log"); ax.set_ylabel("MiB per frame (4096$^2$)")
    ax.set_title("storage cost (dense = 64$\\times$ over budget)"); ax.legend(fontsize=7)
    ax.grid(alpha=.3, axis="y", which="both")
    fig.tight_layout(); fig.savefig(os.path.join(C.FIGS, "fig2_domain_map.png"), dpi=140)
    plt.close(fig)


def fig_negatives():
    j = json.load(open(os.path.join(C.RESULTS, "b1_sky_scan.json"), encoding="utf-8"))
    b6 = json.load(open(os.path.join(C.RESULTS, "b6_gates_audit.json"), encoding="utf-8"))
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    ax = axes[0]
    rows = j["sky_scan_bright"]
    B = np.array([r["sky_e_per_px"] for r in rows]) + 1e-3
    sd = np.array([r["snr_def"] for r in rows])
    ax.semilogx(B, np.array([r["snr_arm_const"] for r in rows]) / sd - 1, "o-", label="neg1 arithmetic constant (must be 0)")
    ax.semilogx(B, np.array([r["snr_arm_trad"] for r in rows]) / sd - 1, "s-", label="neg2 traditional sky-in-signal")
    ax.semilogx(B, np.array([r["snr_arm_raw_frame"] for r in rows]) / sd - 1, "^-", label="neg3 no background subtraction")
    ax.axhline(0, color="k", lw=.8)
    ax.set_xlabel("sky level [e-/px]"); ax.set_ylabel("SNR relative deviation vs truth")
    ax.set_title("negative controls (B1)"); ax.grid(alpha=.3, which="both"); ax.legend(fontsize=7)
    ax = axes[1]
    eff = b6["eff_loss_injection"]
    ax.bar([x["case"] for x in eff], [x["eff_loss"] for x in eff],
           color=["tab:green" if not x["red"] else "tab:red" for x in eff])
    ax.set_ylabel("weight-efficiency loss E"); ax.set_title("replacement gate: E (green/red both reachable)")
    ax.tick_params(axis="x", labelrotation=20, labelsize=7); ax.grid(alpha=.3, axis="y")
    fig.tight_layout(); fig.savefig(os.path.join(C.FIGS, "fig3_negatives_gates.png"), dpi=140)
    plt.close(fig)


if __name__ == "__main__":
    fig_sky(); fig_domain(); fig_negatives()
    print("figures written to", C.FIGS)
