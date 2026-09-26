#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_scan.py - P3 k_corr 补实验总控: 全部实验组一次跑完, 结果落 results/*.json.

实验组:
  G0 自检臂   : 恒等几何(pixfrac=1.0, 输出=源网格)下 k_corr 必须 ~1 (装备有效性;
                文档记载 iid Gaussian ratio 0.997, PHASE2_SAMPLER.md F3(b))
  G1 正本复现 : 正本几何单点复现 + 8 相位 x 8 seed 敏感性网格
  G2 效应分解 : k_corr = k_shape(非高斯边际) x k_geom(相关) 的正本几何分解
  G3 N 扫描   : 正本几何下 k_corr(N_retained), 含 N=5 三种形状
  G4 几何扫描 : k_corr(rho, pixfrac) 网格, rho = 输出/源像素尺度比
  G5 多帧变体 : k 帧等权 co-drizzle(dither) 对 k_corr 的影响
  G6 拟合     : k_corr(N) = k_inf x (1 + c/(N-1)) 最小二乘
"""
from __future__ import annotations

import csv
import json
import math
import os
import sys
import time
from datetime import datetime, timezone

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mc_kcorr as m  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "results")
os.makedirs(OUT, exist_ok=True)

SEED_IID = 990001          # 形状臂独立流(写死)
SEED_PHASE = 990100        # 相位网格基(写死)
SEED_SCAN = 991000         # G3/G4/G5 基(写死)
SEED_G4 = 992000           # G4 专用(写死)
SEED_G5 = 993000           # G5 专用(写死)

def dump(name: str, obj: dict) -> None:
    obj["written_utc"] = datetime.now(timezone.utc).isoformat()
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)
    print(f"[written] {name}")

def cx_cy(op_meta_frameless: bool = True) -> tuple:
    return (m.W_SRC / 2 * m.SRC_SCALE_ARCSEC, m.H_SRC / 2 * m.SRC_SCALE_ARCSEC)

# ---------------- G0 自检臂 ----------------
def g0() -> dict:
    # 恒等几何: 输出网格=源网格, pixfrac=1.0, phase=(0.5,0.5) => M=I(逐位),
    # patch = iid 高斯样本。判据: 大 N 档 k_corr ~ 1(装备有效性);
    # 小 N 档必须等于 iid 高斯有限 N 参考 k_gauss(N)(G3b/直接定征), 不是 1。
    op = m.build_operator(src_scale=300.0, out_scale=300.0, pixfrac=1.0, phase=(0.5, 0.5))
    ident = bool(np.allclose(op.M, np.eye(op.M.shape[0])))
    cx, cy = cx_cy()
    patch = m.patch_nearest(op, 289, cx, cy)
    res = m.run_config(op, patch, nmc=4000, sigma=m.SIGMA_SRC, seed0=SEED_SCAN, iid_seed=SEED_IID)
    patch25 = m.patch_nearest(op, 25, cx, cy)
    res25 = m.run_config(op, patch25, nmc=4000, sigma=m.SIGMA_SRC, seed0=SEED_SCAN + 1,
                         iid_seed=SEED_IID, with_diag=False)
    out = dict(group="G0_sanity", identity_M=ident, identity_geometry=op.meta,
               N289=res, N25=res25,
               reference_k_gauss=dict(N25=1.0826, N289=None,
                                      note="k_gauss(N) from g3b_gauss_ref.json / direct characterization"),
               gate=dict(pass_identity_M=ident,
                         pass289=bool(abs(res["k_corr"] - 1.0) < 0.06),
                         pass25=bool(abs(res25["k_corr"] - 1.0826) < 0.10)))
    dump("g0_sanity.json", out)
    return out

# ---------------- G1 正本复现 ----------------
def g1() -> dict:
    phases = [(a, b) for a in (0.05, 0.3, 0.55, 0.8) for b in (0.05, 0.3, 0.55, 0.8)]
    rows = []
    for i, ph in enumerate(phases):
        op = m.build_operator(phase=ph)
        patch = op.touched_idx
        seeds = [m.SEED_BASE + 1000 * s for s in range(8)]
        ks = []
        for s in seeds:
            V = m.mc_medians(op.M, patch, m.NMC_CANONICAL, m.SIGMA_SRC, s, op.M.shape[1])
            ks.append(m.median_stats(V)["k_corr"])
        ks = np.array(ks)
        rows.append(dict(phase=list(ph), n_touched=int(patch.size),
                         k_corr_mean=float(ks.mean()), k_corr_sd=float(ks.std(ddof=1)),
                         k_corr_min=float(ks.min()), k_corr_max=float(ks.max())))
    allk = np.array([r["k_corr_mean"] for r in rows])
    out = dict(group="G1_canonical_repro", nmc=m.NMC_CANONICAL,
               target_from_repo=1.3883, frozen_default=1.4,
               phases=rows,
               k_corr_range=[float(allk.min()), float(allk.max())],
               k_corr_mean_over_phases=float(allk.mean()),
               k_corr_sd_over_phases=float(allk.std(ddof=1)))
    dump("g1_canonical.json", out)
    return out

# ---------------- G2 效应分解 ----------------
def g2() -> dict:
    op = m.build_operator(phase=(0.3, 0.55))
    patch = op.touched_idx
    res = m.run_config(op, patch, nmc=m.NMC_CANONICAL, sigma=m.SIGMA_SRC,
                       seed0=m.SEED_BASE, iid_seed=SEED_IID)
    out = dict(group="G2_decomposition", phase=op.meta["phase"], n_touched=int(patch.size),
               **res)
    dump("g2_decomposition.json", out)
    return out

# ---------------- G3 N 扫描 ----------------
def g3() -> dict:
    op = m.build_operator(phase=(0.3, 0.55))
    cx, cy = cx_cy()
    shapes = m.patch_shapes(op, cx, cy)
    n_touched = int(op.touched_idx.size)
    ns = [5, 9, 17, 25, 49, 81, 121, 169, n_touched]
    rows = []
    for n in ns:
        if n > n_touched:
            continue
        patch = m.patch_nearest(op, n, cx, cy)
        r = m.run_config(op, patch, nmc=m.NMC_SCAN, sigma=m.SIGMA_SRC,
                         seed0=SEED_SCAN + n, iid_seed=SEED_IID + n)
        r["shape"] = "compact_nearest"
        rows.append(r)
    for name, patch in shapes.items():
        r = m.run_config(op, patch, nmc=m.NMC_SCAN, sigma=m.SIGMA_SRC,
                         seed0=SEED_SCAN + 7, iid_seed=SEED_IID + 7)
        r["shape"] = name
        rows.append(r)
    out = dict(group="G3_N_scan", phase=op.meta["phase"], n_touched_full=n_touched,
               rows=rows)
    dump("g3_nscan.json", out)
    return out

# ---------------- G4 几何扫描 ----------------
def g4() -> dict:
    rhos = [0.5, 0.7071, 1.0, m.healpix_leaf_arcsec(512) / 300.0, 2.0]
    pixfracs = [0.5, 0.7, 0.8, 1.0]
    rows = []
    for rho in rhos:
        for pf in pixfracs:
            op = m.build_operator(out_scale=rho * 300.0, pixfrac=pf, phase=(0.3, 0.55))
            patch = op.touched_idx
            r = m.run_config(op, patch, nmc=m.NMC_CANONICAL, sigma=m.SIGMA_SRC,
                             seed0=SEED_G4 + int(round(rho * 1000)) * 10 + int(pf * 10),
                             iid_seed=SEED_IID, with_diag=False)
            r.update(rho=rho, pixfrac=pf, n_touched=int(patch.size))
            rows.append(r)
            print(f"G4 rho={rho:.4f} pf={pf} N={r['N']} k={r['k_corr']:.4f} "
                  f"k_shape={r['k_shape']:.4f}", flush=True)
    out = dict(group="G4_geometry_scan", rows=rows)
    dump("g4_geometry_scan.json", out)
    return out

# ---------------- G5 多帧变体 ----------------
def g5() -> dict:
    dithers = {1: [(0.0, 0.0)],
               2: [(0.0, 0.0), (0.5, 0.5)],
               4: [(0.0, 0.0), (0.5, 0.5), (0.5, 0.0), (0.0, 0.5)]}
    rows = []
    for k, offs in dithers.items():
        op = m.build_operator(phase=(0.3, 0.55), frame_offsets=offs)
        patch = op.touched_idx
        r = m.run_config(op, patch, nmc=m.NMC_CANONICAL, sigma=m.SIGMA_SRC,
                         seed0=SEED_G5 + k, iid_seed=SEED_IID, with_diag=False)
        r.update(n_frames=k, n_touched=int(patch.size))
        rows.append(r)
    out = dict(group="G5_frames", rows=rows)
    dump("g5_frames.json", out)
    return out

# ---------------- G6 拟合 ----------------
def g6(g3_out: dict) -> dict:
    rows = [r for r in g3_out["rows"] if r["shape"] == "compact_nearest"]
    ns = np.array([r["N"] for r in rows], dtype=float)
    ks = np.array([r["k_corr"] for r in rows], dtype=float)
    # 模型: k = k_inf * (1 + c/(N-1)); 线性最小二乘 on k vs 1/(N-1)
    x = 1.0 / (ns - 1.0)
    A = np.vstack([np.ones_like(x), x]).T
    coef, *_ = np.linalg.lstsq(A, ks, rcond=None)
    k_inf, c = float(coef[0]), float(coef[1])
    pred = k_inf * (1.0 + c / (ns - 1.0))
    resid = float(np.max(np.abs(pred - ks) / ks))
    out = dict(group="G6_fit", model="k_corr(N) = k_inf * (1 + c/(N-1))",
               k_inf=k_inf, c=c, max_rel_resid=resid,
               N=ns.tolist(), k=ks.tolist(), pred=pred.tolist())
    dump("g6_fit.json", out)
    return out

def main() -> None:
    t0 = time.time()
    g0()
    g1()
    g2()
    g3o = g3()
    g4()
    g5()
    g6(g3o)
    # 汇总 CSV
    rows = []
    with open(os.path.join(OUT, "g3_nscan.json"), encoding="utf-8") as fh:
        g3_res = json.load(fh)
    for r in g3_res["rows"]:
        rows.append(["G3", r["shape"], r["N"], r["k_corr"], r["k_shape"], r.get("k_geom"),
                     r.get("nn_corr", {}).get("mean_nn_corr") if r.get("nn_corr") else None])
    with open(os.path.join(OUT, "g4_geometry_scan.json"), encoding="utf-8") as fh:
        g4_res = json.load(fh)
    for r in g4_res["rows"]:
        rows.append(["G4", f"rho={r['rho']:.4f},pf={r['pixfrac']}", r["N"], r["k_corr"],
                     r["k_shape"], r["k_geom"], None])
    with open(os.path.join(OUT, "g5_frames.json"), encoding="utf-8") as fh:
        g5_res = json.load(fh)
    for r in g5_res["rows"]:
        rows.append(["G5", f"frames={r['n_frames']}", r["N"], r["k_corr"], r["k_shape"],
                     r["k_geom"], None])
    with open(os.path.join(OUT, "summary.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["group", "config", "N", "k_corr", "k_shape", "k_geom", "mean_nn_corr"])
        for r in rows:
            w.writerow(r)
    print(f"[done] total {time.time() - t0:.1f}s")

if __name__ == "__main__":
    main()
