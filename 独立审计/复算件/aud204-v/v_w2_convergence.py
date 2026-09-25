#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W2 独立复算：|z|=2/3（u+v=1）缝位形上"逐叶误差不随 drop 细分收敛"的平台是真的吗？

与成稿不同的三点（避免复读）：
 1) 不做 face 切分、不做 chart 正/逆映射、不做"按顶点自然 face 逐 face 裁剪"——
    我在**球面**上直接算 a_jp = area(drop ∩ leaf)，leaf 边界用第三方 astropy_healpix
    的像素内等面积偏移坐标 healpix_to_lonlat(ipix, dx, dy) 采样真曲线（K 段/边），
    drop 用球面大圆弧细分折线；两多边形都凸 => 球面 S-H 半空间裁剪 + Van Oosterom
    扇形面积 = 精确交叠面积。成稿那种"跨 face 解析延拓缺失"的构造性误差在本实现里
    **不存在**，所以若我这里仍出现平台，平台就是几何的；若收敛，平台是它的构造产物。
 2) nside 与位形不同源：N=2048 与 8192（成稿 32768），face/赤纬组合不同，并加
    **近缝不跨缝对照**（drop 上缘距缝 0.75·hp_res）与南缝（face 8-11 支）对照。
 3) 真值 = Richardson(seg=256,512) 外推；同时把误差**拆开**报：
    (a) drop 侧折线误差（本平台问题的主体）
    (b) leaf 侧表示误差（弦四边形 vs 真曲线）——这条与细分无关，天生是平台。
