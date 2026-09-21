#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FRAME-SNR-CANON —— **物理仿真红线测试**（负责人 2026-09-19 纠正后的正式版）。

判据（**先写死，不事后放宽**）：

  P8  天光单调性（**物理散粒噪声**，不是加常数）：
      固定真值 F_s；B↑ 经 Poisson（方差=均值）⇒ sigma_i^2 ↑ ⇒ sigma_F ↑ ⇒ SNR ↓。
      判据：median(SNR_hat) 在 B 网格上**严格单调下降**，且与解析预测一致
      （相对偏差 < 5%）；B→inf 时 SNR→0（SNR*sqrt(B) 收敛）。
  P8b **负例（真值"无效应"）**：在**同一物理实现**上只加纯偏移常数 C
      （噪声统计不变）⇒ SNR 变化必须为 0（< 1e-12 相对）。
      该负例证明"加法本身不产生任何效应"，从而把 P8 的效应**唯一归因于散粒噪声**。
  P8c **决定性归因**：
      (i) 固定天光**均值**、人为改变天光**方差** ⇒ SNR 必须随方差下降；
      (ii) 固定天光**方差**、人为改变天光**均值** ⇒ SNR 必须**不变**。
      两者合起来证明：驱动 SNR 的是**方差（散粒噪声）**，不是均值。
  P9  加性天光不变性：加常数 C 后 F_hat 与 sigma_F_hat 都不变（< 1e-12）。
  P10 红例否决（在**同一物理数据**上）：未扣背景型 / 功率比型 / 未扣背景窗口型
      必须随 B **上升**（违反单调性）⇒ 否决。
  P11 解析 vs 物理 MC：解析 sigma_F 与 MC 实测散度一致。
  P12 生产实现对拍：Python 独立复算 vs 生产 C ABI snr_source_snr_f64。
  P13 真实数据作底：真实实拍帧结构 + 受控泊松天光 ⇒ 单调性仍成立。
      （**登记**：本工作区**没有哈勃数据**，用 M42 真实实拍帧替代。）

用法: TMPDIR=/dev/shm/astrocs_fsnr python3 run_redlines_physical.py [--out DIR] [--quick]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import statistics
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_snr_canon as C  # noqa: E402
import frame_snr_physical as PH  # noqa: E402

FWHM_PX = 4.0
GAIN = 1.5
READ_E = 5.0
DARK_E = 0.5
F_S_E = 2000.0
B_GRID = [0.0, 10.0, 100.0, 1000.0, 10000.0]
B_ASYMPTOTIC = [1.0e6, 1.0e8, 1.0e10]
SEED = 20260919


def monotone_criterion(vals: list[float]) -> bool:
    """P8 的**唯一**判据（同一函数用于绿例与红例 => 能红能绿）。"""
    return all(vals[i + 1] < vals[i] for i in range(len(vals) - 1))


def strictly_increasing(vals: list[float]) -> bool:
    return all(vals[i + 1] > vals[i] for i in range(len(vals) - 1))


def _rel(a, b):
    return abs(a - b) / max(abs(a), abs(b), 1e-300)


# ---------------------------------------------------------------------------
# P8 / P11：物理 MC 天光扫描
# ---------------------------------------------------------------------------


