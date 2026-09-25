# -*- coding: utf-8 -*-
"""
AUD-204 / T5 : (a) chart 快速路径的 drop 侧折线收敛律
               (b) 完全覆盖叶的 support = D_p/A_cell 实测（弦 quad vs 真曲线叶）
               (c) 换源退化自测：注入错 Jacobian / 弦化 leaf / 关细分，谁会红
"""
import json
import math

import numpy as np
import importlib.util
import astropy.units as u
from astropy_healpix import lonlat_to_healpix

spec = importlib.util.spec_from_file_location("t4", "t4_three_way_perleaf.py")
t4 = importlib.util.module_from_spec(spec)
import sys
sys.modules["t4"] = t4          # 避免 __main__ 重跑
spec.loader.exec_module(t4)

PI, K_JAC = math.pi, math.pi / 3.0
chart, xyz_to_chart = t4.chart, t4.xyz_to_chart
PA, poly_area = t4.PA, t4.poly_area


def chart_path(drop, nside, subdiv, jac=K_JAC):
    """chart+鞋带+轴对齐裁剪；subdiv = 每条大圆弧额外细分段数（>=1 表示仅角点）。
       返回 {ipix: a} 与 chart 侧总面积（恒等式右端）。"""
    pts = []
    m = len(drop)
    for i in range(m):
        A, B = drop[i], drop[(i + 1) % m]
        ang = math.atan2(np.linalg.norm(np.cross(A, B)), float(np.dot(A, B)))
        pts.append(A)
        for k in range(1, subdiv):
            t = k / subdiv
            so = math.sin(ang)
            if so < 1e-300:
                continue
            w1, w2 = math.sin((1 - t) * ang) / so, math.sin(t * ang) / so
            p = A * w1 + B * w2
            pts.append(p / np.linalg.norm(p))
    poly = np.array(pts)
    faces = []
    for p in poly:
        f = t4.best_face_chart(p)[0]
        if f not in faces:
            faces.append(f)
    out, idsum = {}, 0.0
    cell = 1.0 / nside
    for f in faces:
        pr = np.array([xyz_to_chart(f, p)[:2] for p in poly])
        ins = t4.clip_box(pr, 0.0, 0.0, 1.0, 1.0)
        if len(ins) < 3:
            continue
        idsum += t4.shoelace(ins)
        i0 = max(0, int(math.floor(ins[:, 0].min() * nside)))
        i1 = min(nside - 1, int(math.floor(ins[:, 0].max() * nside)))
        j0 = max(0, int(math.floor(ins[:, 1].min() * nside)))
        j1 = min(nside - 1, int(math.floor(ins[:, 1].max() * nside)))
        for i in range(i0, i1 + 1):
            for j in range(j0, j1 + 1):
                q = t4.clip_box(ins, i * cell, j * cell, (i + 1) * cell, (j + 1) * cell)
                ar = jac * t4.shoelace(q)
                if ar <= 0:
                    continue
                c = chart(f, np.array([(i + .5) * cell]), np.array([(j + .5) * cell]))[0]
                ip = int(lonlat_to_healpix(
                    np.array([math.atan2(c[1], c[0]) % (2 * PI)]) * u.rad,
                    np.array([math.asin(max(-1.0, min(1.0, c[2])))]) * u.rad,
                    nside, order="nested")[0])
                out[ip] = out.get(ip, 0.0) + ar
    return out, jac * idsum


def truth_perleaf(drop, nside, ips, K=48):
    nrm = t4.clip_normals_of(drop)
    res = {}
    for ip in ips:
        o1 = t4.sh_clip_sphere(t4.leaf_curve(ip, nside, K), nrm)
        o2 = t4.sh_clip_sphere(t4.leaf_curve(ip, nside, 2 * K), nrm)
        v1 = PA(o1) if len(o1) >= 3 else 0.0
        v2 = PA(o2) if len(o2) >= 3 else 0.0
        res[ip] = (4 * v2 - v1) / 3.0
    return res


def part_a():
    rows = []
    for (tag, ra, dec, ns, sc) in [("eq", 40.0, 10.0, 32768, 6.3),
                                   ("seam_z23", 120.0, math.degrees(math.asin(2 / 3)),
                                    32768, 6.3),
                                   ("polarcap", 60.0, 78.0, 32768, 6.3)]:
        T = t4.gnomonic_basis(ra, dec)
        drop = t4.make_drop(*T, 0.5 * math.radians(sc / 3600.0), 1)
        A = PA(drop)
        truth = truth_perleaf(drop, ns, t4.candidates(drop, ns))
        for subdiv in (1, 2, 4, 8, 16, 32):
            aC, idsum = chart_path(drop, ns, subdiv)
            ks = sorted(set(list(aC) + list(truth)))
            C = np.array([aC.get(k, 0.0) for k in ks])
            Tr = np.array([truth.get(k, 0.0) for k in ks])
            with np.errstate(divide="ignore", invalid="ignore"):
                e = np.where(Tr > 1e-30, np.abs(C / Tr - 1.0), 0.0)
            rows.append({"pos": tag, "nside": ns, "drop_seg_per_edge": subdiv,
                         "cl_C": abs(C.sum() / A - 1.0),
                         "id_identity": abs(C.sum() / idsum - 1.0),
                         "pl_C_max": float(e.max()),
                         "mis_C": float(np.abs(C - Tr).sum() / (2 * A))})
            print(json.dumps(rows[-1]), flush=True)
    return rows