"""
import json
import math
import os

import numpy as np
import astropy.units as u
from astropy_healpix import HEALPix, nside_to_pixel_area

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "w2_out.json")
PI = math.pi
R = {}


def lonlat_to_xyz(lon_deg, lat_deg):
    lon = np.radians(np.asarray(lon_deg, float))
    lat = np.radians(np.asarray(lat_deg, float))
    cl = np.cos(lat)
    return np.stack([cl * np.cos(lon), cl * np.sin(lon), np.sin(lat)], axis=-1)


def tri_area(a, b, c):
    det = np.einsum("...i,...i->...", a, np.cross(b, c))
    s = (1.0 + np.einsum("...i,...i->...", a, b)
         + np.einsum("...i,...i->...", b, c) + np.einsum("...i,...i->...", c, a))
    return 2.0 * np.arctan2(det, s)


def poly_area(pts):
    n = pts.shape[-2]
    tot = 0.0
    for i in range(1, n - 1):
        tot = tot + tri_area(pts[0], pts[i], pts[i + 1])
    return float(abs(tot))


def order_ccw(poly, ref):
    """把小多边形顶点按绕 ref（单位球心方向）的逆时针重排。"""
    ctr = ref / np.linalg.norm(ref)
    t = poly - np.dot(poly, ctr)[:, None] * ctr
    tmp = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(ctr, tmp))) > 0.9:
        tmp = np.array([1.0, 0.0, 0.0])
    e1 = np.cross(ctr, tmp)
    e1 = e1 / np.linalg.norm(e1)
    e2 = np.cross(ctr, e1)
    ang = np.arctan2(t @ e2, t @ e1)
    return poly[np.argsort(ang)]


def sh_clip_sphere(subject, clip):
    """凸球面多边形裁剪：subject 用 clip 的半空间逐边裁。两表同为绕序。"""
    out = subject
    m = len(clip)
    c = clip.sum(axis=0)
    c = c / np.linalg.norm(c)
    for i in range(m):
        a, b = clip[i], clip[(i + 1) % m]
        n = np.cross(a, b)
        ln = np.linalg.norm(n)
        if ln < 1e-300:
            continue
        n = n / ln
        if np.dot(n, c) < 0:
            n = -n
        if len(out) == 0:
            return out
        d = out @ n
        keep = d >= -1e-16
        if keep.all():
            continue
        if not keep.any():
            return np.zeros((0, 3))
        nxt = []
        L = len(out)
        for j in range(L):
            cur, prev = out[j], out[j - 1]
            dc, dp = float(cur @ n), float(prev @ n)
            if dc >= -1e-16:
                if dp < -1e-16:
                    t = dp / (dp - dc)
                    p = prev + t * (cur - prev)
                    nxt.append(p / np.linalg.norm(p))
                nxt.append(cur)
            elif dp >= -1e-16:
                t = dp / (dp - dc)
                p = prev + t * (cur - prev)
                nxt.append(p / np.linalg.norm(p))
        out = np.array(nxt) if nxt else np.zeros((0, 3))
    return out


def leaf_boundary(nside, ipix, K):
    """leaf 真曲线边界：像素内等面积偏移方格每边 K 段（含角点）。"""
    hp = HEALPix(nside, order="nested", frame="icrs")
    t = np.linspace(0.0, 1.0, K + 1)
    corners = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    xs, ys = [], []
    for e in range(4):
        a = corners[e]
        b = corners[(e + 1) % 4]
        for k in range(K):     # 不含末点，避免重复
            f = k / K
            xs.append(a[0] + (b[0] - a[0]) * f)
            ys.append(a[1] + (b[1] - a[1]) * f)
    lon, lat = hp.healpix_to_lonlat(np.full(len(xs), ipix, dtype=np.int64),
                                    dx=np.array(xs), dy=np.array(ys))
    return lonlat_to_xyz(lon.deg, lat.deg)


def leaf_chord(nside, ipix):
    hp = HEALPix(nside, order="nested", frame="icrs")
    lon, lat = hp.healpix_to_lonlat(np.full(4, ipix, dtype=np.int64),
                                    dx=np.array([0.0, 1.0, 1.0, 0.0]),
                                    dy=np.array([0.0, 0.0, 1.0, 1.0]))
    return lonlat_to_xyz(lon.deg, lat.deg)


def make_drop(dec0, ra0, half_deg, seg):
    ra0r, dec0r = math.radians(ra0), math.radians(dec0)
    ctr = lonlat_to_xyz([ra0], [dec0])[0]
    e_ra = np.array([-math.sin(ra0r), math.cos(ra0r), 0.0])
    e_dec = np.array([-math.sin(dec0r) * math.cos(ra0r),
                      -math.sin(dec0r) * math.sin(ra0r), math.cos(dec0r)])
    hd = math.radians(half_deg)

    def rot(vec, ang, axis):
        cc, ss = math.cos(ang), math.sin(ang)
        return (vec * cc + np.cross(axis, vec) * ss
                + axis * np.dot(axis, vec) * (1 - cc))

    corners = []
    for sa, sb in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
        p = rot(rot(ctr, sb * hd, e_ra), sa * hd, e_dec)
        corners.append(p / np.linalg.norm(p))
    poly = []
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        w = math.acos(max(-1.0, min(1.0, float(np.dot(a, b)))))
        for t in range(seg):
            f = t / seg
            p = ((math.sin((1 - f) * w) * a + math.sin(f * w) * b) / math.sin(w)
                 if w > 1e-14 else a * (1 - f) + b * f)
            poly.append(p / np.linalg.norm(p))
    return order_ccw(np.stack(poly), ctr), ctr


def run_case(nside, dec0, ra0, hw_deg, segs, K=24):
    hp = HEALPix(nside, order="nested", frame="icrs")
    hp_res = math.sqrt(PI / 3.0) / nside
    a_pix = float(nside_to_pixel_area(nside).value)
    poly_ref, ctr = make_drop(dec0, ra0, hw_deg, 512)
    rmax = max(float(np.arccos(np.clip(np.dot(p, ctr), -1, 1))) for p in poly_ref)
    lon_c = u.Quantity(math.atan2(ctr[1], ctr[0]) % (2 * PI), u.rad)
    lat_c = u.Quantity(math.asin(max(-1.0, min(1.0, ctr[2]))), u.rad)
    cands = np.asarray(hp.cone_search_lonlat(lon_c, lat_c,
                                             radius=u.Quantity(rmax + 2.5 * hp_res, u.rad)),
                       dtype=np.int64)
    leaves = {}
    for ip in cands:
        lb = leaf_boundary(nside, int(ip), K)
        if len(lb) < 4:
            continue
        A = poly_area(sh_clip_sphere(poly_ref, lb))
        if A > 1e-8 * a_pix:
            leaves[int(ip)] = (lb, A)
    # 真值：seg=256 与 512 的 Richardson（弦误差 ~ seg^-2）
    truth = {}
    for ip, (lb, _) in leaves.items():
        a256 = poly_area(sh_clip_sphere(make_drop(dec0, ra0, hw_deg, 256)[0], lb))
        a512 = poly_area(sh_clip_sphere(poly_ref, lb))
        truth[ip] = (4.0 * a512 - a256) / 3.0
    rows = {}
    A_ref = sum(truth.values())
    for seg in segs:
        poly, _ = make_drop(dec0, ra0, hw_deg, seg)
        a = {}
        for ip, (lb, _) in leaves.items():
            a[ip] = poly_area(sh_clip_sphere(poly, lb))
        tot = sum(a.values())
        errs = [(abs(a[ip] / truth[ip] - 1.0)) for ip in a
                if truth[ip] > 1e-3 * A_ref]
        mis = sum(abs(a[ip] - truth[ip]) for ip in a) / (2.0 * A_ref)
        rows["seg%d" % seg] = {
            "max_perleaf_rel_err": max(errs) if errs else 0.0,
            "median_perleaf_rel_err": float(np.median(errs)) if errs else 0.0,
            "misallocated_flux_fraction": mis,
            "sum_minus_truth_rel": abs(tot / A_ref - 1.0),
            "A_drop_vs_truth": abs(tot / poly_area(poly) - 1.0),
            "n_leaves": len(a),
        }
    # leaf 侧表示误差（与细分无关）：弦四边形 vs 真曲线(K) vs 解析 4pi/npix
    lv = []
    for ip, (lb, _) in leaves.items():
        At = poly_area(lb)
        Ac = poly_area(leaf_chord(nside, ip))
        lv.append((Ac / At - 1.0, Ac / a_pix - 1.0, At / a_pix - 1.0))
    rows["_leaf_representation"] = {
        "K_per_edge": K,
        "chord_vs_truecurve_min": min(x[0] for x in lv),
        "chord_vs_truecurve_max": max(x[0] for x in lv),
        "chord_vs_analytic_min": min(x[1] for x in lv),
        "chord_vs_analytic_max": max(x[1] for x in lv),
        "truecurveK_vs_analytic_max_abs": max(abs(x[2]) for x in lv),
    }
    return rows


def main():
    SEG = [1, 2, 4, 8, 16, 32, 64]
    for N in (2048, 8192):
        hp_res_deg = math.degrees(math.sqrt(PI / 3.0) / N)
        hw = 1.5 * hp_res_deg
        seam_dec = math.degrees(math.asin(2.0 / 3.0))
        cfgs = {
            "equator_dec0_ra37p5": (0.0, 37.5),
            "seam_cross_plus2_3": (seam_dec, 210.0),
            "near_seam_not_crossing": (seam_dec - 3.0 * hw, 210.0),
            "seam_cross_minus2_3": (-seam_dec, 100.0),
            "polar_dec89p95": (89.95, 0.0),
        }
        for tag, (dec0, ra0) in cfgs.items():
            if N == 8192 and tag not in ("seam_cross_plus2_3", "near_seam_not_crossing",
                                         "equator_dec0_ra37p5"):
                continue
            r = run_case(N, dec0, ra0, hw, SEG)
            r["_meta"] = {"nside": N, "dec0": dec0, "ra0": ra0,
                          "half_width_deg": hw, "hp_res_arcsec": hp_res_deg * 3600.0,
                          "dist_to_seam_deg": dec0 - seam_dec}
            R.setdefault("N%d" % N, {})[tag] = r
            print("N%d %s" % (N, tag),
                  " ".join("%s:%.2e" % (k, v["max_perleaf_rel_err"])
                           for k, v in r.items() if k.startswith("seg")),
                  "| leafchord %.3e..%.3e" % (r["_leaf_representation"]["chord_vs_truecurve_min"],
                                              r["_leaf_representation"]["chord_vs_truecurve_max"]))
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(R, fh, ensure_ascii=False, indent=1)


if __name__ == "__main__":
    main()
