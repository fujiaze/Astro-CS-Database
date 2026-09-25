# -*- coding: utf-8 -*-
"""
AUD-204 / T1 : 面坐标卡等面积性 + leaf 轴对齐 + 弦四边形 vs 真边界面积
只读独立复算，不 import 仓库内 Python（图表函数按 polar_common.h 逐式重写）。
对照真值源：astropy_healpix 2.0.0 (BSD-3-Clause) lonlat_to_healpix / nside_to_pixel_area。

判据：
 T1-A  |dOmega/dudv| == pi/3 逐点（有限差分）
 T1-B  chart 方格 -> astropy 像素 双射（leaf 在 chart 内轴对齐 + chart=标准 HEALPix 布局）
 T1-C  真曲线叶面积 == pi/(3 N^2) == astropy nside_to_pixel_area
 T1-D  生产 leaf 表示（4 角大圆弦四边形）面积 / 真叶面积 的全天偏差
 T1-E  弦四边形面积求和 == 4pi（铺满恒等 => 闭合判据对 T1-D 失明）
"""
import json
import math
import sys

import numpy as np
import astropy.units as u
from astropy_healpix import healpix_to_lonlat, nside_to_pixel_area
from astropy_healpix import lonlat_to_healpix

PI = math.pi
K_JAC = PI / 3.0
TWO_THIRD = 2.0 / 3.0

