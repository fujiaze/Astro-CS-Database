#!/usr/bin/env python3
"""Issue #92 —— 独立 WCSLIB/Astropy 坐标 oracle（不复用生产 WcsTransform 数学）。

输入: 真实 p1_wcs.json（含 wcs.sip）。输出: JSON 对照。

做四件事:
  A. astropy(WCSLIB) 前向  all_pix2world(64 samples x,y, 0-based)  vs 产品 samples[].{ra,dec}
  B. astropy 逆向  all_world2pix(samples ra,dec)  vs 产品 samples[].{x,y}
       B1 使用产品 AP/BP 作为初值 (astropy 会迭代收敛)
       B2 去掉 AP/BP (astropy 纯迭代)
  C. 一次性应用 AP/BP（复刻生产 C++ 的 m_sip_ap!=nullptr 分支, 不迭代）后的前向残差
       resid = (u + A(u,v)) - u_lin, 其中 u = u_lin + AP(u_lin, v_lin)
  D. numpy clean-room TAN+SIP（与 astropy 相互独立）复核 A/B。
"""
from __future__ import annotations

import argparse
import json
import math

import numpy as np


def sip_poly(coef, dx, dy, order):
    """coef[i*6+j] * dx^i * dy^j, i+j<=order（生产 wcs_transform evalSip 同式）。"""
    acc = np.zeros_like(dx, dtype=float)
    for i in range(order + 1):
        for j in range(order - i + 1):
            acc = acc + coef[i * 6 + j] * (dx ** i) * (dy ** j)
    return acc


