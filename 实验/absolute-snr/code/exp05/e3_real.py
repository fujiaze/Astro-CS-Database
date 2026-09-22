#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 C：**testdata 真实数据**（最高设计 12.2 第 3 类，只读）。

数据：testdata/M42_T2T3_mosaic_Flying_dutchman/T2/{M1,M2,M4,M5} 的 300S-Red 实拍帧。
真值口径（**零像素重叠**）：1 px 棋盘 hold-out，fam=(y+x)%2：
    fam 0 像素 -> 只用于**估计** sigma（帧级与逐 patch）；
    fam 1 像素 -> 只用于**真值** sigma（逐 patch 稳健尺度，自身噪声 1/sqrt(N_fam1) 量级）。
真值自身的统计误差按 1/sqrt(2*N) 逐 patch 估计并在结果中报告（不掩饰）。

本臂回答（真实数据、无仿真假设）：
  * 生产帧级标量 sigma 与 patch 中位 sigma 的**帧内共模因子** c 在真实帧上有多大？
  * 绝对表示与相对表示的电平误差各是多少（以 hold-out 真值为基准）？
  * 同一 panel 内跨帧（天光电平不同）时，两种表示的跨帧比值各偏多少？

只读：仅 astropy.io.fits.getdata + numpy；不写回、不修改任何数据文件。
固定 seed：不涉及随机数（真实数据臂是确定性的）。不运行任何 AstroCS 可执行文件。
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
import exp05_common as X  # noqa: E402

DELTA = 64
CROP = 2048
PANELS = ("M1", "M2", "M4", "M5")
MIN_FAM_PIX = 64          # patch 内有效家族像素下限


def read_frame(path: Path) -> np.ndarray:
    from astropy.io import fits
    with fits.open(path, memmap=False) as hdul:
        data = None
        for h in hdul:
            if getattr(h, "data", None) is not None and h.data.ndim == 2:
                data = np.asarray(h.data, dtype=np.float64)
                break
        if data is None:
            raise RuntimeError("no 2-D image in %s" % path)
    ny, nx = data.shape
    if ny < CROP or nx < CROP:
        raise RuntimeError("frame smaller than crop: %s" % (data.shape,))
    y0, x0 = (ny - CROP) // 2, (nx - CROP) // 2
    return data[y0:y0 + CROP, x0:x0 + CROP]


def fam_patch_sigma(img: np.ndarray, fam: int, delta: int) -> Tuple[np.ndarray, np.ndarray]:
    """按家族取像素后逐 patch 套生产 recipe；返回 (sigma_grid, n_valid_grid)。"""
    ny, nx = img.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    mask = ((yy + xx) % 2 == fam) & np.isfinite(img)
    by, bx = ny // delta, nx // delta
    sub = np.where(mask, img, np.nan)[:by * delta, :bx * delta]
    mm = sub.reshape(by, delta, bx, delta).transpose(0, 2, 1, 3).reshape(by * bx, delta * delta)
    out = np.full(by * bx, np.nan)
    nv = np.zeros(by * bx, dtype=int)
    for i in range(by * bx):
        v = mm[i][np.isfinite(mm[i])]
        nv[i] = v.size
        if v.size >= MIN_FAM_PIX:
            out[i] = float(X.prod_sigma(v)["sigma"])
    return out.reshape(by, bx), nv.reshape(by, bx)


def fam_frame_sigma(img: np.ndarray, fam: int) -> Tuple[float, int]:
    ny, nx = img.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    v = img[((yy + xx) % 2 == fam) & np.isfinite(img)]
    if v.size < 1024:
        return float("nan"), int(v.size)
    return float(X.prod_sigma(v)["sigma"]), int(v.size)


