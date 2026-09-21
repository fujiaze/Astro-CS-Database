#!/usr/bin/env python3
# 实验/SCI-C/code/make_figures.py
"""从 results/*.json 生成图（不改任何数字，只画已落盘的实测值）。"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S


def load(name):
    p = S.RESULTS / name
    return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None


def fig_c1(d, d3):
    if not d:
        return
    c = d["curve_step_vs_injection"]
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
    ax[0].plot(c["amplitude"], c["uncorrected"], "o-", label="raw (uncorrected)")
    ax[0].plot(c["amplitude"], c["predicted"], "s--", label="noiseless prediction")
    ax[0].plot(c["amplitude"], c["delta"], "^-", label=r"raw $-\ \delta_k$ (UPM)")
    ax[0].plot(c["amplitude"], c["full_subtract"], "v:", label=r"raw $- B_k$ (full subtract)")
    ax[0].set_xlabel("injected per-frame additive sky amplitude [e-]")
    ax[0].set_ylabel("median |boundary step| [e-]")
    ax[0].set_title("C1 step vs injection amplitude")
    ax[0].legend(fontsize=8)
    ax[0].grid(alpha=0.3)
    sw = d["out_of_basis_sweep"]
    x = [s["oob_rms"] for s in sw]
    y = [s["seam_max"] for s in sw]
    ax[1].plot(x, y, "o-")
    for s in sw:
        ax[1].annotate("%gpx" % s["wave_px"], (s["oob_rms"], s["seam_max"]),
                       fontsize=7, xytext=(3, 3), textcoords="offset points")
    ax[1].set_xlabel("frame-to-frame sky: B_ref-unrepresentable RMS [e-]")
    ax[1].set_ylabel("residual boundary step (max) [e-]")
    ax[1].set_title("C1 residual seam vs out-of-basis content")
    ax[1].grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c1_additive.png", dpi=110)
    plt.close(fig)


def fig_c2(d):
    if not d:
        return
    fig, ax = plt.subplots(1, 3, figsize=(14, 4.0))
    r = d["ratio_armP"]
    names = [n for n in r]
    ax[0].bar(np.arange(len(names)),
              [np.mean([r[n][k]["median"] for k in r[n]]) for n in names])
    ax[0].axhline(1.0, color="k", ls="--", lw=1)
    ax[0].set_xticks(np.arange(len(names)))
    ax[0].set_xticklabels(names, fontsize=8)
    ax[0].set_ylabel("frame-to-frame multiplicative ratio")
    ax[0].set_title("C2 ratio after Phase1 (target 1.0)")
    ax[0].grid(alpha=0.3, axis="y")
    g = d.get("ma_g_errors", {})
    ax[1].bar([0, 1, 2], [g.get("none", np.nan), g.get("after_phase1", np.nan),
                          g.get("truth", np.nan)])
    ax[1].set_xticks([0, 1, 2])
    ax[1].set_xticklabels(["no Phase1", "Phase1", "oracle"], fontsize=8)
    ax[1].set_ylabel(r"max $|g_k - g_k^{true}|$ (rel.)")
    ax[1].set_title("C2 MA solver gain recovery")
    ax[1].grid(alpha=0.3, axis="y")
    sw = d["multiplicative_sweep"]
    ax[2].plot([s["mult_residual_rms"] for s in sw], [s["seam_max"] for s in sw], "o-")
    ax[2].set_xlabel("out-of-basis multiplicative residual RMS")
    ax[2].set_ylabel("residual seam (max) [e-]")
    ax[2].set_title("C2 seam vs multiplicative residual")
    ax[2].grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c2_multiplicative.png", dpi=110)
    plt.close(fig)


def fig_c4(d):
    if not d:
        return
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
    n = d["null"]
    thr = d["threshold_5sigma"]
    ax[0].hist(np.random.default_rng(0).normal(n["nondeg"]["mean"], n["nondeg"]["std"], 4000),
               bins=40, alpha=0.5, label="non-degenerate null (gauss fit)")
    ax[0].axvline(thr, color="r", ls="--", label="5-sigma threshold")
    inj = d["injected"]["50.0"]["nondeg"]
    ax[0].axvline(inj["mean"], color="g", ls="-", label="injected 50 e- (mean)")
    ax[0].set_xlabel("boundary step metric [e-]")
    ax[0].set_title("C4 metric null vs injected seam")
    ax[0].legend(fontsize=8)
    amps = sorted(float(k) for k in d["injected"])
    rates = [d["detect_rate"][str(a)] if str(a) in d["detect_rate"] else np.nan for a in amps]
    dr = d["detect_rate"]
    xs = [0.0] + [float(k) for k in dr]
    ys = [d["false_positive_rate"]] + [dr[k] for k in dr]
    ax[1].plot(xs, ys, "o-", label="non-degenerate D")
    ax[1].axhline(0.95, color="g", ls=":", label="95% detection")
    ax[1].axhline(0.05, color="r", ls=":", label="5% false positive")
    ax[1].set_xlabel("injected seam amplitude [e-]")
    ax[1].set_ylabel("detection rate")
    ax[1].set_title("C4 red/green two-way response")
    ax[1].legend(fontsize=8)
    ax[1].grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c4_criterion.png", dpi=110)
    plt.close(fig)


def fig_c5(d):
    if not d:
        return
    arms = list(d["rms_median"])
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.0))
    ax[0].bar(np.arange(len(arms)), [d["rms_median"][a] for a in arms])
    ax[0].set_xticks(np.arange(len(arms)))
    ax[0].set_xticklabels(arms, fontsize=8)
    ax[0].set_ylabel("truth-weighted RMS of recovered delta_k [e-]")
    ax[0].set_title("C5 estimator noise (lower = better)")
    ax[0].grid(alpha=0.3, axis="y")
    ax[1].bar(np.arange(len(arms)), [d["leak_median"][a] for a in arms])
    ax[1].set_xticks(np.arange(len(arms)))
    ax[1].set_xticklabels(arms, fontsize=8)
    ax[1].set_ylabel("artifact leakage into clean frames [e-]")
    ax[1].set_title("C5 bias leakage (lower = better)")
    ax[1].grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c5_weights.png", dpi=110)
    plt.close(fig)


def fig_c6(d):
    if not d:
        return
    m = d["upm_memory"]
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.0))
    ax[0].bar([0, 1], [m["sparse_bytes"], m["dense_bytes"]])
    ax[0].set_xticks([0, 1])
    ax[0].set_xticklabels(["sparse model", "dense cache"], fontsize=8)
    ax[0].set_ylabel("bytes")
    ax[0].set_title("C6 persistent size (ratio %.4f)" % d["size_ratio"])
    ax[0].grid(alpha=0.3, axis="y")
    ax[1].bar([0, 1], [m["rss_full_grid_kb"], m["rss_64block_kb"]])
    ax[1].set_xticks([0, 1])
    ax[1].set_xticklabels(["dense materialise 512x512", "on-demand 64x64"], fontsize=8)
    ax[1].set_ylabel("peak RSS [kB]")
    ax[1].set_title("C6 peak memory")
    ax[1].grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c6_memory.png", dpi=110)
    plt.close(fig)


def main():
    S.FIGS.mkdir(parents=True, exist_ok=True)
    fig_c1(load("c1_additive.json"), load("c3_public_plane.json"))
    fig_c2(load("c2_multiplicative.json"))
    fig_c4(load("c4_seam_criterion.json"))
    fig_c5(load("c5_weights.json"))
    fig_c6(load("c6_sparse_dense.json"))
    print("figures ->", S.FIGS)
    return 0


if __name__ == "__main__":
    sys.exit(main())
