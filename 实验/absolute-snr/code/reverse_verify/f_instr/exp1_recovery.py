#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp1: 各通量口径的回收精度 vs 峰值 S/N (真实 L4 帧作底 + 物理噪声).

真值: 注入 Moffat(FWHM=2.5, beta=3.5) 星, 总通量 F_true 由目标峰值 S/N 反解.
度量: dmag = -2.5 log10(F_rec/F_true) 的稳健位置(中值)与稳健散度(MAD*1.4826).
"""
import os, sys, json, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, render_psf, robust_sky_stats, aperture_curve,
                         encircled_energy, est_box5, est_iso, est_aperture_cog,
                         est_psf_nlsq, est_psf_optimal, est_kron, dmag,
                         robust_loc_scale, save_json, add_sky_gradient, moffat_profile)

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "run/reverse_verify/f_instr")
FWHM_TRUE, BETA = 2.5, 3.5
APERTURES = [3.0, 4.0, 6.0, 10.0]


def star_positions(n, size=512, margin=40, minsep=32, seed=0):
    rng = np.random.default_rng(seed)
    pos = []
    tries = 0
    while len(pos) < n and tries < 20000:
        tries += 1
        x = rng.uniform(margin, size - margin)
        y = rng.uniform(margin, size - margin)
        if all((x - a) ** 2 + (y - b) ** 2 > minsep ** 2 for a, b in pos):
            pos.append((x, y))
    return pos


def measure_all(frame, pos, sigma_sky, fwhm_fit=FWHM_TRUE, beta_fit=BETA):
    """返回 dict[name] -> list[F_rec] (与 pos 同序)."""
    res = {k: [] for k in (["box5", "iso5s", "kron", "psf_nlsq", "psf_opt", "aper_cog4"]
                           + ["aper_%g" % r for r in APERTURES])}
    for (x, y) in pos:
        res["box5"].append(est_box5(frame, x, y))
        res["iso5s"].append(est_iso(frame, x, y, nsigma=5.0, sigma=sigma_sky)[0])
        res["kron"].append(est_kron(frame, x, y))
        res["psf_nlsq"].append(est_psf_nlsq(frame, x, y, fwhm_fit, beta=beta_fit))
        res["psf_opt"].append(est_psf_optimal(frame, x, y, fwhm_fit, beta=beta_fit))
        cur, bkg = aperture_curve(frame, x, y, APERTURES + [4.0])
        for i, r in enumerate(APERTURES):
            res["aper_%g" % r].append(cur[i])
        res["aper_cog4"].append(cur[-1] / max(encircled_energy(fwhm_fit, 4.0, beta=beta_fit), 1e-6))
    return res


def frame_sigma(sky, nm, seed=4242, grad=None):
    """帧内真实天光噪声: 渲染一帧纯天光, 量其 sigma-clip 散度 (不用平滑图的散度!)."""
    sk = sky if grad is None else sky * grad
    f, _ = nm.render(sk, None, None, np.random.default_rng(seed))
    _, s = robust_sky_stats(f)
    return float(s)


def run_case(sky, pos, sn_list, fwhm_true=FWHM_TRUE, beta=BETA, nm=None,
             n_real=5, grad_amp=0.0, fwhm_fit=None, seed0=1000, label=""):
    nm = nm or NoiseModel()
    fwhm_fit = fwhm_true if fwhm_fit is None else fwhm_fit
    _grad = add_sky_gradient(sky.shape, grad_amp) if grad_amp > 0 else None
    sigma_sky = frame_sigma(sky, nm, grad=_grad)
    peak_frac = float(moffat_profile(0.0, 0.0, fwhm_true, beta))
    grad = _grad
    out = {"label": label, "fwhm_true": fwhm_true, "fwhm_fit": fwhm_fit,
           "beta": beta, "sigma_sky_adu": sigma_sky, "peak_frac": peak_frac,
           "n_real": n_real, "n_stars": len(pos), "grad_amp": grad_amp,
           "noise_model": nm.as_dict(), "results": {}}
    for sn in sn_list:
        F = sn * sigma_sky / peak_frac
        acc = {}
        for k in range(n_real):
            rng = np.random.default_rng(seed0 + 7919 * k + int(sn))
            src = np.zeros(sky.shape)
            for (x, y) in pos:
                src += render_psf(sky.shape, x, y, fwhm_true, total=F, beta=beta)
            sk = sky if grad is None else sky * grad
            frame, _ = nm.render(sk, src, None, rng)
            m = measure_all(frame, pos, sigma_sky, fwhm_fit=fwhm_fit, beta_fit=beta)
            for name, vals in m.items():
                acc.setdefault(name, []).extend(dmag(np.array(vals), F).tolist())
        row = {}
        for name, vals in acc.items():
            loc, sc = robust_loc_scale(vals)
            row[name] = {"bias_mag": loc, "scatter_mag": sc,
                         "flux_ratio": float(10 ** (-0.4 * loc)) if np.isfinite(loc) else None}
        out["results"]["snr_%.0f" % sn] = {"F_true_adu": F, "by_estimator": row}
    return out


def main():
    t0 = time.time()
    d = np.load(os.path.join(OUT, "scene.npz"))
    sky = d["sky_good"]
    pos = star_positions(40, size=sky.shape[0], seed=11)
    SN = [3, 5, 10, 20, 50, 100, 200, 400]
    rep = {"experiment": "exp1_recovery_accuracy",
           "base": "run/RELEASE-02/L4-rebuild/norm/ 真实标定帧 (sky_good patch)",
           "injection_psf": "Moffat FWHM=%.2f beta=%.2f" % (FWHM_TRUE, BETA),
           "estimators": ["box5(生产复刻)", "iso5s(sdet 复刻)", "kron(FLUX_AUTO 式)",
                          "aper_r(无孔径改正)", "aper_cog4(显式增长曲线改正)",
                          "psf_nlsq(PSF 拟合总通量)", "psf_opt(Naylor/Horne 最优提取)"],
           "cases": []}

    rep["cases"].append(run_case(sky, pos, SN, label="fiducial g=1 RN=4"))

    # 噪声模型敏感性 (g, RN 扫描): 结论是否随声明的噪声参数改变
    for (g, rn) in [(0.5, 4.0), (1.0, 0.0), (2.0, 4.0), (1.0, 10.0)]:
        rep["cases"].append(run_case(sky, pos, [10, 100], nm=NoiseModel(gain=g, rn=rn),
                                     n_real=3, seed0=5000 + int(g * 100 + rn),
                                     label="noise scan g=%.1f RN=%.1f" % (g, rn)))

    # 天光梯度 (受控加性 -> 乘性场) 15% 峰峰
    rep["cases"].append(run_case(sky, pos, [10, 100], grad_amp=0.15, n_real=3,
                                 seed0=9001, label="sky gradient 15% pk-pk"))

    # PSF 模型误差: 注入 FWHM=2.5, 拟合用 2.0 / 3.0
    for ff in (2.0, 3.0):
        rep["cases"].append(run_case(sky, pos, [10, 100], fwhm_fit=ff, n_real=3,
                                     seed0=9100 + int(ff * 10),
                                     label="psf model error fit FWHM=%.1f (true 2.5)" % ff))

    save_json(os.path.join(OUT, "exp1_recovery.json"), rep)

    print("=== exp1 回收精度 (稳健偏差 mag / 稳健散度 mag) ===")
    for case in rep["cases"]:
        print("\n--- %s ---" % case["label"])
        names = list(case["results"][list(case["results"])[0]]["by_estimator"].keys())
        hdr = "%-12s" % "S/N" + "".join("%18s" % n for n in names)
        print(hdr)
        for sk, r in case["results"].items():
            line = "%-12s" % sk
            for n in names:
                v = r["by_estimator"][n]
                line += "%18s" % ("%+.3f/%.3f" % (v["bias_mag"], v["scatter_mag"]))
            print(line)
    print("\nelapsed %.1fs -> %s" % (time.time() - t0, os.path.join(OUT, "exp1_recovery.json")))


if __name__ == "__main__":
    main()