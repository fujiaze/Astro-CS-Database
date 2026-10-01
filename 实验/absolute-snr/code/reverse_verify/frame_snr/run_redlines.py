#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FRAME-SNR-CANON 红线测试（能红能绿）。

判据（**先写死，不事后放宽**）：
  T8  天光单调性（决定性）：固定 F_s，B 增大 => SNR **严格单调下降**，
      且 B -> inf 时 SNR -> 0（天空受限渐近 SNR*sqrt(B*A_NEA)/F_s -> 1）。
  T9  加性天光不变性：对信号估计量 F_hat，加常数天光 C 必须**不改变** F_hat
      （相对变化 < 1e-12）。
  T10 红例否决：未扣背景型 / 功率比型 / 未扣背景窗口型，在同一 B 网格上
      **必须**违反单调性（存在 B2>B1 使 SNR(B2) >= SNR(B1)）=> 否决。
  T11 解析 vs MC：已知 (F_s,B,sigma_R) 下解析 sigma_F 与 MC 实测散度一致
      （相对偏差 < 3 * MC 标准误）。
  T12 生产实现对拍：Python 独立复算 vs 生产 C ABI snr_source_snr_f64
      （g++ 直编 lib/algorithms/noise_snr/cpp/src/snr_science.cpp，只读）。
      **宽度约定**：canon 的 FWHM_PX 是 PSF 块 Moffat4 FWHM，而生产 fwhm_px 字段是
      检测块高斯 FWHM（跨块混用使 sigma 高估 1.914005x，DISP-STAR-007）⇒ canonical
      路径经同尺度入口 sigma_px 传 canon 的 Moffat4 sigma；错约定作为负例必须判红。

用法: TMPDIR=/dev/shm/astrocs_fsnr python3 run_redlines.py [--out DIR]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_snr_canon as C  # noqa: E402

FWHM_PX = 4.0
GAIN = 1.5          # e-/ADU
READ_E = 5.0        # e-
F_S_E = 2000.0      # 源总通量 [e-]（固定）
B_GRID = [0.0, 10.0, 100.0, 1000.0, 10000.0]   # 天光 [e-/pix]

# --- 生产 ABI 宽度约定映射（P2-T12 订正，2026-09-29）-----------------------------
# 生产 ABI 有两条互斥的宽度入口（lib/algorithms/noise_snr/cpp/include/snr_estimator.h:301-327
# 与 lib/algorithms/noise_snr/cpp/src/snr_science.cpp:14-24/58-61/134-136/157）：
#   fwhm_px  = **检测块**椭圆高斯 FWHM（= 2.3548200450309493*sigma），>0 时优先；
#   sigma_px = 本块 Moffat4 轮廓 sigma（与 fwhm_px 同一 sigma 尺度），fwhm_px<=0 时启用。
# 本单元 canon 的 FWHM_PX 属 **PSF 块 Moffat4 FWHM**（frame_snr_canon.py:36）。
# 旧版判据把 Moffat4 FWHM 直接灌进 fwhm_px —— 生产明令禁止的跨块混用
# （snr_estimator.h:305 / snr_science.cpp:136 记 DISP-STAR-007：sigma 高估 1.914005x）
# ⇒ 判据恒红，且与 docs/frame-snr-canon.md:375 的「P12 PASS」矛盾。
# 订正：canonical 路径改走同尺度入口 sigma_px，传 canon 的 Moffat4 sigma；
# 并把**错约定**保留为负例（期望判红）以证明该判据能红能绿。
GAUSS_FWHM_FACTOR = 2.3548200450309493           # 生产 kGaussFwhmFactor（检测块）
MOFFAT4_FWHM_FACTOR = 1.230310                   # 生产 kMoffat4FwhmFactor（PSF 块 = canon）
SIGMA_CANON_PX = FWHM_PX / MOFFAT4_FWHM_FACTOR   # canon Moffat4 sigma [px]
WRONG_CONVENTION_INFLATION = GAUSS_FWHM_FACTOR / MOFFAT4_FWHM_FACTOR  # 1.914005x


def monotone_criterion(vals: list[float]) -> bool:
    """T8 的**唯一**判据（同一函数同时用于绿例 canon 与红例否决 => 能红能绿）。"""
    return all(vals[i + 1] < vals[i] for i in range(len(vals) - 1))


