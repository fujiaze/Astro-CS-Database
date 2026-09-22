#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 表格与图生成（从 results/exp03_*.json 生成 Markdown 表与 PNG）。"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[4]
RES = ROOT / "实验" / "absolute-snr" / "results"


def load(n: str) -> Dict[str, Any]:
    return json.loads((RES / n).read_text(encoding="utf-8"))


def fmt(x, nd=4, pct=False):
    if x is None:
        return "-"
    try:
        v = float(x)
    except Exception:
        return str(x)
    if not math.isfinite(v):
        return "-"
    if pct:
        return "%+.2f%%" % (100.0 * v)
    return ("%%.%df" % nd) % v


def main() -> int:
    lines: List[str] = []
    A = load("exp03_e1_analytic.json")
    B = load("exp03_e3_hst.json")
    P = load("exp03_e2_real.json")
    G = load("exp03_e4_gates.json")

    lines.append("# EXP-03 表格（自动生成，勿手改）\n")
    lines.append("源：`results/exp03_e1_analytic.json`、`exp03_e2_real.json`、"
                 "`exp03_e3_hst.json`、`exp03_e4_gates.json`\n")

    # ---- 表 A1 非退化负例 ----
    lines.append("\n## 表 A1 解析臂 · 非退化负例（真值无结构）\n")
    lines.append("| box | Δ(R0 vs 帧标量) | Δ(R1 vs 帧标量) | 帧标量 vs 真值 | R0 vs 真值 | R1 vs 真值 | R2 vs 真值 | R0 逐区域离散 | 1/sqrt(2N) |")
    lines.append("|---|---|---|---|---|---|---|---|---|")
    for r in A["a1_null"]["rows"]:
        lines.append("| %d | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["box"], fmt(r["delta_R0_vs_scalar_mean"], pct=True),
            fmt(r["delta_R1_vs_scalar_mean"], pct=True),
            fmt(r["scalar_rel_err_mean"], pct=True), fmt(r["R0_rel_err_mean"], pct=True),
            fmt(r["R1_rel_err_mean"], pct=True), fmt(r["R2_rel_err_mean"], pct=True),
            fmt(r["R0_scatter_over_regions"], pct=True),
            fmt(r["pred_stat_error_1_over_sqrt2N"], pct=True)))

    # ---- 表 A2 误差-尺度关系 ----
    lines.append("\n## 表 A2 解析臂 · 误差-尺度关系（逐区域，真值 σ_n=20 ADU）\n")
    lines.append("| 结构 | r=σ_s/σ_n | box | R0 偏差 | R0 统计离散 | R0 RMSE | R1 偏差 | R1 统计离散 | R1 RMSE | R2 偏差 | R2 统计离散 | R2 RMSE |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in A["a2_structure"]["rows"]:
        lines.append("| %s | %s | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["structure"], fmt(r["struct_over_noise"], 2), r["box"],
            fmt(r["R0_bias_median"], pct=True), fmt(r["R0_scatter"], pct=True), fmt(r["R0_rmse"], pct=True),
            fmt(r["R1_bias_median"], pct=True), fmt(r["R1_scatter"], pct=True), fmt(r["R1_rmse"], pct=True),
            fmt(r["R2_bias_median"], pct=True), fmt(r["R2_scatter"], pct=True), fmt(r["R2_rmse"], pct=True)))

    # ---- 表 A3 空间变化 sigma ----
    a3 = A.get("a3_varying_sigma", {})
    if a3.get("available"):
        lines.append("\n## 表 A3 解析臂 · 噪声本身是位置函数（真值 σ 帧内跨 %.2f 倍）\n"
                     % a3["sigma_true_frame_p95_over_p05"])
        lines.append("| box | 帧标量逐区域 p95 误差 | R1 逐区域 p95 误差 | R2 逐区域 p95 误差 |")
        lines.append("|---|---|---|---|")
        for r in a3["rows"]:
            lines.append("| %d | %s | %s | %s |" % (
                r["box"], fmt(r["scalar_abs_rel_err_p95"], pct=True),
                fmt(r["R1_abs_rel_err_p95"], pct=True), fmt(r["R2_abs_rel_err_p95"], pct=True)))

    # ---- 表 A4 闭式对拍 ----
    lines.append("\n## 表 A4 解析臂 · R0 闭式偏差式对拍 σ̂/σ_n = sqrt(1+(sB)²/(12σ²))\n")
    lines.append("| slope | box | 预测 | 实测 | 差 |")
    lines.append("|---|---|---|---|---|")
    for r in A["a4_formula"]["rows"]:
        lines.append("| %s | %d | %s | %s | %s |" % (
            fmt(r["slope"], 3), r["box"], fmt(r["pred_bias"], pct=True),
            fmt(r["meas_bias"], pct=True), fmt(r["abs_diff"], 5)))

    # ---- 表 B1 HST ----
    lines.append("\n## 表 B1 HST 前向臂 · 绝对准确性（有真值）与跨帧一致性\n")
    lines.append("| 场景 | box | 真值 σ_bg | 真值 σ_local | σ_local 离散 | 帧标量 p95 误差 | R0 p95 | R1 vs σ_bg 中位 | R1 vs σ_local 中位 | R1 p95 | R2 vs σ_local 中位 | R2 p95 | 区域跨帧 p95/p05 | A2_reg 中位 | 认证比 |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for s in B["scenarios"]:
        for r in s["rows"]:
            lines.append("| %s | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                s["scenario"], r["box"], fmt(r["sigma_true_bg_median"], 3),
                fmt(r["sigma_true_local_median"], 3), fmt(r["sigma_true_local_p95_over_p05"], 3),
                fmt(r["frame_scalar_abs_rel_err_vs_local_p95"], pct=True),
                fmt(r["R0_abs_rel_err_vs_local_p95"], pct=True),
                fmt(r["R1_rel_err_vs_bg_median"], pct=True),
                fmt(r["R1_rel_err_vs_local_median"], pct=True),
                fmt(r["R1_abs_rel_err_vs_local_p95"], pct=True),
                fmt(r["R2_rel_err_vs_local_median"], pct=True),
                fmt(r["R2_abs_rel_err_vs_local_p95"], pct=True),
                fmt(r["region_cross_frame_p95_over_p05_median"], 3),
                fmt(r["A2_reg_median"], 3), fmt(r["certified_frac"], 2)))

    # ---- 表 C1 真实帧前提检验 ----
    lines.append("\n## 表 C1 真实数据臂 · 前提检验（差分场功率闭合 + 空间结构）\n")
    lines.append("| panel | 帧对 | 均值差 [ADU] | 天电平差 [ADU] | **逐区域 excess 中位** | p05 | p95（单侧） | 全局口径（诊断） | 单帧功率@256 | 差分功率@256 | 去亚像素后@256 | 亚像素残差 [px] |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for p in P["panels"]:
        pcs = {(q["i"], q["j"]): q for q in p["pairs"] if q.get("kind") == "power_closure"}
        for q in p["pairs"]:
            if q.get("kind") == "power_closure" or q["box"] != 64:
                continue
            pc = pcs.get((q["i"], q["j"]), {})
            lines.append("| %s | %d-%d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                p["panel"], q["i"], q["j"], fmt(q.get("mean_diff_adu"), 2),
                fmt(pc.get("sky_level_diff_adu"), 1),
                fmt(q.get("region_excess_median"), pct=True),
                fmt(q.get("region_excess_p05"), pct=True),
                fmt(q.get("region_excess_p95"), pct=True),
                fmt(pc.get("var_excess_frac"), pct=True),
                fmt(q.get("single_frame_power@256"), pct=True),
                fmt(q.get("interaction_power@256"), pct=True),
                fmt(q.get("interaction_power_after_grad@256"), pct=True),
                fmt(q.get("subpix_offset_px"), 3)))

    # ---- 表 C2 跨帧一致性 ----
    lines.append("\n## 表 C2 真实数据臂 · 区域化 σ 与帧级标量的跨帧一致性对照\n")
    lines.append("| panel | 帧数 | 帧级标量值 [ADU] | 帧级标量 p95/p05 | 区域 σ 中位 | 同区域跨帧 p95/p05 中位 | p95 | 帧内区域离散 | 逐帧解 σ [ADU] |")
    lines.append("|---|---|---|---|---|---|---|---|---|")
    for p in P["panels"]:
        cf = p["cross_frame"]
        sv = p["sigma_solve"]
        lines.append("| %s | %d | %s | %s | %s | %s | %s | %s | %s |" % (
            p["panel"], p["n_frames"],
            ", ".join(fmt(x, 2) for x in cf["frame_scalar_values"]),
            fmt(cf["frame_scalar_p95_over_p05"], 3), fmt(cf["region_sigma_median"], 2),
            fmt(cf["region_cross_frame_p95_over_p05_median"], 3),
            fmt(cf["region_cross_frame_p95_over_p05_p95"], 3),
            fmt(cf["region_within_frame_dispersion_median"], 3),
            ", ".join(fmt(x, 2) for x in sv["sigma_per_frame"]) if sv.get("available") else "-"))

    # ---- 表 D1 门与注入 ----
    lines.append("\n## 表 D1 判据与故障注入\n")
    lines.append("| 门/注入 | 结果 | 关键量 |")
    lines.append("|---|---|---|")
    for g in G["gates"]:
        key = ", ".join("%s=%s" % (k, fmt(v, 4)) for k, v in g.items()
                        if k not in ("gate", "pass", "rows", "detail", "note")
                        and isinstance(v, (int, float)))
        lines.append("| %s | %s | %s |" % (g["gate"], "PASS" if g["pass"] else "FAIL", key))
    for i in G["injections"]:
        key = ", ".join("%s=%s" % (k, fmt(v, 4)) for k, v in i.items()
                        if k not in ("injection", "pass", "note")
                        and isinstance(v, (int, float)))
        lines.append("| inj:%s | %s | %s |" % (i["injection"], "PASS" if i["pass"] else "FAIL", key))

    out = RES / "EXP03_TABLES.md"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("wrote", out, len(lines), "lines")

    # ---- 图 ----
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        figdir = RES / "exp03_figs"
        figdir.mkdir(exist_ok=True)
        # 图 1：误差-尺度关系（解析臂）
        fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
        for est, col in (("R0", "tab:red"), ("R1", "tab:orange"), ("R2", "tab:blue")):
            for nm, ls in (("null", ":"), ("corr32", "--"), ("ramp_hi", "-"), ("blob32", "-.")):
                rs = [r for r in A["a2_structure"]["rows"] if r["structure"] == nm]
                rs.sort(key=lambda r: r["box"])
                ax[0].plot([r["box"] for r in rs], [100 * r[est + "_rmse"] for r in rs],
                           ls, color=col, marker="o", ms=3, label="%s/%s" % (est, nm))
        ax[0].set_xscale("log", base=2); ax[0].set_yscale("log")
        ax[0].set_xlabel("region size B [px]"); ax[0].set_ylabel("per-region RMSE [%]")
        ax[0].set_title("analytic: error vs region size"); ax[0].legend(fontsize=5, ncol=2)
        ax[0].grid(alpha=.3, which="both")
        for s in B["scenarios"]:
            if s["scenario"] not in ("hst_null_nostars", "hst_struct_nostars_p999_1000"):
                continue
            rs = sorted(s["rows"], key=lambda r: r["box"])
            for est, col in (("R1", "tab:orange"), ("R2", "tab:blue")):
                ax[1].plot([r["box"] for r in rs],
                           [100 * abs(r[est + "_rel_err_vs_local_median"]) for r in rs],
                           "-o", ms=3, color=col,
                           label="%s/%s" % (est, s["scenario"].replace("hst_", "")))
            ax[1].plot([r["box"] for r in rs],
                       [100 * abs(r["frame_scalar_rel_err_vs_local"][0]) for r in rs],
                       "-s", ms=3, color="k", label="scalar/" + s["scenario"].replace("hst_", ""))
        ax[1].set_xscale("log", base=2); ax[1].set_yscale("log")
        ax[1].set_xlabel("region size B [px]"); ax[1].set_ylabel("|median bias| [%]")
        ax[1].set_title("HST forward sim: absolute bias vs region size")
        ax[1].legend(fontsize=6); ax[1].grid(alpha=.3, which="both")
        fig.tight_layout(); fig.savefig(figdir / "a_error_vs_scale.png", dpi=130); plt.close(fig)
        # 图 2：真实帧前提检验
        fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
        xs, ys = [], []
        for p in P["panels"]:
            for q in p["pairs"]:
                if q.get("kind") == "power_closure":
                    continue
                lags = sorted(int(k.split("@")[1]) for k in q if k.startswith("diff_S_over_S1@"))
                ax[0].plot(lags, [q["diff_S_over_S1@%d" % L] for L in lags], "-o", ms=3,
                           label="%s %d-%d" % (p["panel"], q["i"], q["j"]))
                if q["box"] == 64:
                    xs.append(256 * 1.06)
                    ys.append(1.0 + q["single_frame_power@256"])
        # 同一批帧对的**单帧**结构函数值（右端空心方块）—— 差分把它从 1.7~6.3 压到 1.04~1.14
        ax[0].plot(xs, ys, "s", mfc="none", mec="k", ms=6, ls="none",
                   label="single frame (same pairs, at L=256)")
        ax[0].axhline(1.0, color="k", lw=.8)
        ax[0].set_xscale("log", base=2); ax[0].set_xlabel("lag L [px]")
        ax[0].set_ylabel("S(L)/S(1)"); ax[0].set_yscale("log")
        ax[0].set_title("real: structure function -- difference (lines) vs single frame (squares)")
        ax[0].legend(fontsize=5, ncol=2); ax[0].grid(alpha=.3, which="both")
        # 右：帧级标量 vs 逐区域 sigma 的中位与 5/95 分位 —— 帧级标量高出 2~3 倍且完全丢掉了 10 倍的空间跨度
        for k, p in enumerate(P["panels"]):
            cf = p["cross_frame"]
            sc = cf["frame_scalar_values"]
            ax[1].plot([k - .18] * len(sc), sc, "s", color="k", ms=5)
            rg = [f["region_sigma_median"] for f in p["frames"]]
            ax[1].plot([k + .18] * len(rg), rg, "o", color="tab:blue", ms=5)
            d = p["frames"][0]["region_sigma_dispersion"]
            m = float(np.median(rg))
            ax[1].plot([k + .18, k + .18], [m / max(d, 1e-9) ** .5, m * max(d, 1e-9) ** .5],
                       "-", color="tab:blue", lw=2, alpha=.5)
        ax[1].plot([], [], "s", color="k", label="frame-level scalar (current)")
        ax[1].plot([], [], "o-", color="tab:blue", label="regional sigma (median; bar = p05..p95)")
        ax[1].set_yscale("log"); ax[1].set_xticks(range(len(P["panels"])))
        ax[1].set_xticklabels([p["panel"] for p in P["panels"]])
        ax[1].set_xlabel("panel"); ax[1].set_ylabel("sigma [ADU]")
        ax[1].set_title("real: frame scalar vs regional sigma (log scale)")
        ax[1].legend(fontsize=6); ax[1].grid(alpha=.3, which="both")
        fig.tight_layout(); fig.savefig(figdir / "b_real_premise.png", dpi=130); plt.close(fig)
        print("wrote figures to", figdir)
    except Exception as exc:
        print("figure generation skipped:", repr(exc))
    try:
        tags = patch_report({"e2_real": P})
        print("patched report tables:", tags)
    except Exception as exc:
        print("report patch skipped:", repr(exc))
    return 0


