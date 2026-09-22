#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 臂③：testdata M42 Red 300 s 真实帧（1 px 棋盘 hold-out 真值）。

hold-out：估计量只用族 0 像素，真值只用族 1 像素（零像素重叠）⇒ 真值本身有噪声，
其本征不确定度 EPS_REF = 1.166/ln10/sqrt(N_patch/2) 用平方相减去卷积（B3 同口径）。
评价粒度固定 patch32（真值只到 P=32 patch）。

输出：results/exp04_e3_real.json
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
import exp04_common as E      # noqa: E402
import driver as DR           # noqa: E402

CHEAP = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
         "photutils_zoom3", "photutils_zoom1", "gpr_rbf", "gpr_matern32", "gpr_exp"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e3_real.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    deltas = [16, 64] if a.quick else E.DELTA_GRID
    faces = {}
    all_rows = []
    for i, path in enumerate(E.M42):
        if not os.path.exists(path):
            raise SystemExit("testdata 缺失（fail-closed，禁止静默少面）: " + path)
        img, meta = E.load_m42(path)
        img0, img1, fam = E.holdout_split(img)
        truth = C.sigma_field_fast(img1, E.P_DENSE)          # 族 1 -> 真值
        eps = E.eps_ref_dex(E.P_DENSE)
        rows = DR.run_face(os.path.basename(path)[:24], img0, None, dict(face="testdata_m42", **meta),
                           CHEAP, deltas, modes=("oracle", "estimated"), gran="patch32",
                           truth_patch=truth, img_stat=img0, eps_ref=eps)
        s_field = float(np.nanstd(np.log10(np.where(np.isfinite(truth) & (truth > 0), truth, np.nan))))
        with np.errstate(all="ignore"):
            ell_meas = C.estimate_ell(np.log10(np.where(truth > 0, truth, np.nan)), E.P_DENSE)[0]
        faces[os.path.basename(path)] = dict(meta=meta, s_field_log10_std=s_field,
                                             ell_meas_px=ell_meas, eps_ref_dex=eps, rows=rows)
        all_rows += rows
        print("[%s] ell=%.1f s_field=%.4f rows=%d t=%.0fs"
              % (os.path.basename(path)[:28], ell_meas, s_field, len(rows), time.time() - t0), flush=True)
    obj = dict(experiment="SCI-B / EXP-04 臂③ testdata M42 真实帧：重建算子对比（棋盘 hold-out）",
               frozen_config=dict(crop=E.CROP, delta_grid=deltas, p_dense=E.P_DENSE,
                                  eps_ref_dex=E.eps_ref_dex(E.P_DENSE), seed_base=E.SEED_BASE),
               faces=faces, gates=dict(N_no_skipped=bool(all(not r["skipped"] for r in all_rows)),
                                       n_rows=len(all_rows)),
               generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
