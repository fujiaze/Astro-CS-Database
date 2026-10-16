#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5 paper measurement figures (English axis labels, portable fonts).

Reads only already-persisted machine-readable evidence in this unit; it does not
recompute, interpolate or invent any scientific quantity:
  - docs/S07_results/S07_gain_main.json          (multiplicative four-arm study, main condition)
  - docs/S07_results/S07_boundary_coverage.json  (real 196-boundary coverage audit)
  - docs/S03_P5_COMPONENT_evidence.exec.json     (per-component gauge and S9b connectivity)

Writes 4 PNGs into this directory and P5_figures_manifest.json with per-figure and
per-input sha256.  Usage (repo root):
  python3 实验/additive-sky-seamless/figures/make_p5_figures.py
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = Path(__file__).resolve().parent
UNIT = HERE.parent
DOCS = UNIT / "docs"


def load(p: Path):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fig_arms_rms(main, out: Path):
    """Figure 2: held-out dispersion of the four arms, positive vs negative world."""
    arms = ["rms_A", "rms_AG_JOINT", "rms_AG_ORACLE_JOINT", "rms_AG_SHUFFLE_JOINT"]
    labels = ["A additive", "A+G joint", "A+G oracle", "A+G shuffle"]
    worlds = [("neg", "negative world g=1"), ("pos", "positive world g!=1")]
    fig, ax = plt.subplots(figsize=(8.6, 4.6))
    width = 0.36
    for k, (w, wlabel) in enumerate(worlds):
        vals = [main["summary"][w][a]["median"] for a in arms]
        xpos = [i + (k - 0.5) * width for i in range(len(arms))]
        bars = ax.bar(xpos, vals, width=width, label=wlabel,
                      color=("tab:blue" if k == 0 else "tab:orange"))
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v, "%.4f" % v,
                    ha="center", va="bottom", fontsize=7)
    ax.set_xticks(range(len(arms)))
    ax.set_xticklabels(labels)
    ax.set_ylabel("held-out dispersion RMS (dex, median of 10 seeds)")
    ax.set_title("P5-B: four-arm held-out dispersion (main condition)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_control(main, out: Path):
    """Figure 3: negative/positive world control on the A-(A+G) improvement."""
    fig, ax = plt.subplots(figsize=(7.8, 4.4))
    worlds = [("neg", "negative world g=1"), ("pos", "positive world g!=1")]
    xs = [0, 1]
    med = [main["summary"][w]["diff_A_minus_AG_JOINT"]["median"] for w, _ in worlds]
    lo = [main["summary"][w]["diff_A_minus_AG_JOINT"]["ci"][0] for w, _ in worlds]
    hi = [main["summary"][w]["diff_A_minus_AG_JOINT"]["ci"][1] for w, _ in worlds]
    err = [[m - l for m, l in zip(med, lo)], [h - m for m, h in zip(med, hi)]]
    ax.errorbar(xs, med, yerr=err, fmt="o", capsize=8, color="tab:red")
    ax.axhline(0.0, color="k", ls="--", lw=1)
    for x, m, l, h, (_, wl) in zip(xs, med, lo, hi, worlds):
        ax.annotate("%s\nmedian %.5f\n95%% CI [%.5f, %.5f]" % (wl, m, l, h),
                    (x, m), textcoords="offset points", xytext=(12, 10), fontsize=8)
    ax.set_xticks(xs)
    ax.set_xticklabels([w for _, w in worlds])
    ax.set_xlim(-0.5, 1.9)
    ax.set_ylabel("A - (A+G joint) held-out dispersion difference (dex)")
    ax.set_title("P5-B negative control: joint fit fabricates an improvement "
                 "(CI excludes 0) when no real gain exists")
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_boundary(bnd, out: Path):
    """Figure 4: real 196-boundary coverage and criterion domain."""
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 4.3))
    total = bnd["n_edges_total"]
    valid = bnd["n_edges_valid"]
    excl = bnd["n_edges_excluded"]
    axes[0].bar(["all edges", "in-criterion", "excluded"],
                [total, valid, excl],
                color=["tab:gray", "tab:green", "tab:red"])
    for i, v in enumerate([total, valid, excl]):
        axes[0].text(i, v, str(v), ha="center", va="bottom")
    axes[0].set_ylabel("boundary edges (count)")
    axes[0].set_title("real product: %d/%d edges enter the criterion" % (valid, total))
    note = bnd.get("exclude_kinds", {})
    axes[0].text(0.02, 0.95, "excluded: not_interior=%d" % note.get("not_interior", 0),
                 transform=axes[0].transAxes, va="top", fontsize=9)

    ne = bnd["n_evaluable_by_ctrl_shift"]
    keys = ["200", "128", "64", "32", "16", "8", "4"]
    axes[1].plot([int(k) for k in keys], [ne[k] for k in keys], "o-", color="tab:blue")
    axes[1].axhline(total, color="k", ls=":", label="all %d edges" % total)
    axes[1].set_xscale("log", base=2)
    axes[1].set_xlabel("control-line offset ctrl_shift [px]")
    axes[1].set_ylabel("edges with enough samples on both sides")
    axes[1].set_title("criterion coverage vs control offset "
                      "(N=32 reaches %d -> remainder is not unobserved)" % ne["32"])
    axes[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def fig_component_gap(exe, out: Path):
    """P5 component-overlap figure: S9b four frame covers, two components, gap."""
    sc = exe["scenarios"]["S9b_two_components"]
    covers = sc["covers"]
    spec = sc["component_spec"]
    fig, ax = plt.subplots(figsize=(9.0, 3.9))
    colors = {0: "tab:blue", 1: "tab:blue", 2: "tab:orange", 3: "tab:orange"}
    for fid, (x0, x1) in enumerate(covers):
        comp = "compA" if fid in spec["compA"] else "compB"
        ax.barh(fid, x1 - x0, left=x0, height=0.5, color=colors[fid], alpha=0.75)
        ax.text((x0 + x1) / 2, fid, "frame %d (%s)" % (fid, comp),
                ha="center", va="center", fontsize=8)
    ax.axvspan(150, 170, color="red", alpha=0.18)
    ax.text(160, 3.42, "no overlap\nx in (150,170)", ha="center", fontsize=8,
            color="darkred")
    ax.set_yticks(range(len(covers)))
    ax.set_yticklabels(["frame %d" % i for i in range(len(covers))])
    ax.set_ylim(-0.6, 3.8)
    ax.set_xlabel("synthetic-world x [px]")
    ax.set_title("P5-C: S9b two components (2 frames each); the cross-component "
                 "absolute zero is not identifiable (gap is not a criterion)")
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)