# ---------------------------------------------------------------------------
# 报告内联表的自动生成（保持「报告 = 机器生成表 + 人工解读」的同步）
# ---------------------------------------------------------------------------
DOC = ROOT / "实验" / "absolute-snr" / "docs" / "EXP-03-REGIONAL-SIGMA.md"


def _wrap(tag: str, body: str) -> str:
    return ("<!-- BEGIN AUTO:%s -->\n" % tag) + body.rstrip() + ("\n<!-- END AUTO:%s -->" % tag)


def _blocks(d: Dict[str, Any]) -> Dict[str, str]:
    """从 exp03_e2_real.json 生成报告里三张真实数据表。"""
    # --- 表 C1：逐区域前提闭合 ---
    L = ["| panel | 帧对 | **逐区域 excess 中位** | p05 | p95（单侧判据） | \\|excess\\|>5% 的区域占比 | 全局口径（仅诊断） |",
         "|---|---|---|---|---|---|---|"]
    for p in d["panels"]:
        P = {(x["i"], x["j"]): x for x in p["pairs"] if x.get("box") == 64}
        for q in p["pairs"]:
            if q.get("kind") != "power_closure":
                continue
            x = P[(q["i"], q["j"])]
            L.append("| %s | %d-%d | **%+.2f%%** | %+.1f%% | %+.1f%% | %.0f%% | %+.1f%% |" % (
                p["panel"], q["i"], q["j"], 100 * x["region_excess_median"],
                100 * x["region_excess_p05"], 100 * x["region_excess_p95"],
                100 * x["region_excess_frac_beyond_5pct"], 100 * q["var_excess_frac"]))
    L += ["",
          "`excess_r = 2*sigma_D,r^2/(sigma_a,r^2+sigma_b,r^2) - 1`（`sigma_D,r` = 逐区域 `clipped RMS(I_a-I_b)/sqrt2`）。",
          "**判据是单侧的**：前提被破坏（帧相关空间形态）只会让 `excess` **变正**；",
          "R1 被亚网格结构污染只会让它**变负**。实测 `excess` 中位 -0.1%~-6.7%、p95 <= +5.5% ⇒ **无正超出**。",
          "全局口径（`RMS(D)^2` 对 `median_r(sigma_r)^2` 之和）在异质 sigma 场上不可比，仅作诊断。"]
    premise = "\n".join(L)

    # --- 表 C2：结构函数（位置稳定性的直接证据） ---
    L = ["| panel | 帧对 | 残余亚像素平移 [px] | 单帧 `S(256)/S(1)-1` | **差分场 `S(256)/S(1)-1`** | 位置稳定占比 |",
         "|---|---|---|---|---|---|"]
    for p in d["panels"]:
        for q in p["pairs"]:
            if q.get("box") != 64:
                continue
            s = q["single_frame_power@256"]
            g = q["interaction_power@256"]
            L.append("| %s | %d-%d | %.3f | **+%.1f%%** | +%.1f%% | %.0f%% |" % (
                p["panel"], q["i"], q["j"], q["subpix_offset_px"], 100 * s, 100 * g,
                100 * max(0.0, 1.0 - g / max(s, 1e-9))))
    L += ["", "`S(L) = 0.5*<(I(x+L)-I(x))^2>`，`S(L)/S(1)-1` = 尺度 < L 的图案功率 / 噪声功率（生产裁剪 RMS 稳健估计）。",
          "**注意**：差分场列是**上界**（含未被分离的亚像素错位贡献，见报告 §2.4）。",
          "`interaction_power_after_grad@256`（梯度回归残差）**不是有效分离量**，故不出表。"]
    struct = "\n".join(L)

    # --- 表 C3：跨帧一致性 ---
    L = ["| panel | 帧数 | 帧级标量 sigma [ADU]（跨帧 p95/p05） | 逐区域 sigma 中位 [ADU]（跨帧 p95/p05，区域中位） | 帧内区域 sigma 离散 p95/p05 | 逐帧 sigma 最小二乘解 [ADU] |",
         "|---|---|---|---|---|---|"]
    for p in d["panels"]:
        x = p["cross_frame"]
        sv = p.get("sigma_solve", {})
        sol = (" / ".join("%.2f" % v for v in sv["sigma_per_frame"])) if sv.get("available") else "（K<3，不可解）"
        L.append("| %s | %d | %.2f-%.2f（%.3f） | %.2f-%.2f（%.3f） | %.2f | %s |" % (
            p["panel"], p["n_frames"],
            min(x["frame_scalar_values"]), max(x["frame_scalar_values"]), x["frame_scalar_p95_over_p05"],
            min(f["region_sigma_median"] for f in p["frames"]),
            max(f["region_sigma_median"] for f in p["frames"]),
            x["region_cross_frame_p95_over_p05_median"],
            x["region_within_frame_dispersion_median"], sol))
    xframe = "\n".join(L)
    return {"premise": premise, "structure": struct, "xframe": xframe}


def patch_report(res: Dict[str, Any]) -> List[str]:
    """把三张自动表写回报告（幂等：以 AUTO 注释块为界替换）。"""
    import re
    d = res.get("e2_real")
    if not d or not DOC.exists():
        return []
    txt = DOC.read_text(encoding="utf-8")
    done = []
    for tag, body in _blocks(d).items():
        blk = _wrap(tag, body)
        pat = re.compile(r"<!-- BEGIN AUTO:%s -->.*?<!-- END AUTO:%s -->" % (tag, tag), re.S)
        if pat.search(txt):
            txt = pat.sub(lambda m: blk, txt)
        else:
            ph = {"premise": "REAL_PREMISE_TABLE_PLACEHOLDER",
                  "structure": "REAL_STRUCTURE_TABLE_PLACEHOLDER",
                  "xframe": "REAL_XFRAME_TABLE_PLACEHOLDER"}[tag]
            if ph not in txt:
                continue
            txt = txt.replace(ph, blk)
        done.append(tag)
    DOC.write_text(txt, encoding="utf-8")
    return done



if __name__ == "__main__":
    raise SystemExit(main())
