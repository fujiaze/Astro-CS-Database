#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 臂②：HST M16 真实信号模板 + 完整物理前向仿真（逐像素解析真值 σ）。

物理前向（最高设计 §12.2 第 1 类）：源+天光+暗流在电子域 Poisson，读出噪声电子域
Gaussian，除增益得 ADU；真值 σ(x,y)=sqrt(sig_e+sky+dark+RN²)/gain 逐像素解析已知。
本面是**高对比高分辨率域**（cell 内未分辨结构来自真实恒星）。

输出：results/exp04_e2_hst.json
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
HEAVY = ["idw", "tps", "tps_smooth", "rbf_mq", "rbf_gauss"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e2_hst.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    if not os.path.exists(E.M16):
        raise SystemExit("HST M16 缺失（fail-closed，禁止静默少面）: " + E.M16)
    img, truth, meta = E.load_m16(seed_off=2)
    deltas = [16, 64] if a.quick else E.DELTA_GRID
    rows = DR.run_face("hst_m16", img, truth, dict(face="hst_m16", **meta), CHEAP, deltas,
                       modes=("oracle", "estimated"), gran="pixel")
    if not a.quick:
        rows += DR.run_face("hst_m16", img, truth, dict(face="hst_m16", **meta), HEAVY, [64, 128],
                            modes=("oracle", "estimated"), gran="pixel")
    # 与 B3 同粒度的 patch32 真值（cell 内 RMS σ）供三臂同表比较
    truth32 = E.patch_rms(truth, E.P_DENSE)
    rows32 = DR.run_face("hst_m16", img, None, dict(face="hst_m16", **meta), CHEAP, deltas,
                         modes=("oracle", "estimated"), gran="patch32", truth_patch=truth32)
    s_field = float(np.nanstd(np.log10(truth)))
    with np.errstate(all="ignore"):
        ell_meas = C.estimate_ell(np.log10(truth), 1)[0]
    obj = dict(experiment="SCI-B / EXP-04 臂② HST M16 物理前向仿真：重建算子对比",
               frozen_config=dict(crop=E.CROP, delta_grid=deltas, p_dense=E.P_DENSE,
                                  gain=E.GAIN, rn_e=E.RN, dark_e=E.DARK, sky_e=E.SKY_E,
                                  sig_median_e=E.SIG_MED_E, seed_base=E.SEED_BASE),
               meta=meta, s_field_log10_std=s_field, ell_meas_px=ell_meas,
               rows=rows, rows_patch32=rows32,
               gates=dict(N_no_skipped=bool(all(not r["skipped"] for r in rows + rows32)),
                          n_rows=len(rows) + len(rows32)),
               generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("hst ell=%.1f s_field=%.4f rows=%d wall=%.0fs" % (ell_meas, s_field, len(rows), time.time() - t0))
    print("wrote", a.out)


if __name__ == "__main__":
    main()
