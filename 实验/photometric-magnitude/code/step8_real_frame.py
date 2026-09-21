# -*- coding: utf-8 -*-
"""SCI-A 步骤 8 · testdata 真实帧（第三类数据 · 底参照）

数据：testdata 内 FLI 相机 M42 / 银河中心真实帧（4096²/4500×3600，uint16 + BZERO）。
本步骤不注入任何真值，只用真实帧回答：
  ① 真实帧上的仪器参数（增益/读出噪声/天光/见明度/平场逐像素散度）能否复现 RELEASE-02 的实测值；
  ② Gaia XP 引导检测在真实帧上的匹配率 / 盲检对照 / 算力；
  ③ 单帧自算误差预算能否给出可判定结论（无真值项如实标 "判不了"）。

产出：results/step8_real_frame.json
"""
from __future__ import annotations

import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_gaia as sg
from scia_calib import aperture_flux, calibrate, guided_photometry, psf_vs_aperture_systematics

REPO = sc.REPO
TD = os.path.join(REPO, "testdata")


def find_frames():
    out = []
    for root, _dirs, files in os.walk(TD):
        for f in sorted(files):
            if f.lower().endswith((".fit", ".fits", ".fts")):
                out.append(os.path.join(root, f))
    return out


def load_real(path):
    from astropy.io import fits
    h = fits.open(path, memmap=False)
    d = np.asarray(h[0].data, dtype=np.float64)
    hdr = h[0].header
    h.close()
    return d, hdr


