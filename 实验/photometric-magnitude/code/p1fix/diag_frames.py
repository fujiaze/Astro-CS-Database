#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1 单帧测光诊断：逐帧读数表（只读 run/PHOT-FIX/p1_rerun/**）。

口径依据：
  · docs/ACSD_DESIGN.md §2.1：I_photo = k_photo·m(x,y)·I_cal；阶数由配置请求，数据不足才降阶。
  · docs/science/photometry/PHOTOMETRY.md §1/§3/§5：逐像素空间校准是正路；退化只允许两条；
    判据 = 施加标定后星点星等与 Gaia 残差散度（尺度无关，不冻结绝对门限）。
  · docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md §13.1：r = log10(F_instr/F_syn)，
    scale = 10^(−location)，sigma = MAD/0.6744897501960817。

本脚本只做读数与复算，不改任何产物；输出 JSON 落
实验/photometric-magnitude/results/p1fix/diag_frames.json。

模型复算（用于独立核对，并支撑后续「阶数」实验）：
  r_i = location − log10 m_i,  log10 m_i = Σ_j c_j·(φ_j(x_i,y_i) − center_j)
  ⇒ 拟合后残差 e_i = r_i − (location − log10 m_i)
  其中 location 由 k_photo 反推：location = −log10(k_photo)。
"""
from __future__ import annotations
import json, math, os, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD_SCALE = 0.6744897501960817


def mad_sigma(v):
    if not v:
        return float("nan")
    m = sorted(v)[len(v) // 2]
    dev = sorted(abs(x - m) for x in v)
    return dev[len(dev) // 2] / MAD_SCALE


def basis_terms(xn, yn, order):
    """与产物 basis 同构：total degree 1..order 的单项式（不含常数项）。"""
    out = []
    for d in range(1, order + 1):
        for i in range(d + 1):
            j = d - i
            out.append((xn ** i) * (yn ** j))
    return out


def main():
    rows = []
    for tag in ("t2", "t3"):
        p = SRC / tag / "p1_phot.json"
        if not p.exists():
            print("missing", p, file=sys.stderr)
            continue
        d = json.loads(p.read_text())
        sg_all, pd_all = d["spatial_gain"], d["photscale_detail"]
        for fk in sorted(sg_all):
            sg, pd = sg_all[fk], pd_all.get(fk, {})
            s = sg.get("samples", {})
            r = s.get("r_log10_finstr_over_fsyn", [])
            xs, ys = s.get("x", []), s.get("y", [])
            used = s.get("used", [1] * len(r))
            k_photo = pd.get("k_photo")
            location = (-math.log10(k_photo)) if (k_photo and k_photo > 0) else None

            # ── 口径 A：原始（空间场之前）的 r 散度 ──
            r_used = [ri for ri, u in zip(r, used) if u]
            sigma_raw = mad_sigma(r_used)

            # ── 口径 B：拟合后残差（按产物系数复算）──
            norm = sg.get("norm", {})
            xr, xsc = norm.get("x_ref", 0.0), norm.get("x_scale", 1.0)
            yr, ysc = norm.get("y_ref", 0.0), norm.get("y_scale", 1.0)
            coef, center, order = sg.get("coef", []), sg.get("center", []), sg.get("order", 0)
            e = []
            for ri, xi, yi, u in zip(r, xs, ys, used):
                if not u:
                    continue
                xn = (xi - xr) / xsc
                yn = (yi - yr) / ysc
                phi = basis_terms(xn, yn, order)
                logm = sum(c * (t - ct) for c, t, ct in zip(coef, phi, center))
                e.append(ri - ((location - logm) if location is not None else 0.0))
            # 复算残差围绕自身中位数取散度（gauge 使 log10 m 加权均值为 0，location 已含全局项）
            e_med = sorted(e)[len(e) // 2] if e else 0.0
            sigma_post = mad_sigma([x - e_med for x in e])
            stored_post = sg.get("sigma_spatial_residual_dex")

            nf = sg.get("noise_floor_dex")
            nf_max = sg.get("noise_floor_max_dex")
            # 星点在像面的半径分布（归一化到 [0,1] bbox 中心）
            rad = []
            for xi, yi, u in zip(xs, ys, used):
                if not u:
                    continue
                ux = (xi - xr) / xsc
                uy = (yi - yr) / ysc
                rad.append(math.hypot(ux, uy))
            rad_sorted = sorted(rad)
            def pct(q):
                return rad_sorted[min(len(rad_sorted) - 1, int(q * len(rad_sorted)))] if rad_sorted else float("nan")

            rows.append({
                "tag": tag, "frame": fk,
                "status": sg.get("status"), "identifiable": sg.get("identifiable"),
                "order_requested": sg.get("order_requested"), "order": order,
                "rank_eff": sg.get("rank_eff"), "kappa": sg.get("kappa"),
                "field_ptp_dex": sg.get("field_ptp_dex"), "field_rms_dex": sg.get("field_rms_dex"),
                "n_stars": sg.get("n_stars"), "n_used": sg.get("n_used"),
                "n_outliers": sg.get("n_outliers"), "n_unidentified": sg.get("n_unidentified"),
                "coverage_bbox_frac": sg.get("coverage_bbox_frac"), "coverage_blocks": sg.get("coverage_blocks"),
                "degraded_reason": sg.get("degraded_reason"),
                "noise_floor_dex": nf, "noise_floor_max_dex": nf_max,
                "sigma_raw_dex_recomputed": sigma_raw,
                "sigma_raw_dex_stored": pd.get("sigma_residual_dex"),
                "sigma_post_dex_recomputed": sigma_post,
                "sigma_post_dex_stored": stored_post,
                "ratio_post_over_noise": (sigma_post / nf) if (nf and sigma_post == sigma_post) else None,
                "ratio_post_over_noise_max": (sigma_post / nf_max) if (nf_max and sigma_post == sigma_post) else None,
                "improve_factor_raw_over_post": (sigma_raw / sigma_post) if (sigma_post and sigma_post == sigma_post) else None,
                "k_photo": k_photo, "location": location,
                "n_matched": pd.get("n_matched"), "n_psf_domain": pd.get("n_psf_domain"),
                "n_psf_skipped": pd.get("n_psf_skipped"),
                "radius_p10": pct(0.10), "radius_p50": pct(0.50), "radius_p90": pct(0.90),
                "radius_max": rad_sorted[-1] if rad_sorted else None,
            })
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "diag_frames.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False))
    print(f"frames={len(rows)} -> {OUT/'diag_frames.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
