#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 B：**HST 真实模板 + 完整物理前向仿真**（最高设计 12.2 第 1 类数据，有真值）。

只读 import 仓内共享仿真器 实验/shared/synthetic/{render.py,noise_model.py}（不修改、不复制）。
真值 = 前向链解析方差 Var = lam/g^2 + RN^2/g^2 + 1/12（lam = max(src_e+sky_e+dark_e, 0)），
取 stars.n = 0 的真值帧 ⇒ 真值不含注入源自身的泊松（与「估计器应剔除源」的口径一致）。

本臂回答：在**高对比真实结构**（M16 核心）上，
  * 帧级标量 sigma 被结构污染多少（b_f）？
  * 局部 patch 估计器（R0 朴素 / R1 结构感知）被污染多少（b_c）？
  * 因此**相对表示**的电平误差（= c_agg*b_f）比**绝对表示**（= b_c）大多少？
  * 跨帧（不同天光电平）时，两种表示的跨帧比值各偏多少？

固定 seed = 20260926。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import copy
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp05_common as X  # noqa: E402

_SHARED = X.ROOT / "实验" / "shared" / "synthetic"
if str(_SHARED) not in sys.path:
    sys.path.insert(0, str(_SHARED))
import noise_model as NM  # noqa: E402
import render as RD       # noqa: E402

SEED = X.SEED
SHAPE = (1024, 1024)
DELTA = 64
N_FRAMES = 4
EXPOSURE_S = 300.0
GAIN = 1.3
RN_E = 10.0
SKY_E_PER_S = 0.5
FWHM_DET = 2.5
FW_SCENE = FWHM_DET / 2.3548200450309493 * 1.230310
HST_GLOB = "testdata/HST_M16/*M16*.fits|testdata/HST_M16/*.fits"


