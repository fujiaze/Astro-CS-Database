#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 补充实验 e9：**值域钳制（clip）的对照实验**。

动机（独立审稿 B-1 指出的缺口）：§4 推荐配置含 \`snr_reconstruct_clip = true\`，
但被测算子 \`sextractor_spline\` 本身**没有**钳制，且"钳制把 761–7.8e5 的失控压回有界"
这一因果从未做过对照实验。本脚本补上该对照：

  C1 阶跃场：带/不带钳制的超调与值域（含"输出是否出现非物理 σ<=0"）
  C2 关键面 Δ=64：钳制在算子本已有界时是否**损失精度**
  C3 病态 (面,Δ) 单元：钳制能否把失控的 E 压回有界
  C4 平坦场：钳制是否改变"真值无效应⇒归零"（退化域，只作一致性检查）

输出：results/exp04_e9_clip.json
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
import e1_analytic as E1      # noqa: E402

# 带钳制 / 不带钳制 的配对（同核，唯一差别是最后一步 clip）
PAIRS = [("bicubic_cc", "bicubic_cc_clip"),
         ("spline_natural", "spline_natural_clip"),
         ("sextractor_spline", "sextractor_spline_clip"),
         ("gpr_rbf", "gpr_rbf_clip")]
PAIR_OPS = [x for p in PAIRS for x in p]

# C2 关键面（Δ=64）
KEY_FACES = ["grf_ell64.0_s0.000", "grf_ell64.0_s0.100",
             "unres_ell64.0_s0.100_ellu4.0_au0.30"]
# C3 病态 (面, Δ)：§2.2 最坏 E 的出现处
PATHO = [("grf_ell8.0_s0.300", [16, 256]),
         ("grf_ell16.0_s0.300", [16]),
         ("grf_ell64.0_s0.100", [256])]


def step_test(H=512, D=64):
    """镜像 e6_boundary.py 的 B2 构造：nx/2 左侧 σ=1、右侧 σ=3 的阶跃控制网格。"""
    nx = ny = H // D
    step = np.where(np.arange(nx)[None, :] < nx // 2, 1.0, 3.0) * np.ones((ny, 1))
    lo, hi = float(step.min()), float(step.max())
    amp = hi - lo
    out_rows = []
    for op in PAIR_OPS + ["photutils_zoom3"]:
        o, _, _ = O.run_operator(op, step, D, (H, H))
        overshoot = float(max(o.max() - hi, lo - o.min()) / amp)
        out_rows.append(dict(op=op, overshoot_frac_amp=overshoot,
                             out_min=float(o.min()), out_max=float(o.max()),
                             n_nonpositive=int((o <= 0).sum()),
                             within_ctrl_range=bool(o.min() >= lo - 1e-12 and o.max() <= hi + 1e-12)))
    return dict(ctrl_range=[lo, hi], rows=out_rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e9_clip.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()

    faces = {f["tag"]: f for f in E1.build_faces(quick=a.quick)}
    if not os.path.exists(E.M16):
        raise SystemExit("HST M16 缺失（fail-closed）: " + E.M16)
    hst_img, hst_truth, hst_meta = E.load_m16(seed_off=2)

    # ---------- C1 阶跃场 ----------
    c1 = step_test()

    # ---------- C2 关键面 Δ=64（钳制在已有界时是否损失精度） ----------
    c2 = {}
    for tag in KEY_FACES:
        fc = faces[tag]
        rows = DR.run_face(tag, fc["img"], fc["truth"], fc["meta"], PAIR_OPS, [64],
                           modes=("estimated",), gran="pixel")
        c2[tag] = rows
    rows = DR.run_face("hst_m16", hst_img, hst_truth, dict(face="hst_m16", **hst_meta),
                       PAIR_OPS, [64], modes=("estimated",), gran="pixel")
    c2["hst_m16"] = rows
    print("[C2] done t=%.0fs" % (time.time() - t0), flush=True)

    # ---------- C3 病态单元（钳制能否压回有界） ----------
    c3 = {}
    for tag, deltas in PATHO:
        fc = faces[tag]
        c3[tag] = DR.run_face(tag, fc["img"], fc["truth"], fc["meta"], PAIR_OPS, deltas,
                              modes=("estimated",), gran="pixel")
    c3["hst_m16"] = DR.run_face("hst_m16", hst_img, hst_truth, dict(face="hst_m16", **hst_meta),
                                PAIR_OPS, [128, 256], modes=("estimated",), gran="pixel")
    print("[C3] done t=%.0fs" % (time.time() - t0), flush=True)

    # ---------- C5 真实地面帧（M42，patch32 粒度）：钳制在真实数据上是否损失精度 ----------
    # 动机：独立审稿指出"推荐算子在 M42 类真实地面帧 Δ=64 上劣于双线性"这一适用域缺口
    # 必须在报告里如实登记；此处补测带钳制版本，给出该域的真实数字。
    c5 = {}
    for path in E.M42:
        if not os.path.exists(path):
            raise SystemExit("testdata 缺失（fail-closed）: " + path)
        img, meta = E.load_m42(path)
        img0, img1, _ = E.holdout_split(img)
        truth = C.sigma_field_fast(img1, E.P_DENSE)
        eps = E.eps_ref_dex(E.P_DENSE)
        tag = os.path.basename(path)[:24]
        c5[tag] = DR.run_face(tag, img0, None, dict(face="testdata_m42", **meta),
                              PAIR_OPS + ["bilinear", "sextractor_spline"], [64],
                              modes=("estimated",), gran="patch32",
                              truth_patch=truth, img_stat=img0, eps_ref=eps)
    print("[C5] done t=%.0fs" % (time.time() - t0), flush=True)

    # ---------- C4 平坦场一致性（退化域） ----------
    flat = faces.get("grf_ell64.0_s0.000") or faces.get("grf_ell16.0_s0.000")
    c4 = DR.run_face(flat["tag"], flat["img"], flat["truth"], flat["meta"], PAIR_OPS, [64],
                     modes=("estimated",), gran="pixel")

    # ---------- 配对汇总 ----------
    def pair_table(section):
        out = []
        for tag, rows in section.items():
            for D in sorted({r["delta_px"] for r in rows}):
                for base, clip in PAIRS:
                    b = next((r for r in rows if r["arm"] == base and r["delta_px"] == D), None)
                    c = next((r for r in rows if r["arm"] == clip and r["delta_px"] == D), None)
                    if b is None or c is None:
                        continue
                    out.append(dict(face=tag, delta_px=D, base=base,
                                    eff_base=b["eff_loss"], eff_clip=c["eff_loss"],
                                    ratio=(None if (b["eff_loss"] in (None, 0) or c["eff_loss"] is None)
                                           else float(c["eff_loss"] / b["eff_loss"]))))
        return out

    obj = dict(experiment="SCI-B / EXP-04 补充实验：值域钳制对照（回答审稿 B-1）",
               frozen_config=dict(pairs=PAIRS, key_faces=KEY_FACES, patho=PATHO,
                                  seed_base=E.SEED_BASE, crop=E.CROP),
               C1_step=c1,
               C2_key_faces_delta64=c2, C2_pairs=pair_table(c2),
               C3_pathological=c3, C3_pairs=pair_table(c3),
               C4_flat=c4, C4_pairs=pair_table({"flat": c4}),
               C5_real_m42=c5, C5_pairs=pair_table(c5),
               generated_at=E.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote %s wall=%.0fs" % (a.out, time.time() - t0))


if __name__ == "__main__":
    main()
