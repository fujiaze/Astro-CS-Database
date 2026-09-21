#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp2: 各口径的"孔径依赖曲线" (同一星, 不同孔径/不同 seeing 下的回收通量).

产出 (任务书 ③-14):
  A. 固定 seeing, 扫孔径 r: F_rec(r)/F_true 曲线 (含理论增长曲线 CoG(r) 作对照);
  B. 固定孔径, 扫 seeing: 曲线如何整体平移 => 孔径改正是否随视宁度失效;
  C. 孔径依赖度量: 峰峰散度 (max-min of dmag over r) 与对数斜率 dlog10F/dlog10r.
"""
import os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, render_psf, aperture_curve, encircled_energy,
                         est_box5, est_iso, est_kron, est_psf_nlsq, est_psf_optimal,
                         dmag, robust_loc_scale, save_json, moffat_profile)

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "run/reverse_verify/f_instr")
BETA = 3.5
RADII = [2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 12.0, 14.0, 16.0]
SEEINGS = [1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0]


def positions(n, size=512, margin=40, minsep=32, seed=11):
    rng = np.random.default_rng(seed)
    pos, tries = [], 0
    while len(pos) < n and tries < 20000:
        tries += 1
        x, y = rng.uniform(margin, size - margin), rng.uniform(margin, size - margin)
        if all((x - a) ** 2 + (y - b) ** 2 > minsep ** 2 for a, b in pos):
            pos.append((x, y))
    return pos


def frame_sigma(sky, nm, seed=4242):
    f, _ = nm.render(sky, None, None, np.random.default_rng(seed))
    return robust_loc_scale(f.ravel())[1] if False else float(
        np.median(np.abs(f - np.median(f))) * 1.4826)


def main():
    t0 = time.time()
    d = np.load(os.path.join(OUT, "scene.npz"))
    sky = d["sky_good"]
    nm = NoiseModel()
    sig = frame_sigma(sky, nm)
    pos = positions(30)
    n_real = 3
    rep = {"experiment": "exp2_aperture_dependence",
           "frame_sigma_adu": sig, "radii_px": RADII, "seeings_px": SEEINGS,
           "beta": BETA, "n_stars": len(pos), "n_real": n_real,
           "note": "峰值 S/N 目标 = 100; 偏差为稳健中值 (mag), 正值 = 回收通量偏低",
           "curves": {}, "aperture_dependence": {}}

    for fw in SEEINGS:
        F = 100.0 * sig / float(moffat_profile(0.0, 0.0, fw, BETA))
        acc = {("aper_%g" % r): [] for r in RADII}
        mod = {"box5": [], "iso5s": [], "kron": [], "psf_nlsq": [], "psf_opt": []}
        for k in range(n_real):
            rng = np.random.default_rng(3300 + 17 * k + int(fw * 100))
            src = np.zeros(sky.shape)
            for (x, y) in pos:
                src += render_psf(sky.shape, x, y, fw, total=F, beta=BETA)
            frame, _ = nm.render(sky, src, None, rng)
            for (x, y) in pos:
                cur, bkg = aperture_curve(frame, x, y, RADII)
                for i, r in enumerate(RADII):
                    acc["aper_%g" % r].append(-2.5 * np.log10(cur[i] / F))
                mod["box5"].append(-2.5 * np.log10(max(est_box5(frame, x, y), 1e-9) / F))
                mod["iso5s"].append(-2.5 * np.log10(max(est_iso(frame, x, y, nsigma=5.0, sigma=sig)[0], 1e-9) / F))
                mod["kron"].append(-2.5 * np.log10(max(est_kron(frame, x, y), 1e-9) / F))
                mod["psf_nlsq"].append(-2.5 * np.log10(max(est_psf_nlsq(frame, x, y, fw, beta=BETA), 1e-9) / F))
                mod["psf_opt"].append(-2.5 * np.log10(max(est_psf_optimal(frame, x, y, fw, beta=BETA), 1e-9) / F))
        row = {}
        for name, vals in acc.items():
            loc, sc = robust_loc_scale(vals)
            row[name] = {"dmag": loc, "scatter": sc, "flux_ratio": float(10 ** (-0.4 * loc))}
        for name, vals in mod.items():
            loc, sc = robust_loc_scale(vals)
            row[name] = {"dmag": loc, "scatter": sc, "flux_ratio": float(10 ** (-0.4 * loc))}
        # 理论增长曲线 (对照)
        row["_cog_theory"] = {("aper_%g" % r): float(encircled_energy(fw, r, beta=BETA)) for r in RADII}
        rep["curves"]["seeing_%.1f" % fw] = {"F_true_adu": F, "by_aperture": row}

    # 孔径依赖度量: 对每个 seeing, 孔径族的峰峰散度 与 log-log 斜率
    for fw in SEEINGS:
        row = rep["curves"]["seeing_%.1f" % fw]["by_aperture"]
        dm = np.array([row["aper_%g" % r]["dmag"] for r in RADII])
        rr = np.array(RADII)
        rep["aperture_dependence"]["seeing_%.1f" % fw] = {
            "peak_to_peak_mag_aperture_family": float(np.nanmax(dm) - np.nanmin(dm)),
            "slope_dmag_dlogr": float(np.polyfit(np.log10(rr), dm, 1)[0]),
            "box5_mag": row["box5"]["dmag"], "iso5s_mag": row["iso5s"]["dmag"],
            "psf_nlsq_mag": row["psf_nlsq"]["dmag"], "psf_opt_mag": row["psf_opt"]["dmag"],
        }
    save_json(os.path.join(OUT, "exp2_aperture_dependence.json"), rep)

    print("=== 孔径依赖曲线 dmag(r)  (seeing x 孔径; 峰 S/N=100) ===")
    hdr = "%-8s" % "seeing" + "".join("%9s" % ("r=%g" % r) for r in RADII) + "".join(
        "%10s" % n for n in ("box5", "iso5s", "kron", "psf_nlsq", "psf_opt"))
    print(hdr)
    for fw in SEEINGS:
        row = rep["curves"]["seeing_%.1f" % fw]["by_aperture"]
        line = "%-8.1f" % fw + "".join("%9.3f" % row["aper_%g" % r]["dmag"] for r in RADII)
        line += "".join("%10.3f" % row[n]["dmag"] for n in ("box5", "iso5s", "kron", "psf_nlsq", "psf_opt"))
        print(line)
    print("\n=== 孔径依赖度量 ===")
    for fw in SEEINGS:
        a = rep["aperture_dependence"]["seeing_%.1f" % fw]
        print("seeing=%.1f  孔径族峰峰=%.3f mag  d(dmag)/dlogr=%+.3f  box5=%+.3f  iso5s=%+.3f  psf_nlsq=%+.3f  psf_opt=%+.3f"
              % (fw, a["peak_to_peak_mag_aperture_family"], a["slope_dmag_dlogr"],
                 a["box5_mag"], a["iso5s_mag"], a["psf_nlsq_mag"], a["psf_opt_mag"]))
    print("\nelapsed %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
