#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 摘要：把 results/exp04_*.json 压成一份紧凑文本，供报告引用（不替代原始 JSON）。"""
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


def load(n):
    p = os.path.join(R, n)
    return json.load(open(p, encoding="utf-8")) if os.path.exists(p) else None


def g(r, k):
    v = r.get(k)
    if v is None:
        return "  —  "
    try:
        v = float(v)
    except (TypeError, ValueError):
        return str(v)
    return "%7.4f" % v if np.isfinite(v) else "   nan "


def main():
    e1, e2, e3 = load("exp04_e1_analytic.json"), load("exp04_e2_hst.json"), load("exp04_e3_real.json")
    e4, e5 = load("exp04_e4_gates.json"), load("exp04_e5_cost.json")
    e6, e7, e8 = load("exp04_e6_boundary.json"), load("exp04_e7_crossframe.json"), load("exp04_e8_review_b3.json")

    faces = []
    if e1:
        faces += [(t, fc) for t, fc in e1["faces"].items()]
    if e2:
        faces.append(("HST_M16", dict(rows=e2["rows"], ell_meas_px=e2["ell_meas_px"],
                                      s_field_log10_std=e2["s_field_log10_std"])))
    if e3:
        faces += [("M42:" + t[:16], dict(rows=fc["rows"], ell_meas_px=fc["ell_meas_px"],
                                         s_field_log10_std=fc["s_field_log10_std"]))
                  for t, fc in e3["faces"].items()]

    OPS = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
           "photutils_zoom3", "photutils_zoom1", "idw", "tps", "tps_smooth",
           "rbf_mq", "rbf_gauss", "gpr_rbf", "gpr_matern32", "gpr_exp"]
    FIX = ["dense_P32", "dense_at_D", "frame_median", "frame_global_mad"]

    print("=" * 110)
    print("A. 各面 E（权重效率损失，estimated 模式）随 Δ —— 只列关键臂")
    print("=" * 110)
    for tag, fc in faces:
        print("\n### %s   ell_meas=%s px  s_field=%s dex"
              % (tag, fc.get("ell_meas_px"), fc.get("s_field_log10_std")))
        hdr = "  Δ   " + "".join("%9s" % o[:8] for o in
                                 ["nn", "bilinear", "bicubic", "spline", "sextr", "pzoom3", "pzoom1",
                                  "idw", "gpr_rbf", "gpr_mat3"]) + "".join("%9s" % o[:8] for o in
                                 ["denseP32", "denseAtD", "frame", "frameMAD"])
        print(hdr)
        for D in E.DELTA_GRID:
            idx = {r["arm"]: r for r in fc["rows"] if r["delta_px"] == D and r["mode"] == "estimated"}
            if not idx:
                continue
            line = "%4d  " % D + "".join("%9s" % g(idx.get(o, {}), "eff_loss") for o in
                                         ["nn", "bilinear", "bicubic_cc", "spline_natural",
                                          "sextractor_spline", "photutils_zoom3", "photutils_zoom1",
                                          "idw", "gpr_rbf", "gpr_matern32"])
            line += "".join("%9s" % g(idx.get(o, {}), "eff_loss") for o in FIX)
            best = min((r for r in idx.values() if r.get("eff_loss") is not None
                        and np.isfinite(r["eff_loss"])), key=lambda r: r["eff_loss"], default=None)
            if best:
                line += "   best=%s(%s)" % (best["arm"], g(best, "eff_loss").strip())
            print(line)

    print("\n" + "=" * 110)
    print("B. oracle vs estimated（算子自身插值误差 vs 控制点估计噪声），Δ=64")
    print("=" * 110)
    for tag, fc in faces:
        o = {r["arm"]: r for r in fc["rows"] if r["delta_px"] == 64 and r["mode"] == "oracle"}
        s = {r["arm"]: r for r in fc["rows"] if r["delta_px"] == 64 and r["mode"] == "estimated"}
        if not o or not s:
            continue
        print("\n%s" % tag)
        for op in OPS:
            if op in o and op in s:
                print("   %-18s E_oracle=%s  E_est=%s   ΔE=%s" % (
                    op, g(o[op], "eff_loss").strip(), g(s[op], "eff_loss").strip(),
                    ("%7.4f" % (s[op]["eff_loss"] - o[op]["eff_loss"]))
                    if np.isfinite(s[op]["eff_loss"]) and np.isfinite(o[op]["eff_loss"]) else "  —  "))

    if e4:
        print("\n" + "=" * 110)
        print("C. 负例门")
        print("=" * 110)
        for k in ("G1_zero_effect_zero", "G2_DEGENERATE_flat_field_ranking_never_true",
                  "G3_adversarial_shuffle_red", "G4_tautology_recheck",
                  "G5_nn_baseline_nondegenerate", "G6_frame_arm_selfzero"):
            v = e4.get(k)
            if isinstance(v, dict):
                print("  %-46s %s" % (k, v.get("ok", v.get("all_ok", "登记"))))
        if "G3_adversarial_shuffle_red" in e4:
            print("  G3 frame E=%.4f, 洗牌后 E 范围 %.4f–%.4f" % (
                e4["G3_adversarial_shuffle_red"]["frame_eff_loss"],
                min(r["eff_loss_shuffled"] for r in e4["G3_adversarial_shuffle_red"]["rows"]),
                max(r["eff_loss_shuffled"] for r in e4["G3_adversarial_shuffle_red"]["rows"])))

    if e5:
        print("\n" + "=" * 110)
        print("D. 代价：t(1024²输出) 与 N；存储")
        print("=" * 110)
        for op, fit in e5["n_scaling_fit"].items():
            rr = {r["delta_px"]: r for r in e5["rows"] if r["op"] == op and r["measured"]}
            print("  %-18s slope=%s  t(Δ256,N=16)=%s t(Δ64,N=256)=%s t(Δ16,N=4096)=%s"
                  % (op, ("%.2f" % fit["loglog_slope_vs_N"]) if fit["loglog_slope_vs_N"] is not None else "—",
                     ("%.4f" % rr[256]["recon_s"]) if 256 in rr else "—",
                     ("%.3f" % rr[64]["recon_s"]) if 64 in rr else "—",
                     ("%.2f" % rr[16]["recon_s"]) if 16 in rr else "—"))
        for s in e5["storage"]:
            print("  storage %-7s Δ=%-5s N=%-9d %10d B  %.4f MiB  x预算=%.4f"
                  % (s["kind"], s["delta_px"], s["n_ctrl"], s["bytes_per_frame"], s["MiB"], s["over_budget"]))
        print("  size scaling:", [(r["H"], r["op"], round(r["recon_s"], 3)) for r in e5["size_scaling"]])

    if e6:
        print("\n" + "=" * 110)
        print("E. 边界")
        print("=" * 110)
        for r in e6["B1_invalid"]:
            if r["nan_policy"] == "nearest_valid":
                print("  B1 %-18s finite=%s leak=%d outside_px=%-6d radius=%.1f maxdev_out=%s"
                      % (r["op"], r["finite"], r["nan_leak"], r["dev_px_outside_invalid"],
                         r["prop_radius_cell"], r.get("max_dev_outside_dex")))
        for r in e6["B2_step"]:
            print("  B2 %-18s overshoot=%.4f ringing=%.4f range=[%.3f,%.3f]"
                  % (r["op"], r["overshoot_frac_amp"], r["ringing_frac_amp"], r["out_min"], r["out_max"]))
        for r in e6["B3_border"]:
            print("  B3 %-18s interior=%.5f border=%.5f bias_border=%.5f"
                  % (r["op"], r["rmse_interior_dex"], r["rmse_border_dex"], r["bias_border_dex"]))

    if e7:
        print("\n" + "=" * 110)
        print("F. 跨帧一致性（log10 域离散中位数 dex）")
        print("=" * 110)
        for k, v in e7["C1a_analytic_multi_realization"].items():
            b = (e7.get("C1b_hst_multi_realization") or {}).get(k)
            print("  %-22s 解析=%s  HST=%s" % (k, v.get("scatter_median_dex"),
                                               (b or {}).get("scatter_median_dex")))
        for tag, fc in e7["C2_real_checkerboard_split"].items():
            print("  %s" % tag[:30])
            for k, v in fc["rows"].items():
                print("     %-22s split_scatter=%s" % (k, v.get("split_scatter_median_dex")))

    if e8:
        print("\n" + "=" * 110)
        print("G. B3 复核")
        print("=" * 110)
        for r in e8["R1_hst_cell_mad_bias_vs_delta"]["rows"]:
            print("  HST Δ=%3d cell_bias=%.4f sparse_bias=%.4f dense_bias=%.4f rmse_sparse=%.4f rmse_frame=%.4f worse=%s"
                  % (r["delta_px"], r["bias_cell_mad_vs_patchrms_dex"], r["bias_sparse_vs_patchrms_dex"],
                     r["bias_dense_dex"], r["rmse_sparse"], r["rmse_frame"], r["sparse_worse_than_frame"]))
        print("  HST Δ* = %s (%s)" % (e8["R2_delta_star_hst"]["delta_star_px"],
                                      e8["R2_delta_star_hst"]["status"]))
        for s in e8["R2b_delta_star_synthetic"]:
            print("  synth ℓ=%.0f Δ* = %s (%s)" % (s["ell_nominal_px"], s["delta_star_px"], s["status"]))
        print("  R3:", json.dumps(e8["R3_storage_gate"], ensure_ascii=False))
        print("  R5:", json.dumps(e8["R5_tautology"]["rmse_frame_vs_sfield"], ensure_ascii=False))


if __name__ == "__main__":
    main()
