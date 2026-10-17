#!/usr/bin/env python3
"""Issue #92 —— 用独立 WCSLIB/Astropy 对 2309 颗真实 Gaia 星做端到端交叉核对。

目的: 不依赖 C++ 探针, 直接回答两个问题
  1) 投影本身是否有问题?  → astropy all_world2pix 的入帧数应与 C++ 不动点分支一致
  2) 1e-7 px 门是否必然全拒? → 复算生产一次式 AP/BP 分支的前向残差
输出 evidence/50-astropy-crosscheck.json。
"""
from __future__ import annotations

import argparse
import json
import math

import numpy as np
from astropy.wcs import Sip, WCS


def poly(c, dx, dy, o):
    acc = np.zeros_like(dx, dtype=float)
    for i in range(o + 1):
        for j in range(o - i + 1):
            acc = acc + c[i * 6 + j] * (dx ** i) * (dy ** j)
    return acc


def load_stars(path):
    ra, dec = [], []
    with open(path) as f:
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
    ap.add_argument("--cpp-in-frame", type=int, default=1085)
    a = ap.parse_args()

    d = json.load(open(a.p1wcs)); w = d["wcs"]; sip = w["sip"]
    order = int(sip["order"]); ap_order = int(sip["ap_order"])
    crpix = np.array([w["crpix1"], w["crpix2"]]); crval = np.array([w["crval1"], w["crval2"]])
    cd = np.array([[w["cd11"], w["cd12"]], [w["cd21"], w["cd22"]]])
    A = np.array(sip["a"]); B = np.array(sip["b"])
    AP = np.array(sip["ap"]); BP = np.array(sip["bp"])
    ra, dec = load_stars(a.stars)

    sh = (max(order, ap_order) + 1,) * 2
    A2 = np.zeros(sh); B2 = np.zeros(sh); AP2 = np.zeros(sh); BP2 = np.zeros(sh)
    for i in range(order + 1):
        for j in range(order - i + 1):
            A2[i, j] = A[i * 6 + j]; B2[i, j] = B[i * 6 + j]
    for i in range(ap_order + 1):
        for j in range(ap_order - i + 1):
            AP2[i, j] = AP[i * 6 + j]; BP2[i, j] = BP[i * 6 + j]
    aw = WCS(naxis=2); aw.wcs.crpix = crpix; aw.wcs.crval = crval; aw.wcs.cd = cd
    aw.wcs.ctype = [w.get("ctype1", "RA---TAN-SIP"), w.get("ctype2", "DEC--TAN-SIP")]
    aw.sip = Sip(A2, B2, AP2, BP2, crpix)
    x, y = aw.all_world2pix(ra, dec, 0)
    inb = (x >= 0) & (x < 4096) & (y >= 0) & (y < 4096)

    ra0, dec0 = math.radians(crval[0]), math.radians(crval[1])
    s0, c0 = math.sin(dec0), math.cos(dec0)
    rr, dd = np.radians(ra), np.radians(dec)
    cosc = s0 * np.sin(dd) + c0 * np.cos(dd) * np.cos(rr - ra0)
    xi = np.degrees(np.cos(dd) * np.sin(rr - ra0) / cosc)
    eta = np.degrees((c0 * np.sin(dd) - s0 * np.cos(dd) * np.cos(rr - ra0)) / cosc)
    det = cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0]
    inv = np.array([[cd[1, 1], -cd[0, 1]], [-cd[1, 0], cd[0, 0]]]) / det
    u = inv[0, 0] * xi + inv[0, 1] * eta
    v = inv[1, 0] * xi + inv[1, 1] * eta
    dx = u + poly(AP, u, v, order); dy = v + poly(BP, u, v, order)
    resid = np.hypot(dx + poly(A, dx, dy, order) - u, dy + poly(B, dx, dy, order) - v)

    out = {
        "p1_wcs": a.p1wcs, "stars": a.stars, "n_stars": int(len(ra)),
        "astropy_in_frame": int(inb.sum()),
        "cpp_iterative_in_frame": a.cpp_in_frame,
        "in_frame_agrees": int(inb.sum()) == a.cpp_in_frame,
        "oneshot_production_residual": {
            "gate_tol_px": 1e-7,
            "n_exceed": int((resid > 1e-7).sum()),
            "n": int(len(resid)),
            "min_px": float(resid.min()), "max_px": float(resid.max()),
            "median_px": float(np.median(resid)),
        },
    }
    json.dump(out, open(a.out, "w"), indent=2, ensure_ascii=False)
    print(json.dumps(out, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
