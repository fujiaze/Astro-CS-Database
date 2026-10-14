#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05 图件：四臂覆盖率、k_corr(rho) 与生产域、真实星 z/覆盖率分层、P3 协方差对照。

输入全部是 results/*.json（不重算科学量，只画已落盘读数）。
用法：python3 s05_figures.py --results run/ACSDSDOC-02/S05/results \
        --fig-dir run/ACSDSDOC-02/S05/figures
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


def load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def fig_arms(arms, out):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))
    # (1) 臂 A 校准与覆盖率
    a = arms["arms"][0]
    cov = [r["coverage"]["coverage"]["0.6826894921"]["cov"] for r in a["rows"]]
    ratios = [r["sd_Fhat_over_sigma_F"] for r in a["rows"]]
    axes[0].hist(cov, bins=12, color="tab:blue", alpha=0.8)
    axes[0].axvline(0.6826894921370859, color="k", ls="--", label="nominal 68.27%")
    axes[0].set_title("P2-A: 68% coverage (45 grid points)")
    axes[0].set_xlabel("coverage"); axes[0].legend()
    axes[0].text(0.02, 0.95, "sd/sigma %.4f-%.4f" % (min(ratios), max(ratios)),
                 transform=axes[0].transAxes, va="top", fontsize=9)
    # (2) 臂 C：E_diag 与近邻相关 vs rho
    c = arms["arms"][2]
    rows = [r for r in c["rows"] if r["floor_ratio"] == 0.1]
    rho = np.array([r["rho_output_over_source"] for r in rows])
    e = np.array([r["E_diag_vs_full_psf"] for r in rows])
    pf = np.array([r["pixfrac"] for r in rows])
    for p in sorted(set(pf)):
        m = pf == p
        axes[1].plot(rho[m], -e[m], "o-", label="pixfrac=%.1f" % p)
    axes[1].set_xscale("log"); axes[1].set_yscale("symlog", linthresh=1e-3)
    axes[1].set_xlabel("rho = leaf / source pixel scale")
    axes[1].set_ylabel("variance deficit of diagonal-only")
    axes[1].set_title("P2-C: diagonal-only variance deficit (floor_ratio=0.1)")
    axes[1].legend(fontsize=8)
    # (3) 臂 D：E vs aperture
    d = arms["arms"][3]
    for fw in sorted(set(r["fwhm_px"] for r in d["rows"])):
        rr = [r for r in d["rows"] if r["fwhm_px"] == fw and r["sky_adu"] == 200.0]
        axes[2].plot([r["aperture_radius_fwhm"] for r in rr],
                     [r["E_point_weights_on_diffuse"] for r in rr],
                     "o-", label="FWHM=%.1f px" % fw)
    axes[2].axhline(0.10, color="r", ls=":", label="E=10%")
    axes[2].set_xlabel("diffuse aperture radius [FWHM]")
    axes[2].set_ylabel("E = Var_wrong/Var_opt - 1")
    axes[2].set_title("P2-D: point-source weights misused on diffuse source")
    axes[2].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_kcorr(kc, out):
    fig, axes = plt.subplots(1, 2, figsize=(12.5, 4.4))
    geo = kc["geometry_scan"]
    for to in sorted(set(r["tile_order"] for r in geo)):
        rr = sorted([r for r in geo if r["tile_order"] == to], key=lambda r: r["rho"])
        axes[0].plot([r["rho"] for r in rr], [r["k_corr_production"] for r in rr],
                     "o-", label="tile_order=%d (nside 2^%d)" % (to, to + 9))
    axes[0].axhline(1.4, color="r", ls="--", label="frozen fallback 1.4")
    axes[0].axvline(1.374, color="k", ls=":", label="calibration rho=1.374")
    axes[0].set_xscale("log"); axes[0].set_yscale("log")
    axes[0].set_xlabel("rho = output leaf / source pixel scale")
    axes[0].set_ylabel("k_corr (production estimator)")
    axes[0].set_title("P2-C: k_corr vs rho (pf=0.8, 16x16 leaves)")
    axes[0].legend(fontsize=7)
    # 右图：生产几何检查点
    labels, vals, colors = [], [], []
    for w in kc["probe_window_crosscheck"]:
        if w.get("status") != "PASS":
            continue
        labels.append("%s\nside=%d rho=%.3g" % (w["probe"].replace(".json", ""),
                                                w["window_side_leaves"], w["rho"]))
        vals.append(min(w["k_corr_production"], 1e3))
        colors.append("tab:red" if w["probe"].startswith("prod_") else "tab:blue")
    axes[1].bar(range(len(vals)), vals, color=colors)
    axes[1].axhline(1.4, color="r", ls="--", label="1.4")
    axes[1].set_xticks(range(len(labels)))
    axes[1].set_xticklabels(labels, rotation=25, ha="right", fontsize=6)
    axes[1].set_yscale("log")
    axes[1].set_title("production spherical weights, spatial window")
    axes[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_real(real, out):
    npz = np.load(real["per_star_arrays"]["path"])
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.3))
    z = npz["z_det"]
    is_tr = npz["is_train"]
    axes[0].hist(z[~is_tr], bins=60, range=(-8, 8), color="tab:blue", alpha=0.8)
    for s in (-3, -2, -1, 1, 2, 3):
        axes[0].axvline(s, color="k", lw=0.5, ls=":")
    axes[0].set_title("real paired z (detrended, holdout n=%d)" % int((~is_tr).sum()))
    axes[0].set_xlabel("z = (dmag - trend)/sigma_analytic")
    snr = npz["snr1"]
    edges = [5, 10, 20, 50, 100, 300, 1e9]
    xs, ys, ns = [], [], []
    for lo, hi in zip(edges[:-1], edges[1:]):
        m = (snr >= lo) & (snr < hi) & (~is_tr)
        if m.sum() < 5:
            continue
        xs.append("%g-%g" % (lo, hi)); ns.append(int(m.sum()))
        ys.append(np.sqrt(np.mean(z[m] ** 2)))
    axes[1].bar(range(len(xs)), ys, color="tab:orange")
    axes[1].axhline(1.0, color="k", ls="--", label="z_rms=1")
    axes[1].set_xticks(range(len(xs))); axes[1].set_xticklabels(xs, fontsize=7)
    axes[1].set_xlabel("SNR epoch1 bin"); axes[1].set_ylabel("z_rms (holdout)")
    axes[1].set_title("analytic-only z_rms by SNR (systematic floor)")
    axes[1].legend()
    # 覆盖率对照
    keys = [("raw (analytic sigma)", real["coverage_raw_constant_model"]["all"]),
            ("detrended (analytic)", real["coverage_detrended"]["holdout"]),
            ("detrended + floor", real["empirical_systematic_floor"]["coverage_holdout_after_floor"])]
    levels = ["0.6826894921", "0.9", "0.95"]
    width = 0.25
    for i, (lab, d) in enumerate(keys):
        vals = [d["coverage"][lv]["cov"] if "coverage" in d else d[lv]["cov"] for lv in levels]
        axes[2].bar(np.arange(3) + (i - 1) * width, vals, width=width, label=lab)
    axes[2].set_xticks(range(3))
    axes[2].set_xticklabels(["68.27%", "90%", "95%"])
    axes[2].axhline(0.6826894921370859, color="k", lw=0.5)
    axes[2].axhline(0.95, color="k", lw=0.5)
    axes[2].set_ylim(0, 1.05)
    axes[2].set_title("real coverage: analytic vs +empirical floor")
    axes[2].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_p3(kc, out):
    pairs = kc["p3_prepost"]["pairs"]
    fig, axes = plt.subplots(1, 2, figsize=(12.5, 4.3))
    short = {"vertex": "vertex (S02)", "prod": "production real", "calib": "calibration 300\"/px"}
    labs = []
    for p in pairs:
        if p.get("status") != "PASS":
            continue
        lab = p["label"]
        labs.append("vertex (S02)" if lab.startswith("RA=45") else
                    ("production real" if lab.startswith("生产真帧") else "calibration 300\"/px"))
    eps_f = [p["eps_max_fixed"] for p in pairs if p.get("status") == "PASS"]
    eps_l = [p["eps_max_legacy"] for p in pairs if p.get("status") == "PASS"]
    x = np.arange(len(labs))
    axes[0].bar(x - 0.2, eps_l, 0.4, label="legacy (fault switch)", color="tab:red")
    axes[0].bar(x + 0.2, eps_f, 0.4, label="HEAD (P3 fixed)", color="tab:green")
    axes[0].set_yscale("log"); axes[0].set_xticks(x)
    axes[0].set_xticklabels(labs, rotation=15, ha="right", fontsize=7)
    axes[0].set_ylabel("max |epsilon_j|")
    axes[0].set_title("P3 completeness: fixed vs legacy fault")
    axes[0].legend(fontsize=8)
    rel = [100 * p["k_corr_rel_change"] for p in pairs if p.get("status") == "PASS"]
    se = [100 * p["mc_se_rel_of_k"] for p in pairs if p.get("status") == "PASS"]
    axes[1].bar(x - 0.2, rel, 0.4, label="P3 dk_corr [%]", color="tab:purple")
    axes[1].bar(x + 0.2, se, 0.4, label="P2 MC se of k_corr [%]", color="tab:gray")
    axes[1].set_xticks(x); axes[1].set_xticklabels(labs, rotation=15, ha="right", fontsize=7)
    axes[1].set_title("P3 geometric effect vs P2 noise resolution")
    axes[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="run/ACSDSDOC-02/S05/results")
    ap.add_argument("--fig-dir", default="run/ACSDSDOC-02/S05/figures")
    args = ap.parse_args()
    os.makedirs(args.fig_dir, exist_ok=True)
    arms = load(os.path.join(args.results, "arms_oracle.json"))
    kc = load(os.path.join(args.results, "kcorr_units.json"))
    real = load(os.path.join(args.results, "real117.json"))
    fig_arms(arms, os.path.join(args.fig_dir, "s05_arms.png"))
    fig_kcorr(kc, os.path.join(args.fig_dir, "s05_kcorr.png"))
    fig_real(real, os.path.join(args.fig_dir, "s05_real117.png"))
    fig_p3(kc, os.path.join(args.fig_dir, "s05_p3_covariance.png"))
    print("[S05 figs] wrote 4 figures to %s" % args.fig_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
