#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 臂 B：**哈勃数据 + 理想物理噪声过程重建**（最高设计 12.2 第 1 类数据）。

用 testdata/HST_M16 的真实信号模板作底，按**已知物理参数**（曝光 300 s、增益 1.3 e-/ADU、
读出 10 e-）正向仿真出噪声 ⇒ 「已知噪声过程的真实数据」，真值逐像素解析可得。
在真实结构（含未分辨亮源、星云边缘）上检验物理重建的稳健性。

只读 import 仓内共享仿真器 实验/shared/synthetic/{render.py,noise_model.py}。
固定 seed：20260927。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402

_SHARED = X.ROOT / "实验" / "shared" / "synthetic"
if str(_SHARED) not in sys.path:
    sys.path.insert(0, str(_SHARED))
import noise_model as NM  # noqa: E402
import render as RD       # noqa: E402

SEED = X.SEED
SHAPE = (1024, 1024)
DELTA = 64
EDGE_MARGIN = 1
N_FRAMES = 3
EXPOSURE_S = 300.0
GAIN = 1.3
RN_E = 10.0
FW_SCENE = 2.5 / 2.3548200450309493 * 1.230310
HST_GLOB = "testdata/HST_M16/*M16*.fits|testdata/HST_M16/*.fits"


def make_scene(*, target_p999_e: float, n_stars: int = 0,
               sky_scales: Optional[List[float]] = None, sky_e_per_s: float = 0.5,
               scene_id: str = "exp06_hst", stars_seed: int = 1616) -> Dict[str, Any]:
    sc: Dict[str, Any] = {
        "scene_id": scene_id, "kind": "exp06", "shape": list(SHAPE),
        "exposure_s": EXPOSURE_S,
        "psf": {"model": "moffat4", "fwhm_px": FW_SCENE, "beta": 4},
        "detector": {"gain_e_per_adu": GAIN, "read_noise_e": RN_E,
                     "bias_adu": 1000.0, "full_well_e": 120000.0,
                     "dark_current_e_per_s": 0.0, "dark_ref_temp_c": -20.0},
        "flat": {"prnu_rms": 0.005, "low_order": 0.01, "vignette": 0.03, "seed": 1234},
        "sky": {"level_e_per_s": float(sky_e_per_s), "grad_x_e_per_s": 0.0,
                "grad_y_e_per_s": 0.0, "theta_deg": 0.0},
        "nebula": [],
        "stars": {"n": int(n_stars), "flux_log10_range": [3.5, 6.0],
                  "clustering": "uniform", "seed": int(stars_seed)},
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
        sc["frames"] = [{"sky": {"level_e_per_s": sky_e_per_s * s}} for s in sky_scales]
    return sc


def truth_maps(frame, det: NM.Detector) -> Dict[str, np.ndarray]:
    """前向链解析真值：T1 = 空背景口径（不含源泊松），T2 = 逐像素总方差口径。"""
    g = det.gain_e_per_adu
    src = np.asarray(frame.src_e, dtype=np.float64)
    sky = np.asarray(frame.sky_e, dtype=np.float64)
    dark = np.asarray(frame.dark_e, dtype=np.float64)
    rn_q = (det.read_noise_e ** 2) / (g * g) + (1.0 / 12.0 if det.quantize else 0.0)
    return {"var_bg": np.maximum(sky + dark, 0.0) / (g * g) + rn_q,
            "var_local": np.maximum(src + sky + dark, 0.0) / (g * g) + rn_q,
            "src_e": src, "sky_e": sky, "dark_e": dark}


def run_scenario(name: str, sc: Dict[str, Any], n_frames: int = N_FRAMES) -> Dict[str, Any]:
    t0 = time.time()
    det = NM.Detector(**sc["detector"])
    frames_out: List[Dict[str, Any]] = []
    ctrl_sigma: List[np.ndarray] = []
    ctrl_true: List[np.ndarray] = []
    for k in range(n_frames):
        fr, tr_meta = RD.render_frame(sc, seed=SEED + 1000 * (k + 1), frame_index=k)
        adu = np.asarray(fr.adu, dtype=np.float64)
        tm = truth_maps(fr, det)
        truth = {"var_bg": tm["var_bg"], "var_local": tm["var_local"],
                 "sigma_bg": X.sigma_of_var(tm["var_bg"]),
                 "sigma_local": X.sigma_of_var(tm["var_local"]),
                 "src_e": tm["src_e"], "diffuse_e": tm["sky_e"] + tm["dark_e"]}
        sat = det.saturation_adu
        # 伪迹掩膜（宇宙线/热像素/饱和）：只用于评价
        bg_est, _ = X.diffuse_component(adu, box=DELTA, mode="spline")
        art = adu > (bg_est + 20.0 * np.sqrt(np.maximum(tm["var_local"], 0.0)))
        art |= adu >= sat
        built = X.build_fields(adu, gain=GAIN, delta=DELTA, edge_margin=EDGE_MARGIN)
        smask = X.source_mask(truth, frac_of_floor=0.05) | art
        ev = X.evaluate_fields(built["fields"], truth, sat_adu=sat, adu=adu,
                               src_mask=smask)
        emask = X.eval_mask_of(adu.shape, sat_adu=sat, adu=adu)
        snr_true_frame = 1.0 / float(np.median(truth["sigma_bg"][emask]))
        calp = {}
        for kk in ("phys", "naive_pixel", "frame_scalar"):
            if kk in built["fields"]:
                calp[kk] = X.caliber_p_overshoot(adu, built["fields"][kk], snr_true_frame, emask)
        ctrl_sigma.append(built["ctrl"]["s_r1"])
        ctrl_true.append(X.truth_ctrl(tm["var_local"], DELTA))
        frames_out.append({
            "frame_index": k, "seed": int(SEED + 1000 * (k + 1)),
            "sky_e_median": float(np.median(tm["sky_e"])),
            "src_e_median": float(np.median(tm["src_e"])),
            "src_e_p999": float(np.percentile(tm["src_e"], 99.9)),
            "sigma_bg_median": float(np.median(truth["sigma_bg"])),
            "sigma_local_over_bg_p999": float(np.percentile(
                truth["sigma_local"] / truth["sigma_bg"], 99.9)),
            "sat_frac": float(np.mean(adu >= sat)),
            "art_frac": float(art.mean()), "src_frac": float(smask.mean()),
            "fits": built["fits"], "diag": built["diag"], "methods": ev,
            "caliber_p": calp, "render_meta": {
                "n_stars_in_frame": tr_meta.get("n_stars_in_frame"),
                "saturated_pixels": tr_meta["adu_stats"]["saturated_pixels"],
                "real_base": {kk: tr_meta["real_base"].get(kk) for kk in
                              ("path", "crop_used", "dynamic_range_anchor", "smooth_sigma_px")},
            },
        })
        print("  %-22s frame %d  sigma_bg_med=%.3f  src_frac=%.4f  %.1fs"
              % (name, k, frames_out[-1]["sigma_bg_median"], smask.mean(),
                 time.time() - t0), flush=True)
    # 跨帧一致性（不同天光电平）：控制点 sigma 比 vs 真值比
    xf: Dict[str, Any] = {}
    for k in range(1, n_frames):
        for tag, arr in (("R1_ctrl", ctrl_sigma), ("truth_ctrl", ctrl_true)):
            e = arr[k] / np.maximum(arr[0], 1e-300)
            t = ctrl_true[k] / np.maximum(ctrl_true[0], 1e-300)
            m = np.isfinite(e) & np.isfinite(t) & (t > 0)
            if m.sum() < 8:
                continue
            r = e[m] / t[m]
            xf["%s_frame%d_over_0" % (tag, k)] = {
                "median_ratio": float(np.median(r)),
                "p95_abs_dev": float(np.percentile(np.abs(r - 1.0), 95)), "n": int(m.sum())}
    return {"scenario": name, "n_frames": n_frames, "frames": frames_out,
            "cross_frame": xf, "elapsed_s": time.time() - t0}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e2_hst.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    nf = 1 if a.quick else N_FRAMES
    scen = [
        ("B0_hst_base", make_scene(target_p999_e=1000.0, scene_id="exp06_B0"), 1),
        ("B1_hst_stars", make_scene(target_p999_e=1000.0, n_stars=60,
                                    scene_id="exp06_B1"), nf),
        ("B2_hst_varsky", make_scene(target_p999_e=1000.0, n_stars=60,
                                     sky_scales=[0.7, 1.0, 1.3, 1.6],
                                     scene_id="exp06_B2"), 4 if not a.quick else 2),
        ("B3_hst_lowsky", make_scene(target_p999_e=1000.0, n_stars=60,
                                     sky_e_per_s=0.05, scene_id="exp06_B3"), nf),
        ("B4_hst_highsky", make_scene(target_p999_e=1000.0, n_stars=60,
                                      sky_e_per_s=5.0, scene_id="exp06_B4"), nf),
        ("B5_hst_bright", make_scene(target_p999_e=20000.0, n_stars=60,
                                     scene_id="exp06_B5"), nf),
    ]
    out: Dict[str, Any] = {"meta": {
        "unit": "EXP-06-SNR-PHYS", "arm": "B HST 真实模板 + 完整物理前向仿真",
        "seed_base": SEED, "shape": list(SHAPE), "delta_px": DELTA,
        "exposure_s": EXPOSURE_S, "gain_e_per_adu": GAIN, "read_noise_e": RN_E,
        "hst_template": HST_GLOB,
        "renderer": "实验/shared/synthetic/render.py::render_frame（只读 import）",
        "truth_T1": "var_bg = (sky_e+dark_e)/g^2 + RN^2/g^2 + 1/12",
        "truth_T2": "var_local = (src_e+sky_e+dark_e)/g^2 + RN^2/g^2 + 1/12",
        "note": "真实帧为归一化产品，e_per_adu 是场景参数；不得据此反推真实仪器参数",
    }, "scenarios": []}
    for name, sc, nfr in scen:
        r = run_scenario(name, sc, nfr)
        out["scenarios"].append(r)
        f0 = r["frames"][0]
        m = f0["methods"]
        print("%-16s E(T2): phys=%.5f interp=%.5f frame=%.5f naive=%.5f | %.0fs"
              % (name, m.get("phys", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan")),
                 m.get("interp_spline", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan")),
                 m.get("frame_scalar", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan")),
                 m.get("naive_pixel", {}).get("vs_T2_var_local", {}).get("eff_loss", float("nan")),
                 r["elapsed_s"]), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
