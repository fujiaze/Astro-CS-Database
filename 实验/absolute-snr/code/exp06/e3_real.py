#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 臂 C：**testdata 真实数据**（最高设计 12.2 第 3 类数据，无真值）。

无真值 ⇒ 只能做**一致性检验**，三条互相独立的路径：
  C1 棋盘 hold-out：用**偶族像素**估计驱动量/拟合模型，用**奇族像素**估计 sigma 做验证
     （零像素重叠 ⇒ 模型预测能力的外部检验）；
  C2 二阶差分参考：Var(I[i-1]-2I[i]+I[i+1]) = 6*sigma^2，**线性梯度精确对消**，
     与 C1 的估计器在像素与算法上都独立；
  C3 帧级标量 vs 区域 sigma 的离散（EXP-03 的 +2303.7% 机理在真实帧上的复现）。

诚实边界：真实帧是**归一化产品**（任意 ADU 标度、增益未知）⇒ 斜率不可解释为 1/g，
本臂只检验「sigma^2 对平滑弥散分量线性」这一**形式**与**预测能力**，不反推仪器参数。

固定 seed：20260927。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402
import exp02_common as C2  # noqa: E402
import exp03_common as E3  # noqa: E402
import operators as OP     # noqa: E402

SEED = X.SEED
CROP = 1024
DELTA = 64
EDGE_MARGIN = 1
M42 = [str(X.TESTDATA / p) for p in [
    "M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M2/M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M4/M42_M4_T2_flying_dutchman-20251224@045919-300S-Red.fts",
]]


def load_crop(path: str, n: int = CROP) -> Tuple[np.ndarray, Dict[str, Any]]:
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    ny, nx = raw.shape
    y0, x0 = (ny - n) // 2, (nx - n) // 2
    img = raw[y0:y0 + n, x0:x0 + n]
    return img, {"file": Path(path).name, "shape": [ny, nx], "crop": [y0, x0, n, n],
                 "exptime": float(hdr.get("EXPTIME", 0.0)),
                 "filter": str(hdr.get("FILTER", "")).strip(),
                 "gain_header": hdr.get("GAIN"), "rdnoise_header": hdr.get("RDNOISE"),
                 "bunit": str(hdr.get("BUNIT", "")).strip()}


def parity(img: np.ndarray, fam: int) -> np.ndarray:
    yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
    return (yy + xx) % 2 == int(fam)


def family_background(img: np.ndarray, fam: int, delta: int = DELTA, n_iter: int = 3,
                      n_rounds: int = 2, k: float = 3.0, min_pix: int = 64
                      ) -> Tuple[np.ndarray, np.ndarray]:
    """只用**单一阵列族**像素估计的平滑弥散分量（保证 hold-out 零像素重叠）。"""
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    m = parity(a, fam) & np.isfinite(a)
    by, bx = ny // delta, nx // delta
    B = np.zeros((ny, nx), dtype=np.float64)
    for _ in range(max(int(n_iter), 1)):
        r = np.where(m, a - B, np.nan)
        sub = r[:by * delta, :bx * delta]
        mm = (sub.reshape(by, delta, bx, delta).transpose(0, 2, 1, 3)
              .reshape(by * bx, delta * delta))
        loc = np.full(by * bx, np.nan)
        for i in range(by * bx):
            v = mm[i][np.isfinite(mm[i])]
            if v.size >= min_pix:
                loc[i] = float(C2.production_clip_sigma(v, n_rounds=n_rounds, k=k)["background"])
        g = loc.reshape(by, bx)
        if not np.isfinite(g).any():
            break
        filled, _ = OP.prefill_nan(g, "nearest_valid")
        g = np.where(np.isfinite(filled), filled, float(np.nanmedian(g)))
        dB = OP.run_operator("spline_natural_clip", g, delta, (ny, nx))[0]
        B = B + np.where(np.isfinite(dB), dB, 0.0)
    return B, m


def second_difference_sigma(img: np.ndarray, delta: int = DELTA, n_iter: int = 2,
                            k: float = 3.0) -> np.ndarray:
    """二阶差分 sigma 估计（线性梯度精确对消）：Var(I[i-1]-2I[i]+I[i+1]) = 6*sigma^2。"""
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    by, bx = ny // delta, nx // delta
    sub = a[:by * delta, :bx * delta]
    ap = np.pad(sub, ((0, 0), (1, 1)), mode="reflect")   # 边界镜像填充，保持控制网格对齐
    d2 = ap[:, :-2] - 2.0 * ap[:, 1:-1] + ap[:, 2:]
    mm = (d2.reshape(by, delta, bx, delta)
          .transpose(0, 2, 1, 3).reshape(by * bx, delta * delta))
    out = np.full(by * bx, np.nan)
    for i in range(by * bx):
        v = mm[i][np.isfinite(mm[i])]
        if v.size >= 64:
            out[i] = float(C2.production_clip_sigma(v, n_rounds=n_iter, k=k)["sigma"]) / np.sqrt(6.0)
    return out.reshape(by, bx)


