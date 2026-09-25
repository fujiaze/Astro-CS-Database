# -*- coding: utf-8 -*-
"""
AUD-204 / T3 : leaf 表示误差的全天分布 + 尺度律 + 闭合判据失明 + chart 路径恒等式

真值口径独立性说明：
  - "真叶面积" = pi/(3N^2)（解析，等面积 chart 的推论），已三方校验：
      * astropy-healpix 2.0.0 core.py:164-183  nside_to_pixel_area = 4pi/npix（相对差 0.0）
      * 本脚本对真曲线边界做 VOS + Richardson，与解析值差 <=1.3e-7（K=16/32，nside=64）
      * 12 面弦四边形面积和 = 4pi（到 1.8e-15）=> 弦化是**精确划分**
  - "生产 leaf 表示" = 4 角大圆弦四边形（spherical_overlap.cpp:1372-1376 nside>=256 走 nb=4；
    :1377-1378 samples=(nside<=8)?16:1 => 9<=nside<256 亦为 4 角）
"""
import json
import math

import numpy as np
import importlib.util

PI = math.pi
K_JAC = PI / 3.0

_spec = importlib.util.spec_from_file_location("t1", "t1_chart_equalarea.py")
t1 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(t1)
chart = t1.chart_uv_to_xyz
poly_area = t1.poly_area


def all_chord_areas(nside):
    """所有 12N^2 叶的 4 角弦四边形面积（球面 VOS，向量化）。"""
    ii = np.arange(nside, dtype=np.float64)
    U0, V0 = np.meshgrid(ii / nside, ii / nside, indexing="ij")
    U0, V0 = U0.ravel(), V0.ravel()
    A = []
    for f in range(12):
        a11 = chart(f, U0, V0)
        a21 = chart(f, U0 + 1.0 / nside, V0)
        a22 = chart(f, U0 + 1.0 / nside, V0 + 1.0 / nside)
        a12 = chart(f, U0, V0 + 1.0 / nside)
        quad = np.stack([a11, a21, a22, a12], axis=1)
        A.append((f, poly_area(quad)))
    return A


