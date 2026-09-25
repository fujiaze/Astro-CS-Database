# -*- coding: utf-8 -*-
"""
AUD-204 / T6 : (1) 球面残差 delta = A_drop/(pf^2 A_pixel) - 1 的独立复算（对文档二阶式）
               (2) k = D_p/N_p 与 pf^2 的关系（含 delta 一阶展开）
               (3) hips_profile=1 的 covered_area u8 量化对 signal/variance 的相对误差
               (4) 56.25% / 144.14% / 0.48 星等三个数的事实基础
"""
import json
import math

import numpy as np
import importlib.util

spec = importlib.util.spec_from_file_location("t4", "t4_three_way_perleaf.py")
t4 = importlib.util.module_from_spec(spec)
import sys
sys.modules["t4"] = t4
spec.loader.exec_module(t4)
PI = math.pi
PA = t4.PA


def quad_area(ra0, dec0, scale_arcsec, pf, rot_deg=0.0, off_px=(0.0, 0.0)):
    T = t4.gnomonic_basis(ra0, dec0)
    half = 0.5 * pf * math.radians(scale_arcsec / 3600.0)
    d = 0.5 * math.radians(scale_arcsec / 3600.0)
    # 像元中心偏移 off_px（像素单位）-> 直接平移切平面坐标
    a, b = math.radians(rot_deg), None
    cs = [(-half, -half), (half, -half), (half, half), (-half, half)]
    pts = []
    for (x, y) in cs:
        xr = math.cos(a) * x - math.sin(a) * y + math.radians(off_px[0] * scale_arcsec / 3600.0)
        yr = math.sin(a) * x + math.cos(a) * y + math.radians(off_px[1] * scale_arcsec / 3600.0)
        p = T[0] + yr * T[1] + xr * T[2]
        pts.append(p / np.linalg.norm(p))
    return PA(np.array(pts))


def part12():
    rows = []
    for th in (0.2, 2.0, 6.3, 60.0, 300.0):
        for pf in (0.8,):
            Ap = quad_area(40.0, 10.0, th, 1.0)
            Ad = quad_area(40.0, 10.0, th, pf)
            d_meas = Ad / (pf * pf * Ap) - 1.0
            t = math.radians(th / 3600.0)
            ana = (1 - pf * pf) * t * t * 0.25            # r_c=0 的主项
            rows.append({"theta_arcsec": th, "pixfrac": pf, "A_pixel_sr": Ap,
                         "A_drop_sr": Ad, "delta_measured": d_meas,
                         "delta_doc_leading_r0": ana,
                         "ratio_meas_over_doc": (d_meas / ana) if ana else None})
    # 离轴：xi_c 依赖（同一 th，把像元中心沿 e1 移 n 像素）
    off = []
    for n in (0, 5, 20, 100):
        Ap = quad_area(40.0, 10.0, 300.0, 1.0, off_px=(n, 0.0))
        Ad = quad_area(40.0, 10.0, 300.0, 0.8, off_px=(n, 0.0))
        t = math.radians(300.0 / 3600.0)
        xi = math.radians(n * 300.0 / 3600.0)
        r2 = xi * xi
        ana = (1 - 0.64) * t * t * (0.25 / (1 + r2) - 0.625 * xi * xi / (1 + r2) ** 2)
        off.append({"off_px": n, "xi_c_rad": xi, "delta_measured": Ad / (0.64 * Ap) - 1.0,
                    "delta_doc_formula": ana,
                    "ratio": (Ad / (0.64 * Ap) - 1.0) / ana if ana else None})
    # k 的一阶：k = pf^2 (1 + <delta>)，逐源 a_jp 加权
    krow = {"pf": 0.8, "k_equals_pf2_if_planar": 0.64,
            "k_with_spherical_delta_at_2as": 0.64 * (1 + 8.46e-12),
            "k_with_spherical_delta_at_300as": 0.64 * (1 + 1.90e-7)}
    return {"delta_vs_theta": rows, "delta_vs_offaxis": off, "k_relation": krow}


def part3():
    """u8 covered_area 量化：sig_pub = F_p*k/A_cov，S_p = F_p*k/D_p
       => 相对误差 = D_p/A_cov - 1；A_cov = (round(255 sup)/255)A_cell。"""
    out = []
    for sup in (1.0, 0.64, 0.5, 0.25, 0.05, 0.01, 1 / 255.0, 0.4 / 255.0):
        q = round(255 * sup)
        acov = q / 255.0
        rel_sig = (sup / acov - 1.0) if acov > 0 else None
        out.append({"support_true": sup, "u8": q, "signal_rel_err": rel_sig,
                    "variance_rel_err": (2 * sup / acov - 2.0) if acov > 0 else None,
                    "coverage_lost": q == 0,
                    "mag_err_if_lost_or_biased": (2.5 * math.log10(sup / acov)
                                                  if acov > 0 else None)})
    # 均匀覆盖下（sup ~ U(0,1]）最大相对误差的包络 = 1/(510 sup)
    env = [{"sup": s, "max_rel_sig_envelope": 1.0 / (510.0 * s)}
           for s in (1.0, 0.64, 0.1, 0.01)]
    return {"per_value": out, "envelope": env}


def part4():
    pf = 0.8
    return {"signal_factor_1_over_pf2": 1 / pf ** 2,
            "signal_excess_percent": (1 / pf ** 2 - 1) * 100,
            "variance_factor_1_over_pf4": 1 / pf ** 4,
            "variance_excess_percent": (1 / pf ** 4 - 1) * 100,
            "mag_offset": 2.5 * math.log10(1 / pf ** 2)}


if __name__ == "__main__":
    r = {"p1_2": part12(), "p3": part3(), "p4": part4()}
    print(json.dumps(r["p4"], ensure_ascii=False))
    for row in r["p1_2"]["delta_vs_theta"]:
        print(json.dumps(row, ensure_ascii=False))
    for row in r["p1_2"]["delta_vs_offaxis"]:
        print(json.dumps(row, ensure_ascii=False))
    print(json.dumps(r["p1_2"]["k_relation"], ensure_ascii=False))
    for row in r["p3"]["per_value"]:
        print(json.dumps(row, ensure_ascii=False))
    print(json.dumps(r["p3"]["envelope"], ensure_ascii=False))
    with open("t6_out.json", "w", encoding="utf-8") as fh:
        json.dump(r, fh, indent=1, ensure_ascii=False)
