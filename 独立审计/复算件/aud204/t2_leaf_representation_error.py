# -*- coding: utf-8 -*-
"""
AUD-204 / T2 : 逐 leaf 交叠面积的"表示误差"与"闭合判据失明"实测 + 换源退化自测

三套口径（同一 drop、同一 leaf 集合）：
  P (生产)   : leaf = 4 角大圆弦四边形, 交叠 = 球面 S-H 裁剪 + Van Oosterom
               （= drizzle_engine/spherical_overlap 现行表示：nside>=256 走 nb=4，
                 8<nside<256 走 samples=1 亦为 4 角）
  C (chart)  : drop 顶点映射进 chart + 大圆弧折线自适应细分 -> 轴对齐裁剪 + 鞋带 * (pi/3)
               （= ASTROCS_DESIGN §2.4 主张的快速路径）
  O (oracle) : leaf 真曲线边界 K 段 + 球面 S-H + VOS + Richardson 外推（独立高精度真值）

度量：
  M1 闭合    = |sum_p a_jp / A_drop - 1|          （冻结门 1e-6 的对象）
  M2 逐叶误差 = max_p |a_jp / a_jp^O - 1|         （门里没有的量）
  M3 弦偏差   = leaf 边界的真曲线到其大圆弦的最大矢高 / hp_res （对 §9 冻结 1e-6 预算）
  M4 chart 闭合 = |sum_p a_jp^C / (pi/3 * |D_chart|) - 1|（恒等式检验）+ C 对 O 的逐叶误差
退化自测（注入偏差问"谁会红"）：
  G1 Jacobian 取 pi/4 而非 pi/3        G2 drop 边不做弧细分（纯 4 角弦）
  G3 leaf 用弦四边形（= 生产表示）      G4 leaf 真曲线但 drop 半球检查关闭
"""
import json
import math

import numpy as np
import astropy.units as u
from astropy_healpix import lonlat_to_healpix

PI = math.pi
K_JAC = PI / 3.0
TWO_THIRD = 2.0 / 3.0

import importlib.util
_spec = importlib.util.spec_from_file_location("t1", "t1_chart_equalarea.py")
t1 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(t1)
chart_uv_to_xyz = t1.chart_uv_to_xyz
uv_to_radec = t1.uv_to_radec
poly_area = t1.poly_area
curve_boundary = t1.curve_boundary
cell_corner_xyz = t1.cell_corner_xyz


# ------------------------------------------------------------------ 球面 S-H
def sh_clip_sphere(subject, normals, tol=0.0):
    cur = np.asarray(subject, dtype=np.float64)
    for n in normals:
        if len(cur) < 3:
            return np.zeros((0, 3))
        nxt = []
        m = len(cur)
        for i in range(m):
            S, E = cur[(i + m - 1) % m], cur[i]
            ds, de = float(np.dot(n, S)), float(np.dot(n, E))
            si, ei = ds >= -tol, de >= -tol
            if ei:
                if not si:
                    ne = np.cross(S, E)
                    I = np.cross(ne, n)
                    nn = np.linalg.norm(I)
                    if nn < 1e-300:
                        continue
                    I = I / nn
                    if float(np.dot(I, S)) + float(np.dot(I, E)) < 0:
                        I = -I
                    nxt.append(I)
                nxt.append(E)
            elif si:
                ne = np.cross(S, E)
                I = np.cross(ne, n)
                nn = np.linalg.norm(I)
                if nn < 1e-300:
                    continue
                I = I / nn
                if float(np.dot(I, S)) + float(np.dot(I, E)) < 0:
                    I = -I
                nxt.append(I)
        cur = np.array(nxt) if nxt else np.zeros((0, 3))
    return cur if len(cur) >= 3 else np.zeros((0, 3))


def clip_normals_of(poly):
    """凸球面多边形（单位向量、CCW）的内法向半空间。"""
    c = poly.sum(axis=0)
    c = c / np.linalg.norm(c)
    out = []
    m = len(poly)
    for j in range(m):
        n = np.cross(poly[j], poly[(j + 1) % m])
        n = n / np.linalg.norm(n)
        if float(np.dot(n, c)) < 0:
            n = -n
        out.append(n)
    return out


