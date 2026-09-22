#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 臂 A：**纯理想合成数据 + 理想物理噪声重建**（最高设计 12.2 第 2 类数据）。

真值解析可得（analytic_scene 的 var_bg / var_local 两口径），检验
「用结构分离后的平滑弥散分量 + 帧级特性做物理重建」能否还原真值 sigma/SNR 场，
并与 EXP-04 的插值框架、帧级标量、逐像素代入亮度（错误做法）对照。

判据（全部非退化）：电平比 / p95 / RMSE(log10) / 权重效率损失 E / 方差域 R^2；
源区指标 src_level_ratio、src_snr_p05（伪暗洞）、snr_max_all；
负例：平坦真值场（真值无空间效应 ⇒ 重建场必须退化为常数）、零噪声真值（必须判退化）；
错误臂：逐像素代入亮度、不做结构分离、caliber P（逐像素显著性）⇒ 必须判红。

固定 seed：20260927。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402

SEED = X.SEED
SHAPE = (1024, 1024)
DELTA = 64
EDGE_MARGIN = 1


def scenes() -> List[Tuple[str, Dict[str, Any]]]:
    """(name, kwargs)。全部参数显式登记；seed 由 MC 循环注入。"""
    stars = dict(n_stars=80, star_flux_log10=(3.0, 6.0), star_fwhm_px=3.0)
    return [
        ("A0_flat", dict(sky_e_per_s=0.5)),
        ("A1_flat_bright_pixels", dict(sky_e_per_s=0.5, n_stars=6,
                                       star_flux_log10=(6.5, 7.0))),
        ("A2_gradient", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.5, sky_theta_deg=30.0)),
        ("A3_smooth_nebula", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.3, sky_theta_deg=110.0,
                                  blobs=[{"cy":330, "cx":260, "sigma_px":260, "amp_e_per_s":1.4},
                                         {"cy":180, "cx":760, "sigma_px":200, "amp_e_per_s":0.9}])),
        ("A4_realistic", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                              blobs=[{"cy":600, "cx":400, "sigma_px":300, "amp_e_per_s":1.2},
                                     {"cy":300, "cx":760, "sigma_px":200, "amp_e_per_s":0.7}],
                              filament={"y1":150, "x1":60, "y2":860, "x2":940,
                                        "width_px":3.0, "amp_e":8e3},
                              cr_rate_per_frame=400.0, prnu_rms=0.004, flat_low_order=0.01,
                              **stars)),
        ("A5_unresolved", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                               blobs=[{"cy":512, "cx":512, "sigma_px":40, "amp_e_per_s":1.0}],
                               **stars)),
        ("A6_strong_gradient", dict(sky_e_per_s=0.6, sky_grad_e_per_s=2.4, sky_theta_deg=15.0,
                                    **stars)),
        ("A7_read_noise_dominated", dict(sky_e_per_s=0.02, read_noise_e=15.0, **stars)),
    ]


def run_frame(name: str, kw: Dict[str, Any], seed: int) -> Dict[str, Any]:
    t0 = time.time()
    sc = X.analytic_scene(SHAPE, seed=seed, **kw)
    img, tr, meta = sc["adu"], sc["truth"], sc["meta"]
    g = float(meta["gain"])
    built = X.build_fields(img, gain=g, delta=DELTA, edge_margin=EDGE_MARGIN)
    ev = X.evaluate_fields(built["fields"], tr)
    emask = X.eval_mask_of(img.shape)
    smask = X.source_mask(tr, frac_of_floor=0.05)
    snr_true_frame = 1.0 / float(np.median(tr["sigma_bg"][emask]))
    calp = {}
    for k in ("phys", "naive_pixel", "frame_scalar"):
        if k in built["fields"]:
            calp[k] = X.caliber_p_overshoot(img, built["fields"][k], snr_true_frame, emask)
    return {
        "scene": name, "seed": int(seed), "kwargs": {k: v for k, v in kw.items()},
        "meta": {k: v for k, v in meta.items() if k != "stars"},
        "fits": built["fits"], "diag": built["diag"],
        "diffuse_meta": built["diffuse_meta"], "methods": ev, "caliber_p": calp,
        "snr_true_frame_canon": snr_true_frame,
        "elapsed_s": time.time() - t0,
    }