def tan_world_to_intermediate(ra, dec, crval1, crval2):
    ra_r, dec_r = np.radians(ra), np.radians(dec)
    ra0, dec0 = math.radians(crval1), math.radians(crval2)
    s0, c0 = math.sin(dec0), math.cos(dec0)
    sd, cdd = np.sin(dec_r), np.cos(dec_r)
    dra = ra_r - ra0
    cosc = s0 * sd + c0 * cdd * np.cos(dra)
    xi = cdd * np.sin(dra) / cosc
    eta = (c0 * sd - s0 * cdd * np.cos(dra)) / cosc
    return np.degrees(xi), np.degrees(eta)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("p1_wcs", help="真实 p1_wcs.json")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    d = json.load(open(a.p1_wcs))
    w = d["wcs"]
    sip = w["sip"]
    order = int(sip["order"])
    ap_order = int(sip.get("ap_order", 0))
    crpix = np.array([float(w["crpix1"]), float(w["crpix2"])])
    crval = np.array([float(w["crval1"]), float(w["crval2"])])
    cd = np.array([[float(w["cd11"]), float(w["cd12"])],
                   [float(w["cd21"]), float(w["cd22"])]])
    A = np.array(sip["a"], dtype=float)
    B = np.array(sip["b"], dtype=float)
    AP = np.array(sip["ap"], dtype=float)
    BP = np.array(sip["bp"], dtype=float)

    samples = d["samples"]
    sx = np.array([s["x"] for s in samples], dtype=float)
    sy = np.array([s["y"] for s in samples], dtype=float)
    sra = np.array([s["ra"] for s in samples], dtype=float)
    sdec = np.array([s["dec"] for s in samples], dtype=float)

    result = {"source": a.p1_wcs, "n_samples": len(samples),
              "sip_order": order, "sip_ap_order": ap_order,
              "sip_a0": A[0], "sip_b0": B[0], "sip_ap0": AP[0], "sip_bp0": BP[0]}

    # ---------- astropy / WCSLIB ----------
    astro_note = None
    try:
        from astropy.wcs import WCS
        from astropy.wcs import Sip

        aw = WCS(naxis=2)
        aw.wcs.crpix = crpix
        aw.wcs.crval = crval
        aw.wcs.cd = cd
        aw.wcs.ctype = [str(w.get("ctype1", "RA---TAN-SIP")), str(w.get("ctype2", "DEC--TAN-SIP"))]
        shape = (max(order, ap_order) + 1, max(order, ap_order) + 1)
        A2 = np.zeros(shape); B2 = np.zeros(shape); AP2 = np.zeros(shape); BP2 = np.zeros(shape)
        for i in range(order + 1):
            for j in range(order - i + 1):
                A2[i, j] = A[i * 6 + j]; B2[i, j] = B[i * 6 + j]
        for i in range(ap_order + 1):
            for j in range(ap_order - i + 1):
                AP2[i, j] = AP[i * 6 + j]; BP2[i, j] = BP[i * 6 + j]
        aw.sip = Sip(A2, B2, AP2, BP2, crpix)

        # 参考点自检: 0-based (crpix-1) 应给出 crval
        ra0, dec0 = aw.all_pix2world(crpix[0] - 1.0, crpix[1] - 1.0, 0)
        result["astropy_center_check_deg"] = [float(ra0), float(dec0)]
        result["astropy_center_err_arcsec"] = float(
            np.hypot((ra0 - crval[0]) * 3600 * math.cos(math.radians(crval[1])),
                     (dec0 - crval[1]) * 3600))

        # A. 前向 (含 SIP A/B)
        fra, fdec = aw.all_pix2world(sx, sy, 0)
        ferr = np.hypot((fra - sra) * 3600 * np.cos(np.radians(sdec)),
                        (fdec - sdec) * 3600)
        result["A_forward_astropy"] = {
            "max_err_arcsec": float(np.max(ferr)),
            "median_err_arcsec": float(np.median(ferr)),
        }

        # B1. 逆向 (AP/BP 作为初值 + astropy 迭代)
        ix, iy = aw.all_world2pix(sra, sdec, 0)
        r1 = np.hypot(ix - sx, iy - sy)
        result["B1_inverse_astropy_with_apbp_px"] = {
            "max": float(np.max(r1)), "median": float(np.median(r1))}

        # B2. 逆向（去掉 AP/BP, astropy 纯迭代）
        aw2 = WCS(naxis=2)
        aw2.wcs.crpix = crpix; aw2.wcs.crval = crval; aw2.wcs.cd = cd
        aw2.wcs.ctype = [str(w.get("ctype1", "RA---TAN-SIP")), str(w.get("ctype2", "DEC--TAN-SIP"))]
        aw2.sip = Sip(A2, B2, None, None, crpix)
        jx, jy = aw2.all_world2pix(sra, sdec, 0)
        r2 = np.hypot(jx - sx, jy - sy)
        result["B2_inverse_astropy_nosip_iterated_px"] = {
            "max": float(np.max(r2)), "median": float(np.median(r2))}
    except Exception as e:  # pragma: no cover
        astro_note = f"{type(e).__name__}: {e}"
    result["astropy_note"] = astro_note

    # ---------- C. 一次性 AP/BP（复刻生产 C++ 分支）----------
    cd_det = cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0]
    cd_inv = np.array([[cd[1, 1], -cd[0, 1]], [-cd[1, 0], cd[0, 0]]]) / cd_det
    xi, eta = tan_world_to_intermediate(sra, sdec, crval[0], crval[1])
    u = cd_inv[0, 0] * xi + cd_inv[0, 1] * eta
    v = cd_inv[1, 0] * xi + cd_inv[1, 1] * eta

    def one_shot(ap_ord):
        """生产 C++ 的一次性分支。注意 WcsTransform 只有一个 m_sip_order,
        对 AP/BP 也用 sip_order 求值 (pc_api 只传 sip_order, ap_order 丢失)。"""
        dx = u + sip_poly(AP, u, v, ap_ord)
        dy = v + sip_poly(BP, u, v, ap_ord)
        # C++ 前向残差门用的也是 m_sip_order
        check_x = dx + sip_poly(A, dx, dy, order)
        check_y = dy + sip_poly(B, dx, dy, order)
        resid = np.hypot(check_x - u, check_y - v)
        perr = np.hypot(dx + (crpix[0] - 1.0) - sx, dy + (crpix[1] - 1.0) - sy)
        return resid, perr

    resid3, perr3 = one_shot(order)          # 生产实际: AP 截断到 sip_order=3
    resid5, perr5 = one_shot(ap_order)       # 若按 ap_order=5 求值
    result["C_oneshot_apbp"] = {
        "as_production_ap_evaluated_at_sip_order": {
            "ap_eval_order": order,
            "residual_px_max": float(np.max(resid3)),
            "residual_px_median": float(np.median(resid3)),
            "n_exceed_gate_1e-7": int(np.sum(resid3 > 1e-7)),
            "predicted_pixel_err_px_max": float(np.max(perr3)),
            "predicted_pixel_err_px_median": float(np.median(perr3)),
        },
        "if_ap_evaluated_at_ap_order": {
            "ap_eval_order": ap_order,
            "residual_px_max": float(np.max(resid5)),
            "residual_px_median": float(np.median(resid5)),
            "n_exceed_gate_1e-7": int(np.sum(resid5 > 1e-7)),
            "predicted_pixel_err_px_max": float(np.max(perr5)),
        },
        "gate_tol_px": 1e-7,
        "n_samples": int(len(resid3)),
    }
    # 参考点解析恒等式: 在切点 (u,v)=(0,0), 一次性 AP 给出 (AP0,BP0),
    # 前向 A(AP0,BP0)≈0 ⇒ 残差 = |AP0| (或 |BP0|)
    result["C_center_residual_identity"] = {
        "abs_ap0": float(abs(AP[0])), "abs_bp0": float(abs(BP[0])),
        "gt_gate": bool(max(abs(AP[0]), abs(BP[0])) > 1e-7),
    }

    # ---------- D. numpy clean-room（独立于 astropy 的 TAN+SIP）----------
    # 前向: pixel(0-based) -> ra/dec
    ddx = sx - (crpix[0] - 1.0)
    ddy = sy - (crpix[1] - 1.0)
    uu = ddx + sip_poly(A, ddx, ddy, order)
    vv = ddy + sip_poly(B, ddx, ddy, order)
    xii = cd[0, 0] * uu + cd[0, 1] * vv
    etai = cd[1, 0] * uu + cd[1, 1] * vv
    xr, er = np.radians(xii), np.radians(etai)
    rho = np.hypot(xr, er)
    s0, c0 = math.sin(math.radians(crval[1])), math.cos(math.radians(crval[1]))
    c = np.arctan(rho); sc, cc = np.sin(c), np.cos(c)
    with np.errstate(invalid="ignore", divide="ignore"):
        sdec_c = cc * s0 + er * sc * c0 / rho
        dec_c = np.arcsin(np.clip(sdec_c, -1, 1))
        dra_c = np.arctan2(xr * sc, rho * c0 * cc - er * s0 * sc)
    ra_c = np.degrees(math.radians(crval[0]) + dra_c)
    dec_c = np.degrees(dec_c)
    m = rho > 1e-12
    err_c = np.hypot((ra_c[m] - sra[m]) * 3600 * np.cos(np.radians(sdec[m])),
                     (dec_c[m] - sdec[m]) * 3600)
    result["D_numpy_forward_vs_samples_arcsec"] = {
        "max": float(np.max(err_c)), "median": float(np.median(err_c))}

    json.dump(result, open(a.out, "w"), indent=2, ensure_ascii=False)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
