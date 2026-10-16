#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05-A：P2 四臂解析 Oracle（独立真值 / 可复现 / 不 import 生产 lib/**）。

四臂（任务书「必须交付 1」）：
  A 白噪 + 光子噪声（背景已知）：电子域泊松 + 读噪前向；真值 F_ref 已知；
    估计量 GLS 幅度；68/90/95% 覆盖率与二项误差；有效独立样本数。
  B 自由背景（GLS）：同批像素自估常数背景 J=[P,1]，量化背景估计代价。
  C 线性 R 重采样（高相关 / 独立底）：C_out = R V R^T (+ eps^2 I)，
    完整协方差 vs 仅对角；方差比、覆盖率、近邻相关、条件数、N_eff、k_corr。
  D 点源 / 弥散源两种被估量：点源 PSF 权重 vs 面亮度逆方差权重；E 与水平偏差。

每个主张带一个**零效应阴性**（度量必须为 0）与一个**已知注入阳性**
（按先声明方向判红）。判红判据：覆盖率 68% 的 Wilson 95% 上界 < 标称（欠覆盖）。

用法：
  python3 s05_arms_oracle.py --out run/ACSDSDOC-02/S05/results/arms_oracle.json
"""
from __future__ import annotations

import argparse
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s05_common as C          # noqa: E402
import s05_drizzle_geom as G    # noqa: E402

SEED = 20261014
N_MC_A = 20000
N_MC_B = 8000
N_MC_C = 20000
ZP_ORACLE = 25.0          # 本 Oracle 内部声明量：F_ref = 10^(-0.4(m_ref-ZP))，无一手文献
FWHM_BETA = 4.0
NOMINAL_68 = 0.6826894921370859


def flux_from_mag(m, zp=ZP_ORACLE):
    return float(10.0 ** (-0.4 * (m - zp)))


def make_stamp(fwhm_px):
    return C.moffat_stamp(fwhm_px, beta=FWHM_BETA)


# ------------------------------------------------------------------ 臂 A

def arm_A():
    mags = [14.0, 16.0, 18.0, 20.0, 22.0]
    skies = [10.0, 200.0, 2000.0]
    fwhms = [1.5, 2.0, 3.0]
    rows = []
    for m in mags:
        F = flux_from_mag(m)
        for sky in skies:
            for fw in fwhms:
                tag = "A_m%g_sky%g_fwhm%g" % (m, sky, fw)
                rng = C.rng_for(tag, SEED)
                P = make_stamp(fw)
                pn = P.reshape(-1)
                A = pn.reshape(-1, 1)
                var = C.sigma_pix_doc(F, P, sky).reshape(-1)
                sig = C.sigma_F_diag(pn, var)
                shape = P.shape
                rate = (sky + F * P) * C.DEFAULT_GAIN
                Fc = np.empty(N_MC_A)
                Cinv = np.diag(1.0 / var)
                ATCiA = float((A.T @ Cinv @ A)[0, 0])
                for b in range(N_MC_A):
                    # 电子域泊松 + 读噪；均值 sky+F*P，方差 = 正本 §3.3
                    y = rng.poisson(rate) / C.DEFAULT_GAIN
                    y = y + rng.normal(0.0, C.DEFAULT_RN / C.DEFAULT_GAIN, size=shape)
                    # 背景已知 ⇒ 先扣除已知 sky 再求幅度（否则 sky 进分子造成偏置）
                    Fc[b] = float((A.T @ Cinv @ (y.reshape(-1) - sky))[0] / ATCiA)
                var_f = C.sigma_pix_doc_gain_inverted(F, P, sky).reshape(-1)
                sig_fault = C.sigma_F_diag(pn, var_f)
                cov = C.coverage_report((Fc - F) / sig)
                cov_fault = C.coverage_report((Fc - F) / sig_fault)
                # 零效应阴性：常数方差白噪下对角式与严格式必须逐位相同
                var_w = np.full(pn.size, 41.573)
                Cw = np.diag(var_w)
                s_diag = C.sigma_F_diag(pn, var_w)
                s_strict = C.sigma_F_strict(pn, Cw)
                Pw = pn / var_w
                neff = float((Pw.sum() ** 2) / np.sum(Pw * Pw))
                rows.append({
                    "tag": tag, "m_ref": m, "F_ref_adu": F, "sky_adu": sky,
                    "fwhm_px": fw, "beta": FWHM_BETA, "n_mc": N_MC_A,
                    "A_nea_px": float(1.0 / np.sum(pn * pn)),
                    "neff_variance_weighted_px": neff,
                    "sigma_F_analytic_adu": sig,
                    "sd_Fhat_over_sigma_F": float(np.std(Fc, ddof=1) / sig),
                    "coverage": cov,
                    "negative_gain_inverted": {
                        "sigma_F_faulty_adu": sig_fault,
                        "ratio_faulty_over_true": sig_fault / sig,
                        "declared": "同一批实现换错误分母 ⇒ 覆盖率必须偏离标称（判红）",
                        "coverage": cov_fault,
                        "red_any_level_wilson_outside": bool(any(
                            not v["contains_nominal"] for v in cov_fault["coverage"].values())),
                    },
                    "zero_effect_white_diag_vs_strict": {
                        "sigma_diag": s_diag, "sigma_strict": s_strict,
                        "rel_diff": abs(s_diag - s_strict) / s_strict,
                    },
                })
    fails = [(r["tag"], lv, v["cov"]) for r in rows
             for lv, v in r["coverage"]["coverage"].items() if not v["contains_nominal"]]
    return {"arm": "A",
            "model": "poisson+readout, known background, diagonal GLS",
            "seed": SEED, "rows": rows,
            "nominal_ci_misses": len(fails),
            "nominal_ci_tests": 3 * len(rows),
            "sd_over_sigma_range": [
                min(r["sd_Fhat_over_sigma_F"] for r in rows),
                max(r["sd_Fhat_over_sigma_F"] for r in rows)],
            "zero_effect_max_rel_diff": max(
                r["zero_effect_white_diag_vs_strict"]["rel_diff"] for r in rows),
            "negative_red_count": sum(
                1 for r in rows if r["negative_gain_inverted"]["red_any_level_wilson_outside"])}


# ------------------------------------------------------------------ 臂 B

def arm_B():
    mags = [14.0, 16.0, 18.0, 20.0]
    skies = [10.0, 200.0, 2000.0]
    fwhms = [1.5, 2.0]
    rows = []
    for m in mags:
        F = flux_from_mag(m)
        for sky in skies:
            for fw in fwhms:
                tag = "B_m%g_sky%g_fwhm%g" % (m, sky, fw)
                rng = C.rng_for(tag, SEED + 1)
                P = make_stamp(fw)
                pn = P.reshape(-1)
                shape = P.shape
                var = C.sigma_pix_doc(F, P, sky).reshape(-1)
                sig_known = C.sigma_F_diag(pn, var)
                sig_free = C.sigma_F_freebg(pn, var)
                A_known = pn.reshape(-1, 1)
                A_free = np.column_stack([pn, np.ones(pn.size)])
                Cinv = np.diag(1.0 / var)
                ck = float(np.linalg.inv(A_known.T @ Cinv @ A_known)[0, 0])
                cf = np.linalg.inv(A_free.T @ Cinv @ A_free)
                rate = (sky + F * P) * C.DEFAULT_GAIN
                Fk = np.empty(N_MC_B)
                Ff = np.empty(N_MC_B)
                Fn = np.empty(N_MC_B)
                for b in range(N_MC_B):
                    y = rng.poisson(rate) / C.DEFAULT_GAIN
                    y = y + rng.normal(0.0, C.DEFAULT_RN / C.DEFAULT_GAIN, size=shape)
                    yv = y.reshape(-1)
                    Fk[b] = float(ck * (A_known.T @ Cinv @ (yv - sky))[0])
                    Ff[b] = float((cf @ (A_free.T @ Cinv @ yv))[0])
                    # 不建模也不扣背景：同一幅度估计量的有偏读数
                    Fn[b] = float(ck * (A_known.T @ Cinv @ yv)[0])
                cov_free = C.coverage_report((Ff - F) / sig_free)
                cov_wrong = C.coverage_report((Ff - F) / sig_known)
                rows.append({
                    "tag": tag, "m_ref": m, "F_ref_adu": F, "sky_adu": sky, "fwhm_px": fw,
                    "n_mc": N_MC_B,
                    "sigma_F_known_bg_adu": sig_known,
                    "sigma_F_free_bg_adu": sig_free,
                    "sigma_F_free_over_known": sig_free / sig_known,
                    "E_free_vs_known": (sig_free / sig_known) ** 2 - 1.0,
                    "sd_Fhat_free_over_sigma_free": float(np.std(Ff, ddof=1) / sig_free),
                    "sd_Fhat_known_over_sigma_known": float(np.std(Fk, ddof=1) / sig_known),
                    "coverage_freebg_correct_sigma": cov_free,
                    "negative_freebg_with_knownbg_sigma": {
                        "declared": "用已知背景 sigma 标准化自由背景估计 ⇒ 必须欠覆盖（判红）",
                        "coverage": cov_wrong,
                        "red_68_wilson_below_nominal": bool(
                            cov_wrong["coverage"]["0.6826894921"]["wilson95"][1] < NOMINAL_68),
                    },
                    "horizontal_bias_no_bg_subtraction_pct": float(
                        100.0 * (np.mean(Fn) - F) / F),
                })
    return {"arm": "B",
            "model": "free constant background GLS J=[P,1] vs known background",
            "seed": SEED + 1, "rows": rows,
            "sd_over_sigma_free_range": [
                min(r["sd_Fhat_free_over_sigma_free"] for r in rows),
                max(r["sd_Fhat_free_over_sigma_free"] for r in rows)],
            "negative_red_count": sum(
                1 for r in rows
                if r["negative_freebg_with_knownbg_sigma"]["red_68_wilson_below_nominal"])}


# ------------------------------------------------------------------ 臂 C

def arm_C():
    """线性 R 重采样：完整协方差 vs 仅对角。

    输入像素白噪（方差 v_in = rho，使大 rho 下重采样后的叶方差 ≈ pixfrac^2），
    输出叶噪声 C_out = R V R^T + eps^2 I（eps^2 = 逐输出像素加入、不参与重采样的
    独立底），floor_ratio = eps^2/Var_res ∈ {0.01, 0.1, 1.0}。
    无覆盖的输出叶按生产语义剔除（retained），N_retained = 保留叶数。
    两个统计量：(i) 归一 PSF 加权通量（点源被估量）；(ii) patch median（生产控制点）。
    """
    side = 8
    rhos = [0.25, 0.5, 0.75, 1.0, 1.4, 2.0, 4.0, 16.0, 128.0, 430.0]
    pixfracs = [0.5, 0.8, 1.0]
    floor_ratios = [0.01, 0.1, 1.0]
    rows = []
    for rho in rhos:
        for pf in pixfracs:
            v_in = rho
            Cbase, idx = G.drizzle_covariance_2d(side, rho, pf, v=v_in)
            n = Cbase.shape[0]
            var_res = float(np.mean(np.diag(Cbase)))
            py = idx[:, 0]
            px = idx[:, 1]
            cy = (py.min() + py.max()) / 2.0
            cx = (px.min() + px.max()) / 2.0
            alpha = 1.5 / (2.0 * math.sqrt(2.0 ** (1.0 / FWHM_BETA) - 1.0))
            r2 = (px - cx) ** 2 + (py - cy) ** 2
            pv = (1.0 + r2 / (alpha * alpha)) ** (-FWHM_BETA)
            a = pv / pv.sum()
            within = bool((px.min() <= cx <= px.max()) and (py.min() <= cy <= py.max()))
            for fr in floor_ratios:
                tag = "C_rho%g_pf%g_floorfr%g" % (rho, pf, fr)
                rng = C.rng_for(tag, SEED + 2)
                Cfull = Cbase + fr * var_res * np.eye(n)
                var_full = float(a @ Cfull @ a)
                var_diag = float(np.sum(a * a * np.diag(Cfull)))
                sd_full = math.sqrt(var_full)
                sd_diag = math.sqrt(var_diag)
                eigs = np.linalg.eigvalsh(Cfull)
                L = np.linalg.cholesky(Cfull)
                y = rng.standard_normal((N_MC_C, n)) @ L.T
                t = y @ a
                cov_full = C.coverage_report(t / sd_full)
                cov_diag = C.coverage_report(t / sd_diag)
                med = np.median(y, axis=1)
                mad = 1.482602218505602 * np.median(np.abs(y - med[:, None]), axis=1)
                sd_med = float(np.std(med, ddof=1))
                var_leaf = float(np.mean(np.diag(Cfull)))
                k_true = sd_med ** 2 / (1.5707963267948966 * var_leaf / n)
                k_mad = sd_med ** 2 / (1.5707963267948966 * float(np.mean(mad * mad)) / n)
                lookup = {(int(i), int(j)): k for k, (i, j) in enumerate(idx)}
                num = 0.0
                den = 0.0
                cnt = 0
                for k, (i, j) in enumerate(idx):
                    for di, dj in ((0, 1), (1, 0)):
                        q = lookup.get((int(i) + di, int(j) + dj))
                        if q is not None:
                            num += Cfull[k, q]
                            den += Cfull[k, k]
                            cnt += 1
                r1 = (num / cnt) / (den / cnt) if cnt else 0.0
                rows.append({
                    "tag": tag, "rho_output_over_source": rho, "pixfrac": pf,
                    "v_in_adu2": v_in, "var_resampled_adu2": var_res,
                    "floor_ratio": fr, "floor_adu2": fr * var_res,
                    "n_retained_leaves": n, "n_mc": N_MC_C,
                    "statistic_center_inside_retained": within,
                    "sd_full_cov_adu": sd_full, "sd_diag_only_adu": sd_diag,
                    "E_diag_vs_full_psf": var_diag / var_full - 1.0,
                    "diag_claims_fraction_of_true_var": var_diag / var_full,
                    "neighbor_corr_mean": r1,
                    "min_eig_adu2": float(eigs[0]), "max_eig_adu2": float(eigs[-1]),
                    "condition_number": float(eigs[-1] / max(eigs[0], 1e-300)),
                    "neff_psf_statistic": C.neff_from_cov(Cfull, a),
                    "neff_patch_median": n / k_true if k_true > 0 else None,
                    "k_corr_patch_median_true_sigma": k_true,
                    "k_corr_patch_median_production_mad": k_mad,
                    "coverage_full_cov": cov_full,
                    "negative_diag_only": {
                        "declared": "只用对角方差标准化 ⇒ 低估方差 ⇒ 必须欠覆盖（判红）",
                        "coverage": cov_diag,
                        "E_diag": var_diag / var_full - 1.0,
                        "red_68_wilson_below_nominal": bool(
                            cov_diag["coverage"]["0.6826894921"]["wilson95"][1] < NOMINAL_68),
                    },
                })
    Cid, idxid = G.drizzle_covariance_2d(side, 1.0, 1.0, v=1.0)
    off = float(np.max(np.abs(Cid - np.diag(np.diag(Cid)))))
    zero = {"test": "rho=1, pixfrac=1 (R=I)", "n_retained": int(Cid.shape[0]),
            "max_offdiag_abs": off,
            "diag_equals_full_bitwise": bool(off == 0.0),
            "declared": "零效应：非对角必须逐位为 0"}
    return {"arm": "C",
            "model": "linear R resampling covariance C_out = R V R^T + eps^2 I",
            "seed": SEED + 2, "zero_effect_identity": zero, "rows": rows,
            "negative_red_count": sum(
                1 for r in rows if r["negative_diag_only"]["red_68_wilson_below_nominal"]),
            "E_diag_min": min(r["E_diag_vs_full_psf"] for r in rows)}


# ------------------------------------------------------------------ 臂 D

def arm_D():
    """点源总通量 vs 弥散源面亮度（孔径平均）。

    弥散被估量 mu_N = 孔径内 N_ap 像素的面亮度平均：
      最优（逆方差）权重 a=1/N_ap，Var_opt = sigma^2/N_ap；
      误用 PSF 权重（在**同一孔径内**重归一 a=P/Sum_mask(P)，保证无偏）：
      Var = sigma^2*Sum_mask(P^2)/Sum_mask(P)^2；
      E = N_ap*Sum_mask(P^2)/Sum_mask(P)^2 - 1。
    反向（等权估点源总通量）给出同一个 E（闭式对称）。
    等价条件：孔径内 PSF 平坦 ⇒ A_NEA,aperture -> 1 ⇒ E -> 0。
    """
    rows = []
    fwhms = [1.5, 2.0, 3.0]
    skies = [10.0, 200.0, 2000.0]
    ap_radii_fwhm = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0]
    for fw in fwhms:
        P = make_stamp(fw)
        pn = P.reshape(-1)
        sig2 = 100.0
        yy, xx = np.mgrid[0:P.shape[0], 0:P.shape[1]].astype(float)
        cy = cx = (P.shape[0] - 1) / 2.0
        r2 = (xx - cx) ** 2 + (yy - cy) ** 2
        for sky in skies:
            for R in ap_radii_fwhm:
                mask = (r2 <= (R * fw) ** 2).reshape(-1)
                N_ap = int(mask.sum())
                if N_ap < 2:
                    continue
                psum = float(np.sum(pn[mask]))
                p2sum = float(np.sum(pn[mask] ** 2))
                E1 = (sig2 * p2sum / (psum * psum)) / (sig2 / N_ap) - 1.0
                E2 = (sig2 * N_ap / (psum * psum)) / (sig2 / p2sum) - 1.0
                rows.append({
                    "tag": "D_fwhm%g_sky%g_R%g" % (fw, sky, R),
                    "fwhm_px": fw, "sky_adu": sky,
                    "aperture_radius_fwhm": R, "N_aperture_px": N_ap,
                    "A_nea_aperture_px": (psum * psum) / p2sum,
                    "sumP_in_aperture": psum,
                    "E_point_weights_on_diffuse": E1,
                    "E_diffuse_weights_on_point": E2,
                    "E_symmetry_abs_diff": abs(E1 - E2),
                    "E_closed_form": N_ap * p2sum / (psum * psum) - 1.0,
                    "E_gt_10pct": bool(E1 > 0.10),
                    "declared_red_direction": "E 必须随孔径单调增；孔径内 PSF 平坦时必须为 0",
                })
    M = 49
    pflat = np.full(M, 1.0 / M)
    Eflat = M * float(np.sum(pflat ** 2)) / float(np.sum(pflat)) ** 2 - 1.0
    zero = {"test": "flat PSF inside aperture (A_NEA,aperture=1)", "E": Eflat,
            "A_nea_aperture": float(np.sum(pflat) ** 2 / np.sum(pflat ** 2)),
            "declared": "零效应：E 必须为 0"}
    return {"arm": "D",
            "model": "point-source total flux vs diffuse surface brightness (aperture mean)",
            "seed": SEED + 3, "zero_effect_flat_psf": zero, "rows": rows,
            "E_symmetry_max_abs_diff": max(r["E_symmetry_abs_diff"] for r in rows),
            "E_closed_form_max_dev": max(
                abs(r["E_point_weights_on_diffuse"] - r["E_closed_form"]) for r in rows)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/ACSDSDOC-02/S05/results/arms_oracle.json")
    args = ap.parse_args()
    out = {
        "experiment_id": "P2-A-F-S05-arms",
        "paper_id": "P2",
        "evidence_level": "ORACLE",
        "oracle_scope": "纯 numpy 独立实现，不 import/链接生产 lib/**（SELF_CHECK 级）",
        "env": C.env_fingerprint(),
        "constants": {"gain_e_per_adu": C.DEFAULT_GAIN, "read_noise_e": C.DEFAULT_RN,
                      "moffat_beta": FWHM_BETA, "ZP_oracle_mag": ZP_ORACLE,
                      "independent_floor_adu2": 41.573,
                      "nominal_coverage_levels": [0.6826894921370859, 0.90, 0.95]},
        "arms": [arm_A(), arm_B(), arm_C(), arm_D()],
    }
    C.dump_json(args.out, out)
    print("[S05 arms] wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
