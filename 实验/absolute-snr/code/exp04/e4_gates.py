#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 负例与判据非退化审查（红/绿必须都能出现）。

负例清单：
  G1 真值无效应⇒归零（**绿**）：平坦 σ 场 + oracle 控制值 ⇒ 任一算子 E=0、
     空间增益 G=E_frame−E_op=0（数值容差 1e-9）。
  G2 平坦场排序判据退化登记（**登记，不作证据**）：s=0 时帧级常数臂 RMSE≡0，
     "空间臂严格更优"在数学上不可能出现 ⇒ 恒真门。
  G3 对抗洗牌（**红**）：结构化真值场 + 把控制值随机洗牌 ⇒ 最优算子必须劣于帧级臂
     （E_op > E_frame）。若洗牌后仍"胜出"，说明判据无法识别错误重建 ⇒ 判红。
  G4 恒真判据复核（§8b tautology_demo）："帧级臂 RMSE ≤ K·s_field" 对任意真值场
     恒真（K 足够大），在对抗洗牌场下仍绿 ⇒ 不得充当证据。
  G5 最近邻基线非退化：在可分辨结构域上 nn 必须严格劣于至少一个光滑算子（E 或 RMSE），
     否则判据对"算子质量"不敏感 ⇒ 判红。
  G6 零效应⇒零（帧级臂自证）：平坦真值场下 frame_median 臂 E=0、rmse=0。

输出：results/exp04_e4_gates.json
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

