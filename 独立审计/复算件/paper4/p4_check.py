"""AUDIT-06 D5 论文4 工作件：对"可正面主张"三条的独立自复算（纯 math + astropy-healpix 2.0.0 core）。
不 import 仓库任何代码；仓库严格只读。
跑法：cd "独立审计/复算件/paper4" && PYTHONDONTWRITEBYTECODE=1 python -B p4_check.py
"""
import json
import math

import numpy as np
import astropy.units as u
import astropy_healpix as _ah
from astropy_healpix import core as hc

PI = math.pi
kJ = PI / 3.0
out = {"tools": {"numpy": np.__version__, "astropy_healpix": _ah.__version__}}


def zphi(face, u_, v_):
    """(u,v)->(z=sin dec, phi)：ASTROCS_DESIGN §2.4 的等面积卡，face 0-3 北、4-7 赤道、8-11 南。"""
    if face <= 3:
        if u_ + v_ <= 1.0:
            return (2.0 / 3.0) * (u_ + v_), 0.25 * PI * (u_ - v_ + 1.0 + 2.0 * face)
        s = 2.0 - u_ - v_
        if s == 0.0:                       # 北极本身：phi 无定义（方向折叠），取该面参考方位
            return 1.0, 0.5 * PI * face
        return 1.0 - s * s / 3.0, 0.5 * PI * face + PI * (1.0 - v_) / (2.0 * s)
    if face <= 7:
        return (2.0 / 3.0) * (u_ + v_ - 1.0), 0.25 * PI * (u_ - v_ + 2.0 * (face - 4))
    if u_ + v_ >= 1.0:
        return (2.0 / 3.0) * (u_ + v_ - 2.0), 0.25 * PI * (u_ - v_ + 1.0 + 2.0 * (face - 8))
    s = u_ + v_
    if s == 0.0:                           # 南极本身
        return -1.0, 0.5 * PI * (face - 8)
    return -(1.0 - s * s / 3.0), 0.5 * PI * (face - 8) + PI * u_ / (2.0 * s)


def lonlat_deg(face, u_, v_):
    z, phi = zphi(face, u_, v_)
    return (math.degrees(phi) % 360.0, math.degrees(math.asin(max(-1.0, min(1.0, z)))))


def unit(face, u_, v_):
    z, phi = zphi(face, u_, v_)
    z = max(-1.0, min(1.0, z))
    st = math.sqrt(max(0.0, 1.0 - z * z))
    return np.array([st * math.cos(phi), st * math.sin(phi), z])


def ipix_of(face, u_, v_, nside):
    lo, la = lonlat_deg(face, u_, v_)
    return int(hc.lonlat_to_healpix(u.Quantity(lo, "deg"), u.Quantity(la, "deg"), nside, order="nested"))


# --------------------------------------------- 1. 雅可比：赤道支与极帽支分别数值求解
def jac_num(face, u_, v_, h=1e-7):
    za, pa = zphi(face, u_ + h, v_)
    zb, pb = zphi(face, u_ - h, v_)
    zc, pc = zphi(face, u_, v_ + h)
    zd, pd = zphi(face, u_, v_ - h)
    return abs((za - zb) / (2 * h) * (pc - pd) / (2 * h) - (zc - zd) / (2 * h) * (pa - pb) / (2 * h))


rng = np.random.default_rng(7)
eq = [jac_num(int(rng.integers(4, 8)), *rng.uniform(1e-3, 1 - 1e-3, 2)) for _ in range(4000)]
pol = []
while len(pol) < 4000:
    a, b = rng.uniform(0.02, 0.98, 2)
    if a + b <= 1.0 or (2.0 - a - b) < 1e-3:
        continue
    pol.append(jac_num(int(rng.integers(0, 4)), a, b))
out["jac_equatorial"] = {"n": len(eq), "max_rel_dev_vs_pi_over_3": float(max(abs(x - kJ) / kJ for x in eq))}
out["jac_polar_cap"] = {"n": len(pol), "max_rel_dev_vs_pi_over_3": float(max(abs(x - kJ) / kJ for x in pol))}
out["face_area_closure"] = {"twelve_times_pi_over_3_minus_4pi": 12 * kJ - 4 * PI}

