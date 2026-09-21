#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B2：σ_F 噪声项组成（天光/读噪/暗流/PSF/增益/源强）对拍 Horne 1986。

假说：
  H1 定义式 σ_F⁻² = Σ P_i²/σ_i²，σ_i² = (B + D + RN² + F·P_i)/g²（电子域 CCD 方程）
     在全部扫描点上与 MC 经验散度一致（|z| ≤ 3，z 用 chi2 的 std 标准误）。
  H2 σ_F 对天光/暗流/读噪的依赖方向与幅度由上述组成式预言（相对偏差 ≤ 3σ）。
  H3 天空受限极限 σ_F → σ_pix/sqrt(ΣP²)（离散归一化 Moffat4 的 ΣP²，不是高斯近似）。
  H4 生产三臂中"经验总 σ_sky + (RN/g)²"（调度器在 snr 配置给 gain/RN 时的路径）在
     RN 主导处出现可量化正偏差（σ_F 高估 ⇒ SNR 低估），并给出闭式边界。
  H5 生产 C++（snr_science.cpp）与 Python 镜像逐位一致（≤1e-12 相对），
     且 C++ 三臂对 MC 真值的偏差与 Python 镜像一致。
输出：results/b2_noise_terms.json
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

GAIN0, RN0, DARK0, SIG0, F0, B0 = 1.3, 10.0, 0.5, 1.5, 1000.0, 100.0
HALF, N_MC = 30, 1000
R_IN, R_OUT = 10.0, 30.0
SCANS = {
    "read_noise_e": [0.0, 2.0, 5.0, 10.0, 20.0, 50.0],
    "dark_e_per_px": [0.0, 1.0, 10.0, 100.0, 1000.0],
    "psf_sigma_px": [0.8, 1.0, 1.5, 2.0, 3.0, 5.0],
    "gain_e_per_adu": [0.5, 1.0, 1.3, 2.0, 4.0],
    "source_flux_e": [10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0, 10000.0],
}


def annulus_mask(shape, r_in, r_out):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    cy = (shape[0] - 1) / 2.0; cx = (shape[1] - 1) / 2.0
    return (np.hypot(yy - cy, xx - cx) >= r_in) & (np.hypot(yy - cy, xx - cx) <= r_out)


def run_point(F, B, D, RN, g, sig, seed_off):
    P, sum_p2, p_center, half = C.moffat4_grid(sig, HALF)
    shape = P.shape
    mask = annulus_mask(shape, R_IN, R_OUT)
    Pf = P.ravel()
    r = C.rng(seed_off)
    lam = F * P + B + D
    e = r.poisson(lam, size=(N_MC,) + shape).astype(np.float64)
    if RN > 0:
        e += r.normal(0.0, RN, size=(N_MC,) + shape)
    d = e / g
    bg = d[:, mask]
    b_hat = np.median(bg, axis=1)
    sig_hat = C.K_MAD_TO_SIGMA * np.median(np.abs(bg - b_hat[:, None]), axis=1)
    dsub = d - b_hat[:, None, None]
    # oracle（固定真值方差）
    var_o = (B + D + RN ** 2 + F * Pf) / g ** 2
    w_o = 1.0 / var_o
    den_o = float((Pf * Pf * w_o).sum())
    F_or = (dsub.reshape(N_MC, -1) @ (Pf * w_o)) / den_o
    # 逐帧 σ̂ 估计臂
    def arm(mode):
        Fa = np.empty(N_MC); sF = np.empty(N_MC)
        for k in range(N_MC):
            Fk = 0.0
            for _ in range(2):
                if mode == "total":
                    var_i = sig_hat[k] ** 2 + np.maximum(Fk * Pf, 0.0) / g
                elif mode == "empirical_plus_rn":
                    var_i = sig_hat[k] ** 2 + (RN / g) ** 2 + np.maximum(Fk * Pf, 0.0) / g
                elif mode == "skyonly_plus_rn":
                    var_i = (B + D) / g ** 2 + (RN / g) ** 2 + np.maximum(Fk * Pf, 0.0) / g
                w = 1.0 / var_i
                den = float((Pf * Pf * w).sum())
                Fk = float((Pf * w * dsub[k].ravel()).sum()) / den
            Fa[k] = Fk; sF[k] = float(np.sqrt(1.0 / den))
        return Fa, sF
    F_tot, sF_tot = arm("total")
    F_ern, sF_ern = arm("empirical_plus_rn")
    F_srn, sF_srn = arm("skyonly_plus_rn")
    sigF_def = C.horne_sigma_f_theory(F, B, D, RN, g, P)
    sigF_mc = float(np.std(F_tot, ddof=1))
    # 闭式双计预言（同一公式两条方差组成的比值，不依赖 MC）
    def sigma_f_from(var_adu):
        return float(np.sqrt(1.0 / (Pf * Pf / var_adu).sum()))
    v_corr = (B + D + RN ** 2 + F * Pf) / g ** 2
    v_skyonly = ((B + D) / g ** 2 + (RN / g) ** 2 + F * Pf / g ** 2)
    v_emp = v_corr + (RN / g) ** 2          # 经验总 σ（含 RN）+ 再加 RN²/g² ⇒ 双计
    src_dominance = float((F * Pf / g).max() / v_corr.min())
    return dict(
        F_e=F, sky_e_per_px=B, dark_e_per_px=D, read_noise_e=RN, gain_e_per_adu=g,
        psf_sigma_px=sig, sum_p2=sum_p2,
        sigma_f_def_adu=sigF_def, sigma_f_mc_adu=sigF_mc,
        snr_def=float(F / g / sigF_def), snr_mc=float(F / g / sigF_mc),
        sigma_f_oracle_adu=float(np.std(F_or, ddof=1)),
        sigma_f_total_adu=[float(np.mean(sF_tot)), float(np.std(sF_tot, ddof=1))],
        sigma_f_skyonly_rn_adu=[float(np.mean(sF_srn)), float(np.std(sF_srn, ddof=1))],
        sigma_f_empirical_rn_adu=[float(np.mean(sF_ern)), float(np.std(sF_ern, ddof=1))],
        pred_doublecount_bias=float(sigma_f_from(v_emp) / sigma_f_from(v_corr) - 1.0),
        source_term_dominance=src_dominance,
        sky_limited_sigma_f_pred_adu=float(np.sqrt((B + D + RN ** 2) / g ** 2 / sum_p2)))