def process_frame(path: Path) -> Dict[str, Any]:
    t0 = time.time()
    img = read_frame(path)
    sf, n0 = fam_frame_sigma(img, 0)
    sp, nv = fam_patch_sigma(img, 0, DELTA)
    st, _ = fam_patch_sigma(img, 1, DELTA)
    sp_r1 = X.sigma_patch_resid_fam(img, 0, DELTA)
    n_fam1 = 2.0 * np.asarray(nv, dtype=np.float64)          # 家族 1 的像素数
    # 真值自身的统计误差（稳健尺度在 N 样本上的相对标准误 ~ 1/sqrt(2N)）
    se_true = 1.0 / np.sqrt(2.0 * np.maximum(n_fam1, 1.0))
    valid = np.isfinite(sp) & np.isfinite(st)
    c = X.frame_common_factor(sp[valid], sf)
    lv_abs = X.level_dev(sp[valid], st[valid])
    lv_rel = X.level_dev(sp[valid] * c, st[valid])
    v1 = np.isfinite(sp_r1) & np.isfinite(st)
    c_r1 = X.frame_common_factor(sp_r1[v1], sf)
    lv_abs_r1 = X.level_dev(sp_r1[v1], st[v1])
    lv_rel_r1 = X.level_dev(sp_r1[v1] * c_r1, st[v1])
    return {
        "file": path.name, "panel": path.parent.name,
        "shape": list(img.shape), "n_fam0_frame_px": n0,
        "sigma_frame_fam0": sf, "sigma_patch_median_fam0": float(np.median(sp[valid])),
        "sigma_true_patch_median_fam1": float(np.median(st[valid])),
        "c_sigma": c,
        "b_patch": lv_abs["median_ratio"],
        "abs_snr_dev": lv_abs["snr_rel_dev"], "rel_snr_dev": lv_rel["snr_rel_dev"],
        "abs_p95": lv_abs["p95_abs_dev"], "rel_p95": lv_rel["p95_abs_dev"],
        "c_sigma_R1": c_r1, "b_patch_R1": lv_abs_r1["median_ratio"],
        "abs_snr_dev_R1": lv_abs_r1["snr_rel_dev"],
        "rel_snr_dev_R1": lv_rel_r1["snr_rel_dev"],
        "sigma_patch_median_R1": float(np.median(sp_r1[v1])),
        "truth_se_median": float(np.median(se_true[valid])),
        "n_patch_valid": int(valid.sum()),
        "crop_median_adu": float(np.median(img[np.isfinite(img)])),
        "_sp": sp, "_st": st, "_sp_r1": sp_r1, "_valid": valid,
        "elapsed_s": time.time() - t0,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e3_real.json"))
    ap.add_argument("--panels", default=",".join(PANELS))
    a = ap.parse_args()
    t0 = time.time()
    panels = [p.strip() for p in a.panels.split(",") if p.strip()]
    out: Dict[str, Any] = {"meta": {
        "delta_px": DELTA, "crop": CROP, "panels": panels,
        "data_class": "testdata 真实数据（最高设计 12.2 第 3 类，只读）",
        "data_root": "testdata/M42_T2T3_mosaic_Flying_dutchman/T2",
        "truth": "1px 棋盘 hold-out：fam=(y+x)%2，fam0 估计 / fam1 真值（零像素重叠）；真值自身 SE 逐 patch 报告",
        "estimators": {"frame": "fam0 全帧 2 轮裁剪 RMS（生产 recipe 镜像）",
                       "patch": "fam0 逐 64px patch 同一 recipe"},
    }, "frames": [], "panels": []}
    for p in panels:
        d = X.TESTDATA / "M42_T2T3_mosaic_Flying_dutchman" / "T2" / p
        files = sorted(f for f in d.glob("*.fts") if "300S-Red" in f.name)
        if not files:
            print("PANEL %s: no 300S-Red frames" % p, flush=True)
            continue
        recs = [process_frame(f) for f in files]
        out["frames"].extend({k: v for k, v in r.items() if not k.startswith("_")}
                             for r in recs)
        sps = [r["_sp"] for r in recs]
        sts = [r["_st"] for r in recs]
        cs = [r["c_sigma"] for r in recs]
        sps1 = [r["_sp_r1"] for r in recs]
        cs1 = [r["c_sigma_R1"] for r in recs]
        xf_abs = X.cross_frame_ratio_dev(sps, sts, ref=0) if len(recs) > 1 else {}
        xf_rel = (X.cross_frame_ratio_dev([sps[k] * cs[k] for k in range(len(recs))],
                                          sts, ref=0) if len(recs) > 1 else {})
        xf_abs_r1 = X.cross_frame_ratio_dev(sps1, sts, ref=0) if len(recs) > 1 else {}
        xf_rel_r1 = (X.cross_frame_ratio_dev([sps1[k] * cs1[k] for k in range(len(recs))],
                                             sts, ref=0) if len(recs) > 1 else {})
        out["panels"].append({
            "panel": p, "n_frames": len(recs),
            "files": [r["file"] for r in recs],
            "c_sigma_per_frame": cs,
            "sigma_frame_per_frame": [r["sigma_frame_fam0"] for r in recs],
            "sigma_patch_median_per_frame": [r["sigma_patch_median_fam0"] for r in recs],
            "sigma_true_median_per_frame": [r["sigma_true_patch_median_fam1"] for r in recs],
            "abs_snr_dev_per_frame": [r["abs_snr_dev"] for r in recs],
            "rel_snr_dev_per_frame": [r["rel_snr_dev"] for r in recs],
            "cross_frame_abs": xf_abs, "cross_frame_rel": xf_rel,
            "cross_frame_abs_R1": xf_abs_r1, "cross_frame_rel_R1": xf_rel_r1,
            "c_sigma_R1_per_frame": cs1,
            "abs_snr_dev_R1_per_frame": [r["abs_snr_dev_R1"] for r in recs],
            "rel_snr_dev_R1_per_frame": [r["rel_snr_dev_R1"] for r in recs],
            "c_spread_p95_over_p05": (float(np.percentile(cs, 95) / np.percentile(cs, 5))
                                      if len(cs) > 1 else float("nan")),
        })
        print("panel %-3s n=%d c=%s absSNR=%s relSNR=%s"
              % (p, len(recs), ["%.3f" % x for x in cs],
                 ["%+.4f" % r["abs_snr_dev"] for r in recs],
                 ["%+.4f" % r["rel_snr_dev"] for r in recs]), flush=True)
        if xf_abs:
            print("   xframe absSNRdev=%s relSNRdev=%s"
                  % ({k: round(v["snr_rel_dev"], 4) for k, v in xf_abs.items()},
                     {k: round(v["snr_rel_dev"], 4) for k, v in xf_rel.items()}), flush=True)
    out["meta"]["elapsed_s"] = time.time() - t0
    X.save_json(a.out, out)
    print("wrote", a.out, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