def main():
    res = {"step": "8_real_frame", "seed": sc.SCIA_SEED, "frames": []}
    paths = find_frames()
    res["n_testdata_frames"] = len(paths)
    res["testdata_files"] = [os.path.relpath(p, REPO) for p in paths]
    if not paths:
        res["verdict"] = "NO_DATA"
        sc.jdump(res, os.path.join(sc.RESULTS, "step8_real_frame.json"))
        return
    # 选 M42 M1 T2 Red 300s 作为主真实帧
    main_p = None
    for p in paths:
        b = os.path.basename(p)
        if "M42" in b and "M1" in b and "T2" in b and "Red" in b:
            main_p = p; break
    main_p = main_p or paths[0]
    res["primary_frame"] = os.path.relpath(main_p, REPO)
    d, hdr = load_real(main_p)
    bzero = float(hdr.get("BZERO", 0.0))
    # astropy 对 uint16 + BZERO 已给出**物理值**（本例中位 1202 ADU），不得再减一次；
    # 只有读回带符号负值时才说明未施加，此时补回 BZERO。
    img = d + bzero if float(np.median(d)) < 0 else d
    H, W = img.shape
    res["shape"] = [int(H), int(W)]
    res["header_subset"] = {k: (hdr[k] if not isinstance(hdr[k], (bytes,)) else str(hdr[k]))
                            for k in ("EXPTIME", "GAIN", "RDNOISE", "CCD-TEMP", "FILTER",
                                      "XBINNING", "BZERO", "INSTRUME", "NAXIS1", "NAXIS2")
                            if k in hdr}
    # ---- 仪器参数（帧内自算） ----
    dd = np.diff(img, axis=1)
    bg_rms_adu = float(np.median(np.abs(dd - np.median(dd))) * sc.MAD_TO_SIGMA / np.sqrt(2.0))
    sky_med = float(np.median(img))
    res["in_frame_instrument"] = dict(
        sky_median_adu=sky_med, bg_rms_adu_adjacent_diff=bg_rms_adu,
        img_p01=float(np.percentile(img, 1)), img_p99=float(np.percentile(img, 99)),
        img_max=float(img.max()), n_sat_like=int((img >= 65535).sum()),
        note="与 run/RELEASE-02/parallel/out/ptc2.json 的实测值（增益 1.333 e-/ADU、"
             "RN 8.59 e-、天光 199.6 ADU/px、FWHM 1.98 px、平场逐像素 0.32%）对照")
    # ---- WCS 精化 + Gaia 引导检测 ----
    from astropy.wcs import WCS
    w = WCS(hdr)
    crval = [float(w.wcs.crval[0]), float(w.wcs.crval[1])]
    t0 = time.perf_counter()
    cat = sg.XpCatalog(sg.dump_cone(crval[0], crval[1], 0.30, 18.0,
                                    f"real_{int(crval[0]*100)}_{int(crval[1]*100)}"))
    print(f"[step8] cone+catalog {time.perf_counter()-t0:.1f}s n={cat.n_src}", flush=True)
    res["gaia_cone"] = dict(center=crval, radius_deg=0.30, mag_limit=18.0,
                            n_sources=int(cat.n_src))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from step3_forward_vs_photflam import refine_shift
    t0 = time.perf_counter()
    shift, n_match, n_in, tol = refine_shift(w, img, cat.ra, cat.dec)
    print(f"[step8] refine_shift {time.perf_counter()-t0:.1f}s shift={shift} "
          f"n_match={n_match}/{n_in}", flush=True)
    res["wcs_refinement"] = dict(shift_px=[float(shift[0]), float(shift[1])],
                                 shift_arcsec=float(np.hypot(*shift) * abs(w.wcs.cd[0][0]) * 3600),
                                 n_matched_within_tol=int(n_match), n_in_frame=int(n_in),
                                 tol_px=float(tol))
    # ---- 引导测光 ----
    import scia_sim as ss
    inst = ss.Instrument(gain=1.333, read_noise=8.59, dark_rate=0.002, fwhm_px=1.98,
                         beta_fit=4.0, beta_inject=3.5, ellipticity=0.0,
                         saturation_adu=65535.0)
    var = (np.clip(img, 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    t0 = time.perf_counter()
    g = guided_photometry(img, w, cat.ra, cat.dec, inst, var_map=var,
                          wcs_shift=shift, mag_limit=16.5, mag_g=cat.magG)
    t_g = time.perf_counter() - t0
    print(f"[step8] guided {t_g:.1f}s candidates={g['idx'].size} "
          f"fit_ok={int(g['fit_ok'].sum())}", flush=True)
    # ---- 盲检测对照 ----
    t0 = time.perf_counter()
    bx, by, _ = sc.blind_detect(img, k_sigma=5.0, smooth_sigma=2.0, min_area=3)
    t_b = time.perf_counter() - t0
    print(f"[step8] blind {t_b:.1f}s n_det={bx.size}", flush=True)
    ka, kb, sep = sc.match_catalogs(g["x"][g["fit_ok"]], g["y"][g["fit_ok"]], bx, by, 1.5)
    n_cat_used = int(g["idx"].size)
    res["guided_vs_blind_real"] = dict(
        guided_candidates=n_cat_used, guided_fit_ok=int(g["fit_ok"].sum()),
        guided_wall_time_s=float(t_g), guided_sec_per_fit=float(t_g / max(n_cat_used, 1)),
        blind_detections=int(bx.size), blind_wall_time_s=float(t_b),
        matched_guided_to_blind=int(ka.size),
        guided_completeness_vs_blind=float(ka.size / max(n_cat_used, 1)),
        blind_precision_vs_guided=float(ka.size / max(bx.size, 1)),
        note="盲检测在真实帧上找不到的星，引导路径直接给出；两路交集用于交叉验证")
    # ---- 单帧自算误差预算（无真值项如实标 null） ----
    sel = g["fit_ok"] & np.isfinite(g["flux"]) & (g["flux"] > 0) \
        & np.isfinite(g["chi2"]) & (g["chi2"] < 4.0) & (cat.magG[g["idx"]] < 16.0)
    F = g["flux"][sel]
    if F.size >= 8:
        # 参考侧通量：用 Gaia XP 与 Baader R 通带合成（真实数据，无真值星等）
        Tw, Tv = sc.load_filter("Baader R")
        Qw, Qv = sc.load_qe("KAF-16803")
        f_syn = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qv, Qw)
                          for i in g["idx"][sel]])
        cal = calibrate(F, f_syn, None, g["x"][sel], g["y"][sel], m_degree=2)
        f_ap4 = aperture_flux(img, g["x"][sel], g["y"][sel], r_ap=4.0, r_in=6.0, r_out=10.0)
        sysres = psf_vs_aperture_systematics(F, f_ap4, g["flux_err"][sel])
        sigma_pix_e = float(np.sqrt(inst.read_noise ** 2 + sky_med * inst.gain))
        struct = float(max(1.0, bg_rms_adu / (sigma_pix_e / inst.gain)))
        from scia_common import mc_sigma_obs, noise_sigma_mag
        import scia_pipeline as pl
        # σ_flat 是**可自算**的：低阶 m(x,y) 拟合后的残余星等散度（calibrate 内已算）
        sig_flat_self = float(cal["delta_after_m"]) if np.isfinite(cal["delta_after_m"]) else 0.0
        b = pl.build_budget(F, inst, sigma_pix_e, sysres["sigma_psfsys"], 0.0, 0.0,
                            sig_flat_self, struct, cal["n_inliers"], tag="real")
        res["single_frame_gate"] = dict(
            n=int(F.size), k_photo=float(cal["k_photo"]),
            sigma_obs_mag=cal["sigma_obs_mag"], n_inliers=int(cal["n_inliers"]),
            sigma_floor=b.sigma_floor, sigma_ceiling=b.sigma_ceiling,
            verdict=sc.gate_verdict(cal["sigma_obs_mag"], b),
            items=dict(sigma_pix_e=sigma_pix_e, structure_factor=struct,
                       sigma_psfsys_inframe=sysres["sigma_psfsys"],
                       sigma_flat=sig_flat_self, sigma_color=None, sigma_gaia=None),
            unavailable_items=["sigma_color（真实帧无注入通带真值，不可自算）",
                               "sigma_gaia（真实帧无参考侧真值，不可自算）"],
            note="σ_flat 由帧内 m(x,y) 拟合残差自算；σ_color/σ_gaia 不可自算 ⇒ 上界不完整。"
                 "若判定为 ABOVE_CEILING，必须先归因到「上界缺项」而不是直接判失败。")
    else:
        res["single_frame_gate"] = dict(verdict="INSUFFICIENT_SAMPLE", n=int(F.size))
    sc.jdump(res, os.path.join(sc.RESULTS, "step8_real_frame.json"))


if __name__ == "__main__":
    main()
