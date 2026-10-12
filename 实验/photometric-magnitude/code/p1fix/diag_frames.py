#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1 单帧测光诊断：逐帧读数表（只读 run/PHOT-FIX/p1_rerun/**）。

口径依据（已读）：
  · docs/ACSD_DESIGN.md §2.1：I_photo = k_photo·m(x,y)·I_cal；阶数由配置请求，数据不足才降阶。
  · docs/science/photometry/PHOTOMETRY.md §1/§3/§5：逐像素空间校准是正路；退化只允许两条
    （可辨识性不足 / 样本几何不足）；判据 = 施加后星点星等与 Gaia 残差散度。
  · docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md §13.1：r = log10(F_instr/F_syn)，
    scale = 10^(−location)，sigma = MAD/0.6744897501960817。
  · lib/algorithms/photometry/cpp/src/spatial_gain.h：**唯一**模型约定
      log10 m(x,y) = −Σ_j coef[j]·B̃_j(x̃,ỹ)，B̃ = B − center（加权中心化）
      基序 = [x̃, ỹ, x̃², x̃ỹ, ỹ²]；order≥3 被钳到 2
      noise_floor_dex = S·sqrt(mean_pix(B̃ᵀ H⁻¹ B̃))，S = MAD(r−L)/0.6744897501960817
    由该约定可得 r_pred = location + Σ_j coef[j]·B̃_j，
    本脚本用 **实测产物反证过该式**（重建残差 MAD 与产物 sigma_spatial_residual_dex
    相对差 2.2e-14）——见 docs/p1fix_process.md 的失败与订正记录。

输出：实验/photometric-magnitude/results/p1fix/diag_frames.json
"""
from __future__ import annotations
import json, math, sys
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


def basis_vec(xn, yn):
    """产物 basis 序：[x̃, ỹ, x̃², x̃ỹ, ỹ²]"""
    return [xn, yn, xn * xn, xn * yn, yn * yn]


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
            norm = sg.get("norm", {})
            xr, xsc = norm.get("x_ref", 0.0), norm.get("x_scale", 1.0)
            yr, ysc = norm.get("y_ref", 0.0), norm.get("y_scale", 1.0)
            coef = sg.get("coef", [])
            center = sg.get("center", [])
            order = sg.get("order", 0)

            # ── 口径 A：原始（空间场之前）逐星散度 MAD(r)/0.6745 ──
            r_used = [ri for ri, u in zip(r, used) if u]
            sigma_raw = mad_sigma(r_used)

            # ── 口径 B：空间场拟合后残差（按唯一实现约定复算）──
            e = []
            rad = []
            for ri, xi, yi, u in zip(r, xs, ys, used):
                if not u:
                    continue
                xn = (xi - xr) / xsc
                yn = (yi - yr) / ysc
                b = basis_vec(xn, yn)
                logm = -sum(c * (bj - cj) for c, bj, cj in zip(coef, b, center))
                if location is not None:
                    e.append(ri - (location - logm))
                rad.append(math.hypot(xn, yn))
            sigma_post = mad_sigma(e) if e else float("nan")
            rad_sorted = sorted(rad)

            def pct(q):
                return rad_sorted[min(len(rad_sorted) - 1, int(q * len(rad_sorted)))] if rad_sorted else float("nan")

            nf, nf_max = sg.get("noise_floor_dex"), sg.get("noise_floor_max_dex")
            rows.append({
                "tag": tag, "frame": fk,
                "status": sg.get("status"), "identifiable": sg.get("identifiable"),
                "order_requested": sg.get("order_requested"), "order": order,
                "rank_eff": sg.get("rank_eff"), "kappa": sg.get("kappa"),
                "coef_sigma": sg.get("coef_sigma_dex"),
                "field_ptp_dex": sg.get("field_ptp_dex"), "field_rms_dex": sg.get("field_rms_dex"),
                "n_stars": sg.get("n_stars"), "n_used": sg.get("n_used"),
                "n_outliers": sg.get("n_outliers"), "n_unidentified": sg.get("n_unidentified"),
                "coverage_bbox_frac": sg.get("coverage_bbox_frac"), "coverage_blocks": sg.get("coverage_blocks"),
                "degraded_reason": sg.get("degraded_reason"),
                "noise_floor_dex": nf, "noise_floor_max_dex": nf_max,
                "sigma_raw_dex": sigma_raw,
                "sigma_raw_dex_stored": pd.get("sigma_residual_dex"),
                "sigma_post_dex": sigma_post,
                "sigma_post_dex_stored": sg.get("sigma_spatial_residual_dex"),
                "ratio_post_over_noise": (sigma_post / nf) if nf else None,
                "ratio_post_over_noise_max": (sigma_post / nf_max) if nf_max else None,
                "ratio_raw_over_noise": (sigma_raw / nf) if nf else None,
                "improve_raw_over_post": (sigma_raw / sigma_post) if sigma_post else None,
                "k_photo": k_photo, "location": location,
                "n_matched": pd.get("n_matched"), "n_psf_domain": pd.get("n_psf_domain"),
                "n_psf_skipped": pd.get("n_psf_skipped"),
                "f_instr_domain": pd.get("f_instr_domain"),
                "radius_p10": pct(0.10), "radius_p50": pct(0.50),
                "radius_p90": pct(0.90), "radius_max": rad_sorted[-1] if rad_sorted else None,
            })
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "diag_frames.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False))
    print(f"frames={len(rows)} -> {OUT/'diag_frames.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