# ---------------------------------------------------------------- chart 正映射
def chart_uv_to_xyz(face, u, v):
    """polar_common.h::chart_uv_to_ang 的向量版；st 用 sqrt(1-z^2)（无 acos 相消）。"""
    u = np.asarray(u, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    z = np.empty_like(u)
    phi = np.empty_like(u)
    if face <= 3:
        eq = (u + v) <= 1.0
        z = np.where(eq, TWO_THIRD * (u + v), 0.0)
        s = np.where(eq, 1.0, 2.0 - u - v)          # s>0 仅在极冠支
        zp = 1.0 - s * s / 3.0
        z = np.where(eq, z, zp)
        phieq = 0.25 * PI * (u - v + 1.0 + 2.0 * face)
        with np.errstate(divide="ignore", invalid="ignore"):
            phipol = 0.5 * PI * face + PI * (1.0 - v) / (2.0 * s)
        phipol = np.where(s > 0.0, phipol, 0.5 * PI * face)
        phi = np.where(eq, phieq, phipol)
    elif face <= 7:
        z = TWO_THIRD * (u + v - 1.0)
        phi = 0.25 * PI * (u - v + 2.0 * (face - 4))
    else:
        eq = (u + v) >= 1.0
        z = np.where(eq, TWO_THIRD * (u + v - 2.0), 0.0)
        s = np.where(eq, 1.0, u + v)
        zs = -(1.0 - s * s / 3.0)
        z = np.where(eq, z, zs)
        phieq = 0.25 * PI * (u - v + 1.0 + 2.0 * (face - 8))
        with np.errstate(divide="ignore", invalid="ignore"):
            phipol = 0.5 * PI * (face - 8) + PI * u / (2.0 * s)
        phipol = np.where(s > 0.0, phipol, 0.5 * PI * (face - 8))
        phi = np.where(eq, phieq, phipol)
    phi = np.mod(phi, 2.0 * PI)
    st = np.sqrt(np.clip(1.0 - z * z, 0.0, None))
    return np.stack([st * np.cos(phi), st * np.sin(phi), z], axis=-1)


def uv_to_radec(face, u, v):
    r = chart_uv_to_xyz(face, u, v)
    z = np.clip(r[..., 2], -1.0, 1.0)
    dec = np.degrees(np.arcsin(z))
    ra = np.degrees(np.arctan2(r[..., 1], r[..., 0])) % 360.0
    return ra, dec


# ---------------------------------------------------------------- 球面多边形面积
def _tri_area(a, b, c):
    """Van Oosterom & Strackee 有向立体角（单位向量）。"""
    det = (a[..., 0] * (b[..., 1] * c[..., 2] - b[..., 2] * c[..., 1])
           + a[..., 1] * (b[..., 2] * c[..., 0] - b[..., 0] * c[..., 2])
           + a[..., 2] * (b[..., 0] * c[..., 1] - b[..., 1] * c[..., 0]))
    den = (1.0 + np.sum(a * b, axis=-1) + np.sum(b * c, axis=-1) + np.sum(c * a, axis=-1))
    return 2.0 * np.arctan2(det, den)


def poly_area(pts):
    """pts: (..., n, 3) 单位向量多边形 -> 面积（扇形剖分，取绝对值，>2pi 补）。"""
    a = pts[..., 0, :]
    tot = np.zeros(pts.shape[:-2])
    for i in range(1, pts.shape[-2] - 1):
        tot = tot + _tri_area(a, pts[..., i, :], pts[..., i + 1, :])
    tot = np.abs(tot)
    return np.where(tot > 2.0 * PI, 4.0 * PI - tot, tot)


def cell_corner_xyz(face, Ns, i, j):
    """角序与 leaf_corner_uv 一致：(i,j)->(i+1,j)->(i+1,j+1)->(i,j+1)。"""
    a, b = i / Ns, j / Ns
    c, d = (i + 1) / Ns, (j + 1) / Ns
    uv = np.array([[a, b], [c, b], [c, d], [a, d]])
    return np.stack([chart_uv_to_xyz(face, np.array([p[0]]), np.array([p[1]]))[0] for p in uv])


def curve_boundary(face, Ns, i, j, K):
    """真曲线边界：每边 K 段，点严格落在 chart 映射的像上（与 leaf_boundary_curve_f 同构）。"""
    a, b = i / Ns, j / Ns
    c, d = (i + 1) / Ns, (j + 1) / Ns
    corners = [(a, b), (c, b), (c, d), (a, d)]
    pts = []
    t = (np.arange(K) / K)
    for e in range(4):
        u0, v0 = corners[e]
        u1, v1 = corners[(e + 1) % 4]
        uu = u0 + t * (u1 - u0)
        vv = v0 + t * (v1 - v0)
        pts.append(chart_uv_to_xyz(face, uu, vv))
    return np.concatenate(pts, axis=0)


# ---------------------------------------------------------------- T1-A Jacobian
def jac_pointwise(h=1e-5):
    out = {"h": h, "worst_rel": 0.0, "worst_at": None, "n_sampled": 0,
           "pole_region_rel": None}
    worst = (0.0, None)
    n = 0
    for f in range(12):
        g = np.linspace(1e-4, 1 - 1e-4, 61)
        U, V = np.meshgrid(g, g, indexing="ij")
        Uf, Vf = U.ravel(), V.ravel()
        du = np.stack([chart_uv_to_xyz(f, Uf + e, Vf) for e in (h, -h)])
        dv = np.stack([chart_uv_to_xyz(f, Uf, Vf + e) for e in (h, -h)])
        ru = (du[0] - du[1]) / (2 * h)
        rv = (dv[0] - dv[1]) / (2 * h)
        J = np.linalg.norm(np.cross(ru, rv), axis=-1)
        rel = np.abs(J / K_JAC - 1.0)
        k = int(np.argmax(rel))
        if rel[k] > worst[0]:
            worst = (float(rel[k]), [f, float(Uf[k]), float(Vf[k]), float(J[k])])
        n += rel.size
    out["worst_rel"], out["worst_at"], out["n_sampled"] = worst[0], worst[1], n
    # 解析：两支各自 det(d(z,phi)/d(u,v)) = pi/3（脚本注释给推导）
    return out


# ---------------------------------------------------------------- T1-B 双射
def bijection(Ns, nsub=3, seed=20240918):
    ids = {}
    conflict = 0
    checked = 0
    max_off_err = [0.0]
    for f in range(12):
        for i in range(Ns):
            for j in range(Ns):
                t = (np.arange(nsub) + 0.5) / nsub
                UU, VV = np.meshgrid((i + t) / Ns, (j + t) / Ns, indexing="ij")
                ra, dec = uv_to_radec(f, UU.ravel(), VV.ravel())
                ip, dx, dy = lonlat_to_healpix(np.radians(ra) * u.rad, np.radians(dec) * u.rad, Ns,
                                               order="nested", return_offsets=True)
                s = set(int(x) for x in ip)
                checked += 1
                if len(s) != 1:
                    conflict += 1
                else:
                    ids[s.pop()] = (f, i, j)
                # 独立对照：astropy 的像素内偏移 (dx,dy) 应与本 chart 的
                # 格内归一坐标 (u*Ns-i, v*Ns-j) 一致（进一步证轴对齐+线性）
                off = np.stack([dx * Ns - i, dy * Ns - j], axis=-1)
                want = np.stack([(UU.ravel() * Ns - i), (VV.ravel() * Ns - j)], axis=-1)
                d = np.abs(off - want).max()
                max_off_err[0] = max(max_off_err[0], float(d))
    return {"nside": Ns, "cells": checked, "cells_straddling_pixels": conflict,
            "distinct_pixels": len(ids), "expected_pixels": int(12 * Ns * Ns),
            "max_abs_offset_err_vs_astropy_pixunits": max_off_err[0]}


# ---------------------------------------------------------------- T1-C/D/E
def leaf_areas(Ns, K=32):
    true_a = np.pi / (3.0 * Ns * Ns)          # 解析（Gorski 等面积）
    ref_astropy = float(nside_to_pixel_area(Ns).value)
    worst_chord = (0.0, None)
    best_chord = (1e300, None)
    sum_chord = 0.0
    curve_max_err = 0.0
    samples = []
    rng = np.random.default_rng(7)
    pick = []
    # 全域抽样 + 强制覆盖：极点邻叶、face 角点、|z|=2/3 折线、赤道一般位置
    for f in range(12):
        for (i, j) in [(0, 0), (Ns - 1, Ns - 1), (0, Ns - 1), (Ns - 1, 0),
                       (Ns // 2, Ns // 2), (Ns // 2, Ns // 2 + 1)]:
            pick.append((f, i, j))
    for _ in range(400):
        pick.append((int(rng.integers(0, 12)), int(rng.integers(0, Ns)),
                     int(rng.integers(0, Ns))))
    for (f, i, j) in pick:
        if not (0 <= i < Ns and 0 <= j < Ns):
            continue
        cor = cell_corner_xyz(f, Ns, i, j)
        ach = float(poly_area(cor[None, ...])[0])
        cb = curve_boundary(f, Ns, i, j, K)
        acurve = float(poly_area(cb.reshape(1, cb.shape[0], 3))[0])
        cb2 = curve_boundary(f, Ns, i, j, 2 * K)
        acurve2 = float(poly_area(cb2.reshape(1, cb2.shape[0], 3))[0])
        aext = (4 * acurve2 - acurve) / 3.0
        curve_max_err = max(curve_max_err, abs(aext / true_a - 1.0))
        rel = ach / true_a - 1.0
        if rel > worst_chord[0]:
            worst_chord = (rel, (f, i, j, ach, aext))
        if rel < best_chord[0]:
            best_chord = (rel, (f, i, j, ach, aext))
    # 全格点求和（用同一 4 角弦四边形，向量化）
    for f in range(12):
        ii = np.arange(Ns, dtype=np.float64)
        U0, V0 = np.meshgrid(ii / Ns, ii / Ns, indexing="ij")
        a11 = chart_uv_to_xyz(f, U0.ravel(), V0.ravel())
        a21 = chart_uv_to_xyz(f, U0.ravel() + 1.0 / Ns, V0.ravel())
        a22 = chart_uv_to_xyz(f, U0.ravel() + 1.0 / Ns, V0.ravel() + 1.0 / Ns)
        a12 = chart_uv_to_xyz(f, U0.ravel(), V0.ravel() + 1.0 / Ns)
        quad = np.stack([a11, a21, a22, a12], axis=1)
        Aq = poly_area(quad)
        sum_chord += float(Aq.sum())
        rel = Aq / true_a - 1.0
        k = int(np.argmax(rel))
        if rel[k] > worst_chord[0]:
            worst_chord = (float(rel[k]), (f, int(k % Ns), int(k // Ns),
                                           float(Aq[k]), None))
        k2 = int(np.argmin(rel))
        if rel[k2] < best_chord[0]:
            best_chord = (float(rel[k2]), (f, int(k2 % Ns), int(k2 // Ns),
                                           float(Aq[k2]), None))
    return {"nside": Ns, "area_analytic_pi_over_3N2": true_a,
            "area_astropy_nside_to_pixel_area": ref_astropy,
            "analytic_vs_astropy_rel": true_a / ref_astropy - 1.0,
            "curve_boundary_max_rel_err_vs_analytic": curve_max_err,
            "chordquad_rel_err_max": worst_chord[0], "chordquad_max_at": worst_chord[1],
            "chordquad_rel_err_min": best_chord[0], "chordquad_min_at": best_chord[1],
            "sum_chordquad_all_faces": sum_chord, "sum_minus_4pi": sum_chord - 4 * PI,
            "n_cells_full_sweep": int(12 * Ns * Ns)}


if __name__ == "__main__":
    res = {}
    res["T1A_jacobian_pointwise"] = jac_pointwise()
    res["T1B_bijection_nside16"] = bijection(16, nsub=3)
    res["T1CD_nside64"] = leaf_areas(64, K=16)
    res["T1DE_nside512_chord_only"] = leaf_areas(512, K=4)
    print(json.dumps(res, indent=1, ensure_ascii=False, default=str))


# ------------------------------------------------- chart 逆映射（polar_common.h:201-263 直译）
def xyz_to_chart(face, vin, mode=1):
    import math as _m
    p = np.asarray(vin, dtype=np.float64)
    n = np.linalg.norm(p)
    if not n > 0.0:
        return None
    p = p / n
    phi = _m.atan2(p[1], p[0])
    if phi < 0.0:
        phi += 2.0 * _m.pi
    INF = float("inf")

    def pick(s, diff_base):
        bu = bv = 0.0
        bd = INF
        for k in range(-2, 3):
            diff = diff_base + 8.0 * k
            uu, vv = 0.5 * (s + diff), 0.5 * (s - diff)
            du = max(0.0, max(-uu, uu - 1.0))
            dv = max(0.0, max(-vv, vv - 1.0))
            d = du * du + dv * dv
            if d < bd:
                bd, bu, bv = d, uu, vv
        return bu, bv

    def radial(zz):
        if mode == 0:
            return math.sqrt(max(0.0, 3.0 * (1.0 - zz)))
        rho = math.sqrt(p[0] * p[0] + p[1] * p[1])
        th = math.atan2(rho, zz)
        return math.sqrt(6.0) * math.sin(0.5 * th)

    if face <= 3:
        if p[2] >= TWO_THIRD:
            s = radial(p[2])
            if not s > 0.0:
                return 1.0, 1.0
            pb = phi - 0.5 * PI * face
            bu = bv = 0.0
            bd = INF
            for k in range(-2, 3):
                phit = pb + 2.0 * PI * k
                uu = 1.0 - s + 2.0 * s * phit / PI
                vv = 1.0 - 2.0 * s * phit / PI
                du = max(0.0, max(-uu, uu - 1.0))
                dv = max(0.0, max(-vv, vv - 1.0))
                d = du * du + dv * dv
                if d < bd:
                    bd, bu, bv = d, uu, vv
            return bu, bv
        return pick(1.5 * p[2], 4.0 * phi / PI - 1.0 - 2.0 * face)
    if face <= 7:
        return pick(1.5 * p[2] + 1.0, 4.0 * phi / PI - 2.0 * (face - 4))
    if p[2] <= -TWO_THIRD:
        s = radial(-p[2])
        if not s > 0.0:
            return 0.0, 0.0
        pb = phi - 0.5 * PI * (face - 8)
        bu = bv = 0.0
        bd = INF
        for k in range(-2, 3):
            phit = pb + 2.0 * PI * k
            uu = 2.0 * s * phit / PI
            vv = s - uu
            du = max(0.0, max(-uu, uu - 1.0))
            dv = max(0.0, max(-vv, vv - 1.0))
            d = du * du + dv * dv
            if d < bd:
                bd, bu, bv = d, uu, vv
        return bu, bv
    return pick(1.5 * p[2] + 2.0, 4.0 * phi / PI - 1.0 - 2.0 * (face - 8))
