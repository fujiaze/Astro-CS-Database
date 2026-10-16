#!/usr/bin/env python3
"""P3 实测图生成器（只读 run/ 下的真实读数文件，缺文件即报错，不补造数据）。

用法：
    python3 实验/healpix-polar/figures/make_figures.py [--results-dir run/ACSDSDOC-02/S02/results]

输入（由 实验/healpix-polar/code/s02_build_and_run_matrix.sh 与
run/ACSDSDOC-02/S02/code/s02b_build_pair.sh 产生）：
  - s02b_matrix_cells.csv        单文件受控三臂 300 格矩阵（pre/post/fault）
  - s02b_theta_{pre,post,fault}.json  0.02"-3600" 适用域扫描（252 格/臂）
  - 实验/healpix-polar/docs/S02_P3_acceptance_matrix.json  1500 格独立并行矩阵摘要

被估量：eps_j = sum_p a_jp / A_drop,j - 1（无量纲，判定门 |eps_j| <= 1e-6）。
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

GATE = 1e-6  # ACSD DRIZZLE_GEOMETRY.md §9 逐 drop 完备性门，本轮未改动
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
POS_ORDER = ["leaf_interior_same_lon", "face_seam_ra0", "leaf_edge_mid_same_lon", "vertex_face0_center", "pole_dec90"]
POS_LABEL = {
    "leaf_interior_same_lon": "leaf interior",
    "face_seam_ra0": "face seam RA=0",
    "leaf_edge_mid_same_lon": "leaf edge mid",
    "vertex_face0_center": "face vertex",
    "pole_dec90": "pole dec=90",
}


def load_cells(path):
    with open(path) as fh:
        rows = list(csv.DictReader(fh))
    if not rows:
        raise SystemExit(f"empty matrix: {path}")
    for r in rows:
        for k in ("nside", "pixfrac", "scale_arcsec"):
            r[k] = float(r[k])
        for k in ("eps_pre", "eps_post", "eps_fault"):
            r[k] = float(r[k])
    return rows


def load_theta(path):
    with open(path) as fh:
        d = json.load(fh)
    return d["rows"]


def fig_arms(cells, acceptance, out):
    pre = np.abs([r["eps_pre"] for r in cells])
    post = np.abs([r["eps_post"] for r in cells])
    fault = np.abs([r["eps_fault"] for r in cells])
    m1500 = acceptance["independent_parallel_matrix"]

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
    ax = axes[0]
    names = ["pre\n(pristine)", "fault\n(env rollback)", "post\n(fixed)"]
    vals = [pre.max(), fault.max(), post.max()]
    colors = ["#c0392b", "#c0392b", "#1e8449"]
    bars = ax.bar(names, vals, color=colors)
    ax.set_yscale("log")
    ax.axhline(GATE, color="k", ls="--", lw=1)
    ax.text(2.42, GATE * 1.4, r"gate $\|\epsilon_j\|\leq10^{-6}$", ha="right", fontsize=8)
    for b, v, arm in zip(bars, vals, (pre, fault, post)):
        n_over = int((arm > GATE).sum())
        ax.text(b.get_x() + b.get_width() / 2, v * 1.6, f"{v:.3e}\n{n_over}/300 over", ha="center", fontsize=8)
    ax.set_ylabel(r"$\max_j|\epsilon_j|$  (dimensionless)")
    ax.set_ylim(1e-11, 1e-1)
    ax.set_title("300-cell controlled matrix\n(single-file three arms, pre/fault bitwise equal)", fontsize=9)

    ax = axes[1]
    names = ["pre", "NORM-01\nonly", "post"]
    vals = [
        m1500["pre_fix"]["max_abs_epsilon"],
        m1500["norm01_only"]["max_abs_epsilon"],
        m1500["post_fix"]["max_abs_epsilon"],
    ]
    over = [
        m1500["pre_fix"]["cells_over_1e-6"],
        m1500["norm01_only"]["cells_over_1e-6"],
        m1500["post_fix"]["cells_over_1e-6"],
    ]
    bars = ax.bar(names, vals, color=["#c0392b", "#b9770e", "#1e8449"])
    ax.set_yscale("log")
    ax.axhline(GATE, color="k", ls="--", lw=1)
    for b, v, n in zip(bars, vals, over):
        ax.text(b.get_x() + b.get_width() / 2, v * 1.6, f"{v:.3e}\n{n}/1500 over", ha="center", fontsize=8)
    ax.set_ylabel(r"$\max_j|\epsilon_j|$  (dimensionless)")
    ax.set_ylim(1e-11, 1e-1)
    ax.set_title("1500-cell independent parallel matrix\n(all position classes x nside x pixfrac x scale)", fontsize=9)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_heatmap(cells, out):
    nsides = sorted({int(r["nside"]) for r in cells})
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 4.8), sharey=True)
    for ax, arm, title in (
        (axes[0], "eps_pre", "pre (pristine)"),
        (axes[1], "eps_post", "post (fixed)"),
    ):
        M = np.full((len(POS_ORDER), len(nsides)), np.nan)
        ann = np.empty(M.shape, dtype=object)
        for i, pos in enumerate(POS_ORDER):
            for j, ns in enumerate(nsides):
                vals = [abs(r[arm]) for r in cells if r["pos_class"] == pos and int(r["nside"]) == ns]
                if vals:
                    M[i, j] = math.log10(max(max(vals), 1e-13))
                    ann[i, j] = f"{max(vals):.1e}\n{sum(v > GATE for v in vals)}/{len(vals)} over"
        im = ax.imshow(M, cmap="RdYlGn_r", vmin=-11, vmax=-2, aspect="auto")
        ax.set_xticks(range(len(nsides)), [str(n) for n in nsides])
        ax.set_yticks(range(len(POS_ORDER)), [POS_LABEL[p] for p in POS_ORDER])
        ax.set_xlabel("nside")
        ax.set_title(f"geometry position error, {title}\ncell = max|eps| over pixfrac{0.5,0.8,1} x scale{0.04,0.5,2,10,60}\u2033", fontsize=9)
        for i in range(M.shape[0]):
            for j in range(M.shape[1]):
                ax.text(j, i, ann[i, j], ha="center", va="center", fontsize=6.4)
    cb = fig.colorbar(im, ax=axes, fraction=0.03, pad=0.02)
    cb.set_label(r"$\log_{10}\max_j|\epsilon_j|$  (dimensionless)")
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_negative(cells, out):
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    bins = np.logspace(-12, -1, 45)
    for arm, label, color in (
        ("eps_post", "default path (post)", "#1e8449"),
        ("eps_fault", "injected fault (env rollback)", "#c0392b"),
    ):
        v = np.abs([r[arm] for r in cells])
        ax.hist(v, bins=bins, histtype="step", lw=1.8, color=color, label=f"{label}: {int((v>GATE).sum())}/300 over gate")
    ax.axvline(GATE, color="k", ls="--", lw=1)
    ax.text(GATE * 1.15, ax.get_ylim()[1] * 0.82, r"gate $10^{-6}$", fontsize=8)
    ax.set_xscale("log")
    ax.set_xlabel(r"$|\epsilon_j|$  (dimensionless)")
    ax.set_ylabel("matrix cells (count)")
    ax.set_title("negative control: injected cross-product normal fault must re-break the gate\n(fault arm bitwise identical to pre, 300/300 cells)", fontsize=9)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_domain(theta, out):
    fig, ax = plt.subplots(figsize=(7.6, 5.0))
    for arm, label, color in (("pre", "pre", "#c0392b"), ("post", "post", "#1e8449")):
        rows = theta[arm]
        for pos, marker in (("vertex_face0_center", "o"), ("leaf_edge_mid_same_lon", "s")):
            sub = sorted(
                [r for r in rows if r["pos"] == pos and int(r["nside"]) == 8192 and abs(r["pixfrac"] - 0.8) < 1e-9],
                key=lambda r: r["scale_arcsec"],
            )
            ax.plot(
                [r["scale_arcsec"] for r in sub],
                [abs(r["epsilon_j"]) for r in sub],
                marker=marker,
                ms=3.4,
                lw=1.4,
                color=color,
                alpha=0.9,
                label=f"{label} / {POS_LABEL[pos]}",
            )
    ax.axhline(GATE, color="k", ls="--", lw=1)
    ax.text(4000, GATE * 1.6, r"gate $10^{-6}$", ha="right", fontsize=8)
    ax.axvline(291.7, color="0.55", ls=":", lw=1)
    ax.text(291.7 * 1.12, 6e-3, "planar/spherical area branch\n(drop side 291.70\u2033)", fontsize=7, color="0.35")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(0.015, 6000)
    ax.set_ylim(1e-11, 2e-1)
    ax.set_xlabel("drop scale (arcsec, log)")
    ax.set_ylabel(r"$|\epsilon_j|$  (dimensionless, log)")
    ax.set_title("applicability domain of the per-drop completeness gate\nnside 8192, pixfrac 0.8, 0.02\u2033-3600\u2033", fontsize=9)
    ax.legend(fontsize=7, loc="lower left")
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results-dir", default=os.path.join(REPO, "run", "ACSDSDOC-02", "S02", "results"))
    args = ap.parse_args()
    rd = args.results_dir
    need = [
        os.path.join(rd, "s02b_matrix_cells.csv"),
        os.path.join(rd, "s02b_theta_pre.json"),
        os.path.join(rd, "s02b_theta_post.json"),
        os.path.join(REPO, "实验", "healpix-polar", "docs", "S02_P3_acceptance_matrix.json"),
    ]
    missing = [p for p in need if not os.path.isfile(p)]
    if missing:
        raise SystemExit("missing real input(s), refusing to fabricate figures:\n  " + "\n  ".join(missing))

    cells = load_cells(need[0])
    theta = {"pre": load_theta(need[1]), "post": load_theta(need[2])}
    acceptance = json.load(open(need[3]))
    fig_arms(cells, acceptance, os.path.join(HERE, "fig_P3_matrix_arms.png"))
    fig_heatmap(cells, os.path.join(HERE, "fig_P3_epsilon_heatmap.png"))
    fig_negative(cells, os.path.join(HERE, "fig_P3_negative_control.png"))
    fig_domain(theta, os.path.join(HERE, "fig_P3_domain_scale.png"))
    print("wrote 4 figures to", HERE)


if __name__ == "__main__":
    sys.exit(main())
