#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 A：**纯解析代数合成**（最高设计 12.2 第 2 类数据，真值完全已知）。

回答的问题：**在真值已知时，绝对表示与相对表示各自的 SNR 电平偏差是多少？**

构造（真值 = 逐像素解析方差，无 MC 近似）：
    I(x,y) = P(x,y) + N(0, sigma_true(x,y))          [ADU]
    sigma_true(x,y) 由场景**显式给定**（常数 / 斜坡 / 高斯随机场）；
    P(x,y) 是**结构**（星云状平滑场 + 斜坡），它只污染**估计器**，不改变真值噪声。

三个可分离的效应（本臂的核心贡献，见报告 2）：
  (i)   聚合口径错配 c_agg = RMS(sigma_true)/median_patch(sigma_true)  —— 纯定义性，结构无关；
  (ii)  帧级估计器污染 b_f = sigma_hat_frame/RMS(sigma_true)          —— 结构污染；
  (iii) 局部估计器污染 b_c = median(sigma_hat_patch)/median(sigma_true) —— 作用域缩小后的残余污染。
  相对表示的电平误差由 (i)(ii) 驱动，绝对表示由 (iii) 驱动。

非退化负例（必须归零）：sigma_true 为常数且 P == 0 时，两种表示必须给出**同一个数**
（c_agg = 1、b_f = b_c = 1 + 生产裁剪固有低偏），此时任何「绝对优于相对」的判据都退化。

固定 seed：SEED = 20260926。不运行任何 AstroCS 可执行文件。
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
import exp05_common as X  # noqa: E402

SHAPE = (1024, 1024)
DELTA = 64
N_MC = 8
SIGMA0 = 20.0            # 基准噪声 [ADU]


def smooth_field(shape, rms, corr_px, rng):
    from scipy.ndimage import gaussian_filter
    w = rng.normal(0.0, 1.0, size=shape)
    f = gaussian_filter(w, sigma=float(corr_px), mode="wrap")
    f = f - f.mean()
    sd = float(f.std())
    return f * (rms / sd) if sd > 0 else f


def build_scenarios():
    """返回 [(name, desc, sigma_field_fn, struct_fn)]；rng 由调用方传入。"""
    ny, nx = SHAPE
    yy, xx = np.mgrid[0:ny, 0:nx]

    def s_flat(rng):
        return np.full(SHAPE, SIGMA0)

    def s_vary(rng):
        g = smooth_field(SHAPE, 1.0, 128.0, rng)
        return SIGMA0 * np.exp(0.30 * g)

    def s_ramp(rng):
        r = (xx / float(nx - 1))
        return SIGMA0 * (1.0 + 0.8 * r)

    def p_zero(rng):
        return np.zeros(SHAPE)

    def p_struct(rng):
        g = smooth_field(SHAPE, 30.0, 128.0, rng)
        ramp = 25.0 * (xx / float(nx - 1))
        return g + ramp

    # 第 1 条是**设计好的非退化负例**（真值无效应）：两种表示必须给出同一个数
    return [
        ("flat_no_struct", "真值无效应负例：sigma 常数 + 无结构", s_flat, p_zero),
        ("flat_with_struct", "sigma 常数 + 结构污染（只动帧级估计器）", s_flat, p_struct),
        ("vary_no_struct", "sigma 场有空间离散 + 无结构（只动聚合口径）", s_vary, p_zero),
        ("ramp_no_struct", "sigma 线性斜坡（聚合错配可解析）", s_ramp, p_zero),
        ("vary_with_struct", "两者齐备", s_vary, p_struct),
    ]


