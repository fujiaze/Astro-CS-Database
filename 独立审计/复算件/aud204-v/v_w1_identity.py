#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W1 独立复算（第②层）：面积"构造闭合"是不是代数恒等？错配能否骗过总量守恒型度量？

独立性：本文件不 import 仓库 Python、不读成稿的 t* 脚本、不抄它的 face 约定。
球面几何全部自己实现（Van Oosterom 球面三角形 + 扇形剖分 + 凸多边形半空间判定）；
chart（像素内等面积坐标）用第三方库 astropy_healpix 2.0.0 的
healpix_to_lonlat(ipix, dx, dy) / lonlat_to_healpix(..., return_offsets=True)
——它的 (dx,dy) 就是像素内 1x1 的等面积自然坐标，face 切换/缝/极点由库自己处理，
所以我的构造**不含**成稿那种"按顶点自然 face 逐 face 裁剪"的实现选择。

三段：
  A leaf 表示：4 角大圆弦四边形 vs 解析真值 4pi/npix —— 全天求和恒等 + 逐叶误差
  B 逐叶分配：drop 在多叶间的精确分配（offset 空间细采样），求和型闭合残差
  C 故意错配注入（索引错位 / 一叶面积 x2 邻居吸收 / 两叶互换）后，
    B 的求和型闭合是否照样归零，逐叶误差是否暴涨
