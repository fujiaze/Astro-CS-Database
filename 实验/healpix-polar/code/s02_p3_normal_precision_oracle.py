#!/usr/bin/env python3
"""S02 P3 独立几何 Oracle —— drop 边大圆法向的精度律与修前/修后判据（不复用生产代码）。

对象（与 docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」的逐像元完备性同物量）：
    epsilon_j = (sum_p a_jp) / A_drop,j - 1          [无量纲]
    a_jp = area(drop ∩ leaf_p)   [sr]，A_drop,j = drop 四角球面多边形面积 [sr]

本脚本**不链接生产库、不调用生产球面分割函数**，自行实现：
  · TAN（gnomonic）反投影角点构造
  · 球面凸多边形 S-H 裁剪（半空间 n·x >= 0；交点用点积线性插值稳定式）
  · **Van Oosterom & Strackee** 球面三角形扇剖分面积（对微小多边形无 acos 相消；
    另有 atan2 版 l'Huilier 互校）
  · 叶边界由 healpy.boundaries（独立第三方实现）给出
  · 可选 numpy.longdouble（x86 80-bit，eps≈1.1e-19）复算同一构造作**更高精度对照**

两条法向构造法（唯一差异）：
  A. legacy  ：n_j = normalize(cross(C_j, C_{j+1}))                ← 生产 HEAD 路径
  B. tangent ：alpha_j = C_j/(C_j·c) - c,
               m_j = cross(c, alpha_{j+1}-alpha_j) + cross(alpha_j, alpha_{j+1}),
               n_j = normalize(m_j)（按 n_j·c>0 定向）              ← 生产补丁路径

被检验的命题（P1–P3）见同目录 README/REPORT 章。

用法：
  python3 s02_p3_normal_precision_oracle.py [--nside 8192] [--scales 0.02,0.04,...] [--json out.json]
"""
import argparse
import json
import math
import sys

import numpy as np

D2R = math.pi / 180.0
ARCSEC = D2R / 3600.0
SEAM_DEC = math.degrees(math.asin(2.0 / 3.0))
EPS64 = float(np.finfo(np.float64).eps)
EPS80 = float(np.finfo(np.longdouble).eps)


def _nd(x):
    return np.longdouble(x)


def tan_corner(ra0_deg, dec0_deg, xi, eta, T=np.float64):
    """TAN（gnomonic）反投影：切平面坐标 (xi, eta) [rad] → 单位向量。"""
    ra = T(ra0_deg) * T(D2R)
    dec = T(dec0_deg) * T(D2R)
    r = np.sqrt(T(xi) * T(xi) + T(eta) * T(eta))
    p0 = np.array([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra), np.sin(dec)], dtype=T)
    if float(r) == 0.0:
        return p0
    c = np.arctan(r)
    ux, uy = T(xi) / r, T(eta) / r
    e_hat = np.array([-np.sin(ra), np.cos(ra), T(0)], dtype=T)
    n_hat = np.array([-np.sin(dec) * np.cos(ra), -np.sin(dec) * np.sin(ra), np.cos(dec)], dtype=T)
    p = p0 * np.cos(c) + (ux * e_hat + uy * n_hat) * np.sin(c)
    nrm = np.sqrt(np.sum(p * p))
    return p / nrm


def _angdist(a, b, T=np.float64):
    """atan2 版角距（对微小角无 acos 相消）。"""
    cr = np.cross(a, b)
    return np.arctan2(np.sqrt(np.sum(cr * cr)), np.sum(a * b))


def area_vos_fan(poly):
    """Van Oosterom & Strackee 扇剖分：Ω = Σ 2 atan2(det(c,a,b), 1 + c·a + a·b + b·c)。

    只用于 **较大** 多边形（θ ≳ 1e-4 rad）的互校：其 det 为 O(θ²) 的乘积之差，
    绝对误差 ~ε_mach ⇒ 相对误差 ~ε_mach/θ²，微小球面多边形上不可用。
    """
    P = [np.asarray(p) for p in poly]
    if len(P) < 3:
        return 0.0
    c = np.sum(P, axis=0)
    n = float(np.sqrt(np.sum(c * c)))
    if n < 1e-300:
        return 0.0
    c = c / n
    total = 0.0
    for i in range(len(P)):
        a, b = P[i], P[(i + 1) % len(P)]
        det = float(np.dot(c, np.cross(a, b)))
        den = float(1.0 + np.dot(c, a) + np.dot(a, b) + np.dot(b, c))
        total += 2.0 * math.atan2(det, den)
    return abs(total)


