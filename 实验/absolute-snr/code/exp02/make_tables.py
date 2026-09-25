#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 / 产物生成：关系曲线图 + Markdown 表。

输入：results/exp02_{e1_analytic,e2_hst,e3_real}.json
输出：results/exp02_figs/*.png、results/EXP02_TABLES.md
不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = Path(__file__).resolve().parent
UNIT = HERE.parent.parent
RES = UNIT / "results"
FIGS = RES / "exp02_figs"
BOX_PRIMARY = 32


def load(name: str) -> Dict[str, Any]:
    return json.loads((RES / name).read_text(encoding="utf-8"))


def fig_analytic(d: Dict[str, Any]) -> List[str]:
    rows = d["analytic"]["rows"]
    models = sorted({r["model"] for r in rows})
    FIGS.mkdir(parents=True, exist_ok=True)
    outs = []
    fig, axes = plt.subplots(1, 2, figsize=(13, 5.2))
    for mdl in models:
        rs = [r for r in rows if r["model"] == mdl]
        x = [max(r["struct_over_noise"], 1e-2) for r in rs]
        axes[0].plot(x, [r["prod_rel_err"] for r in rs], "o-", ms=3.5, lw=1.1, label=mdl)
        axes[1].plot(x, [r["boxes"][str(BOX_PRIMARY)]["fix_rel_err"] for r in rs],
                     "o-", ms=3.5, lw=1.1, label=mdl)
    for ax, title in zip(axes, ["F0 production global clipped RMS",
                                "F1b mesh background + residual clipped RMS (box=32)"]):
        ax.axhline(0.0, color="k", lw=0.8)
        ax.axhspan(-0.014, 0.014, color="g", alpha=0.12, label="+/-1.4% budget")
        ax.set_xscale("log")
        ax.set_xlabel("structure rms / noise rms  (r)")
        ax.set_ylabel("sigma_hat / sigma_true - 1")
        ax.set_title(title)
        ax.grid(alpha=0.3)
    axes[0].legend(fontsize=7, loc="upper left")
    fig.suptitle("EXP-02 Arm A (analytic): structure contamination vs r  [seed=20260925]")
    fig.tight_layout()
    p = FIGS / "a_analytic_scan.png"
    fig.savefig(p, dpi=140)
    plt.close(fig)
    outs.append(str(p.relative_to(UNIT)))

    fig, ax = plt.subplots(figsize=(7.5, 5.2))
    for mdl in ("smooth_corr8", "smooth_corr32", "smooth_corr128", "ramp"):
        rs = [r for r in rows if r["model"] == mdl]
        for box in ("32", "64", "128"):
            ax.plot([max(r["struct_over_noise"], 1e-2) for r in rs],
                    [abs(r["boxes"][box]["fix_rel_err"]) for r in rs],
                    "-", lw=1.0, label="%s box=%s" % (mdl, box))
    ax.axhline(0.014, color="r", ls="--", lw=1.0, label="1.4% budget")
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlabel("structure rms / noise rms"); ax.set_ylabel("|sigma_hat_fix/sigma_true - 1|")
    ax.set_title("EXP-02 Arm A: mesh size vs residual structure error")
    ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=6, ncol=2)
    fig.tight_layout()
    p = FIGS / "a_box_dependence.png"
    fig.savefig(p, dpi=140)
    plt.close(fig)
    outs.append(str(p.relative_to(UNIT)))

    base = {r["model"]: r["fix_rel_err"] for r in rows if r["struct_over_noise"] == 0.0}
    fig, axes = plt.subplots(1, 2, figsize=(13, 5.2))
    for mdl in models:
        rs = [r for r in rows if r["model"] == mdl]
        ex = [r["boxes"][str(BOX_PRIMARY)]["fix_rel_err"] - base[mdl] for r in rs]
        axes[0].scatter([r["boxes"][str(BOX_PRIMARY)]["A2"] for r in rs], ex, s=14, label=mdl)
        axes[1].scatter([r["boxes"][str(BOX_PRIMARY)]["A1"] for r in rs], ex, s=14, label=mdl)
    for ax, xl in zip(axes, ["A2 = sigma_fix/(sigma_diff/sqrt2)",
                             "A1 = sigma_prod/(sigma_diff/sqrt2)"]):
        ax.axhline(0.014, color="r", ls="--", lw=1.0)
        ax.axhline(-0.014, color="r", ls="--", lw=1.0)
        ax.axvline(1.014, color="b", ls=":", lw=1.0, label="gate threshold 1+delta")
        ax.set_yscale("symlog", linthresh=0.01)
        ax.set_xscale("log")
        ax.set_xlabel(xl); ax.set_ylabel("true excess over structure-free baseline")
        ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=7)
    fig.suptitle("EXP-02 Arm A: truth-free gate proxies vs true structure error")
    fig.tight_layout()
    p = FIGS / "a_gate_calibration.png"
    fig.savefig(p, dpi=140)
    plt.close(fig)
    outs.append(str(p.relative_to(UNIT)))
    return outs


def fig_hst(d: Dict[str, Any]) -> List[str]:
    rows = [r for r in d["rows"] if r["variant"] == "nostar"]
    FIGS.mkdir(parents=True, exist_ok=True)
    x = [max(r["struct_p999_over_noise"], 1e-2) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 5.4))
    ax.plot(x, [r["prod_rel_err"] for r in rows], "o-", label="F0 production global")
    ax.plot(x, [r["fix_rel_err"] for r in rows], "s-", label="F1b mesh fix (box=32)")
    ax.axhline(0.0, color="k", lw=0.8)
    ax.axhspan(-0.014, 0.014, color="g", alpha=0.12, label="+/-1.4%")
    ax.set_xscale("log"); ax.set_yscale("symlog", linthresh=0.01)
    ax.set_xlabel("structure p99.9 / sigma_sky")
    ax.set_ylabel("sigma_hat / sigma_true - 1")
    ax.set_title("EXP-02 Arm B (HST M16 forward sim, nostar)  [seed=20260925]")
    ax.grid(alpha=0.3, which="both"); ax.legend()
    fig.tight_layout()
    p = FIGS / "b_hst_scan.png"
    fig.savefig(p, dpi=140)
    plt.close(fig)
    return [str(p.relative_to(UNIT))]


def fig_real(d: Dict[str, Any]) -> List[str]:
    rows = d["frames"]
    FIGS.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(1, 2, figsize=(13.5, 5.4))
    panels = sorted({r["panel"] for r in rows})
    cmap = {q: c for q, c in zip(panels, plt.cm.tab10.colors)}
    for r in rows:
        axes[0].scatter(r["prod_keep_frac"], r["prod_sigma"] / r["fix_sigma"],
                        color=cmap[r["panel"]], s=34, label=r["panel"])
        axes[1].scatter(r["A1"], r["prod_sigma"] / r["fix_sigma"],
                        color=cmap[r["panel"]], s=34)
    for ax, xl in zip(axes, ["production clip keep fraction",
                             "A1 = sigma_prod/(sigma_diff/sqrt2)"]):
        ax.axhline(1.0, color="k", lw=0.8)
        ax.set_xlabel(xl); ax.set_ylabel("sigma_prod / sigma_fix")
        ax.grid(alpha=0.3)
    axes[0].set_title("real M42 T2 frames: clip fraction vs contamination")
    axes[1].set_title("real M42 T2 frames: A1 vs contamination")
    fig.suptitle("EXP-02 Arm C (testdata M42 T2 300s Red, 2048^2 centre crop, read-only)")
    fig.tight_layout()
    p = FIGS / "c_real_frames.png"
    fig.savefig(p, dpi=140)
    plt.close(fig)
    return [str(p.relative_to(UNIT))]


def md_analytic(d: Dict[str, Any]) -> str:
    rows = d["analytic"]["rows"]
    L = ["| model | r = sigma_s/sigma_n | F0 prod | F1b fix | F1a mesh-median | "
         "A1 | A2 | D | kf | verdict |",
         "|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        b = r["boxes"][str(BOX_PRIMARY)]
        L.append("| %s | %.2f | %+.3f | %+.4f | %+.4f | %.3f | %.4f | %.2f | %.4f | %s |"
                 % (r["model"], r["struct_over_noise"], r["prod_rel_err"],
                    b["fix_rel_err"], b["fix_rel_err_meshmedian"], b["A1"], b["A2"],
                    b["D"], b["kf"], b["verdict"]))
    return chr(10).join(L)


def md_hst(d: Dict[str, Any]) -> str:
    L = ["| target p99.9 [e-] | variant | struct rms [ADU] | rms/sigma | p99.9/sigma | "
         "F0 prod | F1b fix | A1 | A2 | kf | verdict |",
         "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in d["rows"]:
        L.append("| %.1f | %s | %.1f | %.2f | %.2f | %+.4f | %+.4f | %.3f | %.4f | %.4f | %s |"
                 % (r["target_p999_e"], r["variant"], r["struct_rms_adu"],
                    r["struct_over_noise"], r["struct_p999_over_noise"],
                    r["prod_rel_err"], r["fix_rel_err"], r["A1"], r["A2"],
                    r["prod_keep_frac"], r["verdict"]))
    return chr(10).join(L)


def md_real(d: Dict[str, Any]) -> str:
    L = ["| file | prod sigma | kf | fix sigma | R=prod/fix | A1 | A2 | A2(mesh-med) | D | "
         "block p95/p05 | verdict |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in d["frames"]:
        L.append("| %s | %.4f | %.4f | %.4f | %.3f | %.3f | %.4f | %.4f | %.2f | %.2f | %s |"
                 % (r["file"], r["prod_sigma"], r["prod_keep_frac"], r["fix_sigma"],
                    r["R_struct"], r["A1"], r["A2"], r["A2_meshmedian"], r["D"],
                    r["blk_block_sigma_p95_over_p05"], r["verdict"]))
    return chr(10).join(L)


def main() -> int:
    d1 = load("exp02_e1_analytic.json")
    d2 = load("exp02_e2_hst.json")
    d3 = load("exp02_e3_real.json")
    figs = fig_analytic(d1) + fig_hst(d2) + fig_real(d3)
    NL = chr(10)
    parts = ["# EXP-02 产物表（自动生成，勿手改）", "",
             "生成命令：python3 code/exp02/make_tables.py", "", "## 图", ""]
    parts += ["- " + f for f in figs]
    parts += ["", "## 表 A1 纯解析合成（box=32；r = 结构 rms / 噪声 rms）", "",
              md_analytic(d1), "",
              "## 表 B1 HST M16 真实模板 + 物理前向仿真", "", md_hst(d2), "",
              "## 表 C1 testdata M42 T2 300s Red（2048^2 中心裁剪，只读）", "",
              md_real(d3), ""]
    p = RES / "EXP02_TABLES.md"
    p.write_text(NL.join(parts), encoding="utf-8")
    print("wrote", p)
    for f in figs:
        print("fig", f)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())