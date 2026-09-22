#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 跨帧一致性：同一天区、不同帧重建出的绝对 σ 场是否可比。

三种口径：
  C1 多实现合成帧（解析面 / HST 面）：同一真值 σ 场 + M 个独立噪声实现 ⇒
     逐像素跨帧相对离散 std_k(log10 σ̂_k)，取中位数；并与真值的跨帧离散（=0）对比。
  C2 真实帧棋盘分裂：同一真实帧的族 0 / 族 1 两个独立半集各重建一次 ⇒
     跨"帧"离散（同一天区、同一物理帧的两个独立实现）。
  C3 帧级标量的跨帧可比性：帧级臂的标量在不同实现间的离散（与空间臂对比）。

输出：results/exp04_e7_crossframe.json
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

OPS = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
       "photutils_zoom3", "photutils_zoom1", "idw", "gpr_rbf", "gpr_matern32", "gpr_exp"]
M_REAL = 8          # 独立实现数
D = 64


def scatter(recs):
    """逐像素跨实现相对离散（log10 域），返回中位数与 p95。"""
    a = np.stack([np.log10(np.abs(r)) for r in recs], axis=0)
    a[~np.isfinite(a)] = np.nan
    s = np.nanstd(a, axis=0, ddof=1)
    return float(np.nanmedian(s)), float(np.nanpercentile(s, 95))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e7_crossframe.json"))
    a = ap.parse_args()
    t0 = time.time()
    H = E.CROP
    out = {}

    # ---------- C1a 解析面多实现 ----------
    sigma = E.synth_sigma_face(64.0, 0.10, H, seed_off=701)
    c1 = {}
    for op in OPS:
        recs = []
        for m in range(M_REAL):
            img, _ = E.synth_data_face(sigma, seed_off=710 + m)
            ctrl = E.ctrl_estimated(img, D)
            recs.append(O.run_operator(op, ctrl, D, (H, H))[0])
        med, p95 = scatter(recs)
        c1[op] = dict(scatter_median_dex=med, scatter_p95_dex=p95)
    # 帧级标量的跨实现离散
    frame_vals = []
    for m in range(M_REAL):
        img, _ = E.synth_data_face(sigma, seed_off=710 + m)
        frame_vals.append(float(np.nanmedian(E.ctrl_estimated(img, D))))
    c1["frame_median_scalar"] = dict(
        scatter_median_dex=float(np.std(np.log10(np.abs(frame_vals)), ddof=1)),
        scatter_p95_dex=None, values=frame_vals)
    out["C1a_analytic_multi_realization"] = c1

    # ---------- C1b HST 面多实现 ----------
    if os.path.exists(E.M16):
        from astropy.io import fits
        with fits.open(E.M16, memmap=False) as h:
            raw = np.asarray(h[0].data, np.float64)
        ny, nx = raw.shape
        y0, x0 = (ny - H) // 2, (nx - H) // 2
        sig = np.maximum(raw[y0:y0 + H, x0:x0 + H], 0.0) * (E.SIG_MED_E / max(np.median(raw), 1e-12))
        c1b = {}
        for op in OPS:
            recs = []
            for m in range(M_REAL):
                r = E.rng(720 + m)
                lam = sig + E.SKY_E + E.DARK
                img = (r.poisson(lam).astype(np.float64) + r.normal(0.0, E.RN, size=lam.shape)) / E.GAIN
                ctrl = E.ctrl_estimated(img, D)
                recs.append(O.run_operator(op, ctrl, D, (H, H))[0])
            med, p95 = scatter(recs)
            c1b[op] = dict(scatter_median_dex=med, scatter_p95_dex=p95)
        out["C1b_hst_multi_realization"] = c1b

    # ---------- C2 真实帧棋盘分裂 ----------
    c2 = {}
    for i, path in enumerate(E.M42):
        if not os.path.exists(path):
            raise SystemExit("testdata 缺失（fail-closed）: " + path)
        img, meta = E.load_m42(path)
        img0, img1, _ = E.holdout_split(img)
        rec0 = {op: O.run_operator(op, E.ctrl_estimated(img0, D), D, (H, H))[0] for op in OPS}
        rec1 = {op: O.run_operator(op, E.ctrl_estimated(img1, D), D, (H, H))[0] for op in OPS}
        row = {}
        for op in OPS:
            d = np.abs(np.log10(np.abs(rec0[op]) / np.abs(rec1[op])))
            row[op] = dict(split_scatter_median_dex=float(np.nanmedian(d)),
                           split_scatter_p95_dex=float(np.nanpercentile(d, 95)))
        f0 = float(np.nanmedian(E.ctrl_estimated(img0, D)))
        f1 = float(np.nanmedian(E.ctrl_estimated(img1, D)))
        row["frame_median_scalar"] = dict(split_scatter_median_dex=float(abs(np.log10(f0 / f1))),
                                          split_scatter_p95_dex=None)
        c2[os.path.basename(path)] = dict(meta=meta, rows=row)
        print("[C2 %s] done t=%.0fs" % (os.path.basename(path)[:24], time.time() - t0), flush=True)
    out["C2_real_checkerboard_split"] = c2
    obj = dict(experiment="SCI-B / EXP-04 跨帧一致性", frozen_config=dict(
        delta_px=D, crop=H, m_realization=M_REAL, seed_base=E.SEED_BASE),
        **out, generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
