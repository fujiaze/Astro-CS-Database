# -*- coding: utf-8 -*-
"""
AUD-204 / T4 : 逐 leaf 交叠面积三方对照（生产弦 / 真曲线 oracle / chart+鞋带快速路径）
 + 闭合恒等式与"通量错配"分离

口径：
  P = 生产表示：leaf = 4 角大圆弦四边形；drop = 4 角大圆弦四边形（TAN 直边在球面即大圆弧，
      故 drop 侧对纯 TAN 是**精确**的）；交叠 = 球面 S-H 裁剪 + Van Oosterom
  T = 真值：leaf = 真曲线边界 K 段采样 + Richardson（K=48/96）；drop 同 P
  C = chart 快速路径：drop 顶点进 chart + 大圆弧自适应折线细分 -> 轴对齐裁剪 + 鞋带 * (pi/3)

度量：
  cl_X   = |sum_p a_X / A_drop - 1|            （冻结门 1e-6 的对象）
  id_C   = |sum_p a_C / (pi/3*|D_chart|) - 1|  （构造闭合恒等式）
  pl_XT  = max_p |a_X - a_T|/a_T               （逐叶面积误差 —— 门里没有）
  mis_X = sum_p |a_X - a_T| / (2*A_drop)       （通量错配比例，物理可解释量）
"""
import json
import math

import numpy as np
import importlib.util
import astropy.units as u
from astropy_healpix import lonlat_to_healpix, healpix_to_lonlat

PI = math.pi
K_JAC = PI / 3.0

spec = importlib.util.spec_from_file_location("t1", "t1_chart_equalarea.py")
t1 = importlib.util.module_from_spec(_spec := spec)
spec.loader.exec_module(t1)
chart = t1.chart_uv_to_xyz
xyz_to_chart = t1.xyz_to_chart
poly_area = t1.poly_area


def PA(p):
    return float(poly_area(np.asarray(p)[None, ...])[0])


def best_face_chart(p):
    """独立求 (face,u,v)：对 12 面各自求逆，取落在单位方格内（或最接近）者。"""
    best = None
    for f in range(12):
        uv = xyz_to_chart(f, p)
        if uv is None:
            continue
        d = max(0.0, -uv[0], uv[0] - 1.0) ** 2 + max(0.0, -uv[1], uv[1] - 1.0) ** 2
        if best is None or d < best[0]:
            best = (d, f, uv[0], uv[1])
    return best[1], best[2], best[3], best[0]


def clip_normals_of(poly):
    c = poly.sum(axis=0)
    c = c / np.linalg.norm(c)
    out = []
    m = len(poly)
    for j in range(m):
        n = np.cross(poly[j], poly[(j + 1) % m])
        nn = np.linalg.norm(n)
        if nn < 1e-300:
            continue
        n = n / nn
        if float(np.dot(n, c)) < 0:
            n = -n
        out.append(n)
    return out


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
                    I = np.cross(np.cross(S, E), n)
                    nl = np.linalg.norm(I)
                    if nl < 1e-300:
                        continue
                    I = I / nl
                    if float(np.dot(I, S)) + float(np.dot(I, E)) < 0:
                        I = -I
                    nxt.append(I)
                nxt.append(E)
            elif si:
                I = np.cross(np.cross(S, E), n)
                nl = np.linalg.norm(I)
                if nl < 1e-300:
                    continue
                I = I / nl
                if float(np.dot(I, S)) + float(np.dot(I, E)) < 0:
                    I = -I
                nxt.append(I)
        cur = np.array(nxt) if nxt else np.zeros((0, 3))
    return cur if len(cur) >= 3 else np.zeros((0, 3))


# ------------------------------------------------------------------ 2D 工具（chart 侧）
def clip_half(poly, axis, val, keep_ge):
    if len(poly) == 0:
        return poly
    out = []
    m = len(poly)
    f = (lambda q: q[axis] - val) if keep_ge else (lambda q: val - q[axis])
    for i in range(m):
        A, B = poly[(i + m - 1) % m], poly[i]
        fa, fb = f(A), f(B)
        ia, ib = fa >= 0.0, fb >= 0.0
        if ib:
            if not ia:
                t = fa / (fa - fb)
                out.append([A[0] + t * (B[0] - A[0]), A[1] + t * (B[1] - A[1])])
            out.append(list(B))
        elif ia:
            t = fa / (fa - fb)
            out.append([A[0] + t * (B[0] - A[0]), A[1] + t * (B[1] - A[1])])
    return np.array(out) if out else np.zeros((0, 2))


def clip_box(poly, u0, v0, u1, v1):
    q = poly
    for (ax, val, kg) in [(0, u0, True), (0, u1, False), (1, v0, True), (1, v1, False)]:
        q = clip_half(q, ax, val, kg)
        if len(q) < 3:
            return q
    return q