def area_gnomonic_tangent(poly):
    """以多边形质心 c 为投影中心的 gnomonic 切平面有向面积：
        alpha_i = P_i/(P_i·c) - c   (∈ 切平面)， A = 1/2 |Σ_i (alpha_i × alpha_{i+1})·c|。
    对微小多边形无相消（alpha ~ θ，叉积 ~ θ²），相对误差 ~ε_mach；
    与真球面面积相差相对 O(θ²)（对 sum a_p 与 A_drop 同阶同结构，在 ε 中只留下 O(θ⁴)）。
    """
    P = [np.asarray(p) for p in poly]
    if len(P) < 3:
        return 0.0
    c = np.sum(P, axis=0)
    n = float(np.sqrt(np.sum(c * c)))
    if n < 1e-300:
        return 0.0
    c = c / n
    al = [p / np.dot(p, c) - c for p in P]
    s = 0.0
    for i in range(len(al)):
        s += float(np.dot(np.cross(al[i], al[(i + 1) % len(al)]), c))
    return 0.5 * abs(s)


def area_lhuilier_fan(poly):
    """atan2 版 l'Huilier 球面三角形扇剖分（与 VOS 互校）。"""
    P = [np.asarray(p) for p in poly]
    if len(P) < 3:
        return 0.0
    c = np.sum(P, axis=0)
    n = float(np.sqrt(np.sum(c * c)))
    if n < 1e-300:
        return 0.0
    c = c / n
    total = 0.0
    for i in range(len(P)):
        a, b = P[i], P[(i + 1) % len(P)]
        A = _angdist(b, c)
        B = _angdist(a, c)
        C = _angdist(a, b)
        s = 0.5 * (A + B + C)
        t = math.tan(s / 2) * math.tan((s - A) / 2) * math.tan((s - B) / 2) * math.tan((s - C) / 2)
        total += 4.0 * math.atan(math.sqrt(max(t, 0.0)))
    return abs(total)


def edge_normals_legacy(poly, center, T=np.float64):
    ns = []
    n = len(poly)
    for i in range(n):
        nt = np.cross(poly[i], poly[(i + 1) % n])
        nl = np.sqrt(np.sum(nt * nt))
        if float(nl) < 1e-300:
            continue
        nt = nt / nl
        if float(np.dot(nt, center)) < 0:
            nt = -nt
        ns.append(nt)
    return ns


def edge_normals_tangent(poly, center, T=np.float64):
    ns = []
    alpha = []
    for p in poly:
        d = np.dot(p, center)
        alpha.append(p / d - center)
    n = len(poly)
    for i in range(n):
        a1, a2 = alpha[i], alpha[(i + 1) % n]
        m = np.cross(center, a2 - a1) + np.cross(a1, a2)
        ml = np.sqrt(np.sum(m * m))
        if float(ml) < 1e-300:
            continue
        m = m / ml
        if float(np.dot(m, center)) < 0:
            m = -m
        ns.append(m)
    return ns


def sh_clip(subject, normals, T=np.float64):
    out = [np.asarray(p, dtype=T) for p in subject]
    for nvec in normals:
        if len(out) < 3:
            return []
        inp, out = out, []
        m = len(inp)
        for i in range(m):
            A, B = inp[i], inp[(i + 1) % m]
            da, db = float(np.dot(A, nvec)), float(np.dot(B, nvec))
            ia, ib = da >= 0.0, db >= 0.0
            if ia:
                out.append(A)
            if ia != ib:
                X = (da * B - db * A) / T(da - db)
                out.append(X / np.sqrt(np.sum(X * X)))
    return out