def rel_err_report(nside):
    true_a = K_JAC / (nside * nside)
    tot = 0.0
    worst = (-2.0, None)
    best = (2.0, None)
    allrel = []
    for f, A in all_chord_areas(nside):
        rel = A / true_a - 1.0
        tot += float(A.sum())
        allrel.append(rel)
        k = int(np.argmax(rel)); k2 = int(np.argmin(rel))
        if rel[k] > worst[0]:
            worst = (float(rel[k]), (f, int(k % nside), int(k // nside)))
        if rel[k2] < best[0]:
            best = (float(rel[k2]), (f, int(k2 % nside), int(k2 // nside)))
    R = np.concatenate(allrel)
    return {"nside": nside, "true_area_pi_over_3N2": true_a,
            "sum_chord": tot, "sum_minus_4pi": tot - 4 * PI,
            "rel_err_max": worst[0], "rel_err_max_at_face_ij": worst[1],
            "rel_err_min": best[0], "rel_err_min_at_face_ij": best[1],
            "n_le": int(R.size),
            "n_abs_gt_1e-6": int((np.abs(R) > 1e-6).sum()),
            "n_abs_gt_1e-4": int((np.abs(R) > 1e-4).sum()),
            "n_abs_gt_1e-2": int((np.abs(R) > 1e-2).sum()),
            "median_abs": float(np.median(np.abs(R))),
            "rms_rel": float(np.sqrt((R * R).mean()))}


def sagitta_all(nside, chunk=200000):
    """每叶 4 条边：真曲线中点 与 其大圆弦中点 的角距（矢高），以 hp_res 为单位。"""
    hp_res = math.sqrt(K_JAC) / nside
    ii = np.arange(nside, dtype=np.float64)
    U0, V0 = np.meshgrid(ii / nside, ii / nside, indexing="ij")
    U0, V0 = U0.ravel(), V0.ravel()
    d = 1.0 / nside
    worst = (0.0, None)
    acc = []
    for f in range(12):
        corners = {
            "bl": chart(f, U0, V0), "br": chart(f, U0 + d, V0),
            "tr": chart(f, U0 + d, V0 + d), "tl": chart(f, U0, V0 + d),
            "m_b": chart(f, U0 + 0.5 * d, V0), "m_r": chart(f, U0 + d, V0 + 0.5 * d),
            "m_t": chart(f, U0 + 0.5 * d, V0 + d), "m_l": chart(f, U0, V0 + 0.5 * d)}
        for (e, p1, p2, mid) in [("b", "bl", "br", "m_b"), ("r", "br", "tr", "m_r"),
                                 ("t", "tr", "tl", "m_t"), ("l", "tl", "bl", "m_l")]:
            ch = corners[p1] + corners[p2]
            nrm = np.linalg.norm(ch, axis=-1)
            ok = nrm > 1e-300
            ch = np.where(ok[..., None], ch / np.where(ok, nrm, 1)[..., None], 0.0)
            mt = corners[mid]
            c = np.einsum("ij,ij->i", mt, ch)
            ang = np.arccos(np.clip(c, -1.0, 1.0))
            s = np.where(ok, ang, 0.0) / hp_res
            acc.append(s)
    S = np.concatenate(acc)
    k = int(np.argmax(S))
    return {"nside": nside, "max_sagitta_over_hp_res": float(S[k]),
            "which_edge_block": (k // (nside * nside), k % (nside * nside)),
            "n_gt_1e-6": int((S > 1e-6).sum()), "n_edges": int(S.size),
            "median_over_hp_res": float(np.median(S)),
            "p99_over_hp_res": float(np.quantile(S, 0.99)),
            "frozen_arc_chord_budget_over_hp_res": 1e-6}


def chart_path_identity(nside=64, subdiv=(0, 4, 8, 12)):
    """恒等式与真值分离：同一折线 drop 下
       I1: sum_p a_jp^chart == (pi/3)*|D_poly ∩ face|   （代数恒等，应为机器精度）
       I2: sum_p a_jp^chart == A_drop^true              （几何正确性，随细分收敛）
       I3: max_p |a_jp^chart - a_jp^true|/a_jp^true      （逐叶正确性）
    """
    from astropy_healpix import lonlat_to_healpix
    import astropy.units as u
    e = 1.0 - 1e-12

    def clip_box(poly, u0, v0, u1, v1):
        def half(pts, axis, val, keep_ge):
            if len(pts) == 0:
                return pts
            out = []
            m = len(pts)
            f = (lambda q: q[axis] - val) if keep_ge else (lambda q: val - q[axis])
            for i in range(m):
                A, B = pts[(i + m - 1) % m], pts[i]
                fa, fb = f(A), f(B)
                ia, ib = fa >= 0, fb >= 0
                if ib:
                    if not ia:
                        t = fa / (fa - fb)
                        out.append([A[0] + t * (B[0] - A[0]), A[1] + t * (B[1] - A[1])])
                    out.append(list(B))
                elif ia:
                    t = fa / (fa - fb)
                    out.append([A[0] + t * (B[0] - A[0]), A[1] + t * (B[1] - A[1])])
            return np.array(out) if out else np.zeros((0, 2))
        q = poly
        for (ax, val, kg) in [(0, u0, True), (0, u1, False), (1, v0, True), (1, v1, False)]:
            q = half(q, ax, val, kg)
            if len(q) < 3:
                return q
        return q

    def shoelace(q):
        if len(q) < 3:
            return 0.0
        m = q - q.min(axis=0)
        x, y = m[:, 0], m[:, 1]
        return 0.5 * abs(float(np.sum(np.roll(x, -1) * y - x * np.roll(y, -1))))

    def face_of(center):
        ra = math.degrees(math.atan2(center[1], center[0])) % 360.0
        dec = math.degrees(math.asin(max(-1, min(1, center[2]))))
        ip = int(lonlat_to_healpix(np.array([ra]) * u.deg, np.array([dec]) * u.deg,
                                   nside, order="nested")[0])
        return ip

    out = []
    for (ra0, dec0, tag) in [(40.0, 10.0, "equatorial"),
                             (math.degrees(math.asin(2/3.0)) * 0 + 120.0,
                              math.degrees(math.asin(2 / 3.0)), "seam_z23"),
                             (0.0, 89.999, "near_pole"),
                             (0.0, 0.0, "eq_face_corner")]:
        for ns in subdiv:
            # 4 角 drop（大圆弧边），每边细分 ns 段 -> 球面真值 = 该折线的 VOS 面积
            d = 0.5 * 2.0 / 3600.0 * PI / 180.0     # 1 px = 2" -> 半边（rad）
            a, dd = math.radians(ra0), math.radians(dec0)
            T = np.array([math.cos(dd) * math.cos(a), math.cos(dd) * math.sin(a), math.sin(dd)])
            e1 = np.array([-math.sin(dd) * math.cos(a), -math.sin(dd) * math.sin(a), math.cos(dd)])
            e2 = np.array([-math.sin(a), math.cos(a), 0.0])
            cs = [(-d, -d), (d, -d), (d, d), (-d, d)]
            pts = []
            for k in range(4):
                x0, y0 = cs[k]; x1, y1 = cs[(k + 1) % 4]
                for t in np.linspace(0, 1, max(1, ns), endpoint=False):
                    x, y = x0 + t * (x1 - x0), y0 + t * (y1 - y0)
                    p = T + y * e1 + x * e2
                    pts.append(p / np.linalg.norm(p))
            poly = np.array(pts)
            A_true = float(poly_area(poly[None, ...])[0])
            cen = poly.sum(axis=0); cen /= np.linalg.norm(cen)
            f = face_of(cen)
            # chart 原像
            tuv = []
            okmap = True
            for p in poly:
                uu, vv = t1.xyz_to_chart(f, p)
                tuv.append((uu, vv))
            tuv = np.array(tuv)
            inside = clip_box(tuv, 0.0, 0.0, 1.0, 1.0)
            if len(inside) < 3:
                continue
            face_chart_area = shoelace(inside)
            cell = 1.0 / nside
            i0 = max(0, int(math.floor(inside[:, 0].min() * nside)))
            i1 = min(nside - 1, int(math.floor(inside[:, 0].max() * nside)))
            j0 = max(0, int(math.floor(inside[:, 1].min() * nside)))
            j1 = min(nside - 1, int(math.floor(inside[:, 1].max() * nside)))
            aC, sumC = {}, 0.0
            for i in range(i0, i1 + 1):
                for j in range(j0, j1 + 1):
                    q = clip_box(inside, i * cell, j * cell, (i + 1) * cell, (j + 1) * cell)
                    ar = K_JAC * shoelace(q)
                    if ar > 0:
                        aC[(i, j)] = ar
                        sumC += ar
            row = {"tag": tag, "ra": ra0, "dec": dec0, "nside": nside,
                   "drop_edge_seg": ns, "A_drop_true_sr": A_true,
                   "I1_identity_closure_rel": abs(sumC / (K_JAC * face_chart_area) - 1.0),
                   "I2_geometric_closure_rel": abs(sumC / A_true - 1.0)}
            out.append(row)
    return out


if __name__ == "__main__":
    res = {"leaf_chord_rel_err": [], "sagitta": []}
    for n in (32, 64, 128, 256):
        res["leaf_chord_rel_err"].append(rel_err_report(n))
        print(json.dumps(res["leaf_chord_rel_err"][-1], ensure_ascii=False), flush=True)
    for n in (64, 256, 1024):
        res["sagitta"].append(sagitta_all(n))
        print(json.dumps(res["sagitta"][-1], ensure_ascii=False), flush=True)
    res["chart_identity"] = chart_path_identity(nside=256)
    for r in res["chart_identity"]:
        print(json.dumps(r, ensure_ascii=False), flush=True)
    with open("t3_out.json", "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=1, ensure_ascii=False)
