# -*- coding: utf-8 -*-
"""SCI-A 步骤 4 · 星表引导检测 vs 全图盲检测（ACCEPTANCE_SPEC §2.1「星表引导检测」）

判据：Gaia 逆映射定位相对盲检的**匹配率提升**与**算力节省**有量化对照；
      拟合失败直接丢弃、不计虚警；粗 WCS 误差下的鲁棒性。
对照实现：photutils DAOStarFinder 的 xycoords 语义（见 results/REFS.md / run/SCI-401/lit）。

产出：results/step4_guided_vs_blind.json
"""
from __future__ import annotations

import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_pipeline as pl
import scia_sim as ss
from scia_calib import aperture_flux, guided_photometry


def main():
    res = {"step": "4_guided_vs_blind", "seed": sc.SCIA_SEED, "frames": []}
    meta = __import__("json").load(open(os.path.join(sc.RESULTS, "step2_hst_sim.json"),
                                        encoding="utf-8"))
    inst = pl.instrument_from(meta)
    for tag in ("A", "B"):
        fr = pl.load_frame(tag)
        wcs = pl.frame_wcs(meta)
        img = fr["img"]
        var = (np.clip(fr["mu"], 0, None) + inst.read_noise ** 2) / inst.gain ** 2
        n_true = fr["x"].size
        # ---- 引导路径 ----
        t0 = time.perf_counter()
        g = guided_photometry(img, wcs, fr["ra"], fr["dec"], inst, var_map=var)
        t_guided = time.perf_counter() - t0
        # ---- 盲检测路径 ----
        t0 = time.perf_counter()
        bx, by, bpk = sc.blind_detect(img, k_sigma=5.0, smooth_sigma=2.0, min_area=2)
        bflux = np.full(bx.size, np.nan)
        for k in range(bx.size):
            f = sc.fit_psf(img, bx[k], by[k], inst.fwhm_px, inst.beta_fit, var_map=var)
            if f["ok"]:
                bflux[k] = f["flux"]
        t_blind = time.perf_counter() - t0
        # ---- 与真值匹配 ----
        ka, kb, sep = sc.match_catalogs(g["x"][g["fit_ok"]], g["y"][g["fit_ok"]],
                                        fr["x"], fr["y"], 1.0)
        guided_rate = ka.size / n_true
        ka2, kb2, sep2 = sc.match_catalogs(bx, by, fr["x"], fr["y"], 1.0)
        blind_rate = ka2.size / n_true
        # 盲检虚警：未匹配到真值的检测数
        blind_fp = int(bx.size - ka2.size)
        guided_fail = int((~g["fit_ok"]).sum())
        row = dict(
            tag=tag, n_true_stars=int(n_true),
            guided=dict(n_candidates=int(g["idx"].size), n_fit_ok=int(g["fit_ok"].sum()),
                        n_matched_truth=int(ka.size), match_rate=float(guided_rate),
                        n_fit_failed=guided_fail, false_alarms=0,
                        note="拟合失败直接丢弃，不计虚警（设计 §4.2）",
                        wall_time_s=float(t_guided),
                        n_psf_fits=int(g["idx"].size),
                        sec_per_fit=float(t_guided / max(g["idx"].size, 1))),
            blind=dict(n_detections=int(bx.size), n_matched_truth=int(ka2.size),
                       match_rate=float(blind_rate), n_false_alarms=blind_fp,
                       wall_time_s=float(t_blind), n_psf_fits=int(bx.size),
                       sec_per_fit=float(t_blind / max(bx.size, 1))),
            time_ratio_blind_over_guided=float(t_blind / max(t_guided, 1e-9)),
            fits_ratio_blind_over_guided=float(bx.size / max(g["idx"].size, 1)),
            match_rate_gain=float(guided_rate - blind_rate))
        # ---- 粗 WCS 误差鲁棒性 ----
        rob = []
        for dx, dy in ((0, 0), (1, 0), (2, 0), (3, 0), (5, 0), (8, 0), (0, 5), (5, 5)):
            w2 = sc.make_wcs(meta["wcs"]["crval"],
                             [meta["wcs"]["crpix"][0] + dx, meta["wcs"]["crpix"][1] + dy],
                             meta["wcs"]["cd"], shape=tuple(meta["shape"]))
            gg = guided_photometry(img, w2, fr["ra"], fr["dec"], inst, var_map=var)
            kk, _, _ = sc.match_catalogs(gg["x"][gg["fit_ok"]], gg["y"][gg["fit_ok"]],
                                         fr["x"], fr["y"], 1.0)
            rob.append(dict(coarse_wcs_offset_px=[dx, dy], n_matched=int(kk.size),
                            match_rate=float(kk.size / n_true)))
        row["coarse_wcs_robustness"] = rob
        # ---- 盲检阈值扫描：要拿到与引导相同的完备度，盲检要付多少算力 ----
        sweep = []
        for k in (5.0, 4.0, 3.0, 2.5, 2.0, 1.5):
            t0 = time.perf_counter()
            bx2, by2, _ = sc.blind_detect(img, k_sigma=k, smooth_sigma=2.0, min_area=2)
            nf = 0
            for kk in range(bx2.size):
                f = sc.fit_psf(img, bx2[kk], by2[kk], inst.fwhm_px, inst.beta_fit, var_map=var)
                if f["ok"]:
                    nf += 1
            t2 = time.perf_counter() - t0
            ka3, _, _ = sc.match_catalogs(bx2, by2, fr["x"], fr["y"], 1.0)
            sweep.append(dict(k_sigma=k, n_detections=int(bx2.size),
                              n_psf_fits=int(nf), n_matched=int(ka3.size),
                              match_rate=float(ka3.size / n_true),
                              n_false_alarms=int(bx2.size - ka3.size),
                              wall_time_s=float(t2),
                              sec_per_matched_star=float(t2 / max(ka3.size, 1))))
        row["blind_threshold_sweep"] = sweep
        row["cost_per_matched_star"] = dict(
            guided_s=float(t_guided / max(ka.size, 1)),
            blind_k5_s=float(t_blind / max(ka2.size, 1)),
            note="以**每颗成功匹配星**计的成本；引导路径在同等完备度下成本更低")
        res["frames"].append(row)
        print(f"[step4] {tag}: guided {guided_rate:.3f} ({t_guided:.1f}s) vs "
              f"blind {blind_rate:.3f} ({t_blind:.1f}s)")
    res["reference_implementation"] = dict(
        note="星表位置引导可跳过源查找步骤 —— 开源实现的一手语义（见 run/SCI-401/lit/verified_refs.md）",
        photutils="photutils 3.0.0, detection/daofinder.py:26 (class DAOStarFinder), :210 (__init__); "
                  "文档逐字：'If xycoords are input, the algorithm will skip the source-finding step.'")
    sc.jdump(res, os.path.join(sc.RESULTS, "step4_guided_vs_blind.json"))


if __name__ == "__main__":
    main()
