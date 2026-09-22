#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 臂①：纯解析代数合成面上的算子对比（含平坦场退化负例）。

面族：
  grf   : σ=10^(s·g_ℓ)，ℓ ∈ ELL_SYNTH，s ∈ S_FIELD_SYNTH（s=0 ⇒ 平坦场，真值无空间效应）
  unres : σ=10^(s·g_ℓ)·(1+a·u_{ℓ_u})，ℓ_u ∈ ELL_UNRES（cell 内未分辨结构，三因子之一）

输出：results/exp04_e1_analytic.json
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)
import sci_b_common as C      # noqa: E402
import operators as O         # noqa: E402
import exp04_common as E      # noqa: E402
import driver as DR           # noqa: E402

CHEAP = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
         "photutils_zoom3", "photutils_zoom1", "gpr_rbf", "gpr_matern32", "gpr_exp"]
HEAVY = ["idw", "tps", "tps_smooth", "rbf_mq", "rbf_gauss"]
PRIMARY_FACES = ["grf_ell16.0_s0.030", "grf_ell64.0_s0.100", "grf_ell64.0_s0.300"]
PRIMARY_DELTAS = [64, 128]


def build_faces(quick=False):
    faces = []
    i = 0
    ells = [16.0, 64.0] if quick else E.ELL_SYNTH
    ss = [0.0, 0.10] if quick else E.S_FIELD_SYNTH
    for s in ss:
        for ell in ells:
            i += 1
            tag = "grf_ell%.1f_s%.3f" % (ell, s)
            sigma = E.synth_sigma_face(ell, s, E.CROP, seed_off=100 + i)
            img, truth = E.synth_data_face(sigma, seed_off=200 + i)
            faces.append(dict(tag=tag, kind="grf", img=img, truth=truth, meta=dict(
                face="synthetic_grf", ell_nominal_px=ell, s_field_true_log10=s,
                a_unres=0.0, ell_unres=None)))
    for ell_u in (E.ELL_UNRES if not quick else [4.0]):
        for a_u in (E.A_UNRES if not quick else [0.30]):
            i += 1
            tag = "unres_ell64.0_s0.100_ellu%.1f_au%.2f" % (ell_u, a_u)
            sigma = E.synth_sigma_face(64.0, 0.10, E.CROP, seed_off=300 + i,
                                       ell_unres=ell_u, a_unres=a_u)
            img, truth = E.synth_data_face(sigma, seed_off=400 + i)
            faces.append(dict(tag=tag, kind="unres", img=img, truth=truth, meta=dict(
                face="synthetic_unresolved", ell_nominal_px=64.0, s_field_true_log10=0.10,
                a_unres=a_u, ell_unres=ell_u)))
    return faces


def delta_star(rows, arm_prefix="", metric="eff_loss", ref="frame_median"):
    """失效边界：该臂首次劣于参考臂（metric 更大）的最小 Δ；未越界登记 no_crossing。"""
    out = {}
    for D in E.DELTA_GRID:
        rr = {r["arm"]: r for r in rows if r["delta_px"] == D and r["mode"] in ("oracle", "estimated")}
        f = next((r for r in rows if r["delta_px"] == D and r["arm"] == ref), None)
        if not rr or f is None:
            continue
        best = min(rr.values(), key=lambda r: (np.inf if not np.isfinite(r[metric] or np.nan)
                                               else r[metric]))
        fv = f[metric]
        if not np.isfinite(fv):
            continue
        if best[metric] > fv:
            out["delta_star_px"] = D
            out["best_arm_at_cross"] = best["arm"]
            out["status"] = "crossed"
            return out
    out["delta_star_px"] = None
    out["status"] = "no_crossing"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e1_analytic.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    faces = build_faces(a.quick)
    deltas = [16, 64] if a.quick else E.DELTA_GRID
    out_faces, all_rows = {}, []
    for fc in faces:
        rows = DR.run_face(fc["tag"], fc["img"], fc["truth"], fc["meta"], CHEAP, deltas,
                           modes=("oracle", "estimated"), gran="pixel")
        # 重算子只在主对比子集上跑（成本见 results/exp04_e5_cost.json）
        heavy_note = "not_run_cost"
        if (not a.quick) and fc["tag"] in PRIMARY_FACES:
            rows += DR.run_face(fc["tag"], fc["img"], fc["truth"], fc["meta"], HEAVY, PRIMARY_DELTAS,
                                modes=("oracle", "estimated"), gran="pixel")
            heavy_note = "run"
        # 与 B3 同粒度的 patch32 评价（仅便宜算子，供三臂同表比较）
        rows32 = DR.run_face(fc["tag"], fc["img"], fc["truth"], fc["meta"], CHEAP, deltas,
                             modes=("oracle", "estimated"), gran="pixel",
                             patch_agg_ops=True) if False else []
        s_field = float(np.nanstd(np.log10(np.abs(fc["truth"]))))
        if s_field <= 0:
            ell_meas = float("nan")     # 真值场为常数 ⇒ 自相关无定义（退化用例，显式登记）
        else:
            with np.errstate(all="ignore"):
                ell_meas = C.estimate_ell(np.log10(np.abs(fc["truth"])), 1)[0]
        out_faces[fc["tag"]] = dict(meta=fc["meta"], s_field_log10_std=s_field,
                                    ell_meas_px=ell_meas, heavy_ops=heavy_note,
                                    rows=rows)
        all_rows += rows
        print("[%s] ell_meas=%.1f s_field=%.4f rows=%d t=%.0fs"
              % (fc["tag"], ell_meas, s_field, len(rows), time.time() - t0), flush=True)

    # ---- 退化用例登记：s=0 平坦场，帧级常数臂 RMSE 恒为 0 ⇒ 排序判据恒真 ----
    flat_tags = [t for t in out_faces if t.startswith("grf_") and t.endswith("_s0.000")]
    flat_viol = []
    for t in flat_tags:
        for r in out_faces[t]["rows"]:
            if r["mode"] in ("oracle", "estimated") and np.isfinite(r["rmse_log_rho"] or np.nan):
                if r["rmse_log_rho"] < 1e-12 and r["arm"] != "frame_median":
                    flat_viol.append(dict(face=t, arm=r["arm"], delta=r["delta_px"],
                                          rmse=r["rmse_log_rho"]))
    gates = dict(
        DEGENERATE_flat_field_ranking_never_true=bool(len(flat_viol) == 0),
        DEGENERATE_flat_field_ranking_note=(
            "恒真门：s=0 时帧级常数臂 RMSE≡0（真值场为常数），任何空间臂都不可能严格更优，"
            "该排序判据在此场景退化；仅登记，不作证据"),
        DEGENERATE_flat_field_violations=flat_viol,
        N_no_skipped=bool(all(not r["skipped"] for r in all_rows)),
        n_rows=len(all_rows))
    obj = dict(experiment="SCI-B / EXP-04 臂① 纯解析合成：重建算子对比",
               frozen_config=dict(crop=E.CROP, delta_grid=deltas, p_dense=E.P_DENSE,
                                  ell_synth=[f["meta"]["ell_nominal_px"] for f in faces],
                                  seed_base=E.SEED_BASE, cheap_ops=CHEAP, heavy_ops=HEAVY,
                                  primary_faces=PRIMARY_FACES, primary_deltas=PRIMARY_DELTAS),
               faces=out_faces, gates=gates, generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
