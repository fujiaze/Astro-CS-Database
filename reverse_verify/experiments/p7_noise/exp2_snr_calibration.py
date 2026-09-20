#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7-EXP2 —— **SNR 标定**：实测 SNR（MC 回收散差）vs 三条解析 SNR 方程。

对照的三条方程（一手出处见 p7lib 与报告 §1）
-------------------------------------------
  (a) CCD 方程（Howell 2006；Merline & Howell 1995）
        SNR = N*/sqrt(N* + n_pix*(N_S + N_D + N_R^2))
  (b) PSF 加权最优提取（Horne 1986, PASP 98, 609, DOI 10.1086/131801）
        SNR = F*sqrt(sum_i P_i^2)/sigma_pix
  (c) LSST SMTN-002（Jones, DOI 10.71929/rubin/3408482）
        SNR = C/sqrt(C/g + (B/g + sigma_instr^2)*n_eff), n_eff = 2.266*(FWHM/pixscale)^2

实测 SNR 的定义（**关键**）：对同一真值通量 F 的 N_mc 次独立实现，回收通量 F_hat 的
**样本散差**给出 sigma_F 的无偏估计 ⇒ SNR_meas = F / std(F_hat)。
这与解析式的比较才是"标定"；用单帧的 sigma_F 公式自比是循环论证。

判据（写死）
    C1 无偏：median(F_hat/F_true - 1) 在 SNR>10 档 |·| < 2%
    C2 标定：SNR_meas/SNR_pred ∈ [0.8, 1.25]，且三条方程的预测互相一致到 <5%
    C3 线性：SNR_meas 与 F 在固定天光下成正比（log-log 斜率 1±0.1）
    C4 天光项：SNR ∝ 1/sqrt(B) 的高天光渐近斜率 = -0.5±0.1
    C5 负例：零噪声臂 std(F_hat) < 1e-9 e-（真值无散差 ⇒ 归零）