def shoelace(q, shift=True):
    if q is None or len(q) < 3:
        return 0.0
    if shift:
        m = q - q.min(axis=0)
    else:
        m = q
    x, y = m[:, 0], m[:, 1]
    return 0.5 * abs(float(np.sum(np.roll(x, -1) * y - x * np.roll(y, -1))))


# ------------------------------------------------------------------ 几何构造
def gnomonic_basis(ra0, dec0):
    a, d = math.radians(ra0), math.radians(dec0)
    T = np.array([math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d)])
    e1 = np.array([-math.sin(d) * math.cos(a), -math.sin(d) * math.sin(a), math.cos(d)])
    e2 = np.array([-math.sin(a), math.cos(a), 0.0])
    return T, e1, e2


def make_drop(T, e1, e2, half_pix_rad, seg):
    d = half_pix_rad
    cs = [(-d, -d), (d, -d), (d, d), (-d, d)]
    pts = []
    for k in range(4):
        x0, y0 = cs[k]
        x1, y1 = cs[(k + 1) % 4]
        for t in (np.linspace(0, 1, seg, endpoint=False) if seg > 1 else [0.0]):
            x, y = x0 + t * (x1 - x0), y0 + t * (y1 - y0)
            p = T + y * e1 + x * e2
            pts.append(p / np.linalg.norm(p))
    return np.array(pts)


def leaf_curve(ip, nside, K):
    e = 1.0 - 1e-12
    edges = [((0.0, 0.0), (e, 0.0)), ((e, 0.0), (e, e)),
             ((e, e), (0.0, e)), ((0.0, e), (0.0, 0.0))]
    dxs, dys = [], []
    for (a, b) in edges:
        t = np.arange(K) / K
        dxs += list(a[0] + t * (b[0] - a[0]))
        dys += list(a[1] + t * (b[1] - a[1]))
    lon, lat = healpix_to_lonlat(np.full(4 * K, int(ip)), nside, dx=np.array(dxs),
                                 dy=np.array(dys), order="nested")
    la, be = lon.rad, lat.rad
    return np.stack([np.cos(be) * np.cos(la), np.cos(be) * np.sin(la), np.sin(be)], -1)


def leaf_chord(ip, nside):
    e = 1.0 - 1e-12
    dxs = np.array([0.0, e, e, 0.0])
    dys = np.array([0.0, 0.0, e, e])
    lon, lat = healpix_to_lonlat(np.full(4, int(ip)), nside, dx=dxs, dy=dys, order="nested")
    la, be = lon.rad, lat.rad
    return np.stack([np.cos(be) * np.cos(la), np.cos(be) * np.sin(la), np.sin(be)], -1)


def candidates(drop, nside):
    """候选叶：drop 顶点 + 中心 + 1/N 级网格点反查（穷举到 drop 外扩 1.5 叶）。"""
    cen = drop.sum(axis=0)
    cen = cen / np.linalg.norm(cen)
    hp_res = math.sqrt(K_JAC) / nside
    lat_c = math.asin(max(-1.0, min(1.0, cen[2])))
    lon_c = math.atan2(cen[1], cen[0])
    A1 = np.array([-math.sin(lat_c) * math.cos(lon_c), -math.sin(lat_c) * math.sin(lon_c),
                   math.cos(lat_c)])
    A2 = np.array([-math.sin(lon_c), math.cos(lon_c), 0.0])
    pts = [cen] + [p for p in drop]
    nk = max(3, int(2.0 * np.linalg.norm((drop - cen), axis=-1).max() / hp_res) + 3)
    for iu in range(-nk, nk + 1):
        for iv in range(-nk, nk + 1):
            p = cen + (iv * hp_res * 0.5) * A1 + (iu * hp_res * 0.5) * A2
            pts.append(p / np.linalg.norm(p))
    lons = np.array([math.atan2(p[1], p[0]) for p in pts]) % (2 * PI)
    lats = np.array([math.asin(max(-1.0, min(1.0, p[2]))) for p in pts])
    ip = lonlat_to_healpix(lons * u.rad, lats * u.rad, nside, order="nested")
    return sorted({int(x) for x in ip})


