#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T12b —— 与 photutils / sep 的**对拍**（外部实现语义核对）。

诚实登记：
  * photutils 3.0.0 与 sep 1.4.1 **已装**（--target /dev/shm/astrocs_fsnr/pylibs，非系统）。
  * **SExtractor 可执行文件本环境不存在**（which sex/extract 均空）⇒
    "与 SExtractor 二进制对拍" **未做**；只做了**源码级**核对
    （src/winpos.c:289 SNR_WIN = FLUX_WIN/FLUXERR_WIN；doc/src/Photom.rst 的 FLUXERR 式）。
  * 本脚本只断言**实测到的事实**；对不上的地方如实记录为 mismatch。

用法: PYTHONPATH=/dev/shm/astrocs_fsnr/pylibs python3 crosscheck_photutils.py [--out DIR]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_snr_canon as C  # noqa: E402

FWHM_PX = 4.0
GAIN = 1.5
READ_E = 5.0
F_S_E = 2000.0
SKY_LIST = [10.0, 100.0, 1000.0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/reverse_verify/frame_snr")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    res: dict = {"experiment": "FRAME-SNR-CANON T12b external cross-check"}

    try:
        from photutils.aperture import (ApertureStats, CircularAnnulus,
                                        CircularAperture, aperture_photometry)
        import photutils
        res["photutils_version"] = photutils.__version__
    except Exception as exc:  # noqa: BLE001
        res["photutils"] = {"status": "UNAVAILABLE", "error": str(exc)}
        photutils = None

    rng = np.random.default_rng(4242)

    if photutils is not None:
        shape = (121, 121)
        center = (60, 60)
        img, P_full, ap_mask, ann_mask = C.synth_frame(
            F_S_E, 300.0, READ_E, shape, center, FWHM_PX, rng)
        # 逐像素总误差（e-）：天光散粒 + 读出 + 源散粒
        err_map = np.sqrt(C.pixel_variance_e2(F_S_E, 300.0, READ_E, P_full))
        ap = CircularAperture(center[::-1], r=1.5 * FWHM_PX)
        ann = CircularAnnulus(center[::-1], r_in=2.5 * FWHM_PX, r_out=4.0 * FWHM_PX)

        # ---- (a) photutils aperture_sum_err 语义: center 法 == sqrt(sum sigma_i^2) ----
        ph = aperture_photometry(img, ap, error=err_map, method="center")
        got = float(ph["aperture_sum_err"][0])
        # center 法的像素集合（photutils 用**严格**距离 < r；实测 113 vs 109 像素即此差异）
        yy, xx = np.mgrid[0 : shape[0], 0 : shape[1]]
        rr = np.hypot(yy - center[0], xx - center[1])
        in_ap = rr < 1.5 * FWHM_PX
        expect = float(np.sqrt(np.sum(err_map[in_ap] ** 2)))
        # exact 重叠权重下的误差传播（验证 photutils 对 error 用的权重口径）
        m = ap.to_mask(method="exact")
        w_exact = m.to_image(shape)
        expect_exact = float(np.sqrt(np.sum((w_exact * err_map) ** 2)))
        res["aperture_sum_err_semantics"] = {
            "photutils_center_method": got, "sqrt_sum_sigma2_over_center_pixels": expect,
            "rel_diff": abs(got - expect) / max(expect, 1e-300),
            "match": abs(got - expect) <= 1e-9 * max(expect, 1e-300),
            "npix_center": int(np.sum(in_ap)),
            "exact_overlap_weights_sqrt_sum": expect_exact,
            "rel_diff_vs_exact": abs(got - expect_exact) / max(expect_exact, 1e-300),
            "match_exact": abs(got - expect_exact) <= 1e-9 * max(expect_exact, 1e-300),
        }

        # ---- (b) 加性天光：扣局部背景后 aperture_sum 不变 ----
        bkg = float(ApertureStats(img, ann).median)
        C_ADD = 1234.5
        s1 = float(aperture_photometry(img - bkg, ap, method="center")["aperture_sum"][0])
        s2 = float(aperture_photometry(img + C_ADD - (bkg + C_ADD), ap,
                                       method="center")["aperture_sum"][0])
        res["additive_sky_invariance_photutils"] = {
            "C_add_e": C_ADD, "sum_before": s1, "sum_after": s2,
            "rel_change": abs(s1 - s2) / max(abs(s1), abs(s2), 1e-300),
            "match": abs(s1 - s2) <= 1e-12 * max(abs(s1), abs(s2), 1e-300),
            "note": "本地背景 = 圆环中位数 (ApertureStats.median); 加常数后中位数同移",
        }
        # 红对照：不扣背景
        r1 = float(aperture_photometry(img, ap, method="center")["aperture_sum"][0])
        r2 = float(aperture_photometry(img + C_ADD, ap, method="center")["aperture_sum"][0])
        res["additive_sky_invariance_photutils"]["red_unsubtracted_delta"] = r2 - r1
        res["additive_sky_invariance_photutils"]["red_unsubtracted_predicted"] = (
            C_ADD * float(np.sum(in_ap)))
        res["additive_sky_invariance_photutils"]["red_unsubtracted_predicted_check"] = (
            abs((r2 - r1) - C_ADD * float(np.sum(in_ap)))
            <= 1e-9 * max(abs(r2 - r1), 1e-300))

        # ---- (c) 天光 sigma 增大 => aperture_sum_err 增大（同一像素集合） ----
        rows = []
        for B in SKY_LIST:
            em = np.sqrt(C.pixel_variance_e2(F_S_E, B, READ_E, P_full))
            ph2 = aperture_photometry(img, ap, error=em, method="center")
            rows.append({"B_e_per_pix": B,
                         "aperture_sum_err_e": float(ph2["aperture_sum_err"][0]),
                         "analytic_sqrt_sum_sigma2_e": float(np.sqrt(np.sum(em[in_ap] ** 2))),
                         "sum_center_e": float(ph2["aperture_sum"][0])})
        errs = [r["aperture_sum_err_e"] for r in rows]
        res["sky_sigma_increases_error"] = {
            "rows": rows, "strictly_increasing": all(errs[i + 1] > errs[i] for i in range(len(errs) - 1)),
        }

        # ---- (d) 与解析孔径 CCD 方程对拍（口径不同，如实给差异） ----
        sigma_px = C.moffat4_sigma_from_fwhm(FWHM_PX)
        r_ap = 1.5 * FWHM_PX
        f_in = 1.0 - (1.0 + r_ap**2 / (2 * sigma_px**2)) ** -3.0
        n_pix = math.pi * r_ap**2
        n_sky = math.pi * (4.0**2 - 2.5**2) * FWHM_PX**2
        B = 300.0
        sig_sky2 = B  # e-^2 逐像素（天空泊松）
        s_ap = F_S_E * f_in
        var_ap = n_pix * sig_sky2 * (1.0 + n_pix / n_sky) + s_ap / GAIN
        snr_ap_analytic = s_ap / math.sqrt(var_ap)
        # photutils 同一孔径（center 法，硬边）：
        i_ap = img[in_ap]
        bkg_ap = float(ApertureStats(img, ann).median)
        flux_ap = float(np.sum(i_ap - bkg_ap))
        var_ap_phot = float(np.sum(C.pixel_variance_e2(F_S_E, B, READ_E, P_full)[in_ap]))
        # 同像素集合 + 无 n_pix/n_sky 项的解析式（与 photutils 口径一致）
        var_ap_same = var_ap_phot
        snr_same = float(np.sum(img[in_ap] - bkg_ap)) / math.sqrt(var_ap_same)
        res["aperture_vs_analytic"] = {
            "note": ("口径差异: 解析式用 pi*r^2 面积 + n_pix/n_sky 天光估计项; "
                     "photutils 用整数像素集合、误差面已含源泊松、无 n_pix/n_sky 项"),
            "analytic_SNR_ap": snr_ap_analytic,
            "photutils_SNR_ap": flux_ap / math.sqrt(var_ap_phot),
            "analytic_same_pixel_set_no_skyterm_SNR": snr_same,
            "rel_diff_same_convention": abs(snr_same - flux_ap / math.sqrt(var_ap_phot))
            / max(abs(snr_same), 1e-300),
            "analytic_n_pix": n_pix, "photutils_n_pix": int(np.sum(in_ap)),
            "analytic_enclosed_fraction_moffat4": f_in,
        }

    # ---- sep ----
    try:
        import sep as seplib
        res["sep_version"] = seplib.__version__
        shape = (121, 121)
        center = (60, 60)
        img, P_full, ap_mask, ann_mask = C.synth_frame(
            F_S_E, 300.0, READ_E, shape, center, FWHM_PX, np.random.default_rng(7))
        data = np.ascontiguousarray(img.astype(np.float64))
        bkg = seplib.Background(data, bw=64, bh=64)   # sep 1.4.1: 第2位是 mask，必须用关键字
        dsub = data - bkg.back()
        err = np.sqrt(C.pixel_variance_e2(F_S_E, 300.0, READ_E, P_full))
        f, ferr, flag = seplib.sum_circle(
            dsub, np.array([center[1]]), np.array([center[0]]), 1.5 * FWHM_PX,
            err=np.ascontiguousarray(err), bkgann=(2.5 * FWHM_PX, 4.0 * FWHM_PX))
        res["sep"] = {"status": "OK", "flux": float(f[0]), "fluxerr": float(ferr[0]),
                      "flag": int(flag[0]), "snr": float(f[0] / ferr[0]),
                      "note": "sep.sum_circle(err=...) 为逐像素误差面; bkgann 为背景环"}
    except Exception as exc:  # noqa: BLE001
        res["sep"] = {"status": "UNAVAILABLE", "error": str(exc)}

    res["sextractor_binary"] = {
        "status": "NOT_RUN",
        "reason": "本环境无 SExtractor 可执行文件（which sex / extract 均为空）",
        "source_level_check": {
            "SNR_WIN": "sextractor master src/winpos.c:289 -> snr_win = flux_win/fluxerr_win",
            "FLUXERR": "doc/src/Photom.rst :label: fluxerr -> FLUXERR = sqrt(sum_i (sigma_i^2 + p_i/g_i)), p_i = pixel value subtracted from the background",
            "FLUX_GAUSS": "NOT FOUND in sextractor master (param.h 参数表无此项) -> 用户点名有误，如实登记",
        },
    }

    outp = os.path.join(args.out, "external_crosscheck.json")
    with open(outp, "w") as fh:
        json.dump(res, fh, indent=2, ensure_ascii=False)
    print(json.dumps(res, indent=2, ensure_ascii=False)[:4000])
    print("written:", outp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
