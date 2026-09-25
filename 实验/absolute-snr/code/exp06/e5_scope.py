#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 作用域图谱：**物理建模与插值各自的作用域**（EXP-04 的定位）。

回答三个问题：
  Q1 真值 sigma 场的空间方差有多少能由「平滑弥散分量 + 帧级特性」解释？
  Q2 弥散分量的空间尺度 ell_B 相对控制点间隔 Delta 变化时，物理建模与插值的胜负如何翻转？
  Q3 亮度解释不了的噪声结构（分区读出噪声）由谁承载？

固定 seed：20260927。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402

SEED = X.SEED
SHAPE = (1024, 1024)
DELTA = 64
EDGE_MARGIN = 1
STARS = dict(n_stars=80, star_flux_log10=(3.0, 6.0))


def _eval(kw: Dict[str, Any], seed: int) -> Dict[str, Any]:
    sc = X.analytic_scene(SHAPE, seed=seed, **kw)
    b = X.build_fields(sc["adu"], gain=sc["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
    ev = X.evaluate_fields(b["fields"], sc["truth"])
    return {"scene_kwargs": {k: v for k, v in kw.items() if k != "rn_map"}, "seed": int(seed),
            "fits": b["fits"], "diag": b["diag"], "methods": ev}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e5_scope.json"))
    ap.add_argument("--seeds", type=int, default=3)
    a = ap.parse_args()
    t0 = time.time()
    ns = int(a.seeds)
    out: Dict[str, Any] = {"meta": {
        "unit": "EXP-06-SNR-PHYS", "arm": "E 作用域图谱（物理建模 vs 插值）",
        "seed_base": SEED, "shape": list(SHAPE), "delta_px": DELTA,
        "note": "全部判据为比值口径；E = 权重效率损失（EXP-04 A3，尺度不变）",
    }, "Q1_variance_explained": [], "Q2_scale_sweep": [], "Q3_detector_structure": []}

    # ---- Q1：真值 sigma^2 场的空间方差被物理模型解释的比例（多场景） ----
    q1_scenes = [
        ("gradient_weak", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.2, sky_theta_deg=30.0)),
        ("gradient_strong", dict(sky_e_per_s=0.6, sky_grad_e_per_s=2.4, sky_theta_deg=15.0, **STARS)),
        ("nebula_smooth", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.3,
                               blobs=[{"cy":330, "cx":260, "sigma_px":260, "amp_e_per_s":1.4}],
                               **STARS)),
        ("read_noise_dominated", dict(sky_e_per_s=0.02, read_noise_e=15.0, **STARS)),
    ]
    for nm, kw in q1_scenes:
        for i in range(ns):
            r = _eval(kw, SEED + 3000 + i)
            r["name"] = nm
            out["Q1_variance_explained"].append(r)
            m = r["methods"]
            print("Q1 %-22s s%d  R2var: phys=%.4f interp=%.4f phys_resid=%.4f frame=%.4f | E: phys=%.5f interp=%.5f"
                  % (nm, i,
                     m.get("phys", {}).get("vs_T2_var_local", {}).get("r2_var", float("nan")),
                     m.get("interp_spline", {}).get("vs_T2_var_local", {}).get("r2_var", float("nan")),
                     m.get("phys_resid", {}).get("vs_T2_var_local", {}).get("r2_var", float("nan")),
                     m.get("frame_scalar", {}).get("vs_T2_var_local", {}).get("r2_var", float("nan")),
                     m.get("phys", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan")),
                     m.get("interp_spline", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan"))),
                  flush=True)

    # ---- Q2：弥散分量尺度 ell_B 相对 Delta 的扫描 ----
    for ell in (20.0, 40.0, 80.0, 160.0, 320.0):
        kw = dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                  blobs=[{"cy":512, "cx":512, "sigma_px":ell, "amp_e_per_s":1.0}], **STARS)
        row = {"ell_B_px": ell, "ell_over_delta": ell / DELTA, "runs": []}
        for i in range(ns):
            r = _eval(kw, SEED + 4000 + i)
            row["runs"].append(r)
        def _m(key, field):
            v = [x["methods"].get(key, {}).get("vs_T2_var_local", {}).get(field, float("nan"))
                 for x in row["runs"]]
            return float(np.nanmedian(v))
        row["E_phys"] = _m("phys", "eff_loss")
        row["E_interp"] = _m("interp_spline", "eff_loss")
        row["E_phys_resid"] = _m("phys_resid", "eff_loss")
        row["E_frame"] = _m("frame_scalar", "eff_loss")
        row["E_dense"] = _m("dense_patch", "eff_loss")
        row["level_phys"] = _m("phys", "level_ratio")
        row["level_interp"] = _m("interp_spline", "level_ratio")
        row["gain_dev_phys"] = float(np.nanmedian([
            abs(x["fits"]["free"].get("gain_hat", float("nan")) / 1.3 - 1.0) for x in row["runs"]]))
        out["Q2_scale_sweep"].append(row)
        print("Q2 ell=%6.1f (ell/Delta=%.2f)  E: phys=%.5f interp=%.5f phys_resid=%.5f frame=%.5f dense=%.5f | gain_dev=%.1f%%"
              % (ell, ell / DELTA, row["E_phys"], row["E_interp"], row["E_phys_resid"],
                 row["E_frame"], row["E_dense"], 100 * row["gain_dev_phys"]), flush=True)

    # ---- Q3：亮度解释不了的噪声结构（分区读出噪声） ----
    yy, xx = np.mgrid[0:SHAPE[0], 0:SHAPE[1]]
    rn2 = np.where(xx < SHAPE[1] // 2, 5.0, 20.0)
    q3_scenes = [
        ("flat_sky_two_amp", dict(sky_e_per_s=0.5, rn_map=rn2)),
        ("flat_sky_two_amp_stars", dict(sky_e_per_s=0.5, rn_map=rn2, **STARS)),
        ("prnu_only", dict(sky_e_per_s=0.5, prnu_rms=0.02, **STARS)),
    ]
    for nm, kw in q3_scenes:
        row = {"name": nm, "runs": []}
        for i in range(ns):
            row["runs"].append(_eval(kw, SEED + 5000 + i))
        def _m3(key, field):
            v = [x["methods"].get(key, {}).get("vs_T2_var_local", {}).get(field, float("nan"))
                 for x in row["runs"]]
            return float(np.nanmedian(v))
        for key in ("phys", "interp_spline", "phys_resid", "frame_scalar", "dense_patch"):
            row["E_" + key] = _m3(key, "eff_loss")
            row["r2var_" + key] = _m3(key, "r2_var")
        out["Q3_detector_structure"].append(row)
        print("Q3 %-22s E: phys=%.5f interp=%.5f phys_resid=%.5f frame=%.5f | R2var: phys=%.4f interp=%.4f phys_resid=%.4f"
              % (nm, row["E_phys"], row["E_interp_spline"], row["E_phys_resid"],
                 row["E_frame_scalar"], row["r2var_phys"], row["r2var_interp_spline"],
                 row["r2var_phys_resid"]), flush=True)

    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