def part_b(nside=1024):
    """完全覆盖测试：用铺满的方形 drop 网格覆盖某叶，测 D_p/A_cell。"""
    hp_res = math.sqrt(K_JAC) / nside
    A_cell = K_JAC / (nside * nside)
    rows = []
    # 目标叶：极点叶 / 极冠内一般叶 / 赤道叶 / 缝上叶
    probes = []
    for (la, be, nm) in [(0.0, 90.0, "pole_leaf"), (math.radians(80), 1.2, "polar_cap"),
                         (math.radians(200), math.radians(10), "equatorial"),
                         (math.radians(120), math.asin(2 / 3) - 1e-6, "seam_z23")]:
        ip = int(lonlat_to_healpix(np.array([la]) * u.rad, np.array([be]) * u.rad,
                                   nside, order="nested")[0])
        probes.append((nm, ip))
    for (nm, ip) in probes:
        lon, lat = t4.healpix_to_lonlat(np.array([ip]), nside, dx=np.array([0.5]),
                                        dy=np.array([0.5]), order="nested")
        c_lon, c_lat = float(lon[0].rad), float(lat[0].rad)
        T = t4.gnomonic_basis(math.degrees(c_lon), math.degrees(c_lat))
        s = hp_res * 0.4
        tot = 0.0
        tot_true = 0.0
        n_ok = 0
        for iu in range(-3, 4):
            for iv in range(-3, 4):
                cen = T[0] + (iv * s) * T[1] + (iu * s) * T[2]
                cen = cen / np.linalg.norm(cen)
                # 以该点为切点建小方 drop（边长 0.8s，铺不满 -> 用 1.1s 保证重叠铺满）
                Tl = t4.gnomonic_basis(math.degrees(math.atan2(cen[1], cen[0])),
                                        math.degrees(math.asin(cen[2])))
                d = t4.make_drop(*Tl, 0.55 * s, 1)
                nrm = t4.clip_normals_of(d)
                # 弦 quad 口径（生产）
                q = t4.leaf_chord(ip, nside)
                r = t4.sh_clip_sphere(q, nrm)
                ap = PA(r) if len(r) >= 3 else 0.0
                # 真曲线口径
                o1 = t4.sh_clip_sphere(t4.leaf_curve(ip, nside, 48), nrm)
                o2 = t4.sh_clip_sphere(t4.leaf_curve(ip, nside, 96), nrm)
                v1 = PA(o1) if len(o1) >= 3 else 0.0
                v2 = PA(o2) if len(o2) >= 3 else 0.0
                at = (4 * v2 - v1) / 3.0
                tot += ap
                tot_true += at
                n_ok += 1
        rows.append({"probe": nm, "ipix": ip, "nside": nside,
                     "D_p_over_A_cell_full_cover": tot / A_cell,
                     "D_p_true_over_A_cell": tot_true / A_cell,
                     "support_ratio_chord_over_true": tot / tot_true,
                     "n_drops": n_ok})
        print(json.dumps(rows[-1]), flush=True)
    return rows


def part_c(nside=32768):
    """退化自测：同一配置下各注入谁会红。"""
    T = t4.gnomonic_basis(60.0, 78.0)
    drop = t4.make_drop(*T, 0.5 * math.radians(6.3 / 3600.0), 1)
    A = PA(drop)
    truth = truth_perleaf(drop, nside, t4.candidates(drop, nside))
    ks = sorted(set(list(truth) + list(t4.candidates(drop, nside))))
    Tr = np.array([truth.get(k, 0.0) for k in ks])
    out = {}
    # 基线
    aC, ids = chart_path(drop, nside, 8)
    C = np.array([aC.get(k, 0.0) for k in ks])
    # 注入 1：错 Jacobian pi/4
    aC2, ids2 = chart_path(drop, nside, 8, jac=PI / 4.0)
    C2 = np.array([aC2.get(k, 0.0) for k in ks])
    # 注入 2：关细分（纯 4 角弦）
    aC3, ids3 = chart_path(drop, nside, 1)
    C3 = np.array([aC3.get(k, 0.0) for k in ks])
    # 注入 3：leaf 弦化（生产表示）
    nrm = t4.clip_normals_of(drop)
    P = []
    for k in ks:
        r = t4.sh_clip_sphere(t4.leaf_chord(k, nside), nrm)
        P.append(PA(r) if len(r) >= 3 else 0.0)
    P = np.array(P)
    def rep(name, a):
        with np.errstate(divide="ignore", invalid="ignore"):
            e = np.where(Tr > 1e-30, np.abs(a / Tr - 1.0), 0.0)
        return {"variant": name, "closure_vs_A_drop": abs(a.sum() / A - 1.0),
                "perleaf_max": float(e.max()),
                "misassignment": float(np.abs(a - Tr).sum() / (2 * A)),
                "sum_over_true": a.sum() / Tr.sum() - 1.0}
    out["baseline_chart_seg8"] = rep("chart seg=8", C)
    out["inject_wrong_jacobian_pi/4"] = rep("J=pi/4", C2)
    out["inject_no_drop_subdiv"] = rep("chart seg=1", C3)
    out["production_chord_leaf"] = rep("leaf=4角弦(生产)", P)
    for k, v in out.items():
        print(json.dumps({k: v}), flush=True)
    return out


if __name__ == "__main__":
    res = {"a_chart_convergence": part_a(), "b_support_full_cover": part_b(),
           "c_degeneracy": part_c()}
    with open("t5_out.json", "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=1, ensure_ascii=False)