def make_scene(*, target_p999_e: float, n_stars: int = 0,
               sky_scales: Optional[List[float]] = None, scene_id: str = "exp05_hst"):
    sc: Dict[str, Any] = {
        "scene_id": scene_id, "kind": "exp05", "shape": list(SHAPE),
        "exposure_s": EXPOSURE_S,
        "psf": {"model": "moffat4", "fwhm_px": FW_SCENE, "beta": 4},
        "detector": {"gain_e_per_adu": GAIN, "read_noise_e": RN_E,
                     "bias_adu": 1000.0, "full_well_e": 120000.0,
                     "dark_current_e_per_s": 0.0, "dark_ref_temp_c": -20.0},
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
    """逐像素真值方差 [ADU^2]：背景口径（sky+dark+RN+量化）与局部口径（再加弥漫底图泊松）。"""
    det = NM.Detector(**scene["detector"])
    g = det.gain_e_per_adu
    src = np.asarray(frame.src_e, dtype=np.float64)
    sky = np.asarray(frame.sky_e, dtype=np.float64)
    dark = np.asarray(frame.dark_e, dtype=np.float64)
    rn_q = (det.read_noise_e ** 2) / (g * g) + 1.0 / 12.0
    return {"var_bg": np.maximum(sky + dark, 0.0) / (g * g) + rn_q,
            "var_local": np.maximum(src + sky + dark, 0.0) / (g * g) + rn_q}


def run_scenario(name: str, scene: Dict[str, Any]) -> Dict[str, Any]:
    t0 = time.time()
    truth_scene = copy.deepcopy(scene)
    truth_scene["stars"] = dict(truth_scene["stars"])
    truth_scene["stars"]["n"] = 0
    # **逐帧真值**：每帧自己的天光电平不同 ⇒ 真值方差图必须逐帧取（frame_index=k）。
    # 真值帧 stars.n=0，底图/平场确定性，故真值不含注入源自身的泊松。
    truth_k = []
    for k in range(N_FRAMES):
        tf, _ = RD.render_frame(truth_scene, seed=SEED, frame_index=k)
        truth_k.append(var_maps(truth_scene, tf))
    tmaps = truth_k[0]
    sig_true_local_k = [X.patch_truth_from_var(t["var_local"], DELTA) for t in truth_k]
    sig_true_local = sig_true_local_k[0]
    sig_true_bg = X.patch_truth_from_var(tmaps["var_bg"], DELTA)
    sig_true_frame_rms = float(np.sqrt(np.mean(tmaps["var_local"])))
    sig_true_frame_rms_k = [float(np.sqrt(np.mean(t["var_local"]))) for t in truth_k]

    sps, sfs, adus, sky_med = [], [], [], []
    for k in range(N_FRAMES):
        fr, _tr = RD.render_frame(scene, seed=SEED + 1000 * (k + 1), frame_index=k)
        adu = np.asarray(fr.adu, dtype=np.float64)
        adus.append(adu)
        sky_med.append(float(np.median(np.asarray(fr.sky_e, dtype=np.float64))))
        sfs.append(X.sigma_frame(adu))
        sps.append(X.sigma_patch_raw(adu, DELTA))
    sps_r1 = [X.sigma_patch_resid(a, DELTA) for a in adus]
    c0 = [X.frame_common_factor(sps[k], sfs[k]) for k in range(N_FRAMES)]
    c1 = [X.frame_common_factor(sps_r1[k], sfs[k]) for k in range(N_FRAMES)]

    lv_abs_r0 = X.level_dev(sps[0], sig_true_local)
    lv_rel_r0 = X.level_dev(sps[0] * c0[0], sig_true_local)
    lv_abs_r1 = X.level_dev(sps_r1[0], sig_true_local)
    lv_rel_r1 = X.level_dev(sps_r1[0] * c1[0], sig_true_local)

    # 跨帧比值（同一位置）：绝对表示 vs 相对表示
    xf_abs = X.cross_frame_ratio_dev(sps, sig_true_local_k, ref=0)
    xf_rel = X.cross_frame_ratio_dev([sps[k] * c0[k] for k in range(N_FRAMES)],
                                     sig_true_local_k, ref=0)
    xf_r1 = X.cross_frame_ratio_dev([sps_r1[k] * c1[k] for k in range(N_FRAMES)],
                                    sig_true_local_k, ref=0)
    # 跨帧组合 SNR^2 = sum SNR_k^2（CONTROL_WEIGHT_SNR 8a 第 3 条）
    snr_true = [1.0 / sig_true_local_k[k] for k in range(N_FRAMES)]
    snr_abs = [1.0 / sps[k] for k in range(N_FRAMES)]
    snr_rel = [1.0 / (sps[k] * c0[k]) for k in range(N_FRAMES)]
    comb_true = X.combined_snr_sq(snr_true)
    comb_abs = X.combined_snr_sq(snr_abs)
    comb_rel = X.combined_snr_sq(snr_rel)
    m = np.isfinite(comb_true) & (comb_true > 0)
    comb = {
        "abs_snr_dev": float(np.sqrt(np.median(comb_abs[m] / comb_true[m])) - 1.0),
        "rel_snr_dev": float(np.sqrt(np.median(comb_rel[m] / comb_true[m])) - 1.0),
    }
    return {
        "scenario": name, "n_frames": N_FRAMES, "n_stars": int(scene["stars"]["n"]),
        "target_p999_e": float(scene.get("real_base", {}).get("target_p999_e", 0.0)),
        "sky_e_median_per_frame": sky_med,
        "sigma_true_local_median": float(np.median(sig_true_local)),
        "sigma_true_bg_median": float(np.median(sig_true_bg)),
        "sigma_true_local_p95_over_p05": float(np.percentile(sig_true_local, 95)
                                               / np.percentile(sig_true_local, 5)),
        "sigma_true_frame_rms": sig_true_frame_rms,
        "c_agg": X.aggregation_mismatch(sig_true_local),
        "b_frame": X.frame_est_bias(sfs[0], sig_true_frame_rms_k[0]),
        "b_frame_per_frame": [X.frame_est_bias(sfs[k], sig_true_frame_rms_k[k])
                              for k in range(N_FRAMES)],
        "sigma_true_frame_rms_per_frame": sig_true_frame_rms_k,
        "b_patch_R0": lv_abs_r0["median_ratio"], "b_patch_R1": lv_abs_r1["median_ratio"],
        "c_sigma_R0": c0[0], "c_sigma_R1": c1[0],
        "c_sigma_R0_per_frame": c0, "c_sigma_R1_per_frame": c1,
        "frame_scalar_sigma_per_frame": sfs,
        "abs_snr_dev_R0": lv_abs_r0["snr_rel_dev"], "rel_snr_dev_R0": lv_rel_r0["snr_rel_dev"],
        "abs_snr_dev_R1": lv_abs_r1["snr_rel_dev"], "rel_snr_dev_R1": lv_rel_r1["snr_rel_dev"],
        "abs_p95_R0": lv_abs_r0["p95_abs_dev"], "rel_p95_R0": lv_rel_r0["p95_abs_dev"],
        "abs_p95_R1": lv_abs_r1["p95_abs_dev"], "rel_p95_R1": lv_rel_r1["p95_abs_dev"],
        "cross_frame_abs_R0": xf_abs, "cross_frame_rel_R0": xf_rel,
        "cross_frame_rel_R1": xf_r1,
        "combined_snr_abs_dev": comb["abs_snr_dev"],
        "combined_snr_rel_dev": comb["rel_snr_dev"],
        "elapsed_s": time.time() - t0,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e2_hst.json"))
    a = ap.parse_args()
    t0 = time.time()
    out: Dict[str, Any] = {"meta": {
        "seed": SEED, "shape": list(SHAPE), "delta_px": DELTA, "n_frames": N_FRAMES,
        "gain_e_per_adu": GAIN, "read_noise_e": RN_E, "exposure_s": EXPOSURE_S,
        "sky_e_per_s": SKY_E_PER_S,
        "data_class": "HST 真实模板 + 完整物理前向仿真（最高设计 12.2 第 1 类）",
        "truth": "前向链解析方差 Var = lam/g^2 + RN^2/g^2 + 1/12（lam = max(src_e+sky_e+dark_e,0)）；真值帧 stars.n=0",
        "renderer": "实验/shared/synthetic/render.py::render_frame（只读 import）",
        "hst_template": HST_GLOB,
    }, "scenarios": []}
    scen = [
        ("hst_null", make_scene(target_p999_e=0.0, scene_id="exp05_null")),
        ("hst_struct", make_scene(target_p999_e=1000.0, scene_id="exp05_struct")),
        ("hst_struct_varsky", make_scene(target_p999_e=1000.0,
                                         sky_scales=[0.7, 1.0, 1.3, 1.6],
                                         scene_id="exp05_struct_varsky")),
    ]
    for nm, sc in scen:
        r = run_scenario(nm, sc)
        out["scenarios"].append(r)
        print("%-20s b_f=%.4f b_R0=%.4f b_R1=%.4f c_sig_R0=%.4f | absSNR=%+.4f relSNR=%+.4f | comb abs=%+.4f rel=%+.4f | %.0fs"
              % (nm, r["b_frame"], r["b_patch_R0"], r["b_patch_R1"], r["c_sigma_R0"],
                 r["abs_snr_dev_R0"], r["rel_snr_dev_R0"],
                 r["combined_snr_abs_dev"], r["combined_snr_rel_dev"], r["elapsed_s"]),
              flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