def run_frame(path: str, tag: str) -> Dict[str, Any]:
    t0 = time.time()
    img, meta = load_crop(path)
    B_full, dmeta = X.diffuse_component(img, box=DELTA, mode="spline")
    s_full = X.sigma_ctrl_resid(img, DELTA)
    s0 = X.sigma_ctrl_resid_fam(img, 0, DELTA)
    s1 = X.sigma_ctrl_resid_fam(img, 1, DELTA)
    s2nd = second_difference_sigma(img, DELTA)
    B0, _m0 = family_background(img, 0, DELTA)
    D0 = X.sample_at_ctrl(B0, DELTA)
    Df = X.sample_at_ctrl(B_full, DELTA)
    valid = np.ones(D0.shape, dtype=bool)
    if EDGE_MARGIN > 0:
        m = EDGE_MARGIN
        valid[:m, :] = False
        valid[-m:, :] = False
        valid[:, :m] = False
        valid[:, -m:] = False
    ok0 = np.isfinite(s0) & np.isfinite(D0)
    fit0 = X.fit_nlf(D0, s0, valid=valid)
    fitf = X.fit_nlf(Df, s_full, valid=valid)
    V_ref = fit0["a"] if fit0.get("ok") else float("nan")
    diag = X.lever_arm_diag(D0, V_ref, valid=valid, g_assumed=1.3)
    pw = X.fit_nlf_power(D0, s0, valid=valid)

    # C1：模型预测（由偶族拟合）vs 奇族 hold-out
    pred_ctrl = np.sqrt(np.maximum(
        fit0["a"] + fit0["c"] * (D0 - fit0["D_ref"]), X.VAR_FLOOR)) if fit0.get("ok") else None
    c1: Dict[str, Any] = {}
    if pred_ctrl is not None:
        for nm, ref in (("holdout_fam1", s1), ("second_diff", s2nd), ("R1_full", s_full)):
            mm = valid & np.isfinite(ref) & np.isfinite(pred_ctrl) & (ref > 0)
            if int(mm.sum()) < 8:
                c1[nm] = {"n": int(mm.sum())}
                continue
            r = pred_ctrl[mm] / ref[mm]
            lr = np.log10(pred_ctrl[mm])
            lt = np.log10(ref[mm])
            c1[nm] = {"n": int(mm.sum()), "median_ratio": float(np.median(r)),
                      "p95_abs_dev": float(np.percentile(np.abs(r - 1.0), 95)),
                      "pearson_log10": float(np.corrcoef(lr, lt)[0, 1]),
                      "ref_dispersion_p95_over_p05": float(
                          np.percentile(ref[mm], 95) / np.percentile(ref[mm], 5))}
    # 只用观测的 sigma 场（不经过物理模型）作对照：R1 自身 vs hold-out
    c1_naive: Dict[str, Any] = {}
    for nm, ref in (("holdout_fam1", s1), ("second_diff", s2nd)):
        mm = valid & np.isfinite(ref) & np.isfinite(s_full) & (ref > 0)
        if int(mm.sum()) >= 8:
            r = s_full[mm] / ref[mm]
            c1_naive[nm] = {"n": int(mm.sum()), "median_ratio": float(np.median(r)),
                            "p95_abs_dev": float(np.percentile(np.abs(r - 1.0), 95)),
                            "pearson_log10": float(np.corrcoef(
                                np.log10(s_full[mm]), np.log10(ref[mm]))[0, 1])}
    # C3：帧级标量与区域 sigma 的离散
    sv = s_full[np.isfinite(s_full)]
    frame_scalar = X.recon_frame_scalar(s_full, img.shape)
    c3 = {"sigma_frame_scalar": float(np.median(sv)) if sv.size else float("nan"),
          "sigma_region_p05": float(np.percentile(sv, 5)) if sv.size else float("nan"),
          "sigma_region_p95": float(np.percentile(sv, 95)) if sv.size else float("nan"),
          "region_dispersion": float(np.percentile(sv, 95) / np.percentile(sv, 5))
          if sv.size else float("nan"),
          "model_dispersion": float(np.nanpercentile(pred_ctrl, 95) / np.nanpercentile(pred_ctrl, 5))
          if pred_ctrl is not None else float("nan"),
          "frame_vs_region_max_dev": float(np.max(np.abs(sv / np.median(sv) - 1.0)))
          if sv.size else float("nan")}
    return {"tag": tag, "meta": meta, "fit_family0": fit0, "fit_full": fitf,
            "diag": diag, "power": pw, "diffuse_meta": dmeta,
            "C1_model_vs_holdout": c1, "C1b_R1_vs_holdout": c1_naive, "C3": c3,
            "sigma_stats": {
                "s_full_median": float(np.median(sv)) if sv.size else float("nan"),
                "s0_median": float(np.nanmedian(s0)), "s1_median": float(np.nanmedian(s1)),
                "s2nd_median": float(np.nanmedian(s2nd))},
            "elapsed_s": time.time() - t0}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e3_real.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    frames = [(M42[0], "M42_M1"), (M42[1], "M42_M2"), (M42[2], "M42_M4")]
    if a.quick:
        frames = frames[:1]
    out: Dict[str, Any] = {"meta": {
        "unit": "EXP-06-SNR-PHYS", "arm": "C testdata 真实数据（无真值，一致性检验）",
        "seed_base": SEED, "crop": CROP, "delta_px": DELTA, "edge_margin": EDGE_MARGIN,
        "paths": [p for p, _ in frames],
        "note": "真实帧为归一化产品（任意 ADU 标度、增益未知）⇒ 不反推仪器参数",
    }, "frames": []}
    for p, tag in frames:
        if not Path(p).exists():
            print("MISSING", p, flush=True)
            continue
        r = run_frame(p, tag)
        out["frames"].append(r)
        print("%-8s fit0 c=%.4f a=%.2f lever=%.3f | model/holdout=%.4f r=%.3f | R1/holdout=%.4f | %.0fs"
              % (tag, r["fit_family0"].get("c", float("nan")), r["fit_family0"].get("a", float("nan")),
                 r["diag"]["lever_var"],
                 r["C1_model_vs_holdout"].get("holdout_fam1", {}).get("median_ratio", float("nan")),
                 r["C1_model_vs_holdout"].get("holdout_fam1", {}).get("pearson_log10", float("nan")),
                 r["C1b_R1_vs_holdout"].get("holdout_fam1", {}).get("median_ratio", float("nan")),
                 r["elapsed_s"]), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