"""
import json
import math
import os

import numpy as np
import astropy.units as u
from astropy_healpix import HEALPix, nside_to_pixel_area

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "w1_out.json")
PI = math.pi
R = {}


# ----------------------------------------------------------- 自己的球面几何件
def lonlat_to_xyz(lon_deg, lat_deg):
    lon = np.radians(np.asarray(lon_deg, float))
    lat = np.radians(np.asarray(lat_deg, float))
    cl = np.cos(lat)
    return np.stack([cl * np.cos(lon), cl * np.sin(lon), np.sin(lat)], axis=-1)


def tri_area(a, b, c):
    """Van Oosterom & Strackee 有向球面三角形面积（单位球，数组化）。"""
    cr = np.cross(b, c)
    det = np.einsum("...i,...i->...", a, cr)
    s = (1.0 + np.einsum("...i,...i->...", a, b)
         + np.einsum("...i,...i->...", b, c) + np.einsum("...i,...i->...", c, a))
    return 2.0 * np.arctan2(det, s)


def poly_area(pts):
    """扇形剖分有向累加；pts (...,n,3)。"""
    n = pts.shape[-2]
    tot = np.zeros(pts.shape[:-2])
    for i in range(1, n - 1):
        tot = tot + tri_area(pts[..., 0, :], pts[..., i, :], pts[..., i + 1, :])
    return tot


def gc_mid(p, q):
    m = p + q
    n = np.linalg.norm(m, axis=-1, keepdims=True)
    return m / np.where(n > 0, n, 1.0)


# ------------------------------------------------- astropy 像素内等面积 chart
def pix_corners(nside, ipix):
    """leaf 的 4 个角点（chart 方格角）→ 单位向量 (n,4,3)。"""
    hp = HEALPix(nside, order="nested", frame="icrs")
    n = len(ipix)
    dx = np.tile(np.array([0.0, 1.0, 1.0, 0.0]), n)
    dy = np.tile(np.array([0.0, 0.0, 1.0, 1.0]), n)
    lon, lat = hp.healpix_to_lonlat(np.repeat(ipix, 4), dx=dx, dy=dy)
    return lonlat_to_xyz(lon.deg, lat.deg).reshape(n, 4, 3)


def pix_edges_true_and_chord(nside, ipix, k=5):
    """逐边：真曲线采样点 vs 大圆弧；返回矢高（弧度）与 hp_res 归一。"""
    hp = HEALPix(nside, order="nested", frame="icrs")
    t = np.linspace(0.0, 1.0, k)
    n = len(ipix)
    out = []
    for e in range(4):
        a = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)][e]
        b = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)][(e + 1) % 4]
        dx = a[0] + (b[0] - a[0]) * t
        dy = a[1] + (b[1] - a[1]) * t
        lon, lat = hp.healpix_to_lonlat(np.repeat(ipix, k),
                                        dx=np.tile(dx, n), dy=np.tile(dy, n))
        p = lonlat_to_xyz(lon.deg, lat.deg).reshape(n, k, 3)
        # 该边两端点的**大圆弧**中点 vs 真曲线中点 -> 矢高
        mid_true = p[:, (k - 1) // 2, :]
        mid_arc = gc_mid(p[:, 0, :], p[:, -1, :])
        d = np.clip(np.einsum("ni,ni->n", mid_true, mid_arc), -1.0, 1.0)
        out.append(np.arccos(d))
    return np.stack(out, axis=-1)  # (n,4)


# ================================================= 实验 A：弦 leaf 的划分恒等
def exp_A(nsides=(16, 64, 128)):
    res = {}
    for N in nsides:
        npix = 12 * N * N
        a_true = float(nside_to_pixel_area(N).value)
        ipix = np.arange(npix, dtype=np.int64)
        tot, mn, mx = 0.0, 1e9, -1e9
        mn_at = mx_at = -1
        chunk = 200000
        sum_all = []
        rel_all = []
        for s in range(0, npix, chunk):
            e = min(npix, s + chunk)
            cor = pix_corners(N, ipix[s:e])
            A = np.abs(poly_area(cor))
            sum_all.append(A)
            rel_all.append(A / a_true - 1.0)
        A = np.concatenate(sum_all)
        rel = np.concatenate(rel_all)
        tot = float(A.sum())
        mn, mx = float(rel.min()), float(rel.max())
        mn_at = int(ipix[int(rel.argmin())])
        mx_at = int(ipix[int(rel.argmax())])
        hp = HEALPix(N, order="nested", frame="icrs")
        cl, ca = hp.healpix_to_lonlat(np.array([mn_at, mx_at]))
        res["N%d" % N] = {
            "npix": npix,
            "a_true_sr_astropy": a_true,
            "pi_over_3N2": (PI / 3.0) / (N * N),
            "sum_chord_over_4pi_minus_1": tot / (4.0 * PI) - 1.0,
            "sum_chord_minus_4pi": tot - 4.0 * PI,
            "leaf_rel_err_min": mn, "leaf_rel_err_min_at": [mn_at, float(ca.deg[0])],
            "leaf_rel_err_max": mx, "leaf_rel_err_max_at": [mx_at, float(ca.deg[1])],
            "n_abs_err_gt_1e-6": int((np.abs(rel) > 1e-6).sum()),
            "n_abs_err_gt_1e-4": int((np.abs(rel) > 1e-4).sum()),
            "median_abs_err": float(np.median(np.abs(rel))),
            "frac_gt_1e-6": float((np.abs(rel) > 1e-6).mean()),
        }
    # 矢高（逐边真曲线 vs 大圆弧），按 |z| 分档：缝 / 极冠 / 赤道
    N = 256
    hp = HEALPix(N, order="nested", frame="icrs")
    ipix = np.arange(12 * N * N, dtype=np.int64)
    cl, ca = hp.healpix_to_lonlat(ipix)
    z = np.sin(np.radians(ca.deg))
    cell_arcsec = float(np.sqrt(4 * PI / (12 * N * N)) * 206264.80624709636)
    hp_res_rad = math.sqrt(PI / 3.0) / N
    sel_all = {}
    bands_def = [("seam_abs_z_0.66_0.67", (np.abs(z) > 0.655) & (np.abs(z) < 0.675)),
                 ("polar_abs_z_gt_0.95", np.abs(z) > 0.95),
                 ("equator_abs_z_lt_0.2", np.abs(z) < 0.2),
                 ("all_random_60k", None)]
    rng = np.random.default_rng(20260925)
    rand = rng.choice(len(ipix), size=min(60000, len(ipix)), replace=False)
    band = {}
    for tag, sel in bands_def:
        idx = rand if sel is None else np.nonzero(sel)[0]
        if len(idx) == 0:
            continue
        sg = np.zeros((len(idx), 4))
        for a0 in range(0, len(idx), 20000):
            b0 = min(len(idx), a0 + 20000)
            sg[a0:b0] = pix_edges_true_and_chord(N, ipix[idx[a0:b0]], k=5)
        v = sg / hp_res_rad
        band[tag] = {"n_pix": int(len(idx)),
                     "sagitta_over_hpres_max": float(v.max()),
                     "sagitta_over_hpres_median": float(np.median(v)),
                     "sagitta_over_hpres_p99": float(np.percentile(v, 99)),
                     "frac_edges_gt_1e-6": float((v > 1e-6).mean())}
    hp_res_rad = math.sqrt(PI / 3.0) / N
    res["sagitta_nside256"] = {"hp_res_arcsec": cell_arcsec, "bands": band}
    # 等面积核对：像素内 |dOmega/d(dx dy)| 是否为常数 = a_true
    Ns = 32
    hps = HEALPix(Ns, order="nested", frame="icrs")
    at = float(nside_to_pixel_area(Ns).value)
    worst = 0.0
    for ip in [0, 7, 100, 500, 1000, 3000, 4000]:
        for (u0, v0) in [(0.5, 0.5), (0.31, 0.67), (0.7, 0.2), (0.15, 0.85)]:
            h = 1e-4
            lon, lat = hps.healpix_to_lonlat(np.array([ip] * 4),
                                              dx=np.array([u0 - h, u0 + h, u0, u0]),
                                              dy=np.array([v0, v0, v0 - h, v0 + h]))
            P = lonlat_to_xyz(lon.deg, lat.deg)
            du = (P[1] - P[0]) / (2 * h)
            dv = (P[3] - P[2]) / (2 * h)
            nrm = np.cross(du, dv)
            J = float(np.linalg.norm(nrm))  # |d xyz/dudv| 面积元 = |dOmega/dudv|
            worst = max(worst, abs(J / at - 1.0))
    res["equal_area_check_max_dev"] = worst
    R["A_chord_vs_true"] = res


# ============================== 实验 B/C：逐叶分配 + 错配注入 vs 求和型闭合
def make_drop(dec0, ra0, half_deg, seg):
    """球面小正方形 drop：中心 (ra0,dec0)，半宽 half_deg，每边 seg 段大圆弧。"""
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
    return np.stack(poly), ctr, 2.0 * hd


def point_in_convex_poly(points, poly):
    """凸球面多边形（顶点绕序）半空间判定；points (m,3), poly (n,3)。"""
    n = len(poly)
    ctr = poly.sum(axis=0)
    ctr = ctr / np.linalg.norm(ctr)
    out = np.ones(len(points), bool)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        nn = np.cross(a, b)
        ln = np.linalg.norm(nn)
        if ln < 1e-300:
            continue
        nn = nn / ln
        if np.dot(nn, ctr) < 0:
            nn = -nn
        out &= (points @ nn) >= -1e-16
    return out


def allocate(nside, poly, grid=61):
    """把 drop 分配到 HEALPix 叶：用 astropy 的像素内等面积 (dx,dy) 方格细采样。
    返回 dict ipix -> a_jp（球面度），以及候选叶数。"""
    hp = HEALPix(nside, order="nested", frame="icrs")
    ctr = poly.sum(axis=0) / len(poly)
    ctr = ctr / np.linalg.norm(ctr)
    rmax = max(float(np.arccos(np.clip(np.dot(p, ctr), -1, 1))) for p in poly)
    hp_res = math.sqrt(PI / 3.0) / nside
    cands = np.asarray(hp.cone_search_lonlat(hp_to_lon(ctr), hp_to_lat(ctr),
                                             radius=u.Quantity(rmax + 2.0 * hp_res,
                                                                u.rad)),
                       dtype=np.int64)
    cands = np.asarray(cands, dtype=np.int64)
    t = np.linspace(0.0 + 1.0 / (2 * grid), 1.0 - 1.0 / (2 * grid), grid)
    DX, DY = np.meshgrid(t, t)
    DX = DX.ravel(); DY = DY.ravel()
    ng = DX.size
    a = {}
    a_pix = float(nside_to_pixel_area(nside).value)
    for ip in cands:
        lon, lat = hp.healpix_to_lonlat(np.full(ng, ip, dtype=np.int64), dx=DX, dy=DY)
        P = lonlat_to_xyz(lon.deg, lat.deg)
        inside = point_in_convex_poly(P, poly)
        frac = float(inside.mean())
        if frac > 0.0:
            a[int(ip)] = frac * a_pix
    return a


def hp_to_lon(v):
    return u.Quantity(math.atan2(v[1], v[0]) % (2 * PI), u.rad)


def hp_to_lat(v):
    return u.Quantity(math.asin(max(-1.0, min(1.0, v[2]))), u.rad)


def poly_area_scalar(poly):
    return float(abs(poly_area(poly[np.newaxis, ...])[0]))


def exp_BC(nsides=(512, 2048)):
    """B: 真实逐叶分配 + 求和型闭合；C: 三种保总量错配注入。"""
    configs = {
        "equator_dec0": (0.0, 37.5),
        "seam_plus_2_3": (math.degrees(math.asin(2.0 / 3.0)), 210.0),
        "seam_minus_2_3": (math.degrees(math.asin(-2.0 / 3.0)), 100.0),
        "polar_dec89p9": (89.9, 0.0),
    }
    out = {}
    for N in nsides:
        hp_res_deg = math.degrees(math.sqrt(PI / 3.0) / N)
        for cname, (dec0, ra0) in configs.items():
            half = 1.5 * hp_res_deg          # drop 半宽 = 1.5 个叶宽
            seg = 24
            poly, ctr, _ = make_drop(dec0, ra0, half, seg)
            A_drop = poly_area_scalar(poly)
            a = allocate(N, poly, grid=61)
            if not a:
                out.setdefault("skip", []).append("%d/%s" % (N, cname))
                continue
            vec = a
            tot = sum(vec.values())
            base = {
                "N": N, "n_leaves": len(vec), "A_drop_sr": A_drop,
                "sum_closure_rel": abs(tot / A_drop - 1.0),
            }
            # C1 索引错位（把面积挪到下一个候选叶；面积多重集不变）
            keys = sorted(vec)
            c1 = {keys[(i + 1) % len(keys)]: vec[k] for i, k in enumerate(keys)}
            # C2 一叶 x2，其余按比例吸收（逐叶误差 ~100%，总量逐位不变）
            kk = max(keys, key=lambda k: vec[k])
            take = vec[kk]
            others = [k for k in keys if k != kk]
            so = sum(vec[k] for k in others)
            c2 = {k: vec[k] * (1.0 - take / so) for k in others}
            c2[kk] = vec[kk] + take
            # C3 相邻两叶互换（错分，总量不变）
            c3 = dict(vec)
            if len(keys) >= 2:
                p, q = keys[0], keys[1]
                c3[p], c3[q] = vec[q], vec[p]

            def rep(m):
                s = sum(m.values())
                perleaf = max(abs(m[k] / v - 1.0) for k, v in vec.items() if v > 0)
                return {"sum_rel_change_vs_base": abs(s / tot - 1.0),
                        "closure_rel": abs(s / A_drop - 1.0),
                        "max_perleaf_rel_err_vs_true": perleaf,
                        "sum_minus_base": s - tot}
            base["injections"] = {"C1_index_shift": rep(c1),
                                  "C2_double_one_leaf": rep(c2),
                                  "C3_swap_two_leaves": rep(c3)}
            out.setdefault("%s" % cname, {})[("N%d" % N)] = base
    R["B_alloc_and_C_injections"] = out


def main():
    exp_A()
    exp_BC()
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(R, fh, ensure_ascii=False, indent=1, default=list)
    print(json.dumps(R["A_chord_vs_true"], ensure_ascii=False, indent=1, default=list)[:4000])
    print("==== B/C ====")
    print(json.dumps(R["B_alloc_and_C_injections"], ensure_ascii=False, indent=1, default=list)[:6000])


if __name__ == "__main__":
    main()
