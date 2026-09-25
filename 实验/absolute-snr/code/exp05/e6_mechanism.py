#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 F：**机理核对**（真实数据，无需真值）—— 帧级 sigma 到底被什么撑大？

做法：对每个 panel 的首帧，比较两个**同一 recipe**（整帧 2 轮裁剪 RMS）的读数：
  raw   ：整帧原始像素；
  resid ：减去 64 px mesh 局部背景（只用 fam0 像素估计）后的残差像素。
两者之比 = 「大尺度结构对帧级 sigma 的贡献倍数」——它**不需要真值**，是 c_sigma 机理的直接证据。

只读：astropy.io.fits.getdata + numpy。不运行任何 ACSD 可执行文件。
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
import e3_real as E3      # noqa: E402

DELTA = 64


def mesh_background_fam(img: np.ndarray, fam: int, delta: int = DELTA,
                        n_iter: int = 3, filter_size: int = 3,
                        min_pix: int = 64) -> np.ndarray:
    """只用 fam 家族像素估计的迭代 mesh 背景图（与 sigma_patch_resid_fam 同一实现）。"""
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    m = ((yy + xx) % 2 == fam) & np.isfinite(a)
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
                loc[i] = float(X.prod_sigma(v)["background"])
        g = loc.reshape(by, bx)
        if not np.isfinite(g).any():
            break
        g = np.where(np.isfinite(g), g, float(np.nanmedian(g)))
        g = X.C._median_filter_nan(g, filter_size)
        g = np.where(np.isfinite(g), g, float(np.nanmedian(g)))
        dB = X.C._expand_mesh_map(g, (ny, nx))
        B = B + np.where(np.isfinite(dB), dB, 0.0)
    return B


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e6_mechanism.json"))
    ap.add_argument("--panels", default="M1,M2,M4,M5")
    a = ap.parse_args()
    t0 = time.time()
    rows: List[Dict[str, Any]] = []
    for panel in [p.strip() for p in a.panels.split(",") if p.strip()]:
        d = X.TESTDATA / "M42_T2T3_mosaic_Flying_dutchman" / "T2" / panel
        files = sorted(f for f in d.glob("*.fts") if "300S-Red" in f.name)
        for f in files:
            img = E3.read_frame(f)
            ny, nx = img.shape
            yy, xx = np.mgrid[0:ny, 0:nx]
            m = ((yy + xx) % 2 == 0) & np.isfinite(img)
            raw = float(X.prod_sigma(img[m])["sigma"])
            B = mesh_background_fam(img, 0, DELTA)
            res = (img - B)[m]
            rs = float(X.prod_sigma(res)["sigma"])
            rows.append({"panel": panel, "file": f.name,
                         "sigma_frame_raw": raw, "sigma_frame_struct_removed": rs,
                         "structure_boost": raw / rs})
            print("%-3s %-46s raw %.3f  struct-removed %.3f  boost x%.3f"
                  % (panel, f.name[:46], raw, rs, raw / rs), flush=True)
    out = {"meta": {"delta_px": DELTA,
                    "meaning": "整帧裁剪 RMS 在原始像素 vs 去大尺度结构残差上的读数之比 = 结构对帧级 sigma 的贡献倍数（无需真值）",
                    "data_class": "testdata 真实数据（只读）",
                    "elapsed_s": time.time() - t0},
           "frames": rows}
    X.save_json(a.out, out)
    print("wrote", a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