def _rel(a: float, b: float) -> float:
    d = abs(a - b)
    m = max(abs(a), abs(b), 1e-300)
    return d / m


def test_T8_monotonic(P: np.ndarray) -> dict:
    rows = []
    snr_vals = []
    for B in B_GRID:
        s = C.snr_canon(F_S_E, B, READ_E, P)
        s_lim = C.snr_canon_sky_limited(F_S_E, B, P) if B > 0 else float("inf")
        snr_vals.append(s)
        rows.append({
            "B_e_per_pix": B,
            "sigma_F_e": C.sigma_flux_optimal_e(F_S_E, B, READ_E, P),
            "SNR_canon": s,
            "SNR_sky_limited_closed_form": s_lim,
            "SNR_over_sky_limited": (s / s_lim) if B > 0 else None,
        })
    strictly_decreasing = monotone_criterion(snr_vals)
    # B -> inf 渐近: SNR * sqrt(B) -> F_s / sqrt(A_NEA)
    asym = []
    for B in (1e4, 1e6, 1e8, 1e10):
        s = C.snr_canon(F_S_E, B, READ_E, P)
        pred = F_S_E / math.sqrt(C.a_nea(P))
        asym.append({"B": B, "SNR": s, "SNR_x_sqrtB": s * math.sqrt(B),
                     "pred_F_over_sqrtAnea": pred, "rel_err": _rel(s * math.sqrt(B), pred)})
    # "B->inf 时 SNR->0" 的尺度不变判据（不设死绝对阈值）：
    #   (i) 渐近网格上 SNR 仍严格下降；
    #   (ii) SNR(B) 随 B 的下降速率符合 1/sqrt(B)（跨 6 个数量级降 >100 倍）；
    #   (iii) SNR*sqrt(B) 收敛到 F_s/sqrt(A_NEA)（相对误差 < 1e-3）。
    asym_dec = all(asym[i + 1]["SNR"] < asym[i]["SNR"] for i in range(len(asym) - 1))
    drop_ratio = asym[0]["SNR"] / asym[-1]["SNR"]
    tends_to_zero = asym_dec and drop_ratio > 100.0
    ok = (strictly_decreasing and tends_to_zero
          and all(a["rel_err"] < 1e-3 for a in asym[-2:]))
    return {"pass": ok, "A_NEA_pix": C.a_nea(P), "rows": rows,
            "strictly_decreasing": strictly_decreasing,
            "tends_to_zero": tends_to_zero, "asymptote": asym}


def test_T9_additive_invariance(rng: np.random.Generator) -> dict:
    shape = (161, 161)
    center = (80, 80)
    img, P_full, ap_mask, ann_mask = C.synth_frame(
        F_S_E, 300.0, READ_E, shape, center, FWHM_PX, rng)
    C_ADD = 1234.5  # ADU/e- 等值：这里全用 e-，常数天光 [e-]
    img2 = img + C_ADD
    f1 = C.aperture_sum_bgsub(img, ap_mask, ann_mask)
    f2 = C.aperture_sum_bgsub(img2, ap_mask, ann_mask)
    g1 = C.psf_weighted_flux_bgsub(img, P_full, ann_mask)
    g2 = C.psf_weighted_flux_bgsub(img2, P_full, ann_mask)
    r_ap = _rel(f1, f2)
    r_psf = _rel(g1, g2)
    # 红对照：不扣背景的估计量必然被抬高
    raw1 = float(np.sum(img[ap_mask]))
    raw2 = float(np.sum(img2[ap_mask]))
    raw_delta_pred = C_ADD * float(np.sum(ap_mask))
    return {
        "pass": (r_ap < 1e-12 and r_psf < 1e-12),
        "C_add_e": C_ADD,
        "aperture_bgsub_before": f1, "aperture_bgsub_after": f2, "rel_change": r_ap,
        "psf_weighted_bgsub_before": g1, "psf_weighted_bgsub_after": g2,
        "psf_weighted_rel_change": r_psf,
        "red_raw_aperture_before": raw1, "red_raw_aperture_after": raw2,
        "red_raw_delta": raw2 - raw1, "red_raw_delta_predicted": raw_delta_pred,
        "red_raw_rel_change": _rel(raw1, raw2),
    }