def measure(nside, scale_arcsec, pixfrac, mode, T=np.float64, area=area_gnomonic_tangent):
    import healpy as hp
    ra0, dec0 = 45.0, SEAM_DEC
    half = T(0.5) * T(pixfrac) * T(scale_arcsec) * T(ARCSEC)
    offs = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    drop = [tan_corner(ra0, dec0, T(x) * half, T(y) * half, T) for (x, y) in offs]
    drop = [p / np.sqrt(np.sum(p * p)) for p in drop]
    A_drop = area(drop)
    c_drop = np.sum(drop, axis=0)
    c_drop = c_drop / np.sqrt(np.sum(c_drop * c_drop))
    dn = (edge_normals_legacy if mode == "legacy" else edge_normals_tangent)(drop, c_drop, T)
    # 候选叶：以顶点为中心、覆盖整个 drop 的保守圆盘查询（独立于生产候选枚举）
    rad = math.atan(float(half) * math.sqrt(2.0)) * 1.05 + 1e-12
    leafs = [int(x) for x in hp.query_disc(nside, np.asarray(c_drop, dtype=np.float64), rad,
                                           nest=True, inclusive=True)]
    leafs = sorted(set(leafs))
    s = 0.0
    per = []
    for ip in leafs:
        b = hp.boundaries(nside, ip, step=1, nest=True)
        poly = [np.array([b[0][k], b[1][k], b[2][k]], dtype=T) for k in range(b.shape[1])]
        poly = [p / np.sqrt(np.sum(p * p)) for p in poly]
        clipped = sh_clip(poly, dn, T)
        a = area(clipped) if len(clipped) >= 3 else 0.0
        s += a
        per.append({"ipix": int(ip), "area_sr": float(a), "a_over_Adrop": float(a) / float(A_drop)})
    return {"nside": nside, "scale_arcsec": scale_arcsec, "pixfrac": pixfrac, "mode": mode,
            "dtype": np.dtype(T).name, "theta_rad": pixfrac * scale_arcsec * ARCSEC,
            "A_drop_sr": float(A_drop), "n_leaves": len(leafs),
            "sum_over_Adrop": float(s) / float(A_drop),
            "epsilon_j": float(s) / float(A_drop) - 1.0, "per_leaf": per}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nside", type=int, default=8192)
    ap.add_argument("--scales", default="0.02,0.04,0.1,0.5,2,10")
    ap.add_argument("--json", default=None)
    args = ap.parse_args()
    scales = [float(x) for x in args.scales.split(",")]

    out = {"oracle": "s02_p3_normal_precision_oracle/v1",
           "epsilon_definition": "epsilon_j = sum_p a_jp/A_drop,j - 1 (dimensionless)",
           "area_oracle": "Van Oosterom & Strackee fan (healpy boundaries, independent of production)",
           "config": {"nside": args.nside, "ra_deg": 45.0, "dec_deg": SEAM_DEC,
                      "position": "face-0 center = NESTED 4-leaf common vertex", "pixfrac": 0.8},
           "eps_mach_f64": EPS64, "eps_mach_f80": EPS80, "rows": []}

    print(f"independent oracle  nside={args.nside}  RA=45  dec={SEAM_DEC:.12f}  (face-0 center = 4-leaf vertex)")
    print(f"{'scale\"':>8} {'theta_rad':>11} {'eps_legacy':>13} {'eps_tangent':>13} "
          f"{'legacy/(2em/th2)':>16} {'tangent/(2em/th)':>16} {'eps_tan_longdbl':>16} {'Adrop_vos/gnom-1':>16}")
    for sc in scales:
        r_leg = measure(args.nside, sc, 0.8, "legacy")
        r_tan = measure(args.nside, sc, 0.8, "tangent")
        r_tan_ld = measure(args.nside, sc, 0.8, "tangent", T=np.longdouble)
        # A_drop 两路独立面积互校（VOS 扇 vs gnomonic 切平面）；微小尺度上 VOS 自身失稳，仅作参考
        half = 0.5 * 0.8 * sc * ARCSEC
        drop = [tan_corner(45.0, SEAM_DEC, x * half, y * half) for (x, y) in
                [(-1, -1), (1, -1), (1, 1), (-1, 1)]]
        drop = [p / np.linalg.norm(p) for p in drop]
        adrop_vos = area_vos_fan(drop)
        adrop_gnom = area_gnomonic_tangent(drop)
        th = r_leg["theta_rad"]
        pred_leg = 2 * EPS64 / th ** 2
        pred_tan = 2 * EPS64 / th
        print(f"{sc:8.3f} {th:11.4e} {r_leg['epsilon_j']:+13.5e} {r_tan['epsilon_j']:+13.5e} "
              f"{abs(r_leg['epsilon_j'])/pred_leg:16.3f} {abs(r_tan['epsilon_j'])/pred_tan:16.3f} "
              f"{r_tan_ld['epsilon_j']:+16.5e} {adrop_vos/adrop_gnom-1.0:+16.3e}")
        out["rows"].append({"scale_arcsec": sc,
                            "legacy_f64": r_leg, "tangent_f64": r_tan,
                            "tangent_longdouble": r_tan_ld,
                            "A_drop_vos_over_gnomonic_m1": adrop_vos / adrop_gnom - 1.0,
                            "pred_2emach_over_theta2": pred_leg, "pred_2emach_over_theta": pred_tan})

    if args.json:
        with open(args.json, "w") as f:
            json.dump(out, f, indent=2)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
