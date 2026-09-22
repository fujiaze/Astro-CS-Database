#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HST 真实信号模板 + 完整物理前向仿真臂（§12.2 第 1 类数据）。

复用仓内既有、**只读**的共享仿真器 实验/shared/synthetic/render.py 与 noise_model.py
（不修改、不复制）；本模块只负责"用一组受控场景参数构造场景字典 + 取出真值"。

真值来源（关键）：
  * frame.truth_e  = 期望电子面 = t*(src+sky)*m + t*D  ⇒ 逐像素真值均值；
  * frame.src_e / sky_e / dark_e / flat ⇒ 逐项分量，可算逐像素**真值方差**
    V_i = (src+sky+dark)/g^2 + RN^2/g^2 + 1/12（noise_model.predicted_variance_adu2）。
  * truth["stars_in_frame"] = 逐星真值总通量 [e-]（曝光积分后），用于估计量对照。
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
_SHARED = ROOT / "实验" / "shared" / "synthetic"
if str(_SHARED) not in sys.path:
    sys.path.insert(0, str(_SHARED))

import noise_model as NM          # noqa: E402
import render as RD               # noqa: E402

HST_GLOB = "testdata/HST_M16/*M16*.fits|testdata/HST_M16/*.fits"


def make_scene(*, shape=(1024, 1024), exposure_s=300.0, fwhm_px=2.5,
               sky_e_per_s=0.5, gain=1.3, read_noise_e=10.0, bias_adu=1000.0,
               n_stars=60, flux_log10=(3.5, 6.0), stars_seed=1616,
               use_hst_base=True, prnu_rms=0.005, low_order=0.01, vignette=0.03,
               real_base_glob_index=0, flat_seed=1234, hot_fraction=0.0,
               cr_rate=0.0, mode="physical", target_p999_e=50000.0,
               smooth_sigma_px=1.5) -> Dict[str, Any]:
    """构造一个 render.render_frame 可消费的场景（参数全部显式登记）。"""
    sc: Dict[str, Any] = {
        "scene_id": "exp01_hst_sim",
        "kind": "exp01",
        "shape": list(shape),
        "exposure_s": float(exposure_s),
        "psf": {"model": "moffat4", "fwhm_px": float(fwhm_px), "beta": 4},
        "detector": {"gain_e_per_adu": float(gain), "read_noise_e": float(read_noise_e),
                     "bias_adu": float(bias_adu), "full_well_e": 120000.0},
        "flat": {"prnu_rms": float(prnu_rms), "low_order": float(low_order),
                 "vignette": float(vignette), "seed": int(flat_seed)},
        "sky": {"level_e_per_s": float(sky_e_per_s), "grad_x_e_per_s": 0.0,
                "grad_y_e_per_s": 0.0, "theta_deg": 0.0},
        "nebula": [],
        "stars": {"n": int(n_stars), "flux_log10_range": list(flux_log10),
                  "clustering": "uniform", "seed": int(stars_seed)},
        "artifacts": {"cr_rate_per_frame": float(cr_rate), "cr_mean_charge_e": 900.0,
                      "hot_pixel_fraction": float(hot_fraction)},
        "mode": mode,
    }
    if use_hst_base:
        sc["real_base"] = {
            "path": HST_GLOB, "glob_index": int(real_base_glob_index),
            "auto_bright": {"size": 1024, "block": 256, "mode": "mean"},
            "smooth_sigma_px": float(smooth_sigma_px),
            "target_p999_e": float(target_p999_e), "subtract_percentile": 5,
        }
    return sc


def render(scene: Dict[str, Any], *, seed: int, frame_index: int = 0,
           cache: Optional[Dict[str, Any]] = None) -> Tuple[Any, Dict[str, Any]]:
    """渲染一帧（薄封装，便于统一 seed 与缓存）。"""
    return RD.render_frame(scene, seed=int(seed), frame_index=int(frame_index),
                           canvas_cache=cache)


def truth_variance_adu2(frame, det: NM.Detector) -> np.ndarray:
    """逐像素**真值**方差 [ADU^2]（含量化 1/12；饱和像素另行剔除）。"""
    g = det.gain_e_per_adu
    v = (frame.src_e + frame.sky_e + frame.dark_e) / (g * g)
    v = v + (det.read_noise_e ** 2) / (g * g)
    if det.quantize:
        v = v + 1.0 / 12.0
    return v


def detector_of(scene: Dict[str, Any]) -> NM.Detector:
    return NM.Detector(**{k: v for k, v in scene["detector"].items()})


def truth_sigma_adu(frame, det: NM.Detector) -> float:
    """真值"全帧等效" rms [ADU]：sqrt(mean(V_i))（与生产 recipe 的全局尺度同口径）。"""
    V = truth_variance_adu2(frame, det)
    sat = det.saturation_adu
    m = np.isfinite(frame.adu) & (frame.adu < sat)
    return float(np.sqrt(np.mean(V[m])))