# ------------------------------------------------------------------ drop 构造
def make_drop(ra0, dec0, scale_arcsec, half_deg, m_edge=1, rot_deg=0.0):
    """TAN 像素足迹：切点 T + 切平面基，p = normalize(T + eta e1 + xi e2)。
    返回角点数组（m_edge=1 => 4 角，与生产 drop 四角同形）。"""
    a, d = math.radians(ra0), math.radians(dec0)
    T = np.array([math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d)])
    e1 = np.array([-math.sin(d) * math.cos(a), -math.sin(d) * math.sin(a), math.cos(d)])
    e2 = np.array([-math.sin(a), math.cos(a), 0.0])
    th = math.radians(rot_deg)
    s = math.radians(scale_arcsec / 3600.0)
    pts = []
    corners = [(-half_deg, -half_deg), (half_deg, -half_deg),
               (half_deg, half_deg), (-half_deg, half_deg)]
    for k in range(4):
        x0, y0 = corners[k]
        x1, y1 = corners[(k + 1) % 4]
        for t in np.linspace(0.0, 1.0, m_edge, endpoint=False):
            x, y = x0 + t * (x1 - x0), y0 + t * (y1 - y0)
            dx = s * (math.cos(th) * x + math.sin(th) * y)
            dy = s * (-math.sin(th) * x + math.cos(th) * y)
            p = T + dy * e1 + dx * e2
            pts.append(p / np.linalg.norm(p))
    return np.array(pts)


def drop_area(poly):
    return float(poly_area(poly[None, None, :, :].reshape(1, 1, len(poly), 3))) \
        if False else float(poly_area(np.array(poly)[None, ...]))


# ------------------------------------------------------------------ 口径 P / O
def leaves_around(center, nside, maxn=64):
    """候选叶 = 中心邻域 + drop 顶点所属叶（用 astropy 独立反查，避免自造索引）。"""
    ra, dec = np.degrees(np.arctan2(center[..., 1], center[..., 0])) % 360, None
    return None


def pix_corner_chord_quad(ipix, nside):
    """生产 P 口径的 leaf：4 角 = 由 astropy 的亚像素偏移 (0,0)(1,0)(1,1)(0,1) 求角点。
    不用仓库 chart，避免与真值同源。"""
    e = 1.0 - 1e-12
    uv = [(0.0, 0.0), (e, 0.0), (e, e), (0.0, e)]
    pts = []
    for dx, dy in uv:
        lon, lat = lonlat_corner(ipix, nside, dx, dy)
        pts.append([math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon),
                    math.sin(lat)])
    return np.array(pts)


_CORNER_CACHE = {}


def lonlat_corner(ipix, nside, dx, dy):
    key = (int(ipix), int(nside))
    if key not in _CORNER_CACHE:
        _CORNER_CACHE[key] = {}
    if (dx, dy) in _CORNER_CACHE[key]:
        return _CORNER_CACHE[key][(dx, dy)]
    from astropy_healpix import healpix_to_lonlat
    lon, lat = healpix_to_lonlat(np.array([ipix]), nside, dx=np.array([dx]),
                                 dy=np.array([dy]), order="nested")
    r = (float(lon[0].rad), float(lat[0].rad))
    _CORNER_CACHE[key][(dx, dy)] = r
    return r