def main():
    main_json = DOCS / "S07_results" / "S07_gain_main.json"
    bnd_json = DOCS / "S07_results" / "S07_boundary_coverage.json"
    exe_json = DOCS / "S03_P5_COMPONENT_evidence.exec.json"
    main_d = load(main_json)
    bnd_d = load(bnd_json)
    exe_d = load(exe_json)

    specs = [
        (fig_arms_rms, main_d, "fig_P5_arms_rms.png"),
        (fig_control, main_d, "fig_P5_negative_control.png"),
        (fig_boundary, bnd_d, "fig_P5_boundary_coverage.png"),
        (fig_component_gap, exe_d, "fig_P5_component_overlap.png"),
    ]
    outputs = []
    for fn, data, name in specs:
        p = HERE / name
        fn(data, p)
        outputs.append(p)

    manifest = {
        "schema": "ACSD-P5-FIGURES/1",
        "generator": "实验/additive-sky-seamless/figures/make_p5_figures.py",
        "note": "all figures are drawn only from persisted evidence JSON; no recompute, "
                "no interpolation; figures that were never run are not listed.",
        "inputs": [
            {"path": "实验/additive-sky-seamless/docs/S07_results/S07_gain_main.json",
             "sha256": sha256(main_json)},
            {"path": "实验/additive-sky-seamless/docs/S07_results/S07_boundary_coverage.json",
             "sha256": sha256(bnd_json)},
            {"path": "实验/additive-sky-seamless/docs/S03_P5_COMPONENT_evidence.exec.json",
             "sha256": sha256(exe_json)},
        ],
        "figures": [{"file": p.name, "sha256": sha256(p)} for p in outputs],
    }
    mpath = HERE / "P5_figures_manifest.json"
    with open(mpath, "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("[P5 figs] wrote %d figures + manifest" % len(outputs))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