def z_of(mean_arm, std_arm, sig_mc):
    se = np.sqrt((std_arm / np.sqrt(N_MC)) ** 2 + (sig_mc / np.sqrt(2 * (N_MC - 1))) ** 2)
    return float((mean_arm - sig_mc) / se)


def cpp_crosscheck(seed_off):
    """生产 C++（snr_science.cpp）vs Python 镜像：参数网格逐点对拍。"""
    exe = os.path.join(C.ROOT, "run", "SCI-402", "prod_snr_driver")
    if not os.path.exists(exe):
        return dict(available=False, note="prod_snr_driver 未构建（先跑 code/build_prod_driver.sh）")
    grid = []
    r = C.rng(seed_off)
    for _ in range(40):
        F = float(10 ** r.uniform(1, 4.5))
        sig = float(r.uniform(0.6, 6.0))
        sky = float(10 ** r.uniform(-1, 4))
        g = float(r.uniform(0.4, 5.0))
        rn = float(r.uniform(0, 60))
        half = 30
        grid.append((F, sig, sky, g, rn, half))
    rows = []
    for (F, sig, sky, g, rn, half) in grid:
        out = subprocess.run([exe, repr(F), repr(sig), repr(sky), repr(g), repr(rn),
                              str(half), "0.0"], capture_output=True, text=True, check=True).stdout
        cpp = {}
        for line in out.strip().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                cpp[k] = float(v)
        py = C.prod_mirror_snr(F, sig, sky, g, rn, half)
        rel = lambda a, b: abs(a - b) / max(abs(b), 1e-300)
        rows.append(dict(F_adu=F, sigma_px=sig, sigma_sky_adu=sky, gain=g, rn_e=rn, half=half,
                         rel_snr_optimal=rel(py["snr_optimal"], cpp["snr_optimal"]),
                         rel_sigma_f=rel(py["sigma_f_optimal_adu"], cpp["sigma_f_optimal_adu"]),
                         rel_snr_aperture=rel(py["snr_aperture"], cpp["snr_aperture"]),
                         rel_sum_p2=rel(py["sum_p2"], cpp["sum_p2"]),
                         rel_f_in=rel(py["f_in"], cpp["enclosed_fraction"])))
    worst = max(max(r["rel_snr_optimal"], r["rel_sigma_f"], r["rel_snr_aperture"],
                    r["rel_sum_p2"], r["rel_f_in"]) for r in rows)
    return dict(available=True, n_points=len(rows), worst_rel_diff=float(worst),
                tolerance=1e-12, pass_=bool(worst <= 1e-12), rows=rows,
                driver="lib/algorithms/noise_snr/cpp/src/snr_science.cpp + code/prod_snr_driver.cpp")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b2_noise_terms.json"))
    a = ap.parse_args()
    t0 = time.time()
    results = {}
    seed = 0
    for axis, values in SCANS.items():
        rows = []
        for v in values:
            F, B, D, RN, g, sig = F0, B0, DARK0, RN0, GAIN0, SIG0
            if axis == "read_noise_e":
                RN = v
            elif axis == "dark_e_per_px":
                D = v
            elif axis == "psf_sigma_px":
                sig = v
            elif axis == "gain_e_per_adu":
                g = v
            elif axis == "source_flux_e":
                F = v
            seed += 1000
            row = run_point(F, B, D, RN, g, sig, seed)
            row["axis"] = axis
            row["axis_value"] = v
            row["z_def_vs_mc"] = z_of(row["sigma_f_def_adu"], 0.0, row["sigma_f_mc_adu"]) if False else \
                float((row["sigma_f_def_adu"] - row["sigma_f_mc_adu"]) / (row["sigma_f_mc_adu"] / np.sqrt(2 * (N_MC - 1))))
            row["z_total_vs_mc"] = z_of(row["sigma_f_total_adu"][0], row["sigma_f_total_adu"][1], row["sigma_f_mc_adu"])
            row["z_skyonly_rn_vs_mc"] = z_of(row["sigma_f_skyonly_rn_adu"][0], row["sigma_f_skyonly_rn_adu"][1], row["sigma_f_mc_adu"])
            row["z_empirical_rn_vs_mc"] = z_of(row["sigma_f_empirical_rn_adu"][0], row["sigma_f_empirical_rn_adu"][1], row["sigma_f_mc_adu"])
            row["bias_empirical_rn"] = float(row["sigma_f_empirical_rn_adu"][0] / row["sigma_f_mc_adu"] - 1.0)
            row["ratio_emp_over_skyonly"] = float(row["sigma_f_empirical_rn_adu"][0] / row["sigma_f_skyonly_rn_adu"][0])
            rows.append(row)
            print("%-16s %10.3f  sigF_def=%9.4f mc=%9.4f  z_def=%+6.2f z_tot=%+6.2f z_sky+RN=%+6.2f z_emp+RN=%+7.2f  pred_dc=%+6.3f"
                  % (axis, v, row["sigma_f_def_adu"], row["sigma_f_mc_adu"], row["z_def_vs_mc"],
                     row["z_total_vs_mc"], row["z_skyonly_rn_vs_mc"], row["z_empirical_rn_vs_mc"],
                     row["pred_doublecount_bias"]), flush=True)
        results[axis] = rows
    cpp = cpp_crosscheck(7000)
    gates = {}
    zdef = [abs(r["z_def_vs_mc"]) for rows in results.values() for r in rows]
    ztot = [abs(r["z_total_vs_mc"]) for rows in results.values() for r in rows]
    zsky = [abs(r["z_skyonly_rn_vs_mc"]) for rows in results.values() for r in rows]
    gates["G1_def_all_within_3sigma"] = bool(max(zdef) <= 3.0)
    gates["G1_def_max_abs_z"] = float(max(zdef))
    gates["G1_def_n_points"] = int(len(zdef))
    gates["G2_total_all_within_3sigma"] = bool(max(ztot) <= 3.0)
    gates["G2_total_max_abs_z"] = float(max(ztot))
    gates["G4a_skyonly_rn_all_within_3sigma"] = bool(max(zsky) <= 3.0)
    gates["G4a_skyonly_rn_max_abs_z"] = float(max(zsky))
    # 双计偏差：MC 实测 vs 闭式预言
    preds = [r["pred_doublecount_bias"] for rows in results.values() for r in rows]
    meas = [r["bias_empirical_rn"] for rows in results.values() for r in rows]
    dmax = max(abs(m - p) for m, p in zip(meas, preds))
    gates["G4c_pred_vs_measured_mc_max_abs_diff"] = float(dmax)
    ratios = [r["ratio_emp_over_skyonly"] - 1.0 for rows in results.values() for r in rows]
    dratio = max(abs(a - b) for a, b in zip(ratios, preds))
    gates["G4c_pred_vs_arm_ratio_max_abs_diff"] = float(dratio)
    gates["FINDING_doublecount_pred_matches_arm_ratio_2pp"] = bool(dratio <= 0.02)
    gates["G4c_max_bias_over_all_points"] = float(max(meas))
    gates["G4c_bias_at_base_point"] = float(next(r["bias_empirical_rn"] for r in results["source_flux_e"]
                                                  if r["axis_value"] == F0))
    # 天空受限极限：只在源泊松项不主导（dominance < 5%）的点上判据有效
    lim = []
    for axis, rows in results.items():
        for r in rows:
            if r["source_term_dominance"] < 0.05:
                lim.append(dict(axis=axis, value=r["axis_value"], sigma_px=r["psf_sigma_px"],
                                sigma_f_def=r["sigma_f_def_adu"],
                                sigma_f_sky_limited_pred=r["sky_limited_sigma_f_pred_adu"],
                                rel_diff=float(r["sigma_f_def_adu"] / r["sky_limited_sigma_f_pred_adu"] - 1)))
    gates["G3_sky_limited_check"] = lim
    gates["G3_sky_limited_n_points"] = len(lim)
    gates["G3_sky_limited_max_rel_diff"] = float(max([abs(x["rel_diff"]) for x in lim], default=float("nan")))
    gates["G5_cpp_mirror_pass"] = bool(cpp.get("pass_", False))
    gates["G5_cpp_worst_rel_diff"] = cpp.get("worst_rel_diff")
    obj = dict(experiment="SCI-B / B2 sigma_F noise-term composition vs Horne 1986 + production cross-check",
               frozen_config=dict(base=dict(gain=GAIN0, rn=RN0, dark=DARK0, sigma_psf=SIG0,
                                            F_e=F0, sky_e_per_px=B0, half=HALF, n_mc=N_MC),
                                  scans=SCANS, annulus_px=[R_IN, R_OUT], seed_base=C.SEED_BASE),
               scans=results, cpp_crosscheck=cpp, gates=gates,
               generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)
    print("GATES:", {k: v for k, v in gates.items() if k != "G3_sky_limited_check"})


if __name__ == "__main__":
    main()