def run_scenario(name, desc, sig_fn, struct_fn, n_mc=N_MC, seed=X.SEED):
    rows: List[Dict[str, Any]] = []
    for m in range(n_mc):
        rng = np.random.default_rng(seed + 101 * m + abs(hash(name)) % 1000)
        sig = np.asarray(sig_fn(rng), dtype=np.float64)
        struct = np.asarray(struct_fn(rng), dtype=np.float64)
        img = struct + rng.normal(0.0, 1.0, size=SHAPE) * sig
        var_true = sig * sig
        # --- 真值 ---
        sig_true_frame_rms = float(np.sqrt(np.mean(var_true)))
        sig_true_patch = X.patch_truth_from_var(var_true, DELTA)
        # --- 估计器 ---
        sf = X.sigma_frame(img)
        sp_r0 = X.sigma_patch_raw(img, DELTA)
        sp_r1 = X.sigma_patch_resid(img, DELTA)
        c_sig_r0 = X.frame_common_factor(sp_r0, sf)
        c_sig_r1 = X.frame_common_factor(sp_r1, sf)
        # --- 电平偏差（sigma 空间）---
        lv_abs_r0 = X.level_dev(sp_r0, sig_true_patch)
        lv_abs_r1 = X.level_dev(sp_r1, sig_true_patch)
        lv_rel_r0 = X.level_dev(sp_r0 * c_sig_r0, sig_true_patch)
        lv_rel_r1 = X.level_dev(sp_r1 * c_sig_r1, sig_true_patch)
        rows.append({
            "b_frame": X.frame_est_bias(sf, sig_true_frame_rms),
            "b_patch_R0": lv_abs_r0["median_ratio"],
            "b_patch_R1": lv_abs_r1["median_ratio"],
            "c_sigma_R0": c_sig_r0,
            "c_sigma_R1": c_sig_r1,
            "c_agg": X.aggregation_mismatch(sig_true_patch),
            "abs_sigma_ratio_R0": lv_abs_r0["median_ratio"],
            "rel_sigma_ratio_R0": lv_rel_r0["median_ratio"],
            "abs_snr_dev_R0": lv_abs_r0["snr_rel_dev"],
            "rel_snr_dev_R0": lv_rel_r0["snr_rel_dev"],
            "abs_snr_dev_R1": lv_abs_r1["snr_rel_dev"],
            "rel_snr_dev_R1": lv_rel_r1["snr_rel_dev"],
            "abs_p95_R0": lv_abs_r0["p95_abs_dev"],
            "rel_p95_R0": lv_rel_r0["p95_abs_dev"],
            "abs_p95_R1": lv_abs_r1["p95_abs_dev"],
            "rel_p95_R1": lv_rel_r1["p95_abs_dev"],
            "sigma_true_frame_rms": sig_true_frame_rms,
            "sigma_true_patch_median": float(np.median(sig_true_patch)),
        })
    def med(key):
        v = np.array([r[key] for r in rows], dtype=np.float64)
        return float(np.median(v))

    def sd(key):
        v = np.array([r[key] for r in rows], dtype=np.float64)
        return float(np.std(v, ddof=1)) if v.size > 1 else 0.0

    out = {"scenario": name, "desc": desc, "n_mc": n_mc,
           "c_agg": med("c_agg"), "b_frame": med("b_frame"),
           "b_patch_R0": med("b_patch_R0"), "b_patch_R1": med("b_patch_R1"),
           "c_sigma_R0": med("c_sigma_R0"), "c_sigma_R1": med("c_sigma_R1"),
           "abs_sigma_ratio_R0": med("abs_sigma_ratio_R0"),
           "rel_sigma_ratio_R0": med("rel_sigma_ratio_R0"),
           "abs_snr_dev_R0": med("abs_snr_dev_R0"), "rel_snr_dev_R0": med("rel_snr_dev_R0"),
           "abs_snr_dev_R1": med("abs_snr_dev_R1"), "rel_snr_dev_R1": med("rel_snr_dev_R1"),
           "abs_p95_R0": med("abs_p95_R0"), "rel_p95_R0": med("rel_p95_R0"),
           "abs_p95_R1": med("abs_p95_R1"), "rel_p95_R1": med("rel_p95_R1"),
           "sigma_true_frame_rms": med("sigma_true_frame_rms"),
           "sigma_true_patch_median": med("sigma_true_patch_median"),
           "mc_sd_c_sigma_R0": sd("c_sigma_R0"), "mc_sd_b_frame": sd("b_frame"),
           "rows": rows}
    # 非退化判定：两种表示在 sigma 空间的最大相对差
    out["abs_vs_rel_gap"] = abs(out["rel_sigma_ratio_R0"] / out["abs_sigma_ratio_R0"] - 1.0) \
        if out["abs_sigma_ratio_R0"] else float("nan")
    out["degenerate"] = bool(name == "flat_no_struct"
                             and abs(out["c_sigma_R0"] - 1.0)
                             <= max(3.0 * out["mc_sd_c_sigma_R0"], 1e-12))
    return out


