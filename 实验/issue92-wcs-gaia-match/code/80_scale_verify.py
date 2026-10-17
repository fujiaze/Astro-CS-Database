#!/usr/bin/env python3
"""Issue #92 A1-FIX —— 严格核对「应用标度」：signal 与 variance 的逐像元传播。

对照两次同输入真实 CLI:
  · enabled : photometry.fit.enabled=true  (本修复后, 施加 k_photo 与空间增益 m(x,y))
  · disabled: photometry.fit.enabled=false (未施加光度标度)

逐像元比值 r_s = signal_en/signal_dis、r_v = variance_en/variance_dis。
若标度只做乘性作用且方差按 scale² 传播, 则应有 r_v == r_s²。
（无需知道 m(x,y)：它被同一个逐像元因子吸收。）
同时记录 BUNIT/ACSD_SIGNAL_UNIT 与 ivar 比值 1/r_s²。
"""
from __future__ import annotations

import argparse
import glob
import json
import os

import numpy as np
from astropy.io import fits


def tiles(root: str, frame: str, product: str, order: int):
    pat = os.path.join(root, frame, product, f"Norder{order}", "**", "*.fits")
    return sorted(glob.glob(pat, recursive=True))


def read(path):
    with fits.open(path, memmap=False) as h:
        return np.asarray(h[0].data, dtype=np.float64)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--enabled", required=True)
    ap.add_argument("--disabled", required=True)
    ap.add_argument("--frame", required=True)
    ap.add_argument("--phot", required=True, help="enabled run 的 p1_phot.json")
    ap.add_argument("--orders", default="5,9")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    phot = json.load(open(a.phot))
    k_photo = (phot.get("photscale_detail") or {}).get(a.frame, {}).get("k_photo")
    res = {"frame": a.frame, "k_photo": k_photo, "orders": {}}

    def props(root):
        p = os.path.join(root, a.frame, "signal", "properties")
        out = {}
        if os.path.exists(p):
            for line in open(p):
                if "=" in line:
                    k, v = line.strip().split("=", 1)
                    if k in ("BUNIT", "ACSD_SIGNAL_UNIT", "ACSD_PIXEL_SEMANTICS",
                             "ACSD_PIXEL_AREA_POWER", "hips_data_range"):
                        out[k] = v
        return out

    res["signal_properties_enabled"] = props(a.enabled)
    res["signal_properties_disabled"] = props(a.disabled)

    for order in [int(x) for x in a.orders.split(",") if x]:
        es = tiles(a.enabled, a.frame, "signal", order)
        ds = tiles(a.disabled, a.frame, "signal", order)
        ev = {os.path.relpath(p, os.path.join(a.enabled, a.frame, "signal")): p for p in es}
        dv = {os.path.relpath(p, os.path.join(a.disabled, a.frame, "signal")): p for p in ds}
        common = sorted(set(ev) & set(dv))
        rs_all, rv_all, ivar_all = [], [], []
        for rel in common:
            s_en, s_dis = read(ev[rel]), read(dv[rel])
            vp_en = os.path.join(a.enabled, a.frame, "variance", rel)
            vp_dis = os.path.join(a.disabled, a.frame, "variance", rel)
            ip_en = os.path.join(a.enabled, a.frame, "ivar", rel)
            ip_dis = os.path.join(a.disabled, a.frame, "ivar", rel)
            m = np.isfinite(s_en) & np.isfinite(s_dis) & (s_dis != 0)
            if m.sum() == 0:
                continue
            rs_all.append((s_en[m] / s_dis[m]))
            if os.path.exists(vp_en) and os.path.exists(vp_dis):
                v_en, v_dis = read(vp_en), read(vp_dis)
                mm = m & np.isfinite(v_en) & np.isfinite(v_dis) & (v_dis != 0)
                if mm.sum():
                    rv_all.append(v_en[mm] / v_dis[mm])
            if os.path.exists(ip_en) and os.path.exists(ip_dis):
                i_en, i_dis = read(ip_en), read(ip_dis)
                mi = m & np.isfinite(i_en) & np.isfinite(i_dis) & (i_dis != 0)
                if mi.sum():
                    ivar_all.append(i_en[mi] / i_dis[mi])
        if not rs_all:
            res["orders"][str(order)] = {"n_tiles_common": len(common), "note": "no overlapping data"}
            continue
        rs = np.concatenate(rs_all)
        rec = {
            "n_tiles_common": len(common),
            "n_pixels": int(rs.size),
            "r_signal": {"median": float(np.median(rs)), "min": float(rs.min()),
                         "max": float(rs.max()), "p16": float(np.percentile(rs, 16)),
                         "p84": float(np.percentile(rs, 84))},
            "k_photo": k_photo,
            "r_signal_over_kphoto_median": float(np.median(rs) / k_photo) if k_photo else None,
            "r_signal_spatial_spread_rel": float((np.percentile(rs, 84) - np.percentile(rs, 16)) /
                                                  (2.0 * np.median(rs))) if np.median(rs) else None,
        }
        if rv_all:
            rv = np.concatenate(rv_all)
            n = min(rs.size, rv.size)
            # 逐像元配对需同序；若尺寸相同则直接配对
            if rs.size == rv.size:
                rel_err = np.abs(rv - rs * rs) / np.maximum(np.abs(rs * rs), 1e-300)
                rec["r_variance"] = {"median": float(np.median(rv)), "min": float(rv.min()),
                                     "max": float(rv.max())}
                rec["variance_eq_signal_sq"] = {
                    "median_rel_err": float(np.median(rel_err)),
                    "p99_rel_err": float(np.percentile(rel_err, 99)),
                    "max_rel_err": float(rel_err.max()),
                }
            else:
                rec["variance_note"] = f"pixel count mismatch rs={rs.size} rv={rv.size}"
        if ivar_all:
            iv = np.concatenate(ivar_all)
            if iv.size == rs.size:
                # ivar 应为 1/variance ~ 1/r_s²
                inv = iv * rs * rs
                rec["ivar_eq_inverse_variance"] = {
                    "median": float(np.median(inv)), "p99_abs_dev": float(np.percentile(np.abs(inv - 1.0), 99))}
        res["orders"][str(order)] = rec

    json.dump(res, open(a.out, "w"), indent=2, ensure_ascii=False)
    print(json.dumps(res, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