def run(tag, ra0, dec0, nside, scale_arcsec, pixfrac, seg=1, chart_seg=1):
    hp_res = math.sqrt(K_JAC) / nside
    half = 0.5 * pixfrac * math.radians(scale_arcsec / 3600.0)
    T, e1, e2 = gnomonic_basis(ra0, dec0)
    drop = make_drop(T, e1, e2, half, seg)
    A_drop = PA(drop)
    nrm = clip_normals_of(drop)
    cand = candidates(drop, nside)
    aP, aT = {}, {}
    for ip in cand:
        q = leaf_chord(ip, nside)
        r = sh_clip_sphere(q, nrm)
        aP[ip] = PA(r) if len(r) >= 3 else 0.0
        K = 48
        o1 = sh_clip_sphere(leaf_curve(ip, nside, K), nrm)
        o2 = sh_clip_sphere(leaf_curve(ip, nside, 2 * K), nrm)
        v1 = PA(o1) if len(o1) >= 3 else 0.0
        v2 = PA(o2) if len(o2) >= 3 else 0.0
        aT[ip] = (4 * v2 - v1) / 3.0
    # chart 快速路径（单 face 版：只统计 drop 主 face 的叶；跨 face 由多 face 累加）
    faces_used = []
    for p in drop:
        f = best_face_chart(p)[0]
        if f not in faces_used:
            faces_used.append(f)
    aC = {}
    idC = 0.0
    cell = 1.0 / nside
    for f in faces_used:
        uvp = []
        for p in drop:
            r = xyz_to_chart(f, p)
            uvp.append([r[0], r[1]])
        poly = np.array(uvp)
        ins = clip_box(poly, 0.0, 0.0, 1.0, 1.0)
        if len(ins) < 3:
            continue
        idC += shoelace(ins)
        i0 = max(0, int(math.floor(ins[:, 0].min() * nside)))
        i1 = min(nside - 1, int(math.floor(ins[:, 0].max() * nside)))
        j0 = max(0, int(math.floor(ins[:, 1].min() * nside)))
        j1 = min(nside - 1, int(math.floor(ins[:, 1].max() * nside)))
        for i in range(i0, i1 + 1):
            for j in range(j0, j1 + 1):
                q = clip_box(ins, i * cell, j * cell, (i + 1) * cell, (j + 1) * cell)
                ar = K_JAC * shoelace(q)
                if ar <= 0:
                    continue
                # chart (i,j) -> 该 face 的叶：用中心点反查 ipix（独立于 face 编号约定）
                cu, cv = (i + 0.5) * cell, (j + 0.5) * cell
                c = chart(f, np.array([cu]), np.array([cv]))[0]
                la = math.atan2(c[1], c[0]) % (2 * PI)
                be = math.asin(max(-1.0, min(1.0, c[2])))
                ip = int(lonlat_to_healpix(np.array([la]) * u.rad,
                                           np.array([be]) * u.rad, nside,
                                           order="nested")[0])
                aC[ip] = aC.get(ip, 0.0) + ar
    ipx = sorted(set(list(aP) + list(aT) + list(aC)))
    P = np.array([aP.get(i, 0.0) for i in ipx])
    Tr = np.array([aT.get(i, 0.0) for i in ipx])
    C = np.array([aC.get(i, 0.0) for i in ipx])
    with np.errstate(divide="ignore", invalid="ignore"):
        ePT = np.where(Tr > 1e-30, np.abs(P / Tr - 1.0), 0.0)
        eCT = np.where(Tr > 1e-30, np.abs(C / Tr - 1.0), 0.0)
    return {"tag": tag, "ra": ra0, "dec": dec0, "nside": nside, "hp_res_arcsec":
            math.degrees(hp_res) * 3600.0, "scale_arcsec": scale_arcsec,
            "pixfrac": pixfrac, "A_drop_sr": A_drop, "n_leaf": len(ipx),
            "cl_P": abs(P.sum() / A_drop - 1.0), "cl_T": abs(Tr.sum() / A_drop - 1.0),
            "cl_C": abs(C.sum() / A_drop - 1.0),
            "id_C_vs_chartarea": abs(C.sum() / (K_JAC * idC) - 1.0) if idC > 0 else None,
            "pl_P_max": float(ePT.max()) if ePT.size else 0.0,
            "pl_C_max": float(eCT.max()) if eCT.size else 0.0,
            "mis_P": float(np.abs(P - Tr).sum() / (2 * A_drop)),
            "mis_C": float(np.abs(C - Tr).sum() / (2 * A_drop)),
            "sum_T_over_A_drop": Tr.sum() / A_drop}


if __name__ == "__main__":
    rows = []
    cases = [
        ("matched_eq", 40.0, 10.0, 32768, 6.3, 1.0),
        ("matched_seam_z23", 120.0, math.degrees(math.asin(2 / 3)), 32768, 6.3, 1.0),
        ("matched_polarcap", 60.0, 78.0, 32768, 6.3, 1.0),
        ("pole_exact", 0.0, 89.9999, 32768, 6.3, 1.0),
        ("bigdrop_eq", 40.0, 10.0, 4096, 300.0, 1.0),
        ("bigdrop_polar", 60.0, 78.0, 4096, 300.0, 1.0),
        ("pf08_matched", 40.0, 10.0, 131072, 2.0, 0.8),
        ("pf08_polar", 60.0, 78.0, 131072, 2.0, 0.8),
    ]
    for c in cases:
        try:
            r = run(*c)
        except Exception as ex:
            r = {"tag": c[0], "error": repr(ex)}
        rows.append(r)
        print(json.dumps(r, ensure_ascii=False), flush=True)
    with open("t4_out.json", "w", encoding="utf-8") as fh:
        json.dump(rows, fh, indent=1, ensure_ascii=False)
