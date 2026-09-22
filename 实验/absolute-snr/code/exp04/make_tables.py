#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 汇总表：从 results/exp04_*.json 生成 results/EXP04_TABLES.md。"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)
import sci_b_common as C      # noqa: E402
import exp04_common as E      # noqa: E402

R = C.RESULTS


def load(name):
    p = os.path.join(R, name)
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def f(x, nd=4):
    if x is None:
        return "—"
    try:
        v = float(x)
    except (TypeError, ValueError):
        return str(x)
    if not np.isfinite(v):
        return "nan"
    return ("%%.%df" % nd) % v


def tbl(header, rows):
    out = ["| " + " | ".join(header) + " |",
           "|" + "|".join(["---"] * len(header)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(str(x) for x in r) + " |")
    return "\n".join(out)


def main():
    L = []
    e1, e2, e3 = load("exp04_e1_analytic.json"), load("exp04_e2_hst.json"), load("exp04_e3_real.json")
    e4, e5 = load("exp04_e4_gates.json"), load("exp04_e5_cost.json")
    e6, e7, e8 = load("exp04_e6_boundary.json"), load("exp04_e7_crossframe.json"), load("exp04_e8_review_b3.json")

    faces = []
    if e1:
        faces += [(t, fc["rows"]) for t, fc in e1["faces"].items()]
    if e2:
        faces.append(("hst_m16", e2["rows"]))
    if e3:
        faces += [("m42:" + t[:14], fc["rows"]) for t, fc in e3["faces"].items()]

    if faces:
        L.append("## T1 算子主对比表（Δ=64，estimated 模式：控制值来自数据稳健 MAD）\n")
        arms = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
                "photutils_zoom3", "photutils_zoom1", "idw", "tps", "tps_smooth",
                "rbf_mq", "rbf_gauss", "gpr_rbf", "gpr_matern32", "gpr_exp",
                "dense_P32", "dense_at_D", "frame_median", "frame_global_mad"]
        idx = {}
        for tag, rr in faces:
            for r in rr:
                if r["delta_px"] == 64 and r["mode"] in ("estimated", "fixed"):
                    idx[(tag, r["arm"])] = r
        rows = []
        for arm in arms:
            cells = [arm]; any_ = False
            for tag, _ in faces:
                r = idx.get((tag, arm))
                if r is None:
                    cells.append("—")
                else:
                    any_ = True
                    cells.append("%s / %s" % (f(r["eff_loss"]), f(r["rmse_log_rho"])))
            if any_:
                rows.append(cells)
        L.append(tbl(["arm（E / RMSE dex）"] + [t for t, _ in faces], rows))
        L.append("")

        L.append("## T2 适用域：各面各 Δ 的最优臂（按 E 最小）\n")
        rows = []
        for tag, rr in faces:
            for D in E.DELTA_GRID:
                sel = [r for r in rr if r["delta_px"] == D and r["mode"] in ("estimated", "fixed")
                       and r["eff_loss"] is not None and np.isfinite(r["eff_loss"])]
                if not sel:
                    continue
                best = min(sel, key=lambda r: r["eff_loss"])
                fr = next((r for r in sel if r["arm"] == "frame_median"), None)
                dn = next((r for r in sel if r["arm"] == "dense_P32"), None)
                rows.append([tag, D, best["arm"], f(best["eff_loss"]),
                             f(fr["eff_loss"]) if fr else "—",
                             f(dn["eff_loss"]) if dn else "—",
                             best["arm"] if best["arm"] in ("dense_P32", "frame_median")
                             else "sparse"])
        L.append(tbl(["face", "Δ", "best_arm", "E_best", "E_frame", "E_dense_P32", "best_kind"], rows))
        L.append("")

    if e8:
        L.append("## T3 失效边界 Δ*（sparse 双线性 RMSE 首次超过帧级臂）\n")
        L.append("HST M16（2048 crop）：Δ* = %s（%s）\n" % (e8["R2_delta_star_hst"]["delta_star_px"],
                                                       e8["R2_delta_star_hst"]["status"]))
        L.append(tbl(["Δ", "RMSE_sparse", "RMSE_frame", "sparse_worse"],
                     [[x["delta_px"], f(x["rmse_sparse"]), f(x["rmse_frame"]), x["sparse_worse_than_frame"]]
                      for x in e8["R2_delta_star_hst"]["rows"]]))
        L.append("")
        L.append("纯合成面（无未分辨结构）：")
        for s in e8["R2b_delta_star_synthetic"]:
            L.append("- ℓ=%.0f px ⇒ Δ* = %s（%s）" % (s["ell_nominal_px"], s["delta_star_px"], s["status"]))
        L.append("")
        L.append("HST cell 稳健 MAD 相对同 Δ cell 真值 RMS 的抬偏（dex）：")
        L.append(tbl(["Δ", "cell_bias", "sparse_bias", "dense_bias"],
                     [[x["delta_px"], f(x["bias_cell_mad_vs_patchrms_dex"]),
                       f(x["bias_sparse_vs_patchrms_dex"]), f(x["bias_dense_dex"])]
                      for x in e8["R1_hst_cell_mad_bias_vs_delta"]["rows"]]))
        L.append("")

    if e5:
        L.append("## T4 代价：重建时间（1024² 输出）与 N 标度\n")
        rows = []
        for op, fit in e5["n_scaling_fit"].items():
            rr = [r for r in e5["rows"] if r["op"] == op and r["measured"]]
            if not rr:
                continue
            r64 = next((r for r in rr if r["delta_px"] == 64), None)
            r16 = next((r for r in rr if r["delta_px"] == 16), None)
            rows.append([op, f(r64["recon_s"], 3) if r64 else "—",
                         f(r16["recon_s"], 3) if r16 else "—",
                         f(fit["loglog_slope_vs_N"], 2), fit["n_points"]])
        L.append(tbl(["op", "t(Δ=64,N=256) s", "t(Δ=16,N=4096) s", "log-log slope vs N", "n_pts"], rows))
        L.append("")
        L.append("存储量（生产帧 4096²）：\n")
        L.append(tbl(["kind", "Δ", "N_ctrl", "bytes/frame", "MiB", "相对 1 MiB 预算"],
                     [[s["kind"], s["delta_px"] or "—", s["n_ctrl"], s["bytes_per_frame"],
                       f(s["MiB"], 4), f(s["over_budget"], 4)] for s in e5["storage"]]))
        L.append("")

    if e6:
        L.append("## T5 边界行为\n")
        L.append("无效区（NaN 3×3 cell 块 + 顶行，nearest_valid 策略）：\n")
        L.append(tbl(["op", "有限", "NaN 泄漏", "污染足迹 px", "传播半径 cell", "无效区外最大偏差 dex"],
                     [[r["op"], r.get("finite"), r.get("nan_leak"),
                       r.get("dev_px_outside_invalid"), f(r.get("prop_radius_cell"), 1),
                       f(r.get("max_dev_outside_dex"))] for r in e6["B1_invalid"]
                      if r["nan_policy"] == "nearest_valid"]))
        L.append("")
        L.append("阶跃场（1.0 → 3.0，幅度为 1）：\n")
        L.append(tbl(["op", "超调/幅度", "振铃/幅度", "输出 min", "输出 max"],
                     [[r["op"], f(r["overshoot_frac_amp"]), f(r["ringing_frac_amp"]),
                       f(r["out_min"], 3), f(r["out_max"], 3)] for r in e6["B2_step"]]))
        L.append("")
        L.append("图像边界（最外 Δ 带 vs 内部）：\n")
        L.append(tbl(["op", "RMSE 内部 dex", "RMSE 边界 dex", "边界偏差 dex"],
                     [[r["op"], f(r["rmse_interior_dex"]), f(r["rmse_border_dex"]),
                       f(r["bias_border_dex"])] for r in e6["B3_border"]]))
        L.append("")

    if e7:
        L.append("## T6 跨帧一致性（log10 域跨实现离散，中位数 dex）\n")
        rows = []
        for k in sorted(e7["C1a_analytic_multi_realization"].keys()):
            a = e7["C1a_analytic_multi_realization"][k]
            b = (e7.get("C1b_hst_multi_realization") or {}).get(k)
            rows.append([k, f(a.get("scatter_median_dex")), f(a.get("scatter_p95_dex")),
                         f(b.get("scatter_median_dex")) if b else "—",
                         f(b.get("scatter_p95_dex")) if b else "—"])
        L.append(tbl(["arm", "解析面 中位", "解析面 p95", "HST 中位", "HST p95"], rows))
        L.append("")

    if e4:
        L.append("## T7 负例红/绿\n")
        rows = []
        for k in ("G1_zero_effect_zero", "G2_DEGENERATE_flat_field_ranking_never_true",
                  "G3_adversarial_shuffle_red", "G4_tautology_recheck",
                  "G5_nn_baseline_nondegenerate", "G6_frame_arm_selfzero"):
            v = e4.get(k)
            if not isinstance(v, dict):
                continue
            ok = v.get("ok", v.get("all_ok"))
            rows.append([k, ok if ok is not None else "登记", (v.get("note") or "")[:80]])
        L.append(tbl(["门", "结果", "说明"], rows))
        L.append("")

    if e8:
        L.append("## T8 B3 复核（独立实现重算）\n")
        rows = e8["R1_hst_cell_mad_bias_vs_delta"]["rows"]
        L.append("- HST Δ=%d sparse 水平偏差 = %s dex（B3 报 +0.0297）"
                 % (rows[0]["delta_px"], f(rows[0]["bias_sparse_vs_patchrms_dex"])))
        r256 = next((x for x in rows if x["delta_px"] == 256), None)
        if r256:
            L.append("- HST Δ=256 sparse 水平偏差 = %s dex（B3 报 +0.2626）"
                     % f(r256["bias_sparse_vs_patchrms_dex"]))
        L.append("- 存储门：dense 相对 1 MiB 预算 = %s 倍（B3 报 64 倍，FP32 口径）"
                 % f(e8["R3_storage_gate"]["dense_over_budget"], 1))
        L.append("- 恒真判据：%s" % json.dumps(e8["R5_tautology"]["rmse_frame_vs_sfield"], ensure_ascii=False))
        L.append("")

    txt = "\n".join(L)
    out = os.path.join(R, "EXP04_TABLES.md")
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("# EXP-04 汇总表（由 code/exp04/make_tables.py 生成）\n\n" + txt + "\n")
    print("wrote", out, len(txt), "chars")


if __name__ == "__main__":
    main()