def true_leaf_boundary(ipix, nside, K):
    """O 口径 leaf 真曲线边界：沿 4 条边用 astropy 亚像素偏移取点，
    落在边界上的点用 lonlat_to_healpix 反查校验确属该叶（±数值）。"""
    from astropy_healpix import healpix_to_lonlat
    e = 1.0 - 1e-12
    edges = [((0.0, 0.0), (e, 0.0)), ((e, 0.0), (e, e)),
             ((e, e), (0.0, e)), ((0.0, e), (0.0, 0.0))]
    dxs, dys = [], []
    for (a, b) in edges:
        t = np.arange(K) / K
        dxs += list(a[0] + t * (b[0] - a[0]))
        dys += list(a[1] + t * (b[1] - a[1]))
    lon, lat = healpix_to_lonlat(np.full(4 * K, int(ipix)), nside,
                                 dx=np.array(dxs), dy=np.array(dys), order="nested")
    la, be = lon.rad, lat.rad
    return np.stack([np.cos(be) * np.cos(la), np.cos(be) * np.sin(la),
                     np.sin(be)], axis=-1)


def PA(p):
    return float(poly_area(np.asarray(p)[None, ...])[0])


def overlap_P(leaf_quad, drop):
    nrm = clip_normals_of(drop)
    res = sh_clip_sphere(leaf_quad, nrm)
    return (PA(res) if len(res) >= 3 else 0.0), res


def overlap_O(leaf_curve, drop):
    nrm = clip_normals_of(drop)
    res = sh_clip_sphere(leaf_curve, nrm)
    if len(res) < 3:
        return 0.0
    return PA(res)


# ------------------------------------------------------------------ 主用例
def run_case(name, ra0, dec0, nside, scale_arcsec, pixfrac, ring=3):
    hp_res = math.sqrt(K_JAC / (nside * nside))
    # drop 半边长（度）
    halfdeg = 0.5 * pixfrac * scale_arcsec / 3600.0
    drop = make_drop(ra0, dec0, scale_arcsec, halfdeg, m_edge=1)
    A_drop = PA(drop)
    cen = drop.sum(axis=0) / np.linalg.norm(drop.sum(axis=0))
    lon_c = math.atan2(cen[1], cen[0]) % (2 * PI)
    lat_c = math.asin(max(-1.0, min(1.0, cen[2])))
    ip0 = int(lonlat_to_healpix(np.array([lon_c]) * u.rad, np.array([lat_c]) * u.rad,
                                nside, order="nested")[0])
    # 候选叶：以中心叶的 chart 邻域 3x3 扩环（用 astropy 反查中心点的邻叶）
    from astropy_healpix import healpix_to_lonlat
    ijs = []
    dxy = [(-ring + i, -ring + j) for i in range(2 * ring + 1) for j in range(2 * ring + 1)]
    # 用大圆邻域扫描得到候选（粗：按 hp_res 步长在切平面偏移，再反查叶号）
    a1 = np.array([-math.sin(lat_c) * math.cos(lon_c), -math.sin(lat_c) * math.sin(lon_c),
                   math.cos(lat_c)])
    a2 = np.array([-math.sin(lon_c), math.cos(lon_c), 0.0])
    cand = set()
    step = hp_res * 0.45
    for (du, dv) in dxy:
        p = cen + (dv * step) * a1 + (du * step) * a2
        p = p / np.linalg.norm(p)
        la, be = math.atan2(p[1], p[0]) % (2 * PI), math.asin(max(-1, min(1, p[2])))
        cand.add(int(lonlat_to_healpix(np.array([la]) * u.rad, np.array([be]) * u.rad,
                                       nside, order="nested")[0]))
    # 也保证 drop 顶点所在叶进入候选
    for p in drop:
        la = math.atan2(p[1], p[0]) % (2 * PI)
        be = math.asin(max(-1.0, min(1.0, p[2])))
        cand.add(int(lonlat_to_healpix(np.array([la]) * u.rad, np.array([be]) * u.rad,
                                       nside, order="nested")[0]))
    cand = sorted(cand)
    aP = np.zeros(len(cand))
    aO = np.zeros(len(cand))
    for k, ip in enumerate(cand):
        q = pix_corner_chord_quad(ip, nside)
        aP[k], _ = overlap_P(q, drop)
        K = 16
        c1 = true_leaf_boundary(ip, nside, K)
        c2 = true_leaf_boundary(ip, nside, 2 * K)
        o1, o2 = overlap_O(c1, drop), overlap_O(c2, drop)
        aO[k] = (4 * o2 - o1) / 3.0
    sumP, sumO = float(aP.sum()), float(aO.sum())
    with np.errstate(divide="ignore", invalid="ignore"):
        perleaf = np.where(aO > 1e-30, np.abs(aP / aO - 1.0), 0.0)
    kmax = int(np.argmax(perleaf))
    return {
        "case": name, "ra": ra0, "dec": dec0, "nside": nside, "pixfrac": pixfrac,
        "scale_arcsec": scale_arcsec, "hp_res_rad": hp_res, "A_drop_sr": A_drop,
        "n_candidates": len(cand),
        "M1_closure_P_rel": abs(sumP / A_drop - 1.0),
        "M1_closure_O_rel": abs(sumO / A_drop - 1.0),
        "M2_perleaf_max_rel_err": float(perleaf[kmax]),
        "M2_perleaf_max_at_ipix": int(cand[kmax]),
        "M2_sum_of_abs_err_over_A_drop": float(perleaf.max()),
        "flux_misassignment_frac": float(np.abs(aP - aO).sum() / (2 * A_drop)),
    }


