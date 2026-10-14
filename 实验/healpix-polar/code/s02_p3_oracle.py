#!/usr/bin/env python3
"""s02_p3_oracle.py — S02 独立球面几何 Oracle（不复用生产 C++ 球面裁剪/面积/法向代码）

被估量（与 docs/science/drizzle/DRIZZLE.md「通量守恒门」同口径）：
    epsilon_j = sum_p a_jp / A_drop,j - 1        （无量纲；分母 = drop 自身球面面积）

三条互相独立的读数（同一 drop 位形，同一叶集合）：
  O_EXACT  精确 2D 切平面裁剪：大圆在切平面上的像是直线 ⇒ 在 drop 中心的
           gnomonic 平面内做精确 2D 凸多边形裁剪（算不上叉积/球面 S-H），
           再把交点还原到球面用 Van Oosterom & Strackee 扇形面积。
           ⇒ 给出「生产几何模型（叶边界 = 四角弦大圆弧）」的**真值**，预期 ~1e-15。
  O_LEGACY 三维球面 S-H：法向 = normalize(P1 x P2)，交点 = normalize(cross(cross(S,E), n_clip))
           逐位复刻生产修前算术（独立 Python 实现），预期复现 ~1e-3（未修前）。
  O_FIXED  三维球面 S-H：法向 = 切平面偏移恒等式，交点 = 线性插值稳定式
           (dS*E - dE*S)/(dS - dE)，复刻生产修后算术，预期 ~1e-9。

叶边界角点来自 healpy（与生产 healpix_core 不同的实现），drop 角点由 TAN/gnomonic
解析式给出（不经生产 WcsSip）——两处都与被测量解耦。

用法： python3 实验/healpix-polar/code/s02_p3_oracle.py [输出 JSON 路径]
"""
import json
import math
import os
import sys

import numpy as np

D2R = math.pi / 180.0
ARCSEC = D2R / 3600.0
SEAM_DEC = math.degrees(math.asin(2.0 / 3.0))


# ----------------------------------------------------------------------------
# 基本向量工具
# ----------------------------------------------------------------------------
def unit(v):
    return v / np.linalg.norm(v)


def cross(a, b):
    return np.cross(a, b)


def vos_area(P):
    """Van Oosterom & Strackee 扇形有向立体角（sr）——独立于生产实现。

    注意：对 θ ≲ 1″ 的小多边形，det = a·(b×c) 是 O(1) 乘积相消到 O(θ³) 的差，
    双精度下相对误差可达 ~1e-4（实测 0.032″ drop: +1.51e-4）。因此本 oracle 的
    主面积例程用 l'Huilier（见 lhuilier_area），VOS 只作为大角尺度互校。
    """
    P = [np.asarray(p, float) for p in P]
    if len(P) < 3:
        return 0.0
    s = 0.0
    for i in range(1, len(P) - 1):
        det = float(np.dot(P[0], np.cross(P[i], P[i + 1])))
        den = 1.0 + float(np.dot(P[0], P[i])) + float(np.dot(P[i], P[i + 1])) + float(np.dot(P[i + 1], P[0]))
        s += 2.0 * math.atan2(det, den)
    return abs(s)


def _ang(a, b):
    return math.atan2(float(np.linalg.norm(np.cross(a, b))), float(np.dot(a, b)))


def _lhuilier(a, b, c):
    s = 0.5 * (a + b + c)
    t = math.tan(0.5 * s) * math.tan(0.5 * (s - a)) * math.tan(0.5 * (s - b)) * math.tan(0.5 * (s - c))
    return 4.0 * math.atan(math.sqrt(t)) if t > 0.0 else 0.0


def lhuilier_area(P):
    """l'Huilier 质心扇剖分（小多边形数值稳定）——本 oracle 的主面积例程。

    双精度地板实测（0.032″ 正方程形，相对解析面积）：−2.9e-10；0.5″: −2.7e-11；
    2″: −4.3e-12。⇒ 该例程在最小尺度上的分辨力约 ε_mach/θ。
    """
    P = [np.asarray(p, float) for p in P]
    if len(P) < 3:
        return 0.0
    cen = np.sum(P, axis=0)
    n = np.linalg.norm(cen)
    if n < 1e-300:
        return 0.0
    cen = cen / n
    A = 0.0
    for i in range(len(P)):
        p, q = P[i], P[(i + 1) % len(P)]
        A += _lhuilier(_ang(p, q), _ang(q, cen), _ang(cen, p))
    return abs(A)


