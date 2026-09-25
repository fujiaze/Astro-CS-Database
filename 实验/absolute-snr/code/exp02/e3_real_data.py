#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 / 实验臂 C：**testdata 真实数据**（§12.2 第 3 类数据，只读）。

对象：testdata/M42_T2T3_mosaic_Flying_dutchman/T2/<M1..M6>/*-300S-Red.fts
（M42 马赛克 T2 板块，16 帧；EXP-01 §2.2③ 用的是其中前 8 帧的 2048² 中心裁剪，
本臂默认跑**全部 16 帧**以便与 EXP-01 的子集逐帧对照）。

真实数据臂**没有真值**，因此本臂只做三件可证的事：
  1. 给出生产 noise_sigma 与全部**无需真值**的结构代理量（A1/A2/R/C/D/kf）；
  2. 用"棋盘分半 + 块间离散"给出**帧内自洽性**证据（σ̂ 的空间非均匀性）；
  3. 给出三档判据在真实帧上的分布 —— 检验门是否恒红/恒绿（非退化）。

只读；不修改任何 testdata 文件；不运行任何 ACSD 可执行文件。
固定 seed = 20260925（本臂只用固定 seed 做棋盘分半的确定性切分）。
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
T2 = ROOT / "testdata" / "M42_T2T3_mosaic_Flying_dutchman" / "T2"
PAT = "*-300S-Red.fts"
SEED = 20260925
CROP = 2048
BOX_GRID = (32, 64, 128)
BOX_PRIMARY = 64
DELTA_BUDGET = 0.014


def _block_sigmas(sub: np.ndarray, nb: int) -> Dict[str, float]:
    """nb×nb 分块的 σ̂ 离散（EXP-01 §2.2③ 的 block_sigma_p95_over_p05 口径）。"""
    bs = sub.shape[0] // nb
    blocks = (sub[:nb * bs, :nb * bs].reshape(nb, bs, nb, bs)
                 .transpose(0, 2, 1, 3).reshape(-1, bs, bs))
    prod = np.array([C.production_clip_sigma(b)["sigma"] for b in blocks])
    return {"block_sigma_p50": float(np.median(prod)),
            "block_sigma_p05": float(np.percentile(prod, 5)),
            "block_sigma_p95": float(np.percentile(prod, 95)),
            "block_sigma_p95_over_p05": float(np.percentile(prod, 95)
                                              / max(np.percentile(prod, 5), 1e-12)),
            "n_block": int(nb * nb), "block_size": int(bs)}


def _checkerboard_se(sub: np.ndarray, nb: int) -> Dict[str, float]:
    """棋盘分半 σ 比（结构在一阶抵消）→ 帧内统计误差实测。"""
    bs = sub.shape[0] // nb
    blocks = (sub[:nb * bs, :nb * bs].reshape(nb, bs, nb, bs)
                 .transpose(0, 2, 1, 3).reshape(-1, bs, bs))
    yy, xx = np.mgrid[0:bs, 0:bs]
    m = (yy + xx) % 2 == 0
    r = []
    for b in blocks:
        a = C.production_clip_sigma(b[m])["sigma"]
        c = C.production_clip_sigma(b[~m])["sigma"]
        r.append(a / c - 1.0)
    r = np.array(r)
    return {"se_checkerboard_rms": float(np.sqrt(np.mean(r ** 2)) / math.sqrt(2.0)),
            "se_pred_1_over_sqrt2N": float(0.7071067811865476 / math.sqrt(bs * bs / 2.0))}


def run_frame(path: Path, crop: int) -> Dict[str, Any]:
    from astropy.io import fits
    with fits.open(path, memmap=False) as hdul:
        d = np.asarray(hdul[0].data, dtype=np.float64)
        hdr = hdul[0].header
    ny, nx = d.shape
    y0, x0 = max((ny - crop) // 2, 0), max((nx - crop) // 2, 0)
    sub = d[y0:y0 + crop, x0:x0 + crop]
    row: Dict[str, Any] = {
        "file": path.name, "panel": path.parent.name,
        "shape": [int(ny), int(nx)], "crop": [int(y0), int(x0), int(crop), int(crop)],
        "bitpix": int(hdr.get("BITPIX", 0)), "exptime": hdr.get("EXPTIME"),
        "filter": hdr.get("FILTER"), "object": hdr.get("OBJECT"),
        "min": float(np.min(sub)), "max": float(np.max(sub)),
        "median": float(np.median(sub)),
    }
    p = C.structure_proxies(sub, box=BOX_PRIMARY, filter_size=3, n_iter=2)
    row.update({k: p[k] for k in ("prod_sigma", "prod_keep_frac", "fix_sigma",
                                  "fix_sigma_meshmedian", "R_struct", "C", "A1",
                                  "A1_mad", "A2", "A2_mad", "A2_meshmedian",
                                  "A2_meshmedian_mad", "D", "kf",
                                  "fix_effect_delta", "fix_effect_delta_meshmedian",
                                  "sigma_diff_over_sqrt2",
                                  "sigma_diff_mad_over_sqrt2")})
    row["verdict"] = C.classify(p["R_struct"], p["C"], p["kf"],
                                delta_budget=DELTA_BUDGET, A1=p["A1"],
                                A2=p["A2"], D=p["D"])
    row["fix_rel_vs_prod"] = C.effect_delta(p["fix_sigma"], p["prod_sigma"])
    row["boxes"] = {}
    for box in BOX_GRID:
        m = C.mesh_sigma(sub, box=box, filter_size=3, n_iter=2)
        g = C.convergence_gate(sub, box=box, filter_size=3)
        row["boxes"][str(box)] = {
            "sigma_resid": m["sigma_resid"], "sigma_mesh_median": m["sigma_mesh_median"],
            "sigma_mesh_p05": m["sigma_mesh_p05"], "sigma_mesh_p95": m["sigma_mesh_p95"],
            "dispersion": m["sigma_mesh_dispersion"], "C": g["C"],
            "keep_frac_mesh_p05": m["keep_frac_mesh_p05"],
        }
    row.update({("blk_" + k): v for k, v in _block_sigmas(sub, 8).items()})
    row.update({("cb_" + k): v for k, v in _checkerboard_se(sub, 8).items()})
    return row


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/exp02_e3_real.json")
    ap.add_argument("--crop", type=int, default=CROP)
    ap.add_argument("--max-frames", type=int, default=0)
    ap.add_argument("--panel", default="")
    args = ap.parse_args()
    t0 = time.time()
    pats = [PAT]
    if args.panel:
        pats = ["*-300S-Red.fts"]
    files = sorted(T2.rglob(PAT))
    if args.panel:
        files = [f for f in files if f.parent.name == args.panel]
    if args.max_frames > 0:
        files = files[:args.max_frames]
    rows: List[Dict[str, Any]] = []
    for f in files:
        rows.append(run_frame(f, args.crop))
        print("done", f.name, "%.1fs" % (time.time() - t0), flush=True)
    out = {"meta": {"seed": SEED, "crop": args.crop, "box_grid": list(BOX_GRID),
                    "box_primary": BOX_PRIMARY, "delta_budget": DELTA_BUDGET,
                    "glob": str(T2.relative_to(ROOT)) + "/**/" + PAT,
                    "n_frames": len(rows),
                    "data_class": "testdata 真实数据（最高设计 §12.2 第 3 类，只读）"},
           "frames": rows}
    out["meta"]["elapsed_s"] = time.time() - t0
    p = Path(args.out)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=1, default=float),
                 encoding="utf-8")
    print("%-52s %10s %8s %8s %8s %8s %8s %6s %8s %s" %
          ("file", "prod_sigma", "kf", "fix", "R", "A1", "A2", "D", "p95/p05", "verdict"))
    for r in rows:
        print("%-52s %10.4f %8.4f %8.4f %8.3f %8.3f %8.4f %6.2f %8.2f %s" %
              (r["file"][:52], r["prod_sigma"], r["prod_keep_frac"], r["fix_sigma"],
               r["R_struct"], r["A1"], r["A2"], r["D"],
               r["blk_block_sigma_p95_over_p05"], r["verdict"]))
    print("wrote", p, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