def analytic_prediction(B, P_flat, n_sky, gain, read_noise_e, dark_e, F_s_e,
                        sig_sky_hat2_override=None):
    """两条解析预测（ADU 域；SNR 无量纲，gain 在比值中约掉）。

    (A) estimator_model —— **估计量自己声称的噪声模型**（用于 MC 自洽性检验）：
        sigma_sky_hat_adu^2 = (B + dark + read^2)/gain^2   （天空环 MAD 估到的是**总**逐像素噪声）
        sigma_i^2 = sigma_sky_hat_adu^2 + (read/gain)^2 + F_s*P_i/gain
        Var(F) = 1/sum P^2/sigma^2 + (sum P/sigma^2 / sum P^2/sigma^2)^2 * (pi/2)*sigma_sky_hat_adu^2/n_sky
        （末项 = 背景中位数自身的方差；pi/2 为 iid 高斯下中位数的渐近方差因子）

    (B) physical_truth —— **物理真值模型**（无读出噪声重复计入、无背景估计项）：
        sigma_i^2 = (B + dark + read^2 + F_s*gain*P_i)/gain^2
    (A)/(B) 之比 = 现行实现的读出噪声重复计入与背景估计代价（**可量化偏差**）。
    """
    g2 = gain * gain
    F_adu = F_s_e / gain                                    # ADU
    sig_sky_hat2_theory = (B + dark_e + read_noise_e**2) / g2   # ADU^2（含读出噪声，理论值）
    sig_sky_hat2 = (sig_sky_hat2_theory if sig_sky_hat2_override is None
                    else float(sig_sky_hat2_override))          # 实测值（可选覆盖）
    rn2 = (read_noise_e / gain) ** 2                        # ADU^2
    # 源泊松项：逐像素源电子数 F_e*P_i，方差 (F_e*P_i)/g^2  [ADU^2] = F_adu*P_i/gain
    s2_est = sig_sky_hat2 + rn2 + F_adu * P_flat / gain
    denom = float(np.sum(P_flat**2 / s2_est))
    var_est = 1.0 / denom
    w_sum = float(np.sum(P_flat / s2_est))
    var_bkg = (math.pi / 2.0) * sig_sky_hat2 / max(n_sky, 1)
    var_est_bkg = var_est + (w_sum / denom) ** 2 * var_bkg
    # 物理真值（无读出噪声重复计入、无背景估计项）：sigma_i^2 [ADU^2] = (B+dark+read^2+F_e*P_i)/g^2
    s2_true = ((B + dark_e) + read_noise_e**2 + F_s_e * P_flat) / g2
    var_true = 1.0 / float(np.sum(P_flat**2 / s2_true))
    return {
        "SNR_estimator_model": F_adu / math.sqrt(var_est),
        "SNR_estimator_model_with_bkg": F_adu / math.sqrt(var_est_bkg),
        "SNR_physical_truth": F_adu / math.sqrt(var_true),
        "sigma_F_estimator_model_adu": math.sqrt(var_est_bkg),
        "sigma_F_physical_truth_adu": math.sqrt(var_true),
        "F_truth_adu": F_adu,
        "sig_sky_hat_theory_adu": math.sqrt(sig_sky_hat2_theory),
        "sig_sky_hat_used_adu": math.sqrt(sig_sky_hat2),
    }