def area(P):
    return lhuilier_area(P)


def frame(ra_deg, dec_deg):
    ra, de = ra_deg * D2R, dec_deg * D2R
    c = np.array([math.cos(de) * math.cos(ra), math.cos(de) * math.sin(ra), math.sin(de)])
    e1 = unit(np.cross([0.0, 0.0, 1.0], c))
    e2 = np.cross(c, e1)
    return c, e1, e2


def drop_corners(ra_deg, dec_deg, scale_arcsec, pixfrac):
    """TAN 投影下 1x1 帧（CRVAL 在像素中心）的 drop 四角（解析式，双精度）。"""
    c, e1, e2 = frame(ra_deg, dec_deg)
    h = 0.5 * pixfrac * scale_arcsec * ARCSEC
    offs = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    P = []
    for x, y in offs:
        v = c + x * h * e1 + y * h * e2
        P.append(unit(v))
    return c, e1, e2, P


def tangent_offset(P, c):
    return P / float(np.dot(P, c)) - c


# ----------------------------------------------------------------------------
# 叶几何（healpy）
# ----------------------------------------------------------------------------
def leaf_polygon(import_healpy, nside, ipix):
    hp = import_healpy
    b = hp.boundaries(nside, ipix, step=1, nest=True)   # 3x4
    P = [unit(np.array([b[0][k], b[1][k], b[2][k]])) for k in range(b.shape[1])]
    return P


def candidate_leaves(hp, nside, c, drop_max_angle, margin_rad):
    vecs = hp.ang2vec(math.acos(np.clip(c[2], -1, 1)), math.atan2(c[1], c[0]))
    r = drop_max_angle + margin_rad
    return list(hp.query_disc(nside, vecs, r, nest=True))


# ----------------------------------------------------------------------------
# 2D 切平面精确裁剪（O_EXACT）
# ----------------------------------------------------------------------------
def clip2d(subject2d, clip_edges2d):
    """subject2d: [(x,y)...]；clip_edges2d: [((x1,y1),(x2,y2))...] 直线半平面。

    保留侧 = 使多边形中心在正侧的一侧由调用方保证；此处按「左/右」按顺时针
    顶点序的固定方向处理：对每条裁剪边 (A,B)，保留 cross(B-A, X-A) >= 0 的一侧。
    """
    out = list(subject2d)
    for (A, B) in clip_edges2d:
        if len(out) < 3:
            return []
        d = np.array(B, float) - np.array(A, float)
        inp = out
        out = []
        m = len(inp)
        for i in range(m):
            X = np.array(inp[i], float)
            Y = np.array(inp[(i + 1) % m], float)
            fx = d[0] * (X[1] - A[1]) - d[1] * (X[0] - A[0])
            fy = d[0] * (Y[1] - A[1]) - d[1] * (Y[0] - A[0])
            if fx >= 0.0:
                out.append(tuple(X))
            if (fx >= 0.0) != (fy >= 0.0):
                # 2x2 线性解 (well-conditioned)
                M = np.array([[d[0], -d[1]], [d[1], d[0]]])  # 方向 d 与法向的法方程
                # 直接解 X + t*(Y-X) 落在直线上
                e = Y - X
                den = d[0] * e[1] - d[1] * e[0]
                if abs(den) > 1e-300:
                    t = -(d[0] * (X[1] - A[1]) - d[1] * (X[0] - A[0])) / den
                    out.append(tuple(X + t * e))
        # 去重近邻点
        ded = []
        for p in out:
            if not ded or (p[0] - ded[-1][0]) ** 2 + (p[1] - ded[-1][1]) ** 2 > 0.0:
                ded.append(p)
        if len(ded) > 1 and (ded[0][0] - ded[-1][0]) ** 2 + (ded[0][1] - ded[-1][1]) ** 2 == 0.0:
            ded = ded[:-1]
        out = ded
    return out


def from2d(alpha, c):
    v = np.asarray(alpha, float) + c
    return unit(v)