def cross_frame_arm(seed=X.SEED):
    """跨帧臂：同一结构、**不同天光电平**（sigma_true 不同）的两帧。

    真值：同一位置跨帧的 sigma 比值；两种表示各给一个比值。共模因子 c_k 若相同则严格相消。
    负例：两帧 sigma_true 相同（同电平）⇒ c_k 相同 ⇒ 两种表示的跨帧比值都必须无偏。
    """
    ny, nx = SHAPE
    yy, xx = np.mgrid[0:ny, 0:nx]
    out: Dict[str, Any] = {"cases": []}
    for tag, levels in (("same_level", (20.0, 20.0)), ("diff_level", (20.0, 34.0))):
        recs = []
        for m in range(N_MC):
            rng = np.random.default_rng(seed + 7000 + 37 * m)
            g = smooth_field(SHAPE, 30.0, 128.0, rng)
            struct = g + 25.0 * (xx / float(nx - 1))
            sigs, imgs, sps, sfs, tps = [], [], [], [], []
            for lv in levels:
                sig = np.full(SHAPE, float(lv))
                img = struct + rng.normal(0.0, 1.0, size=SHAPE) * sig
                sigs.append(sig)
                imgs.append(img)
                sps.append(X.sigma_patch_raw(img, DELTA))
                sfs.append(X.sigma_frame(img))
                tps.append(X.patch_truth_from_var(sig * sig, DELTA))
            c = [X.frame_common_factor(sps[k], sfs[k]) for k in range(2)]
            est_abs = [sps[k] for k in range(2)]
            est_rel = [sps[k] * c[k] for k in range(2)]
            recs.append({"c": c,
                         "abs": X.cross_frame_ratio_dev(est_abs, tps, ref=0),
                         "rel": X.cross_frame_ratio_dev(est_rel, tps, ref=0),
                         "frame_snr_arm": X.cross_frame_ratio_dev(
                             [np.full_like(tps[0], sfs[k]) for k in range(2)], tps, ref=0)})
        key = "frame_1_over_0"
        out["cases"].append({
            "case": tag, "sigma_levels": list(levels), "n_mc": N_MC,
            "c_sigma_median": [float(np.median([r["c"][k] for r in recs])) for k in range(2)],
            "abs_median_ratio": float(np.median([r["abs"][key]["median_ratio"] for r in recs])),
            "rel_median_ratio": float(np.median([r["rel"][key]["median_ratio"] for r in recs])),
            "frame_scalar_median_ratio": float(np.median(
                [r["frame_snr_arm"][key]["median_ratio"] for r in recs])),
            "abs_snr_dev": float(np.median([r["abs"][key]["snr_rel_dev"] for r in recs])),
            "rel_snr_dev": float(np.median([r["rel"][key]["snr_rel_dev"] for r in recs])),
        })
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e1_analytic.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    n_mc = 3 if a.quick else N_MC
    out: Dict[str, Any] = {"meta": {
        "seed": X.SEED, "shape": list(SHAPE), "delta_px": DELTA, "n_mc": n_mc,
        "sigma0_adu": SIGMA0,
        "data_class": "纯解析代数合成（最高设计 12.2 第 2 类，真值完全已知）",
        "truth": "I = P + N(0, sigma_true(x,y))，sigma_true 由场景显式给定；patch 真值 = sqrt(mean(sigma_true^2))",
        "convention": "SNR = F_ref/(sigma*sqrt(A_NEA))，报告取 F_ref=1、A_NEA=1；比值判据与之无关",
        "estimators": {"frame": "整帧 2 轮裁剪 RMS（生产 star_detector recipe 镜像）",
                       "R0": "逐 64px patch 同一 recipe（朴素区域化）",
                       "R1": "mesh 局部背景 + 逐 patch 残差裁剪 RMS（结构感知）"},
    }, "scenarios": []}
    for name, desc, sf, st in build_scenarios():
        r = run_scenario(name, desc, sf, st, n_mc=n_mc)
        out["scenarios"].append(r)
        print("scenario %-18s c_agg=%.4f b_f=%.4f b_R0=%.4f c_sig=%.4f absSNR=%+.4f relSNR=%+.4f %s"
              % (name, r["c_agg"], r["b_frame"], r["b_patch_R0"], r["c_sigma_R0"],
                 r["abs_snr_dev_R0"], r["rel_snr_dev_R0"],
                 "DEGENERATE" if r["degenerate"] else ""), flush=True)
    out["cross_frame"] = cross_frame_arm()
    for c in out["cross_frame"]["cases"]:
        print("xframe %-12s c=(%.4f,%.4f) absSNRdev=%+.4f relSNRdev=%+.4f"
              % (c["case"], c["c_sigma_median"][0], c["c_sigma_median"][1],
                 c["abs_snr_dev"], c["rel_snr_dev"]), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