SMOOTH_OPS = ["bilinear", "bicubic_cc", "spline_natural", "photutils_zoom1", "gpr_rbf"]
ALL_OPS = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
           "photutils_zoom3", "photutils_zoom1", "idw", "gpr_rbf", "gpr_matern32", "gpr_exp"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e4_gates.json"))
    a = ap.parse_args()
    t0 = time.time()
    D = 64
    H = E.CROP
    out = {}

    # ---------- G1 / G2 / G6：平坦真值场（真值无空间效应）----------
    flat_truth = np.full((H, H), 2.5)
    img_flat = E.rng(11).normal(0.0, 2.5, size=(H, H))
    ctrl_or = np.full((H // D, H // D), 2.5)          # oracle 控制值：真值本身
    ctrl_est = E.ctrl_estimated(img_flat, D)
    g1 = []
    for op in ALL_OPS:
        dense, _, _ = O.run_operator(op, ctrl_or, D, (H, H))
        m = E.metrics(dense, flat_truth, np.ones((H, H), bool))
        g1.append(dict(op=op, eff_loss=m["eff_loss"], rmse=m["rmse_log_rho"],
                       max_abs_dev=float(np.max(np.abs(dense - 2.5))),
                       ok=bool(abs(m["eff_loss"]) <= 1e-9 and m["rmse_log_rho"] <= 1e-9)))
    frame_flat = E.metrics(np.full((H, H), float(np.median(ctrl_est))), flat_truth, np.ones((H, H), bool))
    out["G1_zero_effect_zero"] = dict(rows=g1, all_ok=bool(all(r["ok"] for r in g1)),
                                      note="平坦真值场 + oracle 控制值 ⇒ 每算子 E=0、RMSE=0、G=0")
    out["G6_frame_arm_selfzero"] = dict(eff_loss=frame_flat["eff_loss"],
                                        rmse=frame_flat["rmse_log_rho"],
                                        ok=bool(abs(frame_flat["eff_loss"]) <= 1e-9
                                                and frame_flat["rmse_log_rho"] <= 1e-9))
    # G2：排序判据退化登记（帧级臂 RMSE 精确为 0 ⇒ 任何空间臂都不可能严格更优）
    viol = [r["op"] for r in g1 if r["rmse"] < 0.0]
    out["G2_DEGENERATE_flat_field_ranking_never_true"] = dict(
        degenerate=True, violations=viol,
        note="恒真门：真值场为常数时帧级常数臂 RMSE≡0，'空间臂严格更优'数学上不可能；仅登记，不作证据")

    # ---------- G3 / G4：结构化场 + 对抗洗牌 ----------
    sigma = E.synth_sigma_face(64.0, 0.10, H, seed_off=901)
    img, truth = E.synth_data_face(sigma, seed_off=902)
    ctrl = E.ctrl_estimated(img, D)
    frame_val = float(np.nanmedian(ctrl))
    m_frame = E.metrics(np.full((H, H), frame_val), truth, np.ones((H, H), bool))
    r = E.rng(903)
    ctrl_shuf = ctrl.ravel()[r.permutation(ctrl.size)].reshape(ctrl.shape)
    g3, g4 = [], []
    for op in ALL_OPS:
        d_ok, _, _ = O.run_operator(op, ctrl, D, (H, H))
        d_sh, _, _ = O.run_operator(op, ctrl_shuf, D, (H, H))
        m_ok = E.metrics(d_ok, truth, np.ones((H, H), bool))
        m_sh = E.metrics(d_sh, truth, np.ones((H, H), bool))
        g3.append(dict(op=op, eff_loss_shuffled=m_sh["eff_loss"], eff_loss_intact=m_ok["eff_loss"],
                       worse_than_frame=bool(m_sh["eff_loss"] > m_frame["eff_loss"]),
                       ok=bool(m_sh["eff_loss"] > m_frame["eff_loss"])))
        # G4 恒真判据：rmse_frame <= K*s_field（s_field = 真值场 log10 std）
        s_field = float(np.nanstd(np.log10(truth)))
        K = 10.0
        g4.append(dict(op=op, rmse_frame=m_frame["rmse_log_rho"], s_field_dex=s_field, K=K,
                       tautology_true=bool(m_frame["rmse_log_rho"] <= K * s_field),
                       eff_loss_shuffled=m_sh["eff_loss"]))
    best_intact = min(g3, key=lambda x: x["eff_loss_intact"])
    out["G3_adversarial_shuffle_red"] = dict(
        rows=g3, frame_eff_loss=m_frame["eff_loss"],
        best_intact_arm=best_intact["op"], best_intact_eff_loss=best_intact["eff_loss_intact"],
        all_ops_worse_than_frame=bool(all(x["worse_than_frame"] for x in g3)),
        ok=bool(all(x["worse_than_frame"] for x in g3)),
        note="洗牌破坏空间结构后，每个算子都必须劣于帧级常数臂；否则判据识别不出错误重建")
    out["G4_tautology_recheck"] = dict(
        rows=g4, all_true=bool(all(x["tautology_true"] for x in g4)),
        note=("'帧级臂 RMSE <= K*s_field' 在对抗洗牌场（E 高达 %.2f）下仍恒真 ⇒ "
              "该判据无证据资格（复核 §8b b6_gates_audit.json::tautology_demo）"
              % max(x["eff_loss_shuffled"] for x in g4)))

    # ---------- G5：最近邻基线非退化 ----------
    g5 = []
    for D2 in (32, 64, 128):
        c2 = E.ctrl_estimated(img, D2)
        m_nn = E.metrics(O.run_operator("nn", c2, D2, (H, H))[0], truth, np.ones((H, H), bool))
        for op in SMOOTH_OPS:
            m_op = E.metrics(O.run_operator(op, c2, D2, (H, H))[0], truth, np.ones((H, H), bool))
            g5.append(dict(delta_px=D2, op=op, eff_nn=m_nn["eff_loss"], eff_op=m_op["eff_loss"],
                           better=bool(m_op["eff_loss"] < m_nn["eff_loss"])))
    out["G5_nn_baseline_nondegenerate"] = dict(
        rows=g5, n_better=int(sum(x["better"] for x in g5)), n_total=len(g5),
        ok=bool(sum(x["better"] for x in g5) >= len(g5) // 2),
        note="最近邻必须被光滑算子稳定击败，否则判据对算子质量不敏感")

    obj = dict(experiment="SCI-B / EXP-04 负例与判据非退化审查", frozen_config=dict(
        delta_px=D, crop=H, seed_base=E.SEED_BASE), **out,
        gates_summary={k: v.get("ok", v.get("all_ok", None))
                       for k, v in out.items() if isinstance(v, dict)},
        generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    for k, v in out.items():
        if isinstance(v, dict) and "ok" in v:
            print("%-34s ok=%s" % (k, v["ok"]))
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
