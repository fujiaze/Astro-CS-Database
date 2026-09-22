#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 / 实验臂 B：**HST 真实信号模板 + 完整物理前向仿真**（§12.2 第 1 类数据）。

复用仓内**只读**共享仿真器 实验/shared/synthetic/{render.py,noise_model.py}
（不修改、不复制、不改写其任何文件），本模块只做：
  1. 用 real_base_surface 取 HST M16 真实帧作**纯信号底图**，用 target_p999_e
     把底图亮度锚定到指定电子数（结构幅度可控）；
  2. 用 render_frame 做完整物理前向：源/天光/暗流电子域 Poisson，读噪电子域
     Gaussian，电子→ADU 过增益/饱和/量化，含平场 PRNU 与低阶空间项；
  3. 真值取**背景口径** σ_sky（天光+暗流+读噪+量化，**不含**底图星云自身的
     Poisson —— 后者单列为 neb_poisson，见报告 §诚实边界）；
  4. 对同一帧算生产 noise_sigma 与候选修法 + 全部无需真值的门代理量。

与 EXP-01 的关系：本臂**独立重写**（不 import exp01 的模块），
p999 网格与 EXP-01 §2.2② 对齐以便逐点对照；差异如实登记。

固定 seed = 20260925。不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp02_common as C  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
_SHARED = ROOT / "实验" / "shared" / "synthetic"
if str(_SHARED) not in sys.path:
    sys.path.insert(0, str(_SHARED))
import noise_model as NM  # noqa: E402
import render as RD       # noqa: E402

SEED = 20260925
GAIN = 1.3
RN_E = 10.0
EXPOSURE_S = 300.0
SKY_E_PER_S = 0.5
SHAPE = (512, 512)
FWHM_DET = 2.5                       # 检测块高斯 FWHM [px]
FW_SCENE = FWHM_DET / 2.3548200450309493 * 1.230310   # → PSF 块 Moffat4 FWHM
N_STARS = 80
HST_GLOB = "testdata/HST_M16/*M16*.fits|testdata/HST_M16/*.fits"
P999_GRID = (0.0, 60.0, 200.0, 1000.0, 50000.0)
BOX_PRIMARY = 32
DELTA_BUDGET = 0.014