"""

from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE))
import p7lib as P  # noqa: E402
import noise_model as NM  # noqa: E402
import render as R  # noqa: E402
import m16_scene as M16  # noqa: E402

SCENES = ["m16_nebula_core", "m16_starfield", "m16_dark_lowsnr"]
SEED = 20260924
PIXSCALE = 0.04


def run_grid(scene_id, mags, sky_mults, n_mc, fwhm=2.0, zero_noise=False, seed0=SEED):
    ov0 = {"inject_stars": {"n": 12, "mode": "abmag", "mag_range": [20.0, 20.0],
                            "seed": 1616, "avoid_bright_base": True, "avoid_k_sigma": 5.0,
                            "min_separation_px": 40.0}}
    _, truth0, _, _ = P.render_scene_frame(scene_id, seed=seed0, overrides=ov0)
    zp = float(truth0["photometric"]["zp_ab"])
    t = float(truth0["exposure_s"])
    sky_ref = float(truth0["real_base"]["pedestal_e_per_s"])
    sites = [(float(s["y"]), float(s["x"])) for s in truth0["stars_in_frame"]]
    out = []
    for mag in mags:
        F = float(M16.ab_mag_to_rate_e_per_s(mag, zp) * t)
        for mult in sky_mults:
            sky = sky_ref * mult
            pos = [[y, x, F] for y, x in sites]
            ov = {"psf": {"model": "moffat4", "fwhm_px": float(fwhm), "beta": 4.0},
                  "sky": {"mode": "const", "level_e_per_s": float(sky)},
                  "inject_stars": {"positions": pos, "avoid_bright_base": False},
                  "mode": NM.MODE_PHYSICAL}
            if zero_noise:
                ov["detector"] = {"read_noise_e": 0.0, "dark_current_e_per_s": 0.0}
                ov["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "tilt_x": 0.0,
                              "tilt_y": 0.0, "vignette": 0.0}
                ov["mode"] = NM.MODE_MEAN_ONLY
            rec = []
            for k in range(n_mc):
                fr, truth, valid, sc = P.render_scene_frame(scene_id, seed=seed0 + 100 * k + 1,
                                                            overrides=ov)
                det = P.det_of(sc)
                kern, _ = R.psf_kernel(sc["psf"])
                sb = NM.clipped_std_adu(fr.adu, mask=valid)
                for s in truth["stars_in_frame"]:
                    e = P.psf_optimal_flux(fr.adu, s["y"], s["x"], kern,
                                           gain=det.gain_e_per_adu, bias=det.bias_adu,
                                           sigma_pix_adu=sb)
                    rec.append(float(e["flux_e"]))
            rec = np.asarray(rec, float)
            rec = rec[np.isfinite(rec)]
            std = float(np.std(rec, ddof=1)) if rec.size > 1 else 0.0
            mean = float(np.mean(rec)) if rec.size else float("nan")
            # --- 解析预测 ---
            det = P.det_of(sc)
            g = det.gain_e_per_adu
            sky_e_pp = sky * t * float(np.mean(fr.flat))         # 每像素天光电子
            dark_e_pp = det.dark_current_at(float(sc["temp_c"])) * t
            kern2, _ = R.psf_kernel(sc["psf"])
            sp2 = P.sum_p2_of_kernel(kern2)
            sig_pix = P.sigma_pix_e(src_e_per_pix=0.0, sky_e_per_pix=sky_e_pp,
                                    dark_e_per_pix=dark_e_pp, read_noise_e=det.read_noise_e)
            n_ap = float((kern2 > 1e-3).sum())
            snr_a = P.snr_ccd_equation(src_e=F, n_pix=n_ap, sky_e_per_pix=sky_e_pp,
                                       dark_e_per_pix=dark_e_pp,
                                       read_noise_e=det.read_noise_e)
            snr_b = P.snr_psf_weighted(flux_e=F, sum_p2=sp2, sigma_pix_e=sig_pix)
            snr_c = P.snr_smtn002(counts_e=F, sky_e_per_pix=sky_e_pp,
                                  read_noise_e=det.read_noise_e, gain_e_per_adu=g,
                                  fwhm_px=float(fwhm), pixel_scale_arcsec=PIXSCALE)
            out.append({
                "ab_mag": float(mag), "flux_true_e": F, "sky_mult": float(mult),
                "sky_level_e_per_s": float(sky), "sky_e_per_pix": sky_e_pp,
                "n_mc": int(rec.size), "flux_hat_mean_e": mean, "flux_hat_std_e": std,
                "bias_rel": (mean / F - 1.0) if np.isfinite(mean) else None,
                "snr_meas": (F / std) if std > 0 else None,
                "snr_ccd_eq": snr_a, "snr_psf_opt": snr_b, "snr_smtn002": snr_c,
                "n_eff_smtn002": P.n_eff_smtn002(fwhm_px=float(fwhm),
                                                 pixel_scale_arcsec=PIXSCALE),
                "sigma_pix_e": sig_pix, "sum_p2": sp2, "n_pix_psf": n_ap,
            })
    return out, {"scene": scene_id, "zp_ab": zp, "exposure_s": t, "sky_ref_e_per_s": sky_ref,
                 "n_sites": len(sites)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--out", default=str(P.P7_DATA / "exp2_snr_calibration.json"))
    a = ap.parse_args()
    n_mc = 8 if a.quick else 24
    mags = [18.0, 19.0, 20.0, 21.0, 22.0] if a.quick else [17.0, 18.0, 19.0, 20.0, 21.0, 22.0, 23.0]
    sky_mults = [0.2, 1.0, 5.0] if a.quick else [0.2, 1.0, 5.0, 20.0]
    res = {"experiment": "P7-EXP2 SNR calibration", "n_mc": n_mc, "scenes": {}}
    for sc in SCENES:
        t0 = time.time()
        rows, meta = run_grid(sc, mags, sky_mults, n_mc)
        # 负例
        neg_rows, _ = run_grid(sc, mags[:2], sky_mults[:1], max(n_mc // 4, 3), zero_noise=True)
        # 判据
        fin = [r for r in rows if r["snr_meas"] and np.isfinite(r["snr_meas"])]
        rat_b = [r["snr_meas"] / r["snr_psf_opt"] for r in fin]
        rat_a = [r["snr_meas"] / r["snr_ccd_eq"] for r in fin]
        rat_c = [r["snr_meas"] / r["snr_smtn002"] for r in fin]
        hi = [r for r in fin if r["snr_meas"] > 10]
        bias_hi = [r["bias_rel"] for r in hi if r["bias_rel"] is not None]
        # log-log 斜率 SNR vs F（固定天光）
        slopes = {}
        for m in sky_mults:
            sub = [r for r in fin if r["sky_mult"] == m and r["snr_meas"] > 0]
            if len(sub) >= 3:
                x = np.log10([r["flux_true_e"] for r in sub])
                y = np.log10([r["snr_meas"] for r in sub])
                slopes["sky_x%.1f" % m] = float(np.polyfit(x, y, 1)[0])
        # SNR vs sky 斜率（固定星等）
        sk_slopes = {}
        for mg in mags:
            sub = [r for r in fin if r["ab_mag"] == mg and r["snr_meas"] > 0]
            if len(sub) >= 3:
                x = np.log10([r["sky_e_per_pix"] for r in sub])
                y = np.log10([r["snr_meas"] for r in sub])
                sk_slopes["mag%.1f" % mg] = float(np.polyfit(x, y, 1)[0])
        neg_std = max([r["flux_hat_std_e"] for r in neg_rows] or [float("nan")])
        res["scenes"][sc] = {
            "meta": meta, "grid": rows, "negative_control": neg_rows,
            "criteria": {
                "C1_bias_hi_snr_median": float(np.median(bias_hi)) if bias_hi else None,
                "C1_pass": bool(bias_hi and abs(float(np.median(bias_hi))) < 0.02),
                "C2_ratio_psf_opt": [float(np.median(rat_b)), float(np.min(rat_b)), float(np.max(rat_b))] if rat_b else None,
                "C2_ratio_ccd_eq": [float(np.median(rat_a)), float(np.min(rat_a)), float(np.max(rat_a))] if rat_a else None,
                "C2_ratio_smtn002": [float(np.median(rat_c)), float(np.min(rat_c)), float(np.max(rat_c))] if rat_c else None,
                "C2_pass": bool(rat_b and 0.8 < float(np.median(rat_b)) < 1.25),
                "C3_loglog_slope_snr_vs_flux": slopes,
                "C3_pass": bool(slopes and all(abs(v - 1.0) < 0.1 for v in slopes.values())),
                "C4_loglog_slope_snr_vs_sky": sk_slopes,
                "C4_pass": bool(sk_slopes and all(-0.6 < v < -0.4 for v in sk_slopes.values())),
                "C5_zero_noise_max_std_e": float(neg_std),
                "C5_pass": bool(np.isfinite(neg_std) and neg_std < 1e-9),
            },
            "wall_s": time.time() - t0,
        }
        c = res["scenes"][sc]["criteria"]
        print("[exp2] %-18s C1=%s(%.4f) C2=%s(med %.3f) C3=%s C4=%s C5=%s(%.2e)  (%.0fs)"
              % (sc, c["C1_pass"], c["C1_bias_hi_snr_median"] or float("nan"),
                 c["C2_pass"], c["C2_ratio_psf_opt"][0] if c["C2_ratio_psf_opt"] else float("nan"),
                 c["C3_pass"], c["C4_pass"], c["C5_pass"], c["C5_zero_noise_max_std_e"],
                 res["scenes"][sc]["wall_s"]), flush=True)
    P.dump_json(a.out, res)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
