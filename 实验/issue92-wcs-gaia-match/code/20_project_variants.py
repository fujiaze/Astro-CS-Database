#!/usr/bin/env python3
"""Issue #92 —— 在 2309 颗真实 Gaia 星上量化「首个错误条件」与候选修法。

复刻生产 `WcsTransform::skyToPixel`（含 1e-7 px 前向残差门）的**同一批**数值，
只切换被怀疑的变量:
  production_*        : C++ 原样 (AP/BP 用 m_sip_order=sip_order 求值, 不迭代)
  ap0_zeroed_*        : 把 AP_0_0/BP_0_0 置零后再一次性应用
  ap_order5_*         : AP/BP 按其真实 ap_order 求值
  iterative_*         : 走生产的无 AP/BP 不动点分支 (64 轮)
  seeded_iterative_*  : 用 AP/BP 作初值再不动点精化 (候选修复)
输出 evidence/20-project-variants.json。
"""
from __future__ import annotations

import argparse
import json
import math

import numpy as np


def sip_poly(coef, dx, dy, order):
    acc = np.zeros_like(dx, dtype=float)
    for i in range(order + 1):
        for j in range(order - i + 1):
            acc = acc + coef[i * 6 + j] * (dx ** i) * (dy ** j)
    return acc


def load_stars(csv_path):
    ra, dec = [], []
    with open(csv_path) as f:
        for line in f:
            if line.startswith("#") or line.startswith("ra,"):
                continue
            p = line.split(",")
            if len(p) >= 2:
                ra.append(float(p[0])); dec.append(float(p[1]))
    return np.array(ra), np.array(dec)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--p1wcs", required=True)
    ap.add_argument("--stars", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--w", type=int, default=4096)
    ap.add_argument("--h", type=int, default=4096)
    ap.add_argument("--gate", type=float, default=1e-7)
    a = ap.parse_args()

    d = json.load(open(a.p1wcs)); w = d["wcs"]; sip = w["sip"]
    order = int(sip["order"]); ap_order = int(sip.get("ap_order", 0))
    crpix1, crpix2 = float(w["crpix1"]), float(w["crpix2"])
    crval1, crval2 = float(w["crval1"]), float(w["crval2"])
    cd = np.array([[float(w["cd11"]), float(w["cd12"])],
                   [float(w["cd21"]), float(w["cd22"])]])
    A = np.array(sip["a"]); B = np.array(sip["b"])
    AP = np.array(sip["ap"]); BP = np.array(sip["bp"])
    ra, dec = load_stars(a.stars)

    det = cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0]
    inv = np.array([[cd[1, 1], -cd[0, 1]], [-cd[1, 0], cd[0, 0]]]) / det

    ra_r, dec_r = np.radians(ra), np.radians(dec)
    ra0, dec0 = math.radians(crval1), math.radians(crval2)
    s0, c0 = math.sin(dec0), math.cos(dec0)
    sd, cdd = np.sin(dec_r), np.cos(dec_r)
    cosc = s0 * sd + c0 * cdd * np.cos(ra_r - ra0)
    xi = np.degrees(cdd * np.sin(ra_r - ra0) / cosc)
    eta = np.degrees((c0 * sd - s0 * cdd * np.cos(ra_r - ra0)) / cosc)
    u = inv[0, 0] * xi + inv[0, 1] * eta
    v = inv[1, 0] * xi + inv[1, 1] * eta

    def run(kind):
        APu, BPu = AP.copy(), BP.copy()
        if kind.startswith("apbp_allzero"):
            APu[:] = 0.0; BPu[:] = 0.0
        elif kind.startswith("ap0_zeroed"):
            APu[0] = 0.0; BPu[0] = 0.0
        pol_order = ap_order if "order5" in kind else order
        if kind.startswith(("production", "ap0_zeroed", "ap_order5", "apbp_allzero")):
            dx = u + sip_poly(APu, u, v, pol_order)
            dy = v + sip_poly(BPu, u, v, pol_order)
            polished = False
        else:  # iterative / seeded_iterative
            if kind.startswith("seeded"):
                dx = u + sip_poly(APu, u, v, pol_order)
                dy = v + sip_poly(BPu, u, v, pol_order)
            else:
                dx, dy = u.copy(), v.copy()
            polished = True
            for _ in range(64):
                f = sip_poly(A, dx, dy, order)
                g = sip_poly(B, dx, dy, order)
                nx, ny = u - f, v - g
                step = np.maximum(np.abs(nx - dx), np.abs(ny - dy))
                dx, dy = nx, ny
                if np.all(step <= 1e-10):
                    break
            # 若开放了精化, 以精化结果为准
        check_x = dx + sip_poly(A, dx, dy, order)
        check_y = dy + sip_poly(B, dx, dy, order)
        resid = np.hypot(check_x - u, check_y - v)
        ok = np.isfinite(resid) & (resid <= a.gate)
        x = dx + (crpix1 - 1.0)
        y = dy + (crpix2 - 1.0)
        in_frame = ok & (x >= 0) & (x < a.w) & (y >= 0) & (y < a.h)
        return {
            "gate_pass": int(np.sum(ok)),
            "n_nan_or_reject": int(len(ok) - np.sum(ok)),
            "n_in_frame": int(np.sum(in_frame)),
            "resid_max_px": float(np.nanmax(np.where(np.isfinite(resid), resid, np.nan))),
            "resid_median_px": float(np.nanmedian(resid)),
            "polished": polished,
            "ap_eval_order": pol_order,
            "ap0": float(APu[0]),
        }

    kinds = ["production_ap_order3", "ap0_zeroed_ap_order3", "apbp_allzero_ap_order3",
             "ap_order5", "ap0_zeroed_ap_order5",
             "iterative", "seeded_iterative_ap_order3", "seeded_iterative_ap_order5"]
    out = {"p1_wcs": a.p1wcs, "stars": a.stars, "n_stars": int(len(ra)),
           "gate_tol_px": a.gate, "frame": [a.w, a.h],
           "variants": {k: run(k) for k in kinds}}
    json.dump(out, open(a.out, "w"), indent=2, ensure_ascii=False)
    for k, v in out["variants"].items():
        print(f"{k:34s} gate_pass={v['gate_pass']:5d} in_frame={v['n_in_frame']:5d} "
              f"resid_max={v['resid_max_px']:.4f} resid_med={v['resid_median_px']:.2e} "
              f"ap_eval_order={v['ap_eval_order']} ap0={v['ap0']:.2e}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