def make_scene(*, n_stars: int, target_p999_e: float) -> Dict[str, Any]:
    """构造 render_frame 可消费的场景（参数全部显式登记）。"""
    sc: Dict[str, Any] = {
        "scene_id": "exp02_hst_sim", "kind": "exp02", "shape": list(SHAPE),
        "exposure_s": EXPOSURE_S,
        "psf": {"model": "moffat4", "fwhm_px": FW_SCENE, "beta": 4},
        "detector": {"gain_e_per_adu": GAIN, "read_noise_e": RN_E,
                     "bias_adu": 1000.0, "full_well_e": 120000.0},
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
    return sc


def truth_sigmas(scene: Dict[str, Any], frame: Any, rb_rate_e_per_s: np.ndarray | None
                 ) -> Dict[str, float]:
    """真值 σ（ADU）：背景口径（不含底图 Poisson）与含底图 Poisson 口径。"""
    det = NM.Detector(**scene["detector"])
    g = det.gain_e_per_adu
    sky_e = np.asarray(frame.sky_e, dtype=np.float64)
    dark_e = np.asarray(frame.dark_e, dtype=np.float64)
    base_bg = sky_e + dark_e
    v_bg = base_bg / (g * g) + (RN_E ** 2) / (g * g) + 1.0 / 12.0
    sig_bg = float(np.sqrt(np.mean(v_bg)))
    if rb_rate_e_per_s is None:
        return {"sigma_sky_true_adu": sig_bg,
                "sigma_pix_true_adu": float(np.sqrt(np.mean(
                    v_bg + np.asarray(frame.src_e) / (g * g)))),
                "neb_poisson_adu": 0.0}
    neb_e = rb_rate_e_per_s * EXPOSURE_S
    v_neb = neb_e / (g * g)
    sig_pix = float(np.sqrt(np.mean(v_bg + v_neb)))
    return {"sigma_sky_true_adu": sig_bg, "sigma_pix_true_adu": sig_pix,
            "neb_poisson_adu": float(math.sqrt(max(sig_pix ** 2 - sig_bg ** 2, 0.0)))}


# 结构幅度按 **p99.9 幅度 / σ_sky**（"结构在多少个 σ 上"）参数化。
# 为什么不用"结构 rms/σ_sky"参数化：底图（HST M16 核心区）的动态范围极大，
# rms 由最亮的 0.01% 像素主导，rms/(p99.9−p5) 可达数百倍 ⇒ 同一个 rms 比值
# 对应完全不同的**裁剪行为**（见 results/exp02_e2_hst.json 的 struct_rms_over_noise
# 与 struct_p999_over_noise 两列，报告 §5.2 讨论）。p99.9 幅度才是驱动裁剪的量。
# real_base_surface 把 p99.9 严格映射到 target_p999_e 电子数，故
#   target_p999_e = ratio · σ_sky[ADU] · gain   ⇒ p99.9/σ_sky = ratio。
RATIO_GRID = (0.0, 1.0, 2.0, 3.0, 5.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 5000.0)


def _sigma_sky_true_adu() -> float:
    """无底图场景的真值背景口径 σ（ADU）：由噪声模型解析给出。"""
    sc = make_scene(n_stars=0, target_p999_e=0.0)
    fr, _ = RD.render_frame(sc, seed=SEED)
    return truth_sigmas(sc, fr, None)["sigma_sky_true_adu"]


def _p999_for_ratio(ratio: float, sig_sky_adu: float) -> float:
    """反解达到 p99.9/σ_sky = ratio 所需的 target_p999_e（电子数）。"""
    return max(ratio, 0.0) * sig_sky_adu * GAIN


# EXP-01 §2.2② 的原始网格（逐点对照用；两臂场景参数完全相同）
EXP01_P999_GRID = (0.0, 60.0, 200.0, 1000.0, 50000.0)


def _scan_grid(grid, sig_sky_adu: float, tag: str) -> List[Dict[str, Any]]:
    """在给定 target_p999_e 网格上跑一遍（含 nostar/raw 两个变体）。"""
    rows: List[Dict[str, Any]] = []
    struct_ref: Dict[str, float] = {}
    for p999 in grid:
        for n_stars, variant in ((0, "nostar"), (N_STARS, "raw")):
            sc = make_scene(n_stars=n_stars, target_p999_e=p999)
            frame, truth = RD.render_frame(sc, seed=SEED)
            rb_rate = None
            if p999 > 0:
                rb_rate, _ = RD.real_base_surface(sc["real_base"], SHAPE, (0, 0), 0)
            ts = truth_sigmas(sc, frame, rb_rate)
            if n_stars == 0:
                src_adu = np.asarray(frame.src_e, dtype=np.float64) / GAIN
                struct_ref = {"std": C.rms_of(src_adu),
                              "p999": float(np.percentile(src_adu, 99.9))
                              - float(np.median(src_adu))}
            base_std_adu = struct_ref["std"]
            p999_adu = struct_ref["p999"]
            p = C.structure_proxies(frame.adu, box=BOX_PRIMARY, filter_size=3,
                                    n_iter=2)
            verdict = C.classify(p["R_struct"], p["C"], p["kf"],
                                 delta_budget=DELTA_BUDGET, A1=p["A1"],
                                 A2=p["A2"], D=p["D"])
            rows.append({
                "grid": tag, "target_p999_e": p999, "variant": variant,
                "n_stars": n_stars, "struct_rms_adu": base_std_adu,
                "struct_p999_adu": p999_adu,
                "struct_over_noise": base_std_adu / ts["sigma_sky_true_adu"],
                "struct_p999_over_noise": p999_adu / ts["sigma_sky_true_adu"],
                **ts,
                "prod_sigma": p["prod_sigma"],
                "prod_rel_err": C.rel_err(p["prod_sigma"], ts["sigma_sky_true_adu"]),
                "prod_rel_err_vs_pix": C.rel_err(p["prod_sigma"], ts["sigma_pix_true_adu"]),
                "fix_sigma": p["fix_sigma"],
                "fix_rel_err": C.rel_err(p["fix_sigma"], ts["sigma_sky_true_adu"]),
                "fix_rel_err_vs_pix": C.rel_err(p["fix_sigma"], ts["sigma_pix_true_adu"]),
                "fix_effect_delta": p["fix_effect_delta"],
                "prod_keep_frac": p["kf"], "R_struct": p["R_struct"],
                "A1": p["A1"], "A2": p["A2"], "C_gate": p["C"], "D": p["D"],
                "verdict": verdict,
                "n_saturated": truth["adu_stats"]["saturated_pixels"],
            })
    return rows


def run_arm() -> Dict[str, Any]:
    rows: List[Dict[str, Any]] = []
    sig_sky_adu = _sigma_sky_true_adu()
    anchors = []
    for ratio in RATIO_GRID:
        p999 = _p999_for_ratio(ratio, sig_sky_adu)
        anchors.append({"ratio_p999_over_sigma_target": ratio,
                        "target_p999_e": p999})
        for n_stars, tag in ((0, "nostar"), (N_STARS, "raw")):
            sc = make_scene(n_stars=n_stars, target_p999_e=p999)
            frame, truth = RD.render_frame(sc, seed=SEED)
            rb_rate = None
            if p999 > 0:
                rb_rate, rb_meta = RD.real_base_surface(
                    sc["real_base"], SHAPE, (0, 0), 0)
            ts = truth_sigmas(sc, frame, rb_rate)
            # 帧内**真实**结构：用 n_stars=0 的同一场景取 frame.src_e（就是底图贡献，
            # 含 expose 的 max(.,0) 截断）—— 这是仿真器给的真值，不依赖解析近似。
            # **必须用 nostar 场景**：raw 场景的 src_e 还含 80 颗星的通量。
            if n_stars == 0:
                src_adu = np.asarray(frame.src_e, dtype=np.float64) / GAIN
                base_std_adu = C.rms_of(src_adu)
                p999_adu = (float(np.percentile(src_adu, 99.9))
                            - float(np.median(src_adu)))
                struct_ref = {"std": base_std_adu, "p999": p999_adu}
            else:
                base_std_adu = struct_ref["std"]
                p999_adu = struct_ref["p999"]
            p = C.structure_proxies(frame.adu, box=BOX_PRIMARY, filter_size=3, n_iter=2)
            verdict = C.classify(p["R_struct"], p["C"], p["kf"],
                                 delta_budget=DELTA_BUDGET, A1=p["A1"],
                                 A2=p["A2"], D=p["D"])
            rows.append({
                "ratio_target": ratio,
                "target_p999_e": p999, "variant": tag, "n_stars": n_stars,
                "struct_rms_adu": base_std_adu,
                "struct_p999_adu": p999_adu,
                "struct_over_noise": base_std_adu / ts["sigma_sky_true_adu"],
                "struct_p999_over_noise": p999_adu / ts["sigma_sky_true_adu"],
                **ts,
                "prod_sigma": p["prod_sigma"], "prod_rel_err":
                    C.rel_err(p["prod_sigma"], ts["sigma_sky_true_adu"]),
                "prod_rel_err_vs_pix": C.rel_err(p["prod_sigma"], ts["sigma_pix_true_adu"]),
                "fix_sigma": p["fix_sigma"],
                "fix_rel_err": C.rel_err(p["fix_sigma"], ts["sigma_sky_true_adu"]),
                "fix_rel_err_vs_pix": C.rel_err(p["fix_sigma"], ts["sigma_pix_true_adu"]),
                "fix_effect_delta": p["fix_effect_delta"],
                "prod_keep_frac": p["kf"], "R_struct": p["R_struct"],
                "A1": p["A1"], "A2": p["A2"], "C_gate": p["C"], "D": p["D"],
                "verdict": verdict,
                "n_saturated": truth["adu_stats"]["saturated_pixels"],
            })
    # 结构无效应负例：不用底图（p999=0）时修法效应必须归零（已在 rows 里给单点，
    # 此处另给多次实现的 3σ）
    # 结构无效应负例：**必须**同时关掉平场 PRNU / 低阶空间项 / 渐晕，否则
    # "无结构"场景其实带真实的乘性空间结构（平场图样），修法会合法地改变数值。
    deltas, flat_on_deltas = [], []
    for i in range(8):
        sc = make_scene(n_stars=0, target_p999_e=0.0)
        sc["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "vignette": 0.0, "seed": 1234}
        fr, _ = RD.render_frame(sc, seed=SEED + 100 + i)
        p = C.structure_proxies(fr.adu, box=BOX_PRIMARY, filter_size=3, n_iter=2)
        deltas.append(p["fix_effect_delta"])
        fr2, _ = RD.render_frame(make_scene(n_stars=0, target_p999_e=0.0),
                                 seed=SEED + 100 + i)
        p2 = C.structure_proxies(fr2.adu, box=BOX_PRIMARY, filter_size=3, n_iter=2)
        flat_on_deltas.append(p2["fix_effect_delta"])
    d = np.array(deltas)
    df = np.array(flat_on_deltas)
    neg = {"n_rep": int(d.size), "delta_mean": float(d.mean()),
           "delta_sem": float(d.std(ddof=1) / math.sqrt(d.size)),
           "delta_3sigma": float(3.0 * d.std(ddof=1) / math.sqrt(d.size)),
           "delta_max_abs": float(np.abs(d).max()),
           "G_null_zero_pass": bool(abs(d.mean()) <= 3.0 * d.std(ddof=1) / math.sqrt(d.size)),
           "flat_off_scene": {"prnu_rms": 0.0, "low_order": 0.0, "vignette": 0.0},
           "delta_with_flat_on_mean": float(df.mean()),
           "delta_with_flat_on_sem": float(df.std(ddof=1) / math.sqrt(df.size)),
           "note": ("平场 ON 时 Δ ≠ 0 是**正确行为**：PRNU/低阶/渐晕是真实的乘性空间结构，"
                    "mesh 修法会把它一并吸收；故本臂的'结构无效应'负例必须用 flat OFF 场景。")}
    return {"rows": rows, "negatives_no_base": neg, "anchors": anchors,
            "rows_exp01_grid": _scan_grid(EXP01_P999_GRID, sig_sky_adu, "exp01")}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp02_e2_hst.json")
    args = ap.parse_args()
    t0 = time.time()
    arm = run_arm()
    out = {"meta": {"seed": SEED, "shape": list(SHAPE), "gain": GAIN, "rn_e": RN_E,
                    "exposure_s": EXPOSURE_S, "sky_e_per_s": SKY_E_PER_S,
                    "fwhm_det_px": FWHM_DET, "p999_grid": list(P999_GRID),
                    "box_primary": BOX_PRIMARY, "delta_budget": DELTA_BUDGET,
                    "hst_glob": HST_GLOB,
                    "data_class": "HST 真实信号模板 + 完整物理前向仿真（最高设计 §12.2 第 1 类）"},
           **arm}
    out["meta"]["elapsed_s"] = time.time() - t0
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float),
                 encoding="utf-8")
    print("%-9s %-7s %12s %11s %11s %12s %12s %8s %8s %8s %s" %
          ("p999_e", "variant", "struct_rms", "rms/sig", "p999/sig", "prod_rel",
           "fix_rel", "kf", "A1", "A2", "verdict"))
    for r in out["rows"]:
        print("%-9.2f %-7s %12.1f %11.2f %11.2f %+12.4f %+12.4f %8.4f %8.3f %8.4f %s" %
              (r["target_p999_e"], r["variant"], r["struct_rms_adu"],
               r["struct_over_noise"], r["struct_p999_over_noise"],
               r["prod_rel_err"], r["fix_rel_err"],
               r["prod_keep_frac"], r["A1"], r["A2"], r["verdict"]))
    print("== EXP-01 §2.2(2) 原始 p999 网格（逐点对照）==")
    print("%-10s %-7s %11s %11s %12s %12s" %
          ("p999_e", "variant", "rms/sig", "p999/sig", "prod_rel", "fix_rel"))
    for r in out["rows_exp01_grid"]:
        print("%-10.1f %-7s %11.2f %11.2f %+12.4f %+12.4f" %
              (r["target_p999_e"], r["variant"], r["struct_over_noise"],
               r["struct_p999_over_noise"], r["prod_rel_err"], r["fix_rel_err"]))
    print("negatives:", json.dumps(out["negatives_no_base"], ensure_ascii=False))
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