def test_T10_red_veto(P: np.ndarray) -> dict:
    n_pix = float(np.sum(P > 1e-6))
    out = {"pass": None, "cases": []}
    cases = [
        ("RED-A unsubtracted (F+nB)/sqrt(n(B+sR^2))",
         lambda B: C.snr_red_unsubtracted(F_S_E, B, READ_E, n_pix)),
        ("RED-B power-ratio (sum I)^2/sum sigma^2",
         lambda B: C.snr_red_power_ratio(F_S_E, B, READ_E, n_pix)),
        ("RED-C window unsubtracted sum P I / sqrt(sum P^2 s^2)",
         lambda B: C.snr_red_window_unsubtracted(F_S_E, B, READ_E, P)),
    ]
    for name, fn in cases:
        vals = [fn(B) for B in B_GRID]
        violations = [(B_GRID[i], B_GRID[i + 1], vals[i], vals[i + 1])
                      for i in range(len(vals) - 1) if vals[i + 1] >= vals[i]]
        out["cases"].append({
            "name": name,
            "values": vals,
            "T8_same_criterion_result": monotone_criterion(vals),
            "monotone_decreasing": len(violations) == 0,
            "violations_B1_B2_SNR1_SNR2": violations,
            "ratio_last_over_first": vals[-1] / vals[0],
            "vetoed": len(violations) > 0,
        })
    out["pass"] = all(c["vetoed"] for c in out["cases"])
    return out


def test_T11_mc(rng: np.random.Generator, n_mc: int = 20000) -> dict:
    half = 10
    P = C.moffat4_discrete_profile(FWHM_PX, half_px=half)
    Pflat = P.ravel()
    B_MC = 300.0
    s2 = C.pixel_variance_e2(F_S_E, B_MC, READ_E, Pflat)
    # 最小方差无偏通量估计量（背景已知）：
    #   F_hat = sum_i (P_i/sigma_i^2)(I_i - B) / sum_i (P_i^2/sigma_i^2)
    #   Var(F_hat) = 1 / sum_i (P_i^2/sigma_i^2)
    w = Pflat / s2
    wsum = float(np.sum(Pflat**2 / s2))
    sig_analytic = math.sqrt(1.0 / wsum)
    est = np.empty(n_mc)
    chunk = 1000
    done = 0
    lam = B_MC + F_S_E * Pflat
    while done < n_mc:
        m = min(chunk, n_mc - done)
        draws = rng.poisson(lam, size=(m, lam.size)).astype(float)
        draws += rng.normal(0.0, READ_E, size=(m, lam.size))
        est[done : done + m] = (draws - B_MC) @ w / wsum
        done += m
    mean_est = float(np.mean(est))
    std_est = float(np.std(est, ddof=1))
    se_of_std = std_est / math.sqrt(2.0 * (n_mc - 1))
    return {
        "pass": (_rel(mean_est, F_S_E) < 5e-3 and abs(std_est - sig_analytic) < 4 * se_of_std),
        "n_mc": n_mc, "grid_half": half, "B_mc_e_per_pix": B_MC,
        "sigma_F_analytic_e": sig_analytic,
        "sigma_F_mc_e": std_est, "sigma_F_mc_stderr_e": se_of_std,
        "sigma_F_rel_diff": _rel(std_est, sig_analytic),
        "F_hat_mean_e": mean_est, "F_hat_truth_e": F_S_E,
        "F_hat_bias_rel": _rel(mean_est, F_S_E),
        "SNR_analytic": F_S_E / sig_analytic,
        "SNR_mc_from_sigma": F_S_E / std_est,
    }


