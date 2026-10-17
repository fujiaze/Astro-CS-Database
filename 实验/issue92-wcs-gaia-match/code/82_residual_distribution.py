#!/usr/bin/env python3
"""Issue #92 A1-FIX —— 残差门通过比例与像素误差分布 (修复前 vs 修复后)。

对每一帧的真实 Gaia 星表 (evidence/40-gaia-stars-<tag>.csv) 复刻生产 C++ 数值:
  · oneshot : 修复前分支 —— AP/BP 一次式 (按 sip_order 求值), 直接进 1e-7 px 门
  · refined : 修复后分支 —— AP/BP 作初值 + 同一有界不动点精化 (<=64 轮), 再过同一门
输出每帧: 门通过数/总数、残差分布 (min/median/p99/max)、与 Astropy 入帧对照。
"""
from __future__ import annotations

import argparse
import glob
import json
import math
import os

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EVID = os.path.join(REPO, "实验/issue92-wcs-gaia-match/evidence")
FIXTURES = os.path.join(REPO, "实验/issue92-wcs-gaia-match/fixtures")


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


def residuals(w, ra, dec, mode):
    sip = w["sip"]; order = int(sip["order"]); ap_order = int(sip.get("ap_order", 0))
    crpix1, crpix2 = float(w["crpix1"]), float(w["crpix2"])
    cd = np.array([[float(w["cd11"]), float(w["cd12"])],
                   [float(w["cd21"]), float(w["cd22"])]])
    A = np.array(sip["a"]); B = np.array(sip["b"])
    AP = np.array(sip["ap"]); BP = np.array(sip["bp"])
    det = cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0]
    inv = np.array([[cd[1, 1], -cd[0, 1]], [-cd[1, 0], cd[0, 0]]]) / det
    ra0, dec0 = math.radians(float(w["crval1"])), math.radians(float(w["crval2"]))
    s0, c0 = math.sin(dec0), math.cos(dec0)
    rr, dd = np.radians(ra), np.radians(dec)
    cosc = s0 * np.sin(dd) + c0 * np.cos(dd) * np.cos(rr - ra0)
    xi = np.degrees(np.cos(dd) * np.sin(rr - ra0) / cosc)
    eta = np.degrees((c0 * np.sin(dd) - s0 * np.cos(dd) * np.cos(rr - ra0)) / cosc)
    u = inv[0, 0] * xi + inv[0, 1] * eta
    v = inv[1, 0] * xi + inv[1, 1] * eta
    if mode == "oneshot":
        dx = u + poly(AP, u, v, order)
        dy = v + poly(BP, u, v, order)
    else:
        dx = u + poly(AP, u, v, order)   # 初值
        dy = v + poly(BP, u, v, order)
        for _ in range(64):
            f = poly(A, dx, dy, order); g = poly(B, dx, dy, order)
            nx, ny = u - f, v - g
            step = np.maximum(np.abs(nx - dx), np.abs(ny - dy))
            dx, dy = nx, ny
            if np.all(step <= 1e-10):
                break
    rx = np.abs(dx + poly(A, dx, dy, order) - u)
    ry = np.abs(dy + poly(B, dx, dy, order) - v)
    resid = np.maximum(rx, ry)          # 逐分量门
    return resid, dx + (crpix1 - 1.0), dy + (crpix2 - 1.0)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(EVID, "82-residual-distribution.json"))
    ap.add_argument("--gate", type=float, default=1e-7)
    a = ap.parse_args()
    frames = []
    for fx in sorted(glob.glob(os.path.join(FIXTURES, "*.json"))):
        tag = os.path.basename(fx)[len("M42_M1_T3_flying_dutchman-"):-len(".json")]
        csv = os.path.join(EVID, f"40-gaia-stars-{tag}.csv")
        if not os.path.exists(csv):
            print("missing", csv); continue
        w = json.load(open(fx))["wcs"]
        ra, dec = load_stars(csv)
        rec = {"tag": tag, "n": int(len(ra))}
        for mode in ("oneshot", "refined"):
            r, x, y = residuals(w, ra, dec, mode)
            fin = np.isfinite(r) & np.isfinite(x) & np.isfinite(y)
            inb = fin & (x >= 0) & (x < 4096) & (y >= 0) & (y < 4096)
            rec[mode] = {
                "gate_pass": int(np.sum(fin & (r <= a.gate))),
                "n_nan": int(np.sum(~fin)),
                "resid_min": float(np.nanmin(r)), "resid_median": float(np.nanmedian(r)),
                "resid_p99": float(np.nanpercentile(r, 99)),
                "resid_max": float(np.nanmax(r)),
                "in_frame": int(np.sum(inb)),
            }
        frames.append(rec)
    out = {"gate_tol_px": a.gate, "frames": frames}
    json.dump(out, open(a.out, "w"), indent=2, ensure_ascii=False)
    print(f"{'frame':<26} {'n':>5} | {'oneshot pass':>12} {'resid med':>11} {'resid max':>11} | "
          f"{'refined pass':>12} {'resid med':>11} {'resid max':>11} | {'in_frame':>8}")
    for f in frames:
        o, r = f["oneshot"], f["refined"]
        print(f"{f['tag']:<26} {f['n']:>5} | {o['gate_pass']:>12} {o['resid_median']:>11.3e} "
              f"{o['resid_max']:>11.3e} | {r['gate_pass']:>12} {r['resid_median']:>11.3e} "
              f"{r['resid_max']:>11.3e} | {r['in_frame']:>8}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