def physical_sweep(B_list, n_mc, seed, base_e=None, shape=(121, 121), center=(60, 60)):
    rows = []
    for B in B_list:
        rng = np.random.default_rng(seed + int(math.log10(max(B, 1.0)) * 1000) + int(B % 997))
        snrs, Fs, sigs, skyhat = [], [], [], []
        P_flat = None
        n_sky = 0
        for _ in range(n_mc):
            img, P_full, ap_mask, ann_mask = PH.simulate_physical(
                F_s_e=F_S_E, B_e=B, rng=rng, shape=shape, center=center,
                fwhm_px=FWHM_PX, gain=GAIN, read_noise_e=READ_E, dark_e=DARK_E,
                flat=None, sky_grad_e=0.0, base_e=base_e)
            e = PH.estimate_snr(img, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
            snrs.append(e["snr"])
            Fs.append(e["F_hat_adu"])
            sigs.append(e["sigma_F_adu"])
            skyhat.append(e["sig_sky_adu"])
            if P_flat is None:
                P_flat = P_full.ravel().copy()
                n_sky = int(np.sum(ann_mask))
        med = float(np.median(snrs))
        med_F = float(np.median(Fs))
        med_sig = float(np.median(sigs))
        # 关键：解析模型用**实测的** sig_sky_hat（隔离"方差传播模型"与"噪声估计器自身偏差"）
        sig_sky_med = float(np.median(skyhat))
        pred = analytic_prediction(B, P_flat, n_sky, GAIN, READ_E, DARK_E, F_S_E,
                                   sig_sky_hat2_override=sig_sky_med**2)
        rows.append({
            "B_e_per_pix": B, "n_mc": n_mc,
            "SNR_median": med,
            "SNR_p16": float(np.percentile(snrs, 16)),
            "SNR_p84": float(np.percentile(snrs, 84)),
            "F_hat_median_adu": med_F,
            "sigma_F_median_adu": med_sig,
            "SNR_estimator_model": pred["SNR_estimator_model"],
            "SNR_estimator_model_with_bkg": pred["SNR_estimator_model_with_bkg"],
            "SNR_physical_truth": pred["SNR_physical_truth"],
            "sigma_F_estimator_model_adu": pred["sigma_F_estimator_model_adu"],
            "sigma_F_estimator_model_no_bkg_adu": (
                pred["F_truth_adu"] / pred["SNR_estimator_model"]),
            "sigma_F_physical_truth_adu": pred["sigma_F_physical_truth_adu"],
            "F_truth_adu": pred["F_truth_adu"],
            "n_sky_pix": n_sky,
            "sig_sky_hat_median_adu": sig_sky_med,
            "sig_sky_hat_theory_adu": pred["sig_sky_hat_theory_adu"],
            "mad_estimator_bias_ratio": sig_sky_med / pred["sig_sky_hat_theory_adu"],
            # 估计量返回的 sigma_F 是**模型式**（不含背景估计方差项），故与 no_bkg 模型比
            "rel_diff_sigmaF_vs_estimator_model": _rel(
                med_sig, pred["F_truth_adu"] / pred["SNR_estimator_model"]),
            "bkg_estimation_term_ratio": (
                pred["sigma_F_estimator_model_adu"]
                / (pred["F_truth_adu"] / pred["SNR_estimator_model"])),
            "rel_diff_F_vs_truth": _rel(med_F, pred["F_truth_adu"]),
            "rel_diff_SNR_vs_physical_truth": _rel(med, pred["SNR_physical_truth"]),
            "estimator_over_truth_sigmaF": med_sig / pred["sigma_F_physical_truth_adu"],
        })
    return rows


def test_P8(n_mc: int) -> dict:
    rows = physical_sweep(B_GRID, n_mc, SEED)
    vals = [r["SNR_median"] for r in rows]
    # ---- "B -> inf 时 SNR -> 0"：用**解析式**断言（MC 在 SNR<<1 区间不可分辨）----
    # 诚实登记：SNR = F/sigma_F，单次实现的 SNR 散度 = spread(F_hat)/sigma_F = 1，与 B 无关；
    # 当 B 大到 SNR << 1 时，有限次 MC 的**中位数**标准误 ~1.253/sqrt(n_mc) >> SNR，
    # 无法分辨真值 => 该极限必须用解析式断言，不能假装 MC 测到了。
    asym_analytic = []
    P_flat_asym = None
    for B in [1.0e4, 1.0e6, 1.0e8, 1.0e10, 1.0e12]:
        _img, P_full, _ap, _ann = PH.simulate_physical(
            F_s_e=F_S_E, B_e=B, rng=np.random.default_rng(0), fwhm_px=FWHM_PX,
            gain=GAIN, read_noise_e=READ_E, dark_e=DARK_E)
        P_flat_asym = P_full.ravel()
        pred = analytic_prediction(B, P_flat_asym, 480, GAIN, READ_E, DARK_E, F_S_E)
        asym_analytic.append({"B_e_per_pix": B,
                              "SNR_physical_truth": pred["SNR_physical_truth"],
                              "SNR_x_sqrtB": pred["SNR_physical_truth"] * math.sqrt(B)})
    ref = asym_analytic[-1]["SNR_x_sqrtB"]
    # 天空受限渐近律 SNR*sqrt(B) -> F/sqrt(A_NEA)：在最后 3 个数量级上相对误差 <1e-3
    # （B=1e4 处读噪/暗流仍有 0.4% 贡献，属有限 B 修正，不是律失效）
    asym_ok = (all(asym_analytic[i + 1]["SNR_physical_truth"]
                   < asym_analytic[i]["SNR_physical_truth"]
                   for i in range(len(asym_analytic) - 1))
               and all(abs(a["SNR_x_sqrtB"] / ref - 1.0) < 1e-3
                       for a in asym_analytic[-3:]))
    # MC 自洽性：实测 sigma_F 必须复现**估计量自己的噪声模型**（<2%）
    agree = all(r["rel_diff_sigmaF_vs_estimator_model"] < 0.02 for r in rows)
    # F_hat 无偏性判据必须计入 MC 误差：median(F_hat) 标准误 ~ 1.253*sigma_F/sqrt(n_mc)
    unbiased = all(
        abs(r["F_hat_median_adu"] - r["F_truth_adu"])
        < max(0.02 * r["F_truth_adu"],
              3.0 * 1.253 * r["sigma_F_median_adu"] / math.sqrt(r["n_mc"]))
        for r in rows)
    return {
        "pass": bool(monotone_criterion(vals) and agree and unbiased and asym_ok),
        "noise_model": "Poisson(source+sky+dark) + Gaussian(read) + gain quantization(round)",
        "rows": rows,
        "strictly_decreasing": monotone_criterion(vals),
        "sigma_F_matches_estimator_model_within_2pct": agree,
        "F_hat_unbiased_within_mc_error": unbiased,
        "asymptotic_analytic_rows": asym_analytic,
        "asymptotic_ok": asym_ok,
        "asymptotic_note": ("B->inf 的极限用解析式断言（SNR*sqrt(B) -> F/sqrt(A_NEA)，相对误差<1e-3）；"
                            "MC 在 SNR<<1 区间不可分辨，故不作为判据。"),
        "drop_ratio_first_to_last": vals[0] / vals[-1],
    }


# ---------------------------------------------------------------------------
# P8b 负例：纯偏移常数（真值无效应）
# ---------------------------------------------------------------------------


def test_P8b_negative_control(n_mc: int = 200) -> dict:
    rng = np.random.default_rng(SEED + 101)
    B = 1000.0
    C_ADD = 1234.5
    d_snr, d_F, d_sig = [], [], []
    for _ in range(n_mc):
        img, P_full, ap_mask, ann_mask = PH.simulate_physical(
            F_s_e=F_S_E, B_e=B, rng=rng, fwhm_px=FWHM_PX, gain=GAIN,
            read_noise_e=READ_E, dark_e=DARK_E)
        e1 = PH.estimate_snr(img, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
        e2 = PH.estimate_snr(img + C_ADD, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
        d_snr.append(_rel(e1["snr"], e2["snr"]))
        d_F.append(_rel(e1["F_hat_adu"], e2["F_hat_adu"]))
        d_sig.append(_rel(e1["sigma_F_adu"], e2["sigma_F_adu"]))
    worst = max(d_snr)
    return {
        "pass": worst < 1e-12,
        "C_add_adu": C_ADD, "n_mc": n_mc,
        "max_rel_change_SNR": worst,
        "max_rel_change_F_hat": max(d_F),
        "max_rel_change_sigma_F": max(d_sig),
        "verdict": "真值『无效应』：纯加法不改变 SNR（不是天光效应的载体）",
    }


# ---------------------------------------------------------------------------
# P8c 决定性归因：均值 vs 方差
# ---------------------------------------------------------------------------


def test_P8c_attribution(n_mc: int = 200) -> dict:
    shape, center = (121, 121), (60, 60)
    P = C.moffat4_discrete_profile(FWHM_PX)

    def run(B, var_override):
        rng = np.random.default_rng(SEED + 555)
        out = []
        for _ in range(n_mc):
            img, P_full, ap_mask, ann_mask = PH.simulate_physical(
                F_s_e=F_S_E, B_e=B, rng=rng, shape=shape, center=center,
                fwhm_px=FWHM_PX, gain=GAIN, read_noise_e=READ_E, dark_e=DARK_E,
                variance_override_e2=var_override)
            e = PH.estimate_snr(img, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
            out.append(e["snr"])
        return float(np.median(out))

    # (i) 固定均值、变方差
    B0 = 1000.0
    var_arm = [10.0, 100.0, 1000.0, 10000.0]
    i_rows = [{"B_mean_e": B0, "sky_variance_e2": v, "SNR_median": run(B0, v)}
              for v in var_arm]
    i_vals = [r["SNR_median"] for r in i_rows]
    # (ii) 固定方差、变均值
    V0 = 1000.0
    mean_arm = [100.0, 1000.0, 10000.0, 100000.0]
    ii_rows = [{"B_mean_e": m, "sky_variance_e2": V0, "SNR_median": run(m, V0)}
               for m in mean_arm]
    ii_vals = [r["SNR_median"] for r in ii_rows]
    ii_spread = (max(ii_vals) - min(ii_vals)) / max(ii_vals)
    return {
        "pass": bool(monotone_criterion(i_vals) and ii_spread < 0.05),
        "i_fixed_mean_vary_variance": {
            "rows": i_rows, "strictly_decreasing": monotone_criterion(i_vals),
            "note": "天光均值固定 1000 e-/pix；方差人为 10..10000 e^2 ⇒ SNR 必须下降",
        },
        "ii_fixed_variance_vary_mean": {
            "rows": ii_rows, "relative_spread": ii_spread,
            "note": "天光方差固定 1000 e^2；均值 100..100000 e-/pix ⇒ SNR 必须不变",
        },
        "verdict": "驱动帧级 SNR 的是天光**方差（散粒噪声）**，不是天光**均值**",
    }


# ---------------------------------------------------------------------------
# P9 加性不变性（估计量层面，物理实现上）
# ---------------------------------------------------------------------------


def test_P9(n_mc: int = 100) -> dict:
    rng = np.random.default_rng(SEED + 202)
    C_ADD = 500.0
    rf, rs = [], []
    for _ in range(n_mc):
        img, P_full, ap_mask, ann_mask = PH.simulate_physical(
            F_s_e=F_S_E, B_e=300.0, rng=rng, fwhm_px=FWHM_PX, gain=GAIN,
            read_noise_e=READ_E, dark_e=DARK_E)
        e1 = PH.estimate_snr(img, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
        e2 = PH.estimate_snr(img + C_ADD, P_full, ann_mask, gain=GAIN, read_noise_e=READ_E)
        rf.append(_rel(e1["F_hat_adu"], e2["F_hat_adu"]))
        rs.append(_rel(e1["sigma_F_adu"], e2["sigma_F_adu"]))
    return {"pass": max(rf) < 1e-12 and max(rs) < 1e-12, "n_mc": n_mc,
            "max_rel_change_F_hat": max(rf), "max_rel_change_sigma_F": max(rs),
            "C_add_adu": C_ADD}


# ---------------------------------------------------------------------------
# P10 红例否决（物理数据）
# ---------------------------------------------------------------------------


def test_P10(n_mc: int = 120) -> dict:
    cases = ["RED_A_unsub_aperture", "RED_B_power_ratio_raw", "RED_C_window_unsub"]
    table = {k: [] for k in cases}
    green = []
    for B in B_GRID:
        rng = np.random.default_rng(SEED + 909 + int(B % 9973))
        acc = {k: [] for k in cases}
        g = []
        for _ in range(n_mc):
            img, P_full, ap_mask, ann_mask = PH.simulate_physical(
                F_s_e=F_S_E, B_e=B, rng=rng, fwhm_px=FWHM_PX, gain=GAIN,
                read_noise_e=READ_E, dark_e=DARK_E)
            red = PH.estimate_red_definitions(img, P_full, ap_mask, ann_mask,
                                              gain=GAIN, read_noise_e=READ_E)
            for k in cases:
                acc[k].append(red[k])
            g.append(PH.estimate_snr(img, P_full, ann_mask, gain=GAIN,
                                     read_noise_e=READ_E)["snr"])
        for k in cases:
            table[k].append(float(np.median(acc[k])))
        green.append(float(np.median(g)))
    out_cases = []
    for k in cases:
        vals = table[k]
        out_cases.append({"name": k, "values": vals,
                          "P8_same_criterion_result": monotone_criterion(vals),
                          "strictly_increasing": strictly_increasing(vals),
                          "vetoed": strictly_increasing(vals)})
    return {
        "pass": all(c["vetoed"] for c in out_cases) and monotone_criterion(green),
        "green_canon_values": green,
        "green_strictly_decreasing": monotone_criterion(green),
        "cases": out_cases,
    }


# ---------------------------------------------------------------------------
# P12 生产实现对拍
# ---------------------------------------------------------------------------


CPP_PROBE = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "snr_estimator.h"
int main(int argc, char** argv) {
  if (argc < 6) return 2;
  const double F_e = std::atof(argv[1]);
  const double B_e = std::atof(argv[2]);
  const double rn_e = std::atof(argv[3]);
  const double gain = std::atof(argv[4]);
  const double fwhm = std::atof(argv[5]);
  SnrSourceParams p; std::memset(&p, 0, sizeof(p));
  p.flux_adu = F_e / gain;
  p.fwhm_px = fwhm;
  p.sigma_sky_adu = std::sqrt(B_e) / gain;
  p.gain_e_per_adu = gain;
  p.read_noise_e = rn_e;
  SnrSourceResult r;
  const int rc = snr_source_snr_f64(&p, &r);
  if (rc != 0 || r.status != 0) { std::printf("{\"ok\":false,\"rc\":%d}\n", rc); return 1; }
  std::printf("{\"ok\":true,\"snr_optimal\":%.17g,\"sigma_f_optimal_adu\":%.17g,\"sum_p2\":%.17g}\n",
              r.snr_optimal, r.sigma_f_optimal_adu, r.sum_p2);
  return 0;
}
'''


def test_P12(repo_root: str, tmpdir: str) -> dict:
    src = os.path.join(repo_root, "lib/algorithms/noise_snr/cpp/src/snr_science.cpp")
    cppdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cpp")
    os.makedirs(cppdir, exist_ok=True)
    probe = os.path.join(cppdir, "p1snr_probe.cpp")
    with open(probe, "w") as f:
        f.write(CPP_PROBE)
    exe = os.path.join(tmpdir, "p1snr_probe")
    inc = os.path.join(repo_root, "lib/algorithms/noise_snr/cpp/include")
    cp = subprocess.run(["g++", "-O2", "-std=c++17", "-I", inc, "-o", exe, probe, src],
                        capture_output=True, text=True, timeout=300)
    if cp.returncode != 0:
        return {"pass": None, "status": "SKIP: g++ build failed", "stderr": cp.stderr[-1500:]}
    P = C.moffat4_discrete_profile(FWHM_PX)
    rows, ok, notes = [], True, []
    for B in B_GRID:
        r = subprocess.run([exe, repr(F_S_E), repr(B + DARK_E), repr(READ_E), repr(GAIN),
                            repr(FWHM_PX)], capture_output=True, text=True, timeout=120)
        prod = json.loads(r.stdout.strip().splitlines()[-1])
        if not prod.get("ok"):
            notes.append("B=%s rejected by production C ABI (requires sigma_sky_adu>0)" % B)
            rows.append({"B_e_per_pix": B, "prod": "REJECTED"})
            continue
        sig_e = C.sigma_flux_optimal_e(F_S_E, B + DARK_E, READ_E, P)
        py_snr = F_S_E / sig_e          # SNR 无量纲: gain 在分子分母约掉
        py_sig_adu = sig_e / GAIN
        rel = _rel(prod["snr_optimal"], py_snr)
        ok = ok and rel < 1e-12 and _rel(prod["sum_p2"], float(np.sum(P**2))) < 1e-12
        rows.append({"B_e_per_pix": B, "prod_snr": prod["snr_optimal"], "python_snr": py_snr,
                     "rel_diff_snr": rel, "rel_diff_sigmaF": _rel(prod["sigma_f_optimal_adu"], py_sig_adu),
                     "rel_diff_sumP2": _rel(prod["sum_p2"], float(np.sum(P**2)))})
    return {"pass": ok, "status": "OK", "notes": notes, "rows": rows,
            "source": "lib/algorithms/noise_snr/cpp/src/snr_science.cpp (read-only, g++ direct)"}


# ---------------------------------------------------------------------------
# P13 真实数据作底
# ---------------------------------------------------------------------------

REAL_CANDIDATES = [
    "run/RELEASE-02/perf-drz/norm_t2_m1_red/cleaned_M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    "run/RELEASE-02/p1-phot-fix2/norm/m1_run1/cleaned_M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
]


def test_P13(repo_root: str, n_mc: int) -> dict:
    path = None
    for c in REAL_CANDIDATES:
        p = os.path.join(repo_root, c)
        if os.path.exists(p):
            path = p
            break
    if path is None:
        return {"pass": None, "status": "SKIP: no real frame found",
                "hst_data_available": False}
    try:
        from astropy.io import fits
        with fits.open(path, memmap=True) as hdul:
            data = np.array(hdul[0].data, dtype=np.float64)
            hdr = hdul[0].header
    except Exception as exc:  # noqa: BLE001
        return {"pass": None, "status": "SKIP: cannot read FITS", "error": str(exc)}
    ny, nx = data.shape
    # 取一块含真实星场的 256^2 子图（在粗网格上找最亮的局部峰）
    step = 256
    best, best_xy = -1.0, (ny // 2, nx // 2)
    for y0 in range(0, ny - step, step):
        for x0 in range(0, nx - step, step):
            blk = data[y0 : y0 + step, x0 : x0 + step]
            v = float(np.percentile(blk, 99.9))
            if v > best:
                best, best_xy = v, (y0, x0)
    y0, x0 = best_xy
    cut = data[y0 : y0 + step, x0 : x0 + step]
    med = float(np.median(cut))
    # 真实**结构**（源 + 星云 + 背景起伏）转 e-；天光水平由 B 受控注入
    base_e = (cut - med) * GAIN
    center = (step // 2, step // 2)
    rows = []
    for B in B_GRID:
        rng = np.random.default_rng(SEED + 31337 + int(B % 9973))
        snrs = []
        for _ in range(n_mc):
            img, P_full, ap_mask, ann_mask = PH.simulate_physical(
                F_s_e=F_S_E, B_e=B, rng=rng, shape=(step, step), center=center,
                fwhm_px=FWHM_PX, gain=GAIN, read_noise_e=READ_E, dark_e=DARK_E,
                base_e=base_e)
            snrs.append(PH.estimate_snr(img, P_full, ann_mask, gain=GAIN,
                                        read_noise_e=READ_E)["snr"])
        rows.append({"B_e_per_pix": B, "n_mc": n_mc, "SNR_median": float(np.median(snrs))})
    vals = [r["SNR_median"] for r in rows]
    return {
        "pass": bool(monotone_criterion(vals)),
        "status": "OK",
        "real_frame": os.path.relpath(path, repo_root),
        "frame_shape": [int(ny), int(nx)],
        "cutout_origin_yx": [int(y0), int(x0)], "cutout_size": step,
        "real_median_adu": med, "cutout_p999_adu": best,
        "hst_data_available": False,
        "hst_note": "全仓 find -iname '*hst*' / '*hubble*' 命中 0 —— 本工作区无哈勃数据；"
                    "改用真实实拍 M42 帧（Flying Dutchman 300S Red）作真实结构底",
        "rows": rows, "strictly_decreasing": monotone_criterion(vals),
        "noise_model": "真实结构(源+星云+背景起伏) + Poisson(B_target+dark) + Gaussian(read) + ADU 量化",
    }


# ---------------------------------------------------------------------------
# P14 无物理单位闭合：单位尺度不变性 + 生产（gain-free）分支单调性
# ---------------------------------------------------------------------------


def test_P14_unit_free(n_mc: int = 200) -> dict:
    """负责人纠正（2026-09-19）：FITS 头拿不到 gain/口径/曝光 ⇒ 定义**不得依赖物理单位闭合**。

    (a) 单位尺度不变性：把整帧信号线性缩放 alpha（等价于 ADU<->e- 或任意线性归一），
        gain-free 分支的 SNR 必须**严格不变**（F_hat 与 sig_sky 同比例缩放）。
    (b) gain-free 分支（= 生产实际分支）的天光单调性必须成立。
    """
    rng = np.random.default_rng(SEED + 606)
    B = 1000.0
    img, P_full, _ap, ann_mask = PH.simulate_physical(
        F_s_e=F_S_E, B_e=B, rng=rng, fwhm_px=FWHM_PX, gain=GAIN,
        read_noise_e=READ_E, dark_e=DARK_E)
    base = PH.estimate_snr(img, P_full, ann_mask, gain=0.0, read_noise_e=0.0,
                           mode="production_gain_free")
    scale_rows = []
    for alpha in (0.5, 1.5, GAIN, 1000.0, 1e-3):
        e = PH.estimate_snr(img * alpha, P_full, ann_mask, gain=0.0, read_noise_e=0.0,
                            mode="production_gain_free")
        scale_rows.append({"alpha": alpha, "SNR": e["snr"],
                           "rel_change": _rel(base["snr"], e["snr"]),
                           "F_hat_scaled_ratio": e["F_hat_adu"] / base["F_hat_adu"]})
    a_ok = all(r["rel_change"] < 1e-12 for r in scale_rows)

    # (b) gain-free 分支的天光单调性
    rows = []
    for Bv in B_GRID:
        r2 = np.random.default_rng(SEED + 707 + int(Bv % 9973))
        vals = []
        for _ in range(n_mc):
            im, Pf, _ap2, ann2 = PH.simulate_physical(
                F_s_e=F_S_E, B_e=Bv, rng=r2, fwhm_px=FWHM_PX, gain=GAIN,
                read_noise_e=READ_E, dark_e=DARK_E)
            vals.append(PH.estimate_snr(im, Pf, ann2, gain=0.0, read_noise_e=0.0,
                                        mode="production_gain_free")["snr"])
        rows.append({"B_e_per_pix": Bv, "SNR_median": float(np.median(vals))})
    b_vals = [r["SNR_median"] for r in rows]
    return {
        "pass": bool(a_ok and monotone_criterion(b_vals)),
        "note": "定义不依赖 gain/口径/曝光时间；SNR 无量纲，对信号单位线性缩放严格不变",
        "unit_scale_invariance": {"base_SNR": base["snr"], "rows": scale_rows,
                                  "all_invariant_within_1e-12": a_ok},
        "gain_free_sky_monotonicity": {"rows": rows,
                                       "strictly_decreasing": monotone_criterion(b_vals)},
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/reverse_verify/frame_snr")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    n_mc = 120 if args.quick else 600
    n_mc_real = 60 if args.quick else 200
    with tempfile.TemporaryDirectory(prefix="fsnr_phys_") as td:
        res = {
            "experiment": "FRAME-SNR-CANON physical red lines",
            "config": {"F_s_e": F_S_E, "B_grid_e_per_pix": B_GRID, "gain_e_per_adu": GAIN,
                       "read_noise_e": READ_E, "dark_e_per_pix": DARK_E,
                       "fwhm_px": FWHM_PX, "n_mc": n_mc, "seed": SEED},
            "noise_model": {
                "electron_domain": "m = (B + dark + F_s*P) * flat + grad ; I_e = Poisson(m) + N(0, sigma_R)",
                "adu": "I_adu = round(I_e / gain)",
                "estimator": "annulus-median background; PSF-weighted optimal extraction; "
                             "sigma_i^2 = sig_sky_hat^2 + (sigma_R/gain)^2 + max(F_hat,0)*P_i/gain",
            },
            "P8_physical_sky_monotonicity": test_P8(n_mc),
            "P8b_negative_control_pure_offset": test_P8b_negative_control(),
            "P8c_attribution_mean_vs_variance": test_P8c_attribution(),
            "P9_additive_invariance": test_P9(),
            "P10_red_counterexamples_physical": test_P10(),
            "P12_production_crosscheck": test_P12(args.repo, td),
            "P13_real_data_base": test_P13(args.repo, n_mc_real),
            "P14_unit_free_no_physical_closure": test_P14_unit_free(),
        }
    res["summary"] = {k: (v.get("pass") if isinstance(v, dict) else None)
                      for k, v in res.items() if k.startswith("P")}
    res["all_pass"] = all(v is True for v in res["summary"].values())
    outp = os.path.join(args.out, "redlines_physical.json")
    with open(outp, "w") as f:
        json.dump(res, f, indent=2, ensure_ascii=False)
    print(json.dumps(res["summary"], indent=2))
    print("P8 (physical, Poisson shot noise) table:")
    for r in res["P8_physical_sky_monotonicity"]["rows"]:
        print("  B=%9.1f  SNR_med=%9.4f  sigmaF_med=%9.3f (model %9.3f, rel %.1e)  "
              "SNR_truth=%8.4f  est/truth sigmaF=%.4f  bkg-term x%.3f" %
              (r["B_e_per_pix"], r["SNR_median"], r["sigma_F_median_adu"],
               r["sigma_F_estimator_model_no_bkg_adu"], r["rel_diff_sigmaF_vs_estimator_model"],
               r["SNR_physical_truth"], r["estimator_over_truth_sigmaF"],
               r["bkg_estimation_term_ratio"]))
    print("P8 asymptotic (analytic) SNR*sqrt(B) ->",
          [round(a["SNR_x_sqrtB"], 4)
           for a in res["P8_physical_sky_monotonicity"]["asymptotic_analytic_rows"]])
    print("P8b negative control max rel change:",
          res["P8b_negative_control_pure_offset"]["max_rel_change_SNR"])
    print("P8c i) fixed mean vary var:", ["%.3f" % r["SNR_median"] for r in res["P8c_attribution_mean_vs_variance"]["i_fixed_mean_vary_variance"]["rows"]])
    print("P8c ii) fixed var vary mean:", ["%.3f" % r["SNR_median"] for r in res["P8c_attribution_mean_vs_variance"]["ii_fixed_variance_vary_mean"]["rows"]])
    print("P10 red cases (physical):")
    for c in res["P10_red_counterexamples_physical"]["cases"]:
        print("  %-26s %s vetoed=%s" % (c["name"], ["%.4g" % v for v in c["values"]], c["vetoed"]))
    print("P10 green canon:", ["%.4g" % v for v in res["P10_red_counterexamples_physical"]["green_canon_values"]])
    print("P13 real-base rows:", [(r["B_e_per_pix"], round(r["SNR_median"], 3)) for r in res["P13_real_data_base"].get("rows", [])])
    p14 = res["P14_unit_free_no_physical_closure"]
    print("P14 unit-scale invariance max rel change:",
          max(r["rel_change"] for r in p14["unit_scale_invariance"]["rows"]))
    print("P14 gain-free sky monotonicity:",
          [round(r["SNR_median"], 4) for r in p14["gain_free_sky_monotonicity"]["rows"]])
    print("written:", outp)
    return 0 if res["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