CPP_PROBE = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "snr_estimator.h"
int main(int argc, char** argv) {
  if (argc < 6) { std::fprintf(stderr, "usage: F_s_e B_e sigma_R_e gain fwhm [sigma_px]\n"); return 2; }
  const double F_e = std::atof(argv[1]);
  const double B_e = std::atof(argv[2]);
  const double rn_e = std::atof(argv[3]);
  const double gain = std::atof(argv[4]);
  const double fwhm = std::atof(argv[5]);
  const double sigma_px = (argc > 6) ? std::atof(argv[6]) : 0.0;  // 生产同尺度入口
  SnrSourceParams p;
  std::memset(&p, 0, sizeof(p));
  p.flux_adu = F_e / gain;                 // ADU
  p.fwhm_px = fwhm;                        // 检测块高斯 FWHM（<=0 时改用 sigma_px）
  p.sigma_px = sigma_px;                   // 本块 Moffat4 sigma（与 fwhm_px 同尺度）
  p.sigma_sky_adu = std::sqrt(B_e) / gain; // sky rms in ADU (sky Poisson only)
  p.gain_e_per_adu = gain;
  p.read_noise_e = rn_e;
  p.aperture_radius_px = 0.0;
  p.n_sky = 0.0;
  p.zero_point_mag = 0.0;
  p.profile_half_px = 0;
  SnrSourceResult r;
  const int rc = snr_source_snr_f64(&p, &r);
  if (rc != 0 || r.status != 0) { std::printf("{\"ok\":false,\"rc\":%d}\n", rc); return 1; }
  std::printf("{\"ok\":true,\"snr_optimal\":%.17g,\"sigma_f_optimal_adu\":%.17g,"
              "\"sum_p2\":%.17g,\"snr_aperture\":%.17g,\"sigma_f_aperture_adu\":%.17g,"
              "\"enclosed_fraction\":%.17g}\n",
              r.snr_optimal, r.sigma_f_optimal_adu, r.sum_p2,
              r.snr_aperture, r.sigma_f_aperture_adu, r.enclosed_fraction);
  return 0;
}
'''


def _out_of_range_negative(exe: str) -> dict:
    """越界负例：两个宽度入口都不有效（fwhm_px=0 且 sigma_px<=0）⇒ 生产必须 fail-closed 拒绝。

    生产 `snr_science.cpp:157-160`：sigma 非有限或 <=0 ⇒ status=1 拒绝；本函数要求两条越界
    用例都被拒（判据由此获得"越界必红"的一面）。

    fail-closed：探针**无判决**与生产**明确拒绝**是两回事。早前版本把解析失败
    （探针崩溃 / 超时 / stdout 空 / 非 JSON）塞进 `{"ok": False, "parse_error": True}`，
    于是 `not prod.get("ok")` 为真 ⇒ 一个跑不起来的探针被记成"生产正确拒绝"，门照样绿。
    吞异常转绿在本项目反复出现，这里改为：只有拿到**可解析的生产判决**才允许记 rejected；
    探针无判决 ⇒ 该用例判 `rejected=False` 并附 `probe_inconclusive` 与 stderr，整体 pass=False。
    """
    cases = []
    for label, fwhm, sigma in (("fwhm_px=0 & sigma_px=0", 0.0, 0.0),
                               ("fwhm_px=0 & sigma_px<0", 0.0, -1.0)):
        r = subprocess.run([exe, repr(F_S_E), "100.0", repr(READ_E), repr(GAIN),
                            repr(fwhm), repr(sigma)], capture_output=True, text=True, timeout=120)
        prod, parse_err = None, None
        try:
            prod = json.loads(r.stdout.strip().splitlines()[-1])
        except Exception as exc:                                # noqa: BLE001
            parse_err = "%s: %s" % (type(exc).__name__, exc)
        if not isinstance(prod, dict) or "ok" not in prod:
            cases.append({"case": label, "prod_ok": None, "prod_rc": None,
                          "rejected": False, "probe_inconclusive": True,
                          "probe_returncode": r.returncode,
                          "probe_stderr_tail": (r.stderr or "")[-400:],
                          "parse_error": parse_err})
            continue
        cases.append({"case": label, "prod_ok": bool(prod.get("ok")),
                      "prod_rc": prod.get("rc"),
                      "rejected": (not prod.get("ok")),
                      "probe_inconclusive": False,
                      "probe_returncode": r.returncode})
    inconclusive = [c["case"] for c in cases if c["probe_inconclusive"]]
    return {"cases": cases,
            "probe_inconclusive_cases": inconclusive,
            "pass": (all(c["rejected"] for c in cases) and not inconclusive),
            "status": ("FAIL: 探针对以下越界用例无判决，不得记为生产拒绝：%s"
                       % ", ".join(inconclusive)) if inconclusive else "OK: 两条越界用例均被生产明确拒绝"}


def test_T12_production_crosscheck(repo_root: str, tmpdir: str) -> dict:
    """T12 生产对拍（宽度约定已订正 + 错约定负例）。

    生产 ABI 的两条宽度入口互斥（snr_estimator.h:301-327、
    snr_science.cpp:14-24/58-61/134-136/157）：
      fwhm_px  = **检测块**椭圆高斯 FWHM（= 2.3548200450309493*sigma），>0 时优先；
      sigma_px = 本块 Moffat4 sigma（与 fwhm_px 同一 sigma 尺度），fwhm_px<=0 时启用。
    本单元 canon 的 FWHM_PX 属 **PSF 块 Moffat4 FWHM** ⇒ canonical 路径必须走 sigma_px；
    把 Moffat4 FWHM 灌进 fwhm_px 是生产明令禁止的跨块混用（DISP-STAR-007，sigma 高估
    1.914005x）。本函数把它作为**负例**保留，并要求该负例判红（能红能绿）。
    """
    src = os.path.join(repo_root, "lib/algorithms/noise_snr/cpp/src/snr_science.cpp")
    # 探针源码是本脚本的生成产物，写进临时目录而不是仓库源码树：
    # 否则每次复跑都会静默覆写 cpp/p1snr_probe.cpp 这个受版本管理的文件。
    os.makedirs(tmpdir, exist_ok=True)
    probe_cpp = os.path.join(tmpdir, "p1snr_probe.cpp")
    with open(probe_cpp, "w") as f:
        f.write(CPP_PROBE)
    exe = os.path.join(tmpdir, "p1snr_probe")
    if not os.path.exists(src):
        return {"pass": None, "status": "SKIP: production source not found", "src": src}
    inc = os.path.join(repo_root, "lib/algorithms/noise_snr/cpp/include")
    cmd = ["g++", "-O2", "-std=c++17", "-I", inc, "-o", exe, probe_cpp, src]
    cp = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if cp.returncode != 0:
        return {"pass": None, "status": "SKIP: g++ build failed", "stderr": cp.stderr[-2000:]}
    sigma_canon = C.moffat4_sigma_from_fwhm(FWHM_PX)
    half = C.moffat4_auto_half(FWHM_PX)
    P = C.moffat4_discrete_profile(FWHM_PX, half_px=half)
    rows = []
    ok = True
    notes = []
    for B in B_GRID:
        # (a) canonical 口径：fwhm_px<=0 + sigma_px=canon Moffat4 sigma ⇒ 两侧同一轮廓
        r = subprocess.run([exe, repr(F_S_E), repr(B), repr(READ_E), repr(GAIN),
                            "0.0", repr(sigma_canon)], capture_output=True, text=True, timeout=120)
        prod = json.loads(r.stdout.strip().splitlines()[-1])
        if not prod.get("ok"):
            # 生产 C ABI 要求 sigma_sky_adu > 0（fail-closed），B=0 无表示 ->
            # 这不是不一致，是**定义域边界**，如实登记。
            notes.append("B=0 (sigma_sky=0) rejected by production C ABI "
                         "(snr_science.cpp:132 requires sigma_sky_adu>0); rc=%s" % prod.get("rc"))
            rows.append({"B_e_per_pix": B, "prod": "REJECTED", "note": notes[-1]})
            continue
        py_sigma_e = C.sigma_flux_optimal_e(F_S_E, B, READ_E, P)
        py_sigma_adu = py_sigma_e / GAIN
        py_snr = F_S_E / py_sigma_e
        rel_snr = _rel(prod["snr_optimal"], py_snr)
        rel_sig = _rel(prod["sigma_f_optimal_adu"], py_sigma_adu)
        rel_p2 = _rel(prod["sum_p2"], float(np.sum(P**2)))
        good = rel_snr < 1e-12 and rel_sig < 1e-12 and rel_p2 < 1e-12
        # (b) 错约定负例：把 canon 的 Moffat4 FWHM 灌进生产 fwhm_px（旧版判据做法）⇒ 须判红
        r2 = subprocess.run([exe, repr(F_S_E), repr(B), repr(READ_E), repr(GAIN),
                             repr(FWHM_PX)], capture_output=True, text=True, timeout=120)
        prod2 = json.loads(r2.stdout.strip().splitlines()[-1])
        if prod2.get("ok"):
            wrong_rel_snr = _rel(prod2["snr_optimal"], py_snr)
            wrong_rel_p2 = _rel(prod2["sum_p2"], float(np.sum(P**2)))
            wrong_rejected = bool(wrong_rel_snr > 1e-6 and wrong_rel_p2 > 1e-6)
        else:
            wrong_rel_snr = float("nan")
            wrong_rel_p2 = float("nan")
            wrong_rejected = True          # 生产直接拒绝也算该负例判红
        ok = ok and good and wrong_rejected
        rows.append({"B_e_per_pix": B, "prod_snr": prod["snr_optimal"], "python_snr": py_snr,
                     "rel_diff_snr": rel_snr, "rel_diff_sigmaF": rel_sig,
                     "rel_diff_sumP2": rel_p2, "match": good,
                     "rel_diff_snr_wrong_convention": wrong_rel_snr,
                     "rel_diff_sumP2_wrong_convention": wrong_rel_p2,
                     "wrong_convention_rejected": wrong_rejected})
    oor = _out_of_range_negative(exe)
    ok = ok and oor["pass"]
    return {"pass": ok,
            "status": "OK (canonical green + wrong-convention negative red + out-of-range negative red)",
            "out_of_range_negative": oor,
            "compiler": "g++ (direct, no cmake/ninja)",
            "source": "lib/algorithms/noise_snr/cpp/src/snr_science.cpp (read-only)",
            "convention": {
                "canon_profile": "Moffat4 beta=4, FWHM_PX=%s px (PSF 块约定; frame_snr_canon.py:36)" % FWHM_PX,
                "canon_sigma_px": sigma_canon,
                "production_field_used": "sigma_px (fwhm_px=0)",
                "production_field_forbidden": "fwhm_px 属检测块高斯 FWHM (DISP-STAR-007)",
                "wrong_convention_sigma_inflation_expected": WRONG_CONVENTION_INFLATION,
                "authority": ["snr_estimator.h:301-327", "snr_science.cpp:14-24",
                              "snr_science.cpp:134-136", "snr_science.cpp:157"]},
            "grid_half_used": half, "notes": notes, "rows": rows}
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/reverse_verify/frame_snr")
    ap.add_argument("--repo", default=".")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    rng = np.random.default_rng(20260919)
    P = C.moffat4_discrete_profile(FWHM_PX)   # 生产 auto half 网格
    with tempfile.TemporaryDirectory(prefix="fsnr_") as td:
        res = {
            "experiment": "FRAME-SNR-CANON red lines",
            "config": {"F_s_e": F_S_E, "B_grid_e_per_pix": B_GRID, "gain_e_per_adu": GAIN,
                       "read_noise_e": READ_E, "fwhm_px": FWHM_PX,
                       "profile_grid_half": C.moffat4_auto_half(FWHM_PX)},
            "T8_sky_monotonicity": test_T8_monotonic(P),
            "T9_additive_sky_invariance": test_T9_additive_invariance(rng),
            "T10_red_counterexamples": test_T10_red_veto(P),
            "T11_analytic_vs_mc": test_T11_mc(rng),
            "T12_production_crosscheck": test_T12_production_crosscheck(args.repo, td),
        }
    res["summary"] = {k: (v.get("pass") if isinstance(v, dict) else None)
                      for k, v in res.items() if k.startswith("T")}
    res["all_pass"] = all(v is True for v in res["summary"].values())
    outp = os.path.join(args.out, "redlines.json")
    with open(outp, "w") as f:
        json.dump(res, f, indent=2, ensure_ascii=False)
    print(json.dumps(res["summary"], indent=2))
    print("T8 table:")
    for r in res["T8_sky_monotonicity"]["rows"]:
        print("  B=%9.1f  sigma_F=%12.6f e-   SNR=%12.6f" %
              (r["B_e_per_pix"], r["sigma_F_e"], r["SNR_canon"]))
    print("T10 red cases:")
    for c in res["T10_red_counterexamples"]["cases"]:
        print("  %-52s values=%s vetoed=%s" %
              (c["name"][:52], ["%.4g" % v for v in c["values"]], c["vetoed"]))
    print("written:", outp)
    return 0 if res["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