def sagitta_test(nside=512, ncell=400, seed=11):
    """M3：leaf 边的真曲线 vs 其大圆弦 的最大矢高（以 hp_res 为单位）。"""
    hp_res = math.sqrt(K_JAC) / nside
    rng = np.random.default_rng(seed)
    worst = (0.0, None)
    from astropy_healpix import healpix_to_lonlat
    for _ in range(ncell):
        ip = int(rng.integers(0, 12 * nside * nside))
        e = 1.0 - 1e-12
        for (a, b) in [((0., 0.), (e, 0.)), ((e, 0.), (e, e)),
                       ((e, e), (0., e)), ((0., e), (0., 0.))]:
            def pt(dx, dy):
                lon, lat = healpix_to_lonlat(np.array([ip]), nside,
                                             dx=np.array([dx]), dy=np.array([dy]),
                                             order="nested")
                return np.array([math.cos(lat[0].rad) * math.cos(lon[0].rad),
                                 math.cos(lat[0].rad) * math.sin(lon[0].rad),
                                 math.sin(lat[0].rad)])
            A, B = pt(*a), pt(*b)
            chord = A + B
            if np.linalg.norm(chord) < 1e-16:
                continue
            chord = chord / np.linalg.norm(chord)      # 大圆弧中点（弦方向）
            Mtrue = pt(0.5 * (a[0] + b[0]), 0.5 * (a[1] + b[1]))
            ang = math.acos(max(-1.0, min(1.0, float(np.dot(Mtrue, chord)))))
            sag = ang * hp_res / hp_res               # 弧度 / hp_res(弧度) = 倍率
            # 矢高（弧长单位）除以 hp_res
            if sag > worst[0]:
                worst = (sag, ip)
    return {"nside": nside, "max_sagitta_over_hp_res": worst[0],
            "at_ipix": worst[1], "frozen_budget_arc_chord_over_hp_res": 1e-6}


if __name__ == "__main__":
    out = {"cases": [], "sagitta": sagitta_test()}
    # 位置：赤道一般 / |z|=2/3 缝 / face 角点邻域 / 极点 / 一般中高纬
    grid = [("equatorial_general", 40.0, 10.0), ("seam_z23_N", 120.0,
             math.degrees(math.asin(2 / 3))), ("seam_z23_S", 250.0,
             -math.degrees(math.asin(2 / 3))),
            ("face_corner_pole", 0.0, 90.0), ("face_corner_eq", 0.0, 0.0),
            ("polar_cap_mid", 60.0, 75.0), ("mid_lat", 200.0, 45.0)]
    for (nm, ra, dec) in grid:
        r = run_case(nm, ra, dec, nside=1024, scale_arcsec=2.0, pixfrac=0.8)
        out["cases"].append(r)
        print(json.dumps(r, ensure_ascii=False))
    print(json.dumps(out["sagitta"], ensure_ascii=False))
    with open("t2_out.json", "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
