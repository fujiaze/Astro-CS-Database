#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 / 实验臂 A：**纯解析代数合成**（最高设计 §12.2 第 2 类数据，真值完全已知）。

回答三件事：
  A1 非退化负例：真值**无结构**时，区域化估计与帧级标量必须给出同一个数（效应归零）；
  A2 **误差-尺度关系曲线**：逐区域偏差 / 逐区域统计误差 / 总 RMSE 随区域尺寸 B 的曲线，
     覆盖 无结构 / 斜坡 / 高斯相关场(ell=8,32,128) / 高斯团块(sigma=8,32) / 真实形态模板(M42,M16)；
  A3 **空间变化的 sigma_n(x,y)**：证明"帧级标量"在噪声本身就是位置函数的帧上
     **结构上不可表示**（不是精度问题，是定义域问题）；
  A4 解析偏差式的对拍（R0 的闭式预测）。

估计器族（详见 exp03_common）：
  R0(B)  区域化、背景=区域自身裁剪中位数（"只把作用域从整帧换成区域"的对照臂）
  R1(B)  区域化、背景=box=B 的 mesh 背景图（SExtractor backsig / photutils background_rms 同构物）
  R2(B)  跨帧差分参考（位置稳定分量自动对消）—— 需要 >=2 帧
  S      现行帧级标量（整帧生产 recipe）

