#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 / 实验臂 P（= 交付项一「裁决前提的实证」+ 跨帧一致性）。

对象：testdata/M42_T2T3_mosaic_Flying_dutchman/T2/<M1..M6>/*-300S-Red.fts
（16 帧，4096^2，**只读**）。同一 panel 的多帧是**同一指向**（含几像素~几十像素抖动），
因此它们是"同一位置、不同帧"的直接样本 —— 这正是裁决前提
「同一位置的天光背景跨帧相同」可被证伪的地方。

本臂做四件事（真实数据**没有真值**，故只报可证项）：
  P-A 逐对差分场的**空间结构检验**：D = I_k - I_j 是否为「常数 + 白噪声」；
  P-B 多帧**方差分解**：m[r,k] = mu + a_r + b_k + e[r,k]，检验 e 是否只是噪声；
  P-C 区域化 sigma 的**跨帧一致性** vs **帧级标量口径**的对照；
  P-D 多帧最小二乘解出**逐帧**区域 sigma^2（>=3 帧的 panel）。

只读 testdata；不修改任何文件；不运行任何 AstroCS 可执行文件。固定 seed = 20260925。
"""

from __future__ import annotations

import argparse
import glob
import json
import math
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp03_common as E  # noqa: E402

ROOT = E.ROOT
T2 = ROOT / "testdata" / "M42_T2T3_mosaic_Flying_dutchman" / "T2"
PAT = "*-300S-Red.fts"
SEED = E.SEED
BOX_MAIN = 64
BOX_AUX = (64, 256)
LAGS = (1, 2, 4, 8, 16, 32, 64, 128, 256)
SAT_ADU = 65000.0


# ---------------------------------------------------------------------------
def load_frame(path: Path) -> Tuple[np.ndarray, Dict[str, Any]]:
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        d = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    return d, {"file": path.name, "panel": path.parent.name,
               "date_obs": hdr.get("DATE-OBS"), "exptime": hdr.get("EXPTIME"),
               "filter": hdr.get("FILTER"), "airmass": hdr.get("AIRMASS"),
               "pa": hdr.get("PA"), "fwhm": hdr.get("FWHM"),
               "shape": [int(d.shape[0]), int(d.shape[1])],
               "median": float(np.median(d)),
               "sat_frac": float(np.mean(d >= SAT_ADU))}


def align_pair(a: np.ndarray, b: np.ndarray, max_shift: int = 128,
               step: int = 8) -> Dict[str, Any]:
    """求 b -> a 的整数平移；同时尝试 180 度旋转（子午翻转帧）。

    返回 {dy, dx, rot180, corr}。判据 = 平移后的归一化互相关最大。
    """
    best = None
    for rot in (False, True):
        bb = np.rot90(b, 2) if rot else b
        dy, dx = E.xcorr_offset_px(a, bb, max_shift=max_shift, step=step)
        cc = np.roll(np.roll(bb, dy, axis=0), dx, axis=1)
        aa = a - a.mean()
        cc = cc - cc.mean()
        # 只比较未卷绕区
        m = ~E.common_valid_mask(a.shape, dy, dx, margin=4)
        num = float(np.mean(aa[m] * cc[m]))
        den = float(np.std(aa[m]) * np.std(cc[m])) + 1e-12
        corr = num / den
        if best is None or corr > best["corr"]:
            best = {"dy": int(dy), "dx": int(dx), "rot180": bool(rot), "corr": float(corr)}
    return best


def prepare(a: np.ndarray, dy: int, dx: int, rot180: bool) -> np.ndarray:
    b = np.rot90(a, 2) if rot180 else a
    return E.shift_crop(b, dy, dx, a.shape)


def valid_mask(shape: Tuple[int, int], dy: int, dx: int, rot180: bool,
               *frames: np.ndarray) -> np.ndarray:
    m = E.common_valid_mask(shape, dy, dx, margin=2)
    for f in frames:
        m = m | E.saturation_mask(f, SAT_ADU)
    return m


def _obs_row(obs: np.ndarray, pid: int, dfield: np.ndarray, box: int) -> None:
    """就地算出该帧对在**每个区域**的差分方差观测（供逐帧 sigma^2 最小二乘解）。

    内存瘦身：不保存差分图，只保存 (n_pair, n_region) 的小表。
    """
    ny, nx = dfield.shape
    by, bx = ny // box, nx // box
    vv2 = (dfield[:by * box, :bx * box]
           .reshape(by, box, bx, box).transpose(0, 2, 1, 3)
           .reshape(by * bx, box * box))
    for r in range(by * bx):
        x = vv2[r]
        x = x[np.isfinite(x)]
        if x.size > 0.5 * box * box:
            obs[pid, r] = E.C.production_clip_sigma(x.astype(np.float64))["sigma"] ** 2


# ---------------------------------------------------------------------------
def run_panel(paths: List[Path], args: argparse.Namespace) -> Dict[str, Any]:
    t0 = time.time()
    frames: List[np.ndarray] = []
    metas: List[Dict[str, Any]] = []
    for p in paths:
        d, m = load_frame(p)
        frames.append(d)
        metas.append(m)
    ref = frames[0]
    shape = ref.shape
    aligns: List[Dict[str, Any]] = [{"dy": 0, "dx": 0, "rot180": False, "corr": 1.0}]
    aligned = [ref]
    for k in range(1, len(frames)):
        al = align_pair(ref, frames[k])
        aligns.append(al)
        aligned.append(prepare(frames[k], al["dy"], al["dx"], al["rot180"]))
        frames[k] = None                      # 释放未对齐副本（内存瘦身）
    del frames
    # 逐帧：帧级标量 + 区域 sigma 图（R1）
    per_frame: List[Dict[str, Any]] = []
    for k, img in enumerate(aligned):
        vm = valid_mask(shape, aligns[k]["dy"], aligns[k]["dx"], aligns[k]["rot180"], img)
        if k > 0:
            vm = vm | valid_mask(shape, 0, 0, False, ref)
        sub = np.where(vm, np.nan, img)
        flat = sub[np.isfinite(sub)]
        sc = E.frame_scalar_sigma(flat)
        r1 = E.region_sigma_resid(np.where(vm, np.nanmedian(flat), img), BOX_MAIN,
                                   n_iter=3, sat_adu=SAT_ADU)
        per_frame.append({
            "index": k, "file": metas[k]["file"], "date_obs": metas[k]["date_obs"],
            "airmass": metas[k]["airmass"], "pa": metas[k]["pa"], "fwhm": metas[k]["fwhm"],
            "median_adu": metas[k]["median"], "sat_frac": metas[k]["sat_frac"],
            "align": aligns[k], "valid_frac": float(np.mean(~vm)),
            "frame_scalar_sigma": float(sc),
            "region_sigma_median": r1["sigma_median"],
            "region_sigma_p05": r1["sigma_p05"],
            "region_sigma_p95": r1["sigma_p95"],
            "region_sigma_dispersion": r1["dispersion"],
            "keep_frac_mesh_p50": r1["mesh_meta"]["keep_frac_mesh_p50"],
            "valid_region_frac": r1["valid_region_frac"],
            "_sigma_map": r1["sigma"],
            "_valid": ~vm,
        })
    # P-A 逐对差分结构检验（内存瘦身：逐对即时计算、差分图存 float32）
    pair_rows: List[Dict[str, Any]] = []
    pair_keys: List[Tuple[int, int]] = []
    K_ALL = len(aligned)
    nreg = (shape[0] // BOX_MAIN) * (shape[1] // BOX_MAIN)
    obs = np.full((K_ALL * (K_ALL - 1) // 2, nreg), np.nan) if K_ALL >= 3 else None
    pid = -1
    vmasks = [valid_mask(shape, aligns[k]["dy"], aligns[k]["dx"], aligns[k]["rot180"], aligned[k])
              for k in range(len(aligned))]
    sf_frames = [None] * len(aligned)
    for i in range(len(aligned)):
        for j in range(i + 1, len(aligned)):
            pid += 1
            pair_keys.append((i, j))
            vm = vmasks[i] | vmasks[j]
            dfield = np.where(vm, np.nan, aligned[i] - aligned[j])
            sf_d = E.structure_function(dfield, LAGS, mask=None)
            if sf_frames[i] is None:
                sf_frames[i] = E.structure_function(aligned[i], LAGS, mask=vm)
            sf_a = sf_frames[i]
            # **亚像素错位分离**：把 D 对参考帧梯度回归掉（系数 = 残余亚像素平移）
            gr = E.gradient_regression(dfield, aligned[i], mask=vm)
            sf_gr = E.structure_function(gr["_resid"], LAGS, mask=None)
            del gr["_resid"]
            for B in BOX_AUX:
                t = E.difference_structure_test(aligned[i], aligned[j], B, mask=vm,
                                                sf_diff=sf_d, sf_frame=sf_a)
                t.update({"subpix_offset_x_px": gr["offset_x_px"],
                          "subpix_offset_y_px": gr["offset_y_px"],
                          "subpix_offset_px": gr["offset_px"],
                          "resid_after_grad_sigma": gr["resid_sigma"],
                          "interaction_power_after_grad@64":
                              sf_gr.get("S_over_S1@64", float("nan")) - 1.0,
                          "interaction_power_after_grad@256":
                              sf_gr.get("S_over_S1@256", float("nan")) - 1.0})
                t.update({"i": i, "j": j, "file_i": metas[i]["file"], "file_j": metas[j]["file"],
                          "n_valid_frac": float(np.mean(~vm)),
                          "mean_diff_adu": float(np.mean((aligned[i] - aligned[j])[~vm]))})
                pair_rows.append(t)
                if B == BOX_MAIN:
                    # **逐区域前提检验**（主判据）：同一区域上
                    #     sigma_D,r^2 / (sigma_a,r^2 + sigma_b,r^2) - 1
                    # 应为 0（位置稳定分量对消、两帧噪声相加）。全局稳健口径会混淆
                    # sigma 场的异质性（M2/M5 帧内 sigma 跨 ~10 倍），故必须逐区域比。
                    r2m = E.region_sigma_diff(aligned[i], aligned[j], BOX_MAIN)["sigma"]
                    sa = per_frame[i]["_sigma_map"]
                    sb = per_frame[j]["_sigma_map"]
                    okm = (np.isfinite(r2m) & np.isfinite(sa) & np.isfinite(sb)
                           & (sa > 0) & (sb > 0))
                    # r2m = RMS(D)/sqrt2（逐帧口径）⇒ 2*r2m^2 = Var(D) = sigma_a^2 + sigma_b^2
                    ex = 2.0 * r2m[okm] ** 2 / (sa[okm] ** 2 + sb[okm] ** 2) - 1.0
                    t.update({
                        "region_excess_median": float(np.median(ex)),
                        "region_excess_p05": float(np.percentile(ex, 5)),
                        "region_excess_p95": float(np.percentile(ex, 95)),
                        "region_excess_abs_median": float(np.median(np.abs(ex))),
                        "region_excess_frac_beyond_5pct": float(np.mean(np.abs(ex) > 0.05)),
                        "n_region_excess": int(okm.sum()),
                    })
                    if K_ALL >= 3:
                        _obs_row(obs, pid, dfield, BOX_MAIN)
            # **总功率闭合**：Var(D) 是否等于两帧噪声方差之和（差额 = 帧相关空间形态的功率上界）
            sd = float(E.C.production_clip_sigma(
                (aligned[i] - aligned[j])[~vm], n_rounds=2, k=E.C.CLIP_K)["sigma"])
            s2a = per_frame[i]["region_sigma_median"] ** 2
            s2b = per_frame[j]["region_sigma_median"] ** 2
            # 异质 sigma 场下「中位数平方」会低估应得方差；同时给出逐区域方差均值版本
            mai, maj = per_frame[i]["_sigma_map"], per_frame[j]["_sigma_map"]
            mm2 = np.isfinite(mai) & np.isfinite(maj) & (mai > 0) & (maj > 0)
            s2a_m = float(np.mean(mai[mm2] ** 2)) if mm2.sum() else float("nan")
            s2b_m = float(np.mean(maj[mm2] ** 2)) if mm2.sum() else float("nan")
            pair_rows.append({
                "kind": "power_closure", "i": i, "j": j, "box": 0,
                "sigma_diff": sd, "sigma_a_reg": per_frame[i]["region_sigma_median"],
                "sigma_b_reg": per_frame[j]["region_sigma_median"],
                "var_sum_expected_regionmean": s2a_m + s2b_m,
                "var_excess_frac_regionmean": ((sd * sd - (s2a_m + s2b_m)) / (s2a_m + s2b_m)
                                               if mm2.sum() else float("nan")),
                "var_sum_expected": s2a + s2b, "var_measured": sd * sd,
                "var_excess": sd * sd - (s2a + s2b),
                "var_excess_frac": (sd * sd - (s2a + s2b)) / (s2a + s2b),
                "interaction_sigma_upper_adu": float(math.sqrt(max(sd * sd - (s2a + s2b), 0.0))),
                "mean_diff_adu": float(np.mean((aligned[i] - aligned[j])[~vm])),
                "sky_level_diff_adu": float(metas[i]["median"] - metas[j]["median"]),
                "mean_diff_over_sky_level_diff": (
                    float(np.mean((aligned[i] - aligned[j])[~vm]))
                    / (metas[i]["median"] - metas[j]["median"])
                    if abs(metas[i]["median"] - metas[j]["median"]) > 1e-9 else float("nan")),
            })
    # P-B 多帧方差分解（区域中位数）
    decomp: Dict[str, Any] = {"available": False}
    K = len(aligned)
    if K >= 3:
        box = BOX_MAIN
        by, bx = shape[0] // box, shape[1] // box
        M = np.full((by * bx, K), np.nan)
        for k in range(K):
            vi = valid_mask(shape, aligns[k]["dy"], aligns[k]["dx"], aligns[k]["rot180"], aligned[k])
            for i in range(K):
                if i == k:
                    continue
                vi = vi | valid_mask(shape, aligns[i]["dy"], aligns[i]["dx"],
                                     aligns[i]["rot180"], aligned[i])
            a = np.where(vi, np.nan, aligned[k])
            v = a[:by * box, :bx * box].reshape(by, box, bx, box).transpose(0, 2, 1, 3)
            M[:, k] = np.nanmean(v.reshape(by * bx, box * box), axis=1)
        ok = np.all(np.isfinite(M), axis=1)
        dec = E.two_way_decomposition(M[ok])
        sig_d = float(np.median([r["sigma_diff"] for r in pair_rows
                                    if r.get("kind") != "power_closure" and r["box"] == box]))
        exp_var = (sig_d ** 2 / 2.0) / float(box * box)
        decomp = {"available": True, "n_region_used": int(ok.sum()), "box": int(box),
                  **dec,
                  "sigma_diff_median": sig_d,
                  "var_expected_from_noise": exp_var,
                  "interaction_over_noise": dec["var_interaction"] / exp_var if exp_var else float("nan")}
    # P-C 区域 sigma 的跨帧一致性（同一区域、不同帧）
    smaps = np.stack([f["_sigma_map"] for f in per_frame], axis=0)     # (K, by, bx)
    scalars = np.array([f["frame_scalar_sigma"] for f in per_frame])
    reg_med = np.median(smaps, axis=0)
    reg_spread = np.percentile(smaps, 95, axis=0) / np.maximum(np.percentile(smaps, 5, axis=0), 1e-12)
    # P-D 逐帧区域 sigma^2 的最小二乘解（>=3 帧）
    solve: Dict[str, Any] = {"available": False}
    if K >= 3:
        box = BOX_MAIN
        by, bx = shape[0] // box, shape[1] // box
        pairs = pair_keys
        A = np.zeros((len(pairs), K))
        for p, (i, j) in enumerate(pairs):
            A[p, i] = 1.0; A[p, j] = 1.0
        good = np.all(np.isfinite(obs), axis=0)
        if good.sum() > 0:
            sol, *_ = np.linalg.lstsq(A, obs[:, good], rcond=None)
            solve = {"available": True, "box": int(box), "n_region_used": int(good.sum()),
                     "n_pairs": len(pairs),
                     "sigma2_per_frame": [float(np.median(np.maximum(sol[k], 0.0)))
                                          for k in range(K)],
                     "sigma_per_frame": [float(math.sqrt(max(float(np.median(np.maximum(sol[k], 0.0))), 0.0)))
                                         for k in range(K)],
                     "residual_rms_frac": float(np.sqrt(np.mean(
                         (A @ sol - obs[:, good]) ** 2)) / max(np.mean(obs[:, good]), 1e-12))}
    # P-E **尺度分辨率误差曲线**（区域平均 sigma 能否代表区域内逐点 sigma）
    res_curve: List[Dict[str, Any]] = []
    if args.res_curve:
        f0 = aligned[0]
        for B in (32, 64, 128, 256):
            res_curve.append(E.resolution_error(f0, B, sat_adu=SAT_ADU))
    for f in per_frame:
        f.pop("_sigma_map", None)
        f.pop("_valid", None)
    return {
        "panel": metas[0]["panel"], "n_frames": K,
        "frames": per_frame,
        "pairs": pair_rows,
        "decomposition": decomp,
        "cross_frame": {
            "frame_scalar_values": [float(x) for x in scalars],
            "frame_scalar_p95_over_p05": float(np.percentile(scalars, 95)
                                                / max(np.percentile(scalars, 5), 1e-12)),
            "frame_scalar_spread_rel": float((scalars.max() - scalars.min())
                                             / max(np.median(scalars), 1e-12)),
            "region_sigma_median": float(np.median(reg_med)),
            "region_cross_frame_p95_over_p05_median": float(np.median(reg_spread)),
            "region_cross_frame_p95_over_p05_p95": float(np.percentile(reg_spread, 95)),
            "region_within_frame_dispersion_median": float(np.median(
                [f["region_sigma_dispersion"] for f in per_frame])),
        },
        "sigma_solve": solve,
        "resolution_curve": res_curve,
        "elapsed_s": time.time() - t0,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp03_e2_real.json")
    ap.add_argument("--panels", default="")
    ap.add_argument("--max-frames", type=int, default=0)
    ap.add_argument("--no-res-curve", dest="res_curve", action="store_false", default=True)
    args = ap.parse_args()
    t0 = time.time()
    files = sorted(T2.rglob(PAT))
    groups: Dict[str, List[Path]] = {}
    for f in files:
        groups.setdefault(f.parent.name, []).append(f)
    want = [p for p in args.panels.split(",") if p] or sorted(groups)
    out: Dict[str, Any] = {"meta": {
        "seed": SEED, "glob": "testdata/M42_T2T3_mosaic_Flying_dutchman/T2/**/" + PAT,
        "box_main": BOX_MAIN, "box_aux": list(BOX_AUX), "sat_adu": SAT_ADU,
        "delta_budget": E.DELTA_BUDGET,
        "data_class": "testdata 真实数据（最高设计 §12.2 第 3 类，只读）",
        "truth": "无真值 —— 差分参考 R2 是唯一可得的无偏噪声参考（其前提由本臂 P-A 检验）"},
        "panels": []}
    for p in want:
        ps = groups.get(p)
        if not ps:
            continue
        if args.max_frames > 0:
            ps = ps[:args.max_frames]
        r = run_panel(ps, args)
        out["panels"].append(r)
        print("panel %s done K=%d %.1fs" % (p, r["n_frames"], r["elapsed_s"]), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    fp = Path(args.out)
    fp.parent.mkdir(parents=True, exist_ok=True)
    fp.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float), encoding="utf-8")
    print("wrote", fp, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
