#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 / 实验臂 B：**HST 真实模板 + 完整物理前向仿真**（最高设计 §12.2 第 1 类数据）。

复用仓内**只读**共享仿真器 实验/shared/synthetic/{render.py,noise_model.py}
（不修改、不复制其任何文件，仅 import）：
  * real_base = testdata/HST_M16/*F657N*.fits 的真实裁剪作**纯信号底图**（结构形态真实）；
  * 逐帧独立噪声实现（同一 seed 序列的不同帧号），**平场图案逐帧固定**（仪器属性）；
  * 真值 = noise_model 前向链的逐像素解析方差：Var = lam/g^2 + RN^2/g^2 + 1/12，
    lam = max(src_e + sky_e + dark_e, 0)（与 expose 逐字同构）。

真值两个口径（显式区分，不混用；两者都**不含被测量源本身**的泊松，因为源项在
SNR 公式里由 F*P_i/g 单独承载 —— 见 docs/plugins/algorithms_phase1/07_noise_snr.md §4.2a）：

  sigma_sky_bg  背景口径：天光 + 暗流 + 读噪 + 量化（**不含**弥漫底图）—— 生产 sigma_sky 的"空天"语义；
  sigma_local   局部口径：背景口径 **加上该位置的弥漫底图（星云）泊松** —— 该位置源测量真正面对的噪声。

真值图用一帧 **stars.n=0** 的渲染取得（底图与平场是确定性的、与帧号无关），
因此真值不含注入星的泊松，与"估计器应剔除源"的口径一致。

本臂回答三件事（**有真值**）：
  B1 **绝对准确性**：区域化 sigma 相对真值的逐区域偏差随区域尺寸 B 的曲线；
  B2 **跨帧一致性**：同一区域在不同帧之间的 sigma 散布，与"帧级标量"口径的对照；
  B3 **逐帧电平调制**：逐帧天光电平不同（真实 M42 帧实测差 35~136 ADU）时，
     区域 sigma 能否跟着逐帧电平走（这正是 UPM「多退少补」要处理的量）。

固定 seed = 20260925。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp03_common as E  # noqa: E402

ROOT = E.ROOT
_SHARED = ROOT / "实验" / "shared" / "synthetic"
if str(_SHARED) not in sys.path:
    sys.path.insert(0, str(_SHARED))
import noise_model as NM  # noqa: E402
import render as RD       # noqa: E402

SEED = E.SEED
SHAPE = (512, 512)
EXPOSURE_S = 300.0
GAIN = 1.3
RN_E = 10.0
SKY_E_PER_S = 0.5
DARK_E_PER_S = 0.0
FWHM_DET = 2.5
FW_SCENE = FWHM_DET / 2.3548200450309493 * 1.230310
HST_GLOB = "testdata/HST_M16/*M16*.fits|testdata/HST_M16/*.fits"
BOXES = (16, 32, 64, 128, 256)
N_FRAMES = 4


def make_scene(*, target_p999_e: float, n_stars: int = 80,
               sky_scales: Optional[List[float]] = None,
               scene_id: str = "exp03_hst") -> Dict[str, Any]:
    sc: Dict[str, Any] = {
        "scene_id": scene_id, "kind": "exp03", "shape": list(SHAPE),
        "exposure_s": EXPOSURE_S,
        "psf": {"model": "moffat4", "fwhm_px": FW_SCENE, "beta": 4},
        "detector": {"gain_e_per_adu": GAIN, "read_noise_e": RN_E,
                     "bias_adu": 1000.0, "full_well_e": 120000.0,
                     "dark_current_e_per_s": DARK_E_PER_S, "dark_ref_temp_c": -20.0},
        "flat": {"prnu_rms": 0.005, "low_order": 0.01, "vignette": 0.03, "seed": 1234},
        "sky": {"level_e_per_s": SKY_E_PER_S, "grad_x_e_per_s": 0.0,
                "grad_y_e_per_s": 0.0, "theta_deg": 0.0},
        "nebula": [],
        "stars": {"n": int(n_stars), "flux_log10_range": [3.5, 6.0],
                  "clustering": "uniform", "seed": 1616},
        "artifacts": {"cr_rate_per_frame": 0.0, "cr_mean_charge_e": 900.0,
                      "hot_pixel_fraction": 0.0},
        "mode": "physical",
    }
    if target_p999_e > 0:
        sc["real_base"] = {
            "path": HST_GLOB, "glob_index": 0,
            "auto_bright": {"size": 1024, "block": 256, "mode": "mean"},
            "smooth_sigma_px": 1.5,
            "target_p999_e": float(target_p999_e), "subtract_percentile": 5,
        }
    if sky_scales:
        sc["frames"] = [{"sky": {"level_e_per_s": SKY_E_PER_S * s}} for s in sky_scales]
    return sc


def var_maps(scene: Dict[str, Any], frame) -> Dict[str, np.ndarray]:
    """逐像素真值方差 [ADU^2]：背景口径与局部口径（均不含被测量源本身）。"""
    det = NM.Detector(**scene["detector"])
    g = det.gain_e_per_adu
    src = np.asarray(frame.src_e, dtype=np.float64)
    sky = np.asarray(frame.sky_e, dtype=np.float64)
    dark = np.asarray(frame.dark_e, dtype=np.float64)
    rn_q = (det.read_noise_e ** 2) / (g * g) + 1.0 / 12.0
    return {"var_bg": np.maximum(sky + dark, 0.0) / (g * g) + rn_q,
            "var_local": np.maximum(src + sky + dark, 0.0) / (g * g) + rn_q}


def region_truth(vmap: np.ndarray, box: int) -> np.ndarray:
    """区域真值 sigma = sqrt(区域内逐像素方差的均值)（**已开方**）。"""
    ny, nx = vmap.shape
    by, bx = ny // box, nx // box
    v = vmap[:by * box, :bx * box].reshape(by, box, bx, box).transpose(0, 2, 1, 3)
    return np.sqrt(np.mean(v.reshape(by * bx, box * box), axis=1)).reshape(by, bx)


def _rel(a: np.ndarray, t: np.ndarray) -> np.ndarray:
    return a / np.maximum(t, 1e-12) - 1.0


def run_scenario(name: str, scene: Dict[str, Any]) -> Dict[str, Any]:
    t0 = time.time()
    # 真值帧：stars.n = 0（底图/平场确定性，与帧号无关）
    truth_scene = copy.deepcopy(scene)
    truth_scene["stars"] = dict(truth_scene["stars"]); truth_scene["stars"]["n"] = 0
    tf, _ = RD.render_frame(truth_scene, seed=SEED, frame_index=0)
    tmaps = var_maps(truth_scene, tf)
    frames = []
    for k in range(N_FRAMES):
        fr, _tr = RD.render_frame(scene, seed=SEED + 1000 * (k + 1), frame_index=k)
        frames.append(fr)
    fvars = [var_maps(scene, fr) for fr in frames]
    adu = [np.asarray(fr.adu, dtype=np.float64) for fr in frames]
    sky_med = [float(np.median(np.asarray(fr.sky_e, dtype=np.float64))) for fr in frames]
    rows: List[Dict[str, Any]] = []
    for B in BOXES:
        sig_bg = region_truth(tmaps["var_bg"], B)
        sig_loc = region_truth(tmaps["var_local"], B)
        r0 = np.stack([E.region_sigma_raw(adu[k], B)["sigma"] for k in range(N_FRAMES)])
        r1 = np.stack([E.region_sigma_resid(adu[k], B, n_iter=3)["sigma"]
                       for k in range(N_FRAMES)])
        r2, r2_true = [], []
        for i in range(N_FRAMES):
            for j in range(i + 1, N_FRAMES):
                r2.append(E.region_sigma_diff(adu[i], adu[j], B)["sigma"])
                r2_true.append(region_truth(0.5 * (fvars[i]["var_local"]
                                                   + fvars[j]["var_local"]), B))
        r2 = np.stack(r2); r2_true = np.stack(r2_true)
        scalars = np.array([E.frame_scalar_sigma(adu[k]) for k in range(N_FRAMES)])
        spread = np.percentile(r1, 95, axis=0) / np.maximum(np.percentile(r1, 5, axis=0), 1e-12)
        a2 = r1[0] / np.maximum(np.mean(r2, axis=0), 1e-12)
        rows.append({
            "box": int(B),
            "sigma_true_bg_median": float(np.median(sig_bg)),
            "sigma_true_local_median": float(np.median(sig_loc)),
            "sigma_true_local_p95_over_p05": float(np.percentile(sig_loc, 95)
                                                    / np.percentile(sig_loc, 5)),
            "frame_scalar_values": [float(x) for x in scalars],
            "frame_scalar_rel_err_vs_local": [float(x / np.median(sig_loc) - 1.0) for x in scalars],
            "frame_scalar_abs_rel_err_vs_local_p95": float(np.percentile(np.abs(
                scalars[:, None, None] / sig_loc[None, :, :] - 1.0), 95)),
            "frame_scalar_p95_over_p05": float(np.percentile(scalars, 95)
                                               / max(np.percentile(scalars, 5), 1e-12)),
            "R0_abs_rel_err_vs_local_p95": float(np.percentile(np.abs(_rel(r0, sig_loc[None])), 95)),
            "R0_rel_err_vs_local_median": float(np.median(_rel(r0, sig_loc[None]))),
            "R1_rel_err_vs_bg_median": float(np.median(_rel(r1, sig_bg[None]))),
            "R1_abs_rel_err_vs_bg_p95": float(np.percentile(np.abs(_rel(r1, sig_bg[None])), 95)),
            "R1_rel_err_vs_local_median": float(np.median(_rel(r1, sig_loc[None]))),
            "R1_abs_rel_err_vs_local_p95": float(np.percentile(np.abs(_rel(r1, sig_loc[None])), 95)),
            "R1_scatter_over_regions": float(np.std(_rel(r1, sig_loc[None]), ddof=1)),
            "R2_rel_err_vs_local_median": float(np.median(_rel(r2, r2_true))),
            "R2_abs_rel_err_vs_local_p95": float(np.percentile(np.abs(_rel(r2, r2_true)), 95)),
            "R2_scatter_over_regions": float(np.std(_rel(r2, r2_true), ddof=1)),
            "region_cross_frame_p95_over_p05_median": float(np.median(spread)),
            "region_cross_frame_p95_over_p05_p95": float(np.percentile(spread, 95)),
            "A2_reg_median": float(np.median(a2)),
            "A2_reg_p95": float(np.percentile(a2, 95)),
            "certified_frac": float(np.mean(a2 <= 1.0 + E.DELTA_BUDGET)),
            "pred_stat_error_1_over_sqrt2N": E.predicted_region_stat_error(B),
            "R1_p95_over_p05": float(np.percentile(r1, 95) / np.percentile(r1, 5)),
        })
    return {"scenario": name, "sky_e_median_per_frame": sky_med,
            "n_frames": N_FRAMES, "n_stars": int(scene["stars"]["n"]),
            "target_p999_e": float(scene.get("real_base", {}).get("target_p999_e", 0.0)),
            "rows": rows, "elapsed_s": time.time() - t0}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp03_e3_hst.json")
    args = ap.parse_args()
    t0 = time.time()
    out: Dict[str, Any] = {"meta": {
        "seed": SEED, "shape": list(SHAPE), "n_frames": N_FRAMES,
        "boxes": list(BOXES), "delta_budget": E.DELTA_BUDGET,
        "gain_e_per_adu": GAIN, "read_noise_e": RN_E, "exposure_s": EXPOSURE_S,
        "sky_e_per_s": SKY_E_PER_S, "dark_e_per_s": DARK_E_PER_S,
        "data_class": "HST 真实模板 + 完整物理前向仿真（最高设计 §12.2 第 1 类）",
        "truth": "前向链解析方差 Var = lam/g^2 + RN^2/g^2 + 1/12（lam = max(src_e+sky_e+dark_e,0)）；"
                 "真值帧 stars.n=0，故真值不含注入源自身的泊松",
        "renderer": "实验/shared/synthetic/render.py::render_frame（只读 import）"},
        "scenarios": []}
    scen = [
        ("hst_null_nostars", make_scene(target_p999_e=0.0, n_stars=0,
                                        scene_id="exp03_null_nostars")),
        ("hst_struct_nostars_p999_1000", make_scene(target_p999_e=1000.0, n_stars=0,
                                                    scene_id="exp03_struct_nostars")),
        ("hst_struct_stars_p999_1000", make_scene(target_p999_e=1000.0, n_stars=80,
                                                  scene_id="exp03_struct_stars")),
        ("hst_struct_varsky_p999_1000", make_scene(target_p999_e=1000.0, n_stars=80,
                                                   sky_scales=[0.7, 1.0, 1.3, 1.6],
                                                   scene_id="exp03_struct_varsky")),
    ]
    for nm, sc in scen:
        r = run_scenario(nm, sc)
        out["scenarios"].append(r)
        print("scenario %s done %.1fs" % (nm, r["elapsed_s"]), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float), encoding="utf-8")
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