固定 seed = 20260925。只读 testdata；不运行任何 AstroCS 可执行文件。
"""

from __future__ import annotations

import argparse
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
SEED = E.SEED
SHAPE = (512, 512)
SIG_N = 20.0                    # 解析真值：逐像素噪声 sigma [ADU]
BOXES = (16, 32, 64, 128, 256)
N_REAL = 8                      # 每个配置的独立实现数


# ---------------------------------------------------------------------------
def _per_region_stats(sig_reg: np.ndarray, true_map: np.ndarray, tag: str) -> Dict[str, float]:
    """逐区域统计：偏差（相对真值图的中位）、空间离散（统计误差代理）、RMSE。"""
    r = np.asarray(sig_reg, dtype=np.float64) / np.asarray(true_map, dtype=np.float64) - 1.0
    return {
        tag + "_bias_median": float(np.median(r)),
        tag + "_bias_p95": float(np.percentile(r, 95)),
        tag + "_scatter": float(np.std(r, ddof=1)),
        tag + "_rmse": float(math.sqrt(float(np.mean(r ** 2)))),
    }


def _templates() -> Dict[str, np.ndarray]:
    """真实形态模板（确定性、无噪声）：M42 T2 星云核心 + HST M16 F657N 核心。"""
    from astropy.io import fits
    out: Dict[str, np.ndarray] = {}
    m42 = sorted((ROOT / "testdata" / "M42_T2T3_mosaic_Flying_dutchman" / "T2" / "M2")
                 .glob("*-300S-Red.fts"))
    if m42:
        with fits.open(m42[0], memmap=False) as h:
            d = np.asarray(h[0].data, dtype=np.float64)
        ny, nx = d.shape
        y0, x0 = ny // 2 - 256, nx // 2 - 256
        sub = d[y0:y0 + 512, x0:x0 + 512].copy()
        sub[sub >= 65000] = np.nan
        sub = np.where(np.isfinite(sub), sub, np.nanmedian(sub))
        from scipy.ndimage import gaussian_filter
        out["m42_template"] = gaussian_filter(sub, 1.5, mode="nearest")
    hst = sorted((ROOT / "testdata" / "HST_M16").glob("*F657N*.fits"))
    if hst:
        with fits.open(hst[0], memmap=False) as h:
            d = np.asarray(h[0].data, dtype=np.float64)
        ny, nx = d.shape
        y0, x0 = ny // 2 - 256, nx // 2 - 256
        sub = d[y0:y0 + 512, x0:x0 + 512].copy()
        sub[~np.isfinite(sub)] = 0.0
        from scipy.ndimage import gaussian_filter
        out["m16_template"] = gaussian_filter(sub, 2.0, mode="nearest")
    return out


def _structure_bank(rng: np.random.Generator) -> List[Tuple[str, np.ndarray]]:
    bank: List[Tuple[str, np.ndarray]] = [("null", np.zeros(SHAPE))]
    bank.append(("ramp_lo", E.struct_ramp(SHAPE, 0.325)))
    bank.append(("ramp_hi", E.struct_ramp(SHAPE, 3.250)))
    for ell in (8.0, 32.0, 128.0):
        bank.append(("corr%02d" % int(ell),
                     E.struct_smooth_field(SHAPE, 3.0 * SIG_N, ell,
                                           np.random.default_rng(7000 + int(ell)))))
    for sb in (8.0, 32.0):
        bank.append(("blob%02d" % int(sb), E.struct_blob(SHAPE, 60.0 * SIG_N, sb)))
    for k, v in _templates().items():
        f = v - np.median(v)
        bank.append((k, E.struct_from_template(f, 3.0 * SIG_N)))
    return bank


# ---------------------------------------------------------------------------
def arm_a1_null(rng: np.random.Generator) -> Dict[str, Any]:
    """A1 非退化负例：真值无结构 ⇒ 区域化 vs 帧级标量的效应必须归零。"""
    rows: List[Dict[str, Any]] = []
    for B in BOXES:
        d0, d1, s_scalar, s0, s1, s2 = [], [], [], [], [], []
        for _ in range(N_REAL):
            n1 = rng.normal(0.0, SIG_N, size=SHAPE)
            n2 = rng.normal(0.0, SIG_N, size=SHAPE)
            sc = E.frame_scalar_sigma(n1)
            m0 = float(np.median(E.region_sigma_raw(n1, B)["sigma"]))
            m1 = float(np.median(E.region_sigma_resid(n1, B, n_iter=3)["sigma"]))
            m2 = float(np.median(E.region_sigma_diff(n1, n2, B)["sigma"]))
            d0.append(E.effect_delta(m0, sc))
            d1.append(E.effect_delta(m1, sc))
            s_scalar.append(sc); s0.append(m0); s1.append(m1); s2.append(m2)
        rows.append({
            "box": int(B),
            "delta_R0_vs_scalar_mean": float(np.mean(d0)),
            "delta_R0_vs_scalar_sem": float(np.std(d0, ddof=1) / math.sqrt(N_REAL)),
            "delta_R1_vs_scalar_mean": float(np.mean(d1)),
            "delta_R1_vs_scalar_sem": float(np.std(d1, ddof=1) / math.sqrt(N_REAL)),
            "scalar_rel_err_mean": float(np.mean([x / SIG_N - 1 for x in s_scalar])),
            "R0_rel_err_mean": float(np.mean([x / SIG_N - 1 for x in s0])),
            "R1_rel_err_mean": float(np.mean([x / SIG_N - 1 for x in s1])),
            "R2_rel_err_mean": float(np.mean([x / SIG_N - 1 for x in s2])),
            "R0_scatter_over_regions": float(np.std(
                E.region_sigma_raw(rng.normal(0.0, SIG_N, size=SHAPE), B)["sigma"]
                / SIG_N - 1.0, ddof=1)),
            "pred_stat_error_1_over_sqrt2N": E.predicted_region_stat_error(B),
        })
    return {"arm": "A1_null", "rows": rows}


def arm_a2_structure(rng: np.random.Generator) -> Dict[str, Any]:
    """A2 误差-尺度关系曲线：逐区域偏差 / 统计离散 / RMSE。"""
    bank = _structure_bank(rng)
    rows: List[Dict[str, Any]] = []
    for name, struct in bank:
        s_rms = float(np.std(struct))
        for B in BOXES:
            acc0, acc1, acc2 = [], [], []
            for _ in range(N_REAL):
                n1 = rng.normal(0.0, SIG_N, size=SHAPE)
                n2 = rng.normal(0.0, SIG_N, size=SHAPE)
                f1 = struct + n1
                f2 = struct + n2
                acc0.append(E.region_sigma_raw(f1, B)["sigma"])
                acc1.append(E.region_sigma_resid(f1, B, n_iter=3)["sigma"])
                acc2.append(E.region_sigma_diff(f1, f2, B)["sigma"])
            m0 = np.mean(acc0, axis=0); m1 = np.mean(acc1, axis=0); m2 = np.mean(acc2, axis=0)
            tru = np.full(m0.shape, SIG_N)
            row: Dict[str, Any] = {"structure": name, "box": int(B),
                                   "struct_rms": s_rms,
                                   "struct_over_noise": s_rms / SIG_N,
                                   "scalar_rel_err": E.rel_err(E.frame_scalar_sigma(struct + rng.normal(0.0, SIG_N, size=SHAPE)), SIG_N)}
            row.update(_per_region_stats(m0, tru, "R0"))
            row.update(_per_region_stats(m1, tru, "R1"))
            row.update(_per_region_stats(m2, tru, "R2"))
            row["pred_R0_bias_ramp_formula"] = (
                E.predicted_region_bias_ramp(B, 3.250, SIG_N) if name == "ramp_hi"
                else (E.predicted_region_bias_ramp(B, 0.325, SIG_N) if name == "ramp_lo" else None))
            rows.append(row)
    return {"arm": "A2_structure", "rows": rows}


def arm_a3_varying_sigma(rng: np.random.Generator) -> Dict[str, Any]:
    """A3 噪声本身是位置函数：帧级标量**结构上不可表示**。

    构造：sigma_n(x,y) = sqrt(sigma0^2 + level(x,y)/g)，level 用真实 M42 形态模板。
    ⇒ 真值 sigma 在帧内跨 1 倍以上；帧级标量只能给一个数。
    判据（非退化）：区域估计的**逐区域**相对误差必须随区域变小而下降；
    帧级标量的逐区域相对误差与区域尺寸**无关**（它根本不看位置）。
    """
    tpl = _templates().get("m42_template")
    if tpl is None:
        return {"arm": "A3_varying_sigma", "available": False}
    # 噪声尺度场必须是**平滑**的位置函数（星点不属于"噪声尺度场"）；
    # 未平滑的真实模板会把星点当成 sigma 尖峰，与"估计器应剔除源"的口径不一致。
    from scipy.ndimage import gaussian_filter
    lvl = gaussian_filter(np.clip(tpl - np.percentile(tpl, 5), 0.0, None), 16.0)
    lo, hi = np.percentile(lvl, 5), np.percentile(lvl, 95)
    u = np.clip((lvl - lo) / max(float(hi - lo), 1e-9), 0.0, 1.0)
    sig_map = 6.0 * (0.75 + 0.5 * u)          # 帧内 sigma 跨 1.5 倍
    rows: List[Dict[str, Any]] = []
    for B in BOXES:
        # 逐区域真值 = 区域内 sigma_map^2 的均值开方
        ny, nx = SHAPE
        by, bx = ny // B, nx // B
        tv = sig_map[:by * B, :bx * B].reshape(by, B, bx, B).transpose(0, 2, 1, 3)
        true_reg = np.sqrt(np.mean(tv.reshape(by * bx, B * B) ** 2, axis=1)).reshape(by, bx)
        # 逐像素高斯噪声，标准差 = sig_map（真值是位置函数）
        n1 = rng.normal(0.0, 1.0, size=SHAPE) * sig_map
        n2 = rng.normal(0.0, 1.0, size=SHAPE) * sig_map
        m1 = E.region_sigma_resid(n1, B, n_iter=3)["sigma"]
        m2 = E.region_sigma_diff(n1, n2, B)["sigma"]
        sc = E.frame_scalar_sigma(n1)
        rel_scalar = sc / true_reg - 1.0
        rows.append({
            "box": int(B),
            "sigma_true_min": float(sig_map.min()), "sigma_true_max": float(sig_map.max()),
            "sigma_true_p95_over_p05_frame": float(np.percentile(sig_map, 95)
                                                   / np.percentile(sig_map, 5)),
            "sigma_true_regional_dispersion": float(
                np.percentile(true_reg, 95) / np.percentile(true_reg, 5)),
            "scalar_value": float(sc),
            "scalar_abs_rel_err_median": float(np.median(np.abs(rel_scalar))),
            "scalar_abs_rel_err_p95": float(np.percentile(np.abs(rel_scalar), 95)),
            "R1_abs_rel_err_median": float(np.median(np.abs(m1 / true_reg - 1.0))),
            "R1_abs_rel_err_p95": float(np.percentile(np.abs(m1 / true_reg - 1.0), 95)),
            "R2_abs_rel_err_median": float(np.median(np.abs(m2 / true_reg - 1.0))),
            "R2_abs_rel_err_p95": float(np.percentile(np.abs(m2 / true_reg - 1.0), 95)),
        })
    return {"arm": "A3_varying_sigma", "available": True,
            "sigma_true_frame_p95_over_p05": float(np.percentile(sig_map, 95)
                                                   / np.percentile(sig_map, 5)),
            "rows": rows}


def arm_a4_formula(rng: np.random.Generator) -> Dict[str, Any]:
    """A4 R0 的解析偏差式对拍：sqrt(1+(s*B)^2/(12 sigma^2)) - 1。"""
    rows: List[Dict[str, Any]] = []
    for slope in (0.1, 0.325, 1.0, 3.25, 10.0):
        struct = E.struct_ramp(SHAPE, slope)
        for B in BOXES:
            meas = []
            for _ in range(N_REAL):
                f = struct + rng.normal(0.0, SIG_N, size=SHAPE)
                meas.append(float(np.median(E.region_sigma_raw(f, B)["sigma"])))
            pred = E.predicted_region_bias_ramp(B, slope, SIG_N)
            rows.append({"slope": slope, "box": int(B),
                         "pred_bias": pred,
                         "meas_bias": float(np.mean(meas) / SIG_N - 1.0),
                         "abs_diff": float(abs(np.mean(meas) / SIG_N - 1.0 - pred))})
    return {"arm": "A4_formula", "rows": rows}


def arm_a5_optimal_box(rng: np.random.Generator) -> Dict[str, Any]:
    """A5 误差-尺度关系的最优区域尺度（由 A2 的实测 RMSE 曲线取最小，并与解析式对拍）。"""
    a2 = arm_a2_structure(np.random.default_rng(SEED + 11))
    best: Dict[str, Any] = {}
    for name in sorted({r["structure"] for r in a2["rows"]}):
        rs = [r for r in a2["rows"] if r["structure"] == name]
        for est in ("R0", "R1", "R2"):
            k = est + "_rmse"
            bb = min(rs, key=lambda r: r[k])
            best.setdefault(est, {})[name] = {"box": bb["box"], "rmse": bb[k],
                                              "bias_median": bb[est + "_bias_median"],
                                              "scatter": bb[est + "_scatter"]}
    return {"arm": "A5_optimal_box", "best": best,
            "analytic_ramp_hi": E.optimal_box_ramp(3.250, SIG_N),
            "analytic_ramp_lo": E.optimal_box_ramp(0.325, SIG_N),
            "analytic_grid_ramp_hi": E.optimal_box_numeric(3.250, SIG_N),
            "analytic_grid_ramp_lo": E.optimal_box_numeric(0.325, SIG_N)}


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp03_e1_analytic.json")
    ap.add_argument("--arms", default="a1,a2,a3,a4,a5")
    args = ap.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    want = set(args.arms.split(","))
    out: Dict[str, Any] = {"meta": {
        "seed": SEED, "shape": list(SHAPE), "sigma_n_adu": SIG_N,
        "boxes": list(BOXES), "n_realizations": N_REAL,
        "data_class": "纯解析代数合成（最高设计 §12.2 第 2 类）",
        "delta_budget": E.DELTA_BUDGET,
        "estimators": {"R0": "区域化 + 区域自身裁剪中位数背景",
                       "R1": "区域化 + box=B mesh 背景图（迭代 3）",
                       "R2": "跨帧差分 clipped RMS(D)/sqrt2",
                       "S": "现行帧级标量（整帧生产 recipe）"}}}
    if "a1" in want:
        out["a1_null"] = arm_a1_null(np.random.default_rng(SEED + 1)); print("A1 done", flush=True)
    if "a2" in want:
        out["a2_structure"] = arm_a2_structure(np.random.default_rng(SEED + 2)); print("A2 done", flush=True)
    if "a3" in want:
        out["a3_varying_sigma"] = arm_a3_varying_sigma(np.random.default_rng(SEED + 3)); print("A3 done", flush=True)
    if "a4" in want:
        out["a4_formula"] = arm_a4_formula(np.random.default_rng(SEED + 4)); print("A4 done", flush=True)
    if "a5" in want:
        out["a5_optimal_box"] = arm_a5_optimal_box(np.random.default_rng(SEED + 5)); print("A5 done", flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float), encoding="utf-8")
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
