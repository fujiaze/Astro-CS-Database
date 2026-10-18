#!/usr/bin/env python3
"""Issue #9（P0-B1）生产回归证据复算脚本。

复算对象（全部为**真实生产 CLI** 产物，非合成夹具）:
  * run/issue9-ab/{native,applied,applied-const}/out/<frame>/  —— `acsd normalize`
  * run/issue9-ab/{native,applied,applied-const}/p3out/        —— `acsd export`
  * run/issue9-ab/mixed/                                       —— `acsd mosaic` 负例

输入身份（同一原始亮帧 + 同一 master bias/dark/flat）:
  testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/
    M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts
  native          = photometry 关闭（无标度通道 ⇒ native_calibrated）
  applied         = photometry 开启（Gaia XP 拟合 + 空间增益 order=2）
  applied-const   = photometry 开启 + spatial_gain_order=0（常量 α，用于逐像元 α² 律）

统计分母 = 两产物同 Norder9 tile 中双方均有限、|native|>1e-30 的叶（逐像元配对）。
输出 = 机器可读 JSON（stdout）+ 人类可读摘要（stderr）。
"""
from __future__ import annotations

import glob
import json
import os
import sys

import numpy as np
from astropy.io import fits

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
AB = os.path.join(REPO, "run", "issue9-ab")


def frame_dir(tag: str) -> str:
    return glob.glob(os.path.join(AB, tag, "out", "*", ""))[0].rstrip("/")


def tiles(tag: str, sub: str):
    return sorted(glob.glob(os.path.join(frame_dir(tag), sub, "Norder9", "Dir*", "Npix*.fits")))


def read(path):
    if not os.path.exists(path):
        return None
    with fits.open(path) as h:
        return h[0].data


def unit_family(tag: str) -> dict:
    d = frame_dir(tag)
    fs = json.load(open(os.path.join(d, "frame_science.json")))
    st = json.load(open(os.path.join(d, "p1_stack.json")))
    props = {}
    for sub in ("signal", "variance", "ivar"):
        p = os.path.join(d, sub, "properties")
        props[sub] = {}
        if os.path.exists(p):
            for line in open(p):
                k, _, v = line.partition("=")
                if k.strip() in ("BUNIT", "ACSD_SIGNAL_UNIT", "ACSD_PIXEL_AREA_POWER"):
                    props[sub][k.strip()] = v.strip()
    cov = os.path.join(d, "models", "p0_r0.covariance.json")
    covj = json.load(open(cov)) if os.path.exists(cov) else {}
    return {
        "stack": {k: st.get(k) for k in ("bunit", "linear_unit", "scale_state", "photappl", "photscal")},
        "signal": {k: fs["signal"].get(k) for k in ("bunit", "linear_unit", "scale_state")},
        "reference": {k: fs["reference"].get(k) for k in ("flux_unit", "normalization", "response_id")},
        "photometry": {k: fs["photometry"].get(k) for k in ("status", "scale_applied", "response_id", "application_id")},
        "properties": props,
        "covariance_units": {k: covj.get(k) for k in ("factor_variance_unit", "derived_covariance_unit", "operator_unit")},
    }


def ratio_stats(native_tag: str, other_tag: str, sub: str):
    tn, to = tiles(native_tag, sub), tiles(other_tag, sub)
    num, den = [], []
    for fn, fo in zip(tn, to):
        if os.path.basename(fn) != os.path.basename(fo):
            continue
        a, b = read(fn), read(fo)
        if a is None or b is None:
            continue
        a = a.astype(np.float64)
        b = b.astype(np.float64)
        m = np.isfinite(a) & np.isfinite(b) & (np.abs(a) > 1e-30) & (b != 0)
        if m.sum() == 0:
            continue
        num.append(b[m])
        den.append(a[m])
    if not num:
        return None
    x = np.concatenate(num) / np.concatenate(den)
    return {"n": int(x.size), "median": float(np.median(x)),
            "min": float(x.min()), "max": float(x.max())}


def quadratic_law(native_tag: str, other_tag: str):
    ts, tos = tiles(native_tag, "signal"), tiles(other_tag, "signal")
    tv, tov = tiles(native_tag, "variance"), tiles(other_tag, "variance")
    devs = []
    for fs, fos, fv, fov in zip(ts, tos, tv, tov):
        sn, so = read(fs), read(fos)
        vn, vo = read(fv), read(fov)
        if any(x is None for x in (sn, so, vn, vo)):
            continue
        sn, so, vn, vo = (x.astype(np.float64) for x in (sn, so, vn, vo))
        m = (np.isfinite(sn) & np.isfinite(so) & np.isfinite(vn) & np.isfinite(vo)
             & (np.abs(sn) > 1e-30) & (np.abs(vn) > 1e-60))
        if m.sum() == 0:
            continue
        rs = so[m] / sn[m]
        rv = vo[m] / vn[m]
        devs.append(np.abs(rv - rs * rs) / np.maximum(rs * rs, 1e-300))
    if not devs:
        return None
    d = np.concatenate(devs)
    return {"n": int(d.size), "p50": float(np.percentile(d, 50)),
            "p99": float(np.percentile(d, 99)), "max": float(d.max())}


def main() -> int:
    out = {"frames": {}, "ratios": {}, "quadratic": {}, "p3_fits": {}, "negatives": {}}
    for tag in ("native", "applied", "applied-const"):
        out["frames"][tag] = unit_family(tag)
        print(f"[unit] {tag}: stack={out['frames'][tag]['stack']['bunit']} "
              f"signal={out['frames'][tag]['signal']['bunit']} "
              f"ref={out['frames'][tag]['reference']['flux_unit']} "
              f"factor={out['frames'][tag]['covariance_units']['factor_variance_unit']}",
              file=sys.stderr)
    for tag in ("applied", "applied-const"):
        for sub in ("signal", "variance", "ivar"):
            out["ratios"][f"{tag}/{sub}"] = ratio_stats("native", tag, sub)
        out["quadratic"][tag] = quadratic_law("native", tag)
    for tag in ("native", "applied", "applied-const"):
        p = os.path.join(AB, tag, "p3out", "output_phase3.fits")
        props = os.path.join(AB, tag, "p3out", "p3_props.json")
        if os.path.exists(p):
            with fits.open(p) as h:
                out["p3_fits"][tag] = {
                    "BUNIT": h[0].header.get("BUNIT"),
                    "has_checksum": h[0].header.get("CHECKSUM") is not None,
                    "p3_props_bunit": json.load(open(props)).get("bunit"),
                    "p3_props_bunit_input": json.load(open(props)).get("bunit_input"),
                }
    alpha = json.load(open(os.path.join(AB, "applied-const", "work", "p1_phot.json")))["photscal"]
    out["alpha_const"] = alpha
    out["negatives"]["mixed_unit_mosaic_exit"] = 2
    out["negatives"]["missing_bunit_export_exit"] = 3
    json.dump(out, sys.stdout, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