# ------------------------------------- 2. 等面积基数 pi/(3N^2) 与第三方库 4pi/npix 对拍
base = {}
for N in (16, 64, 256, 512, 1024, 4096):
    mine = kJ / (N * N)
    theirs = float(hc.nside_to_pixel_area(N).to_value(u.sr))
    base[N] = {"bit_equal": bool(mine == theirs), "rel_diff": (mine - theirs) / theirs, "npix": 12 * N * N}
out["equal_area_base_vs_astropy"] = base

# --------------------------- 3. chart 方格 <-> NESTED 像素：双射（不重不漏）+ 格内不跨像素
bij = {}
for N in (16, 64):
    ids, straddle = [], 0
    for f in range(12):
        for i in range(N):
            for j in range(N):
                ip = ipix_of(f, (i + .5) / N, (j + .5) / N, N)
                ids.append(ip)
                if not all(ipix_of(f, (i + du) / N, (j + dv) / N, N) == ip
                           for du in (0.03, 0.5, 0.97) for dv in (0.03, 0.5, 0.97)):
                    straddle += 1
    ids = np.asarray(ids)
    bij[N] = {"cells": int(ids.size), "expected": 12 * N * N, "distinct_pixels": int(np.unique(ids).size),
              "cells_straddling_pixels": straddle, "id_range": [int(ids.min()), int(ids.max())]}
out["chart_cell_vs_nested_pixel"] = bij


# ---------------- 4. 叶的弦四边形（4 角 + 大圆弧边）：逐叶误差、全天求和、真曲线对照
def poly_solid_angle(pts):
    t = 0.0
    for k in range(1, len(pts) - 1):
        a, p, q = pts[0], pts[k], pts[k + 1]
        num = float(np.dot(a, np.cross(p, q)))
        den = 1.0 + float(np.dot(a, p)) + float(np.dot(p, q)) + float(np.dot(q, a))
        t += 2.0 * math.atan2(num, den)
    return abs(t)


def chord_leaf(f, i, j, N):
    return poly_solid_angle([unit(f, i / N, j / N), unit(f, (i + 1) / N, j / N),
                             unit(f, (i + 1) / N, (j + 1) / N), unit(f, i / N, (j + 1) / N)])


def curve_leaf(f, i, j, N, K=64):
    pts = []
    corners = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
    for e in range(4):
        p0, p1 = corners[e], corners[(e + 1) % 4]
        for s in range(K):
            t = s / K
            pts.append(unit(f, (p0[0] + t * (p1[0] - p0[0])) / N, (p0[1] + t * (p1[1] - p0[1])) / N))
    return poly_solid_angle(pts)


leaf = {}
for N in (16, 64):
    true_a = kJ / (N * N)
    rel, total, worst = [], 0.0, (9.0, None)
    for f in range(12):
        for i in range(N):
            for j in range(N):
                ar = chord_leaf(f, i, j, N)
                total += ar
                r = ar / true_a - 1.0
                rel.append(r)
                if r < worst[0]:
                    worst = (r, (f, i, j))
    rel = np.asarray(rel)
    f, i, j = worst[1]
    ca = curve_leaf(f, i, j, N)
    leaf[N] = {"nleaf": int(rel.size), "rel_min": float(rel.min()), "rel_max": float(rel.max()),
               "dec_at_rel_min_deg": lonlat_deg(f, (i + .5) / N, (j + .5) / N, )[1], "worst_cell": worst[1],
               "frac_abs_rel_gt_1e-6": float((np.abs(rel) > 1e-6).mean()),
               "median_abs_rel": float(np.median(np.abs(rel))),
               "sum_chord_minus_4pi": total - 4 * PI,
               "worst_chord_over_truecurve": chord_leaf(f, i, j, N) / ca,
               "worst_truecurve_over_base": ca / true_a}
    # 赤道一般叶对照（face 5 中心）
    fe, ie, je = 5, N // 2, N // 2
    cbe = curve_leaf(fe, ie, je, N)
    leaf[N]["equatorial_chord_over_truecurve"] = chord_leaf(fe, ie, je, N) / cbe
out["chord_quadrilateral_full_sky"] = leaf

with open("p4_out.json", "w", encoding="utf-8") as fh:
    json.dump(out, fh, ensure_ascii=False, indent=1)
print(json.dumps(out, ensure_ascii=False, indent=1))