def negative_flat(seed: int) -> Dict[str, Any]:
    """真值无空间效应（平坦天光 + 无源）：重建场必须退化为常数。"""
    r = run_frame("N1_flat_truth", dict(sky_e_per_s=0.5), seed)
    return {"name": "N1_flat_truth", "seed": int(seed),
            "dispersion": {k: v["dispersion"] for k, v in r["methods"].items()
                           if not k.startswith("_")},
            "eff_loss_T2": {k: v["vs_T2_var_local"]["eff_loss"] for k, v in r["methods"].items()
                            if not k.startswith("_")},
            "level_ratio_T2": {k: v["vs_T2_var_local"]["level_ratio"]
                               for k, v in r["methods"].items() if not k.startswith("_")}}


def negative_zero_noise(seed: int) -> Dict[str, Any]:
    """真值无效应（零噪声）：全部度量必须判退化，不得静默产出数字。"""
    sc = X.analytic_scene(SHAPE, seed=seed, sky_e_per_s=0.0, dark_e_per_s=0.0,
                          read_noise_e=0.0, n_stars=0, noise=False, quantize=False)
    img = sc["adu"]
    emask = X.eval_mask_of(img.shape)
    is_deg = X.degenerate_zero_truth(sc["truth"]["var_bg"], emask)
    s_r1 = X.sigma_ctrl_resid(img, DELTA)
    fld = X.recon_frame_scalar(s_r1, img.shape)
    r = X.metrics(fld, sc["truth"]["sigma_bg"], emask)
    return {"name": "N2_zero_noise", "seed": int(seed), "truth_is_zero": bool(is_deg),
            "metrics_degenerate_flag": bool(r.get("degenerate", False)),
            "level_ratio": r.get("level_ratio"), "n_eval": r.get("n_eval")}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e1_analytic.json"))
    ap.add_argument("--seeds", type=int, default=6)
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    n_seed = 2 if a.quick else int(a.seeds)
    out: Dict[str, Any] = {"meta": {
        "unit": "EXP-06-SNR-PHYS", "arm": "A 纯解析代数合成 + 理想物理噪声重建",
        "seed_base": SEED, "n_seed": n_seed, "shape": list(SHAPE), "delta_px": DELTA,
        "edge_margin": EDGE_MARGIN,
        "truth_T1": "var_bg 空背景口径（天光+暗流+读出+量化，不含源泊松）",
        "truth_T2": "var_local 逐像素总方差口径（再加源泊松）",
        "canon": "全部判据为比值口径，F_ref=1 ADU、A_NEA=1 px 相消；SNR = 1/sigma",
    }, "frames": [], "negatives": []}
    for name, kw in scenes():
        for i in range(n_seed):
            r = run_frame(name, kw, SEED + 1000 * (i + 1))
            out["frames"].append(r)
            m = r["methods"]
            def _g(k, t, f="eff_loss"):
                return m.get(k, {}).get(t, {}).get(f, float("nan"))
            print("%-24s s%d | E(T2): phys=%.5f interp=%.5f frame=%.5f naive=%.5f unsep=%.5f"
                  % (name, i, _g("phys", "vs_T2_var_local"),
                     _g("interp_spline", "vs_T2_var_local"),
                     _g("frame_scalar", "vs_T2_var_local"),
                     _g("naive_pixel", "vs_T2_var_local"),
                     _g("phys_unsep", "vs_T2_var_local")), flush=True)
    for i in range(min(n_seed, 4)):
        out["negatives"].append(negative_flat(SEED + 1000 * (i + 1)))
    for i in range(min(n_seed, 3)):
        out["negatives"].append(negative_zero_noise(SEED + 1000 * (i + 1)))
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
