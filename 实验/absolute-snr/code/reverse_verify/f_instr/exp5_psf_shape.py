#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp5: PSF 形状自由度对"总通量"回收的影响 (PSF 测光的主要风险面).

四种拟合配置 (注入恒为 Moffat FWHM=2.5, beta=3.5, 真值通量 F):
  C1 固定正确形状 (FWHM=2.5, beta=3.5)     <- 上界
  C2 自由 FWHM, 正确族 (beta=3.5)           <- 生产可实现的形态
  C3 自由 FWHM, 错误族 (beta=4, 库内 dpsf 的族)
  C4 高斯 (自由 FWHM)                       <- 最坏
判据: |bias| <= 0.02 mag 视为可接受 (尺度无关的星等比).
"""
import os, sys, time
import numpy as np
from scipy.optimize import least_squares

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, render_psf, robust_sky_stats, local_cutout,
                         robust_loc_scale, save_json, moffat_profile)


def fit_free(frame, x0, y0, kind, beta, fwhm0, free_fwhm=True):
    """同时拟合 (A, b, dx, dy[, fwhm]) -> 总通量 A."""
    rad = max(8.0, 3.5 * fwhm0)
    R = int(np.ceil(rad)) + 1
    cut, x0b, y0b = local_cutout(frame, x0, y0, R)
    yy, xx = np.mgrid[0:cut.shape[0], 0:cut.shape[1]]
    lx, ly = x0 - x0b, y0 - y0b
    m = (xx - lx) ** 2 + (yy - ly) ** 2 <= rad * rad
    b0, _ = robust_sky_stats(cut)
    A0 = max(np.sum(np.where(cut - b0 > 0, cut - b0, 0.0)), 1.0)

    def resid(p):
        if free_fwhm:
            A, b, dx, dy, fw = p
        else:
            A, b, dx, dy = p
            fw = fwhm0
        fw = max(fw, 0.5)
        pr = render_psf(cut.shape, lx + dx, ly + dy, fw, total=1.0, kind=kind, beta=beta)
        return (b + A * pr)[m] - cut[m]

    p0 = [A0, b0, 0.0, 0.0, fwhm0] if free_fwhm else [A0, b0, 0.0, 0.0]
    try:
        sol = least_squares(resid, x0=np.array(p0, dtype=float), method="lm", max_nfev=400)
        return float(sol.x[0]), (float(sol.x[4]) if free_fwhm else fwhm0)
    except Exception:
        return float("nan"), float("nan")


def main():
    t0 = time.time()
    OUT = "/workspace/Astro CS Database/run/reverse_verify/f_instr"
    d = np.load(os.path.join(OUT, "scene.npz"))
    sky = d["sky_good"]
    nm = NoiseModel()
    f0, _ = nm.render(sky, None, None, np.random.default_rng(4242))
    sig = float(1.4826 * np.median(np.abs(f0 - np.median(f0))))
    FWHM_T, BETA_T = 2.5, 3.5
    rng = np.random.default_rng(11)
    pos = []
    while len(pos) < 30:
        x, y = rng.uniform(40, 472), rng.uniform(40, 472)
        if all((x - a) ** 2 + (y - b) ** 2 > 32 ** 2 for a, b in pos):
            pos.append((x, y))
    F = 100.0 * sig / float(moffat_profile(0.0, 0.0, FWHM_T, BETA_T))
    cfgs = [("C1_fixed_correct", "moffat", BETA_T, FWHM_T, False),
            ("C2_free_fwhm_correct_family", "moffat", BETA_T, FWHM_T, True),
            ("C3_free_fwhm_beta4", "moffat", 4.0, FWHM_T, True),
            ("C4_gaussian_free", "gaussian", BETA_T, FWHM_T, True)]
    acc = {c[0]: {"dmag": [], "fwhm_fit": []} for c in cfgs}
    for k in range(4):
        rng2 = np.random.default_rng(8800 + 13 * k)
        src = np.zeros(sky.shape)
        for (x, y) in pos:
            src += render_psf(sky.shape, x, y, FWHM_T, total=F, beta=BETA_T)
        frame, _ = nm.render(sky, src, None, rng2)
        for (x, y) in pos:
            for (name, kind, beta, fw0, free) in cfgs:
                A, fwf = fit_free(frame, x, y, kind, beta, fw0, free_fwhm=free)
                if np.isfinite(A) and A > 0:
                    acc[name]["dmag"].append(-2.5 * np.log10(A / F))
                    acc[name]["fwhm_fit"].append(fwf)
    rep = {"experiment": "exp5_psf_shape_freedom",
           "truth": {"fwhm_px": FWHM_T, "beta": BETA_T, "F_true_adu": F},
           "sigma_adu": sig, "n_stars": len(pos), "n_real": 4,
           "accept_mag": 0.02, "by_config": {}}
    for name, v in acc.items():
        loc, sc = robust_loc_scale(v["dmag"])
        fm, _ = robust_loc_scale(v["fwhm_fit"])
        rep["by_config"][name] = {"bias_mag": loc, "scatter_mag": sc,
                                  "fitted_fwhm_median": fm,
                                  "pass": bool(abs(loc) <= 0.02)}
    save_json(os.path.join(OUT, "exp5_psf_shape.json"), rep)
    print("=== exp5 PSF 形状自由度 -> 总通量偏差 (注入 Moffat FWHM=2.5 beta=3.5) ===")
    print("%-30s%12s%12s%14s%8s" % ("config", "bias_mag", "scatter", "fwhm_fit", "pass"))
    for name, v in rep["by_config"].items():
        print("%-30s%12.4f%12.4f%14.3f%8s" % (name, v["bias_mag"], v["scatter_mag"],
                                              v["fitted_fwhm_median"], v["pass"]))
    print("elapsed %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