def eps_exact(hp, nside, c, e1, e2, dropP, scale_arcsec, pixfrac):
    """O_EXACT：切平面精确 2D 裁剪 + VOS 面积；分母 = drop 自身 VOS 面积。"""
    Adrop = area(dropP)
    # drop 在切平面内的四角（gnomonic 像）
    drop2d = [tuple(tangent_offset(P, c)) for P in dropP]
    # drop 的裁剪边（按顶点序，保留 c 一侧）
    dev = max(np.linalg.norm(tangent_offset(P, c)) for P in dropP)
    leaves = candidate_leaves(hp, nside, c, dev, 4.0 * math.sqrt(math.pi / 3.0) / nside)
    total = 0.0
    per_leaf = []
    for ip in leaves:
        poly = leaf_polygon(hp, nside, int(ip))
        poly2d = [tuple(tangent_offset(P, c)) for P in poly]
        # 叶在切平面内是多边形；对 drop 的每条边裁剪（保留侧含 c 的像 = 原点）
        edges = []
        for j in range(4):
            A = drop2d[j]
            B = drop2d[(j + 1) % 4]
            edges.append((A, B))
        # 需要保留「原点一侧」：若以 (A,B) 方向看原点在负侧，交换 A/B
        edges2 = []
        for (A, B) in edges:
            d = np.array(B) - np.array(A)
            # f(origin) = d0*(0-A1) - d1*(0-A0) = -d0*A1 + d1*A0
            f0 = d[1] * A[0] - d[0] * A[1]
            edges2.append((A, B) if f0 >= 0 else (B, A))
        cl = clip2d(poly2d, edges2)
        if len(cl) < 3:
            continue
        P3 = [from2d(p, c) for p in cl]
        a = area(P3)
        total += a
        per_leaf.append({"ipix": int(ip), "area_sr": a})
    return {
        "method": "O_EXACT (2D gnomonic exact clip + VOS)",
        "A_drop_sr": Adrop,
        "sum_area_sr": total,
        "epsilon_j": total / Adrop - 1.0 if Adrop > 0 else None,
        "n_leaves": len(per_leaf),
        "per_leaf": per_leaf,
    }


# ----------------------------------------------------------------------------
# 3D 球面 S-H（O_LEGACY / O_FIXED）
# ----------------------------------------------------------------------------
def sh_3d(subject, normals, mode, c):
    out = [np.asarray(p, float) for p in subject]
    for n in normals:
        if len(out) < 3:
            return []
        inp = out
        out = []
        m = len(inp)
        for i in range(m):
            S = inp[i]
            E = inp[(i + 1) % m]
            dS = float(np.dot(S, n))
            dE = float(np.dot(E, n))
            inS = dS >= 0.0
            inE = dE >= 0.0
            if inS:
                out.append(S)
            if inS != inE:
                if mode == "legacy":
                    n_edge = np.cross(S, E)
                    I = np.cross(n_edge, n)
                    nl = np.linalg.norm(I)
                    if nl < 1e-300:
                        continue
                    I = I / nl
                    if float(np.dot(I, S)) + float(np.dot(I, E)) < 0.0:
                        I = -I
                else:  # fixed: linear interpolation form (stable)
                    den = dS - dE
                    if abs(den) < 1e-300:
                        continue
                    I = (dS * E - dE * S) / den
                    nl = np.linalg.norm(I)
                    if nl < 1e-300:
                        continue
                    I = I / nl
                out.append(I)
            ded = []
            for p in out:
                if not ded or np.linalg.norm(p - ded[-1]) > 0.0:
                    ded.append(p)
            if len(ded) > 1 and np.linalg.norm(ded[0] - ded[-1]) == 0.0:
                ded = ded[:-1]
            out = ded
    return out


def drop_normals(dropP, c, mode):
    ns = []
    nd = len(dropP)
    if mode == "legacy":
        for j in range(nd):
            n = np.cross(dropP[j], dropP[(j + 1) % nd])
            if float(np.dot(n, c)) < 0:
                n = -n
            ns.append(unit(n))
    else:
        al = [tangent_offset(P, c) for P in dropP]
        for j in range(nd):
            a1, a2 = al[j], al[(j + 1) % nd]
            m = np.cross(c, a2 - a1) + np.cross(a1, a2)
            if float(np.dot(m, c)) < 0:
                m = -m
            ns.append(unit(m))
    return ns


def eps_sh(hp, nside, c, dropP, mode):
    Adrop = area(dropP)
    dev = max(np.linalg.norm(tangent_offset(P, c)) for P in dropP)
    leaves = candidate_leaves(hp, nside, c, dev, 4.0 * math.sqrt(math.pi / 3.0) / nside)
    ns = drop_normals(dropP, c, mode)
    total = 0.0
    per_leaf = []
    for ip in leaves:
        poly = leaf_polygon(hp, nside, int(ip))
        cl = sh_3d(poly, ns, mode, c)
        if len(cl) < 3:
            continue
        a = area(cl)
        total += a
        per_leaf.append({"ipix": int(ip), "area_sr": a})
    return {
        "method": f"O_{mode.upper()} (3D spherical S-H, {mode} arithmetic)",
        "A_drop_sr": Adrop,
        "sum_area_sr": total,
        "epsilon_j": total / Adrop - 1.0 if Adrop > 0 else None,
        "n_leaves": len(per_leaf),
        "per_leaf": per_leaf,
    }


# ----------------------------------------------------------------------------
def evaluate(hp, ra, dec, scale_arcsec, pixfrac, nside, label):
    c, e1, e2, P = drop_corners(ra, dec, scale_arcsec, pixfrac)
    rec = {
        "label": label,
        "ra_deg": ra, "dec_deg": dec, "nside": nside,
        "scale_arcsec": scale_arcsec, "pixfrac": pixfrac,
        "drop_side_arcsec": pixfrac * scale_arcsec,
        "theta_drop_rad": pixfrac * scale_arcsec * ARCSEC,
    }
    rec["exact"] = eps_exact(hp, nside, c, e1, e2, P, scale_arcsec, pixfrac)
    rec["legacy"] = eps_sh(hp, nside, c, P, "legacy")
    rec["fixed"] = eps_sh(hp, nside, c, P, "fixed")
    return rec


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "results", "s02_oracle.json")
    import healpy as hp

    configs = []
    # 顶点构型（face 0 中心 = 4 叶公共顶点），drop 尺度扫描
    for sc in (0.04, 0.1, 0.5, 2.0, 10.0, 60.0):
        configs.append(("face_vertex_nside8192", 45.0, SEAM_DEC, sc, 0.8, 8192))
    # nside / pixfrac 维度
    for ns in (512, 4096, 16384):
        configs.append((f"face_vertex_nside{ns}", 45.0, SEAM_DEC, 0.04, 0.8, ns))
    for pf in (0.5, 1.0):
        configs.append((f"face_vertex_pf{pf}", 45.0, SEAM_DEC, 0.04, pf, 8192))
    # 其它位置类
    configs.append(("face_seam_ra0", 0.0, SEAM_DEC, 0.04, 0.8, 8192))
    configs.append(("face_seam_ra90", 90.0, SEAM_DEC, 0.04, 0.8, 8192))
    configs.append(("polar_exact_90", 0.0, 90.0, 0.04, 0.8, 8192))
    # 叶边中点（用 nside=8192 的两个相邻叶心的大圆中点）
    p0 = 8192 * 8192 // 4
    v = []
    for ip in (p0 * 4 + 0, p0 * 4 + 1):
        th, ph = hp.pix2ang(8192, ip, nest=True)
        v.append(np.array([math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th)]))
    m = unit(v[0] + v[1])
    configs.append(("leaf_edge_mid_8192", math.degrees(math.atan2(m[1], m[0])) % 360.0,
                    math.degrees(math.asin(m[2])), 0.04, 0.8, 8192))
    configs.append(("leaf_interior_generic", 123.456, -33.333, 0.04, 0.8, 8192))

    out = {
        "oracle": "s02_p3_oracle.py",
        "estimand": "epsilon_j = sum_p a_jp / A_drop,j - 1 (dimensionless)",
        "denominator": "drop's own spherical area A_drop,j (l'Huilier fan; double floor ~eps_mach/theta)",
        "leaf_boundary_model": "4-corner chord great circles (production model at nside>=256)",
        "independent_of_production": ["healpy leaf boundaries", "analytic TAN drop corners",
                                      "2D gnomonic exact clip", "l'Huilier fan area (double floor 2.9e-10 @0.032\")",
                                      "own 3D S-H implementations"],
        "healpy_version": hp.__version__,
        "results": [],
    }
    for (label, ra, dec, sc, pf, ns) in configs:
        r = evaluate(hp, ra, dec, sc, pf, ns, label)
        out["results"].append(r)
        print(f"{label:26s} ns={ns:6d} sc={sc:6.2f}\" pf={pf}  "
              f"exact={r['exact']['epsilon_j']:+.3e}  legacy={r['legacy']['epsilon_j']:+.3e}  "
              f"fixed={r['fixed']['epsilon_j']:+.3e}")
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
