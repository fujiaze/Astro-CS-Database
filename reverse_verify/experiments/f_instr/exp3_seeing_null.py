#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp3: seeing 负例 (真值"无效应") —— 好的口径回收值必须不变 (度量归零).

判据先行 (写死在代码里, 不得事后放宽):
  M_seeing(X) := max_s median(dmag_X(s)) - min_s median(dmag_X(s))   [mag]
  - 孔径无关口径 (psf_nlsq / psf_opt): M_seeing <= 0.010 mag  => PASS (绿)
  - 生产口径 box5 (5x5 固定盒):        M_seeing >= 0.200 mag  => 必须 FAIL (红)
  两条同时成立才算"能红能绿".

等价叙述 (A6 的病灶): 视宁度变化被口径吸收成"假乘性增益"
  k_spurious(s1->s2) = 10^( -0.4*(dmag(s2)-dmag(s1)) )
"""
import os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, render_psf, aperture_curve, encircled_energy,
                         est_box5, est_iso, est_kron, est_psf_nlsq, est_psf_optimal,
                         robust_loc_scale, save_json, moffat_profile, robust_sky_stats)

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "run/reverse_verify/f_instr")
BETA = 3.5
SEEINGS = [1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0]
PASS_THRESHOLD = 0.010     # 绿: 孔径无关口径
RED_THRESHOLD = 0.200      # 红: 生产口径必须超此值


def positions(n, size=512, margin=40, minsep=32, seed=11):
    rng = np.random.default_rng(seed)
    pos, tries = [], 0
    while len(pos) < n and tries < 20000:
        tries += 1
        x, y = rng.uniform(margin, size - margin), rng.uniform(margin, size - margin)
        if all((x - a) ** 2 + (y - b) ** 2 > minsep ** 2 for a, b in pos):
            pos.append((x, y))
    return pos


def main():
    t0 = time.time()
    d = np.load(os.path.join(OUT, "scene.npz"))
    sky = d["sky_good"]
    nm = NoiseModel()
    f0, _ = nm.render(sky, None, None, np.random.default_rng(4242))
    sig = float(1.4826 * np.median(np.abs(f0 - np.median(f0))))
    pos = positions(30)
    n_real = 4
    rep = {"experiment": "exp3_seeing_null_test",
           "frame_sigma_adu": sig, "seeings_px": SEEINGS,
           "criteria": {"aperture_independent_pass_mag": PASS_THRESHOLD,
                        "production_must_fail_mag": RED_THRESHOLD},
           "n_stars": len(pos), "n_real": n_real,
           "per_seeing": {}, "metrics": {}}

    acc = {}
    for fw in SEEINGS:
        F = 100.0 * sig / float(moffat_profile(0.0, 0.0, fw, BETA))   # 固定"真值通量"标度
        vals = {k: [] for k in ("box5", "iso5s", "kron", "psf_nlsq", "psf_opt",
                                "aper_3", "aper_4", "aper_6", "aper_10")}
        for k in range(n_real):
            rng = np.random.default_rng(7700 + 31 * k + int(fw * 100))
            src = np.zeros(sky.shape)
            for (x, y) in pos:
                src += render_psf(sky.shape, x, y, fw, total=F, beta=BETA)
            frame, _ = nm.render(sky, src, None, rng)
            for (x, y) in pos:
                cur, _b = aperture_curve(frame, x, y, [3.0, 4.0, 6.0, 10.0])
                vals["aper_3"].append(cur[0]); vals["aper_4"].append(cur[1])
                vals["aper_6"].append(cur[2]); vals["aper_10"].append(cur[3])
                vals["box5"].append(est_box5(frame, x, y))
                vals["iso5s"].append(est_iso(frame, x, y, nsigma=5.0, sigma=sig)[0])
                vals["kron"].append(est_kron(frame, x, y))
                vals["psf_nlsq"].append(est_psf_nlsq(frame, x, y, fw, beta=BETA))
                vals["psf_opt"].append(est_psf_optimal(frame, x, y, fw, beta=BETA))
        row = {}
        for name, v in vals.items():
            v = np.asarray(v, dtype=np.float64)
            ok = v > 0
            dm = -2.5 * np.log10(v[ok] / F)
            loc, sc = robust_loc_scale(dm)
            row[name] = {"dmag": loc, "scatter": sc}
            acc.setdefault(name, {})[fw] = loc
        rep["per_seeing"]["%.1f" % fw] = {"F_true_adu": F, "by_estimator": row}

    # ---- 度量: M_seeing 与 假增益 ----
    met = {}
    for name, m in acc.items():
        locs = np.array([m[fw] for fw in SEEINGS])
        M = float(np.nanmax(locs) - np.nanmin(locs))
        worst = float(np.nanmax(np.abs(locs)))
        # 假增益: seeing 2.0 -> 4.0
        ks = 10 ** (-0.4 * (m[4.0] - m[2.0]))
        met[name] = {"M_seeing_mag": M, "max_abs_bias_mag": worst,
                     "spurious_gain_seeing2_to_4": float(ks),
                     "spurious_gain_mag": float(-2.5 * np.log10(ks))}
    rep["metrics"] = met
    rep["verdict"] = {}
    for name in met:
        v = "PASS(aperture-independent)" if met[name]["M_seeing_mag"] <= PASS_THRESHOLD else (
            "FAIL" if met[name]["M_seeing_mag"] >= RED_THRESHOLD else "MARGINAL")
        rep["verdict"][name] = v
    rep["red_green_check"] = {
        "green_ok": all(met[n]["M_seeing_mag"] <= PASS_THRESHOLD for n in ("psf_nlsq", "psf_opt")),
        "red_ok": met["box5"]["M_seeing_mag"] >= RED_THRESHOLD,
    }
    save_json(os.path.join(OUT, "exp3_seeing_null.json"), rep)

    print("=== exp3 seeing 负例: 固定真值通量, 只改 seeing (峰值 S/N=100) ===")
    names = ["box5", "iso5s", "kron", "psf_nlsq", "psf_opt", "aper_3", "aper_4", "aper_6", "aper_10"]
    print("%-10s" % "estimator" + "".join("%9s" % ("s=%.1f" % s) for s in SEEINGS)
          + "%12s%12s%14s" % ("M_seeing", "max|bias|", "假增益2->4"))
    for n in names:
        line = "%-10s" % n + "".join("%9.3f" % acc[n][s] for s in SEEINGS)
        line += "%12.3f%12.3f%14.3f" % (met[n]["M_seeing_mag"], met[n]["max_abs_bias_mag"],
                                        met[n]["spurious_gain_seeing2_to_4"])
        print(line)
    print("\n判据: 绿 M_seeing<=%.3f mag (psf_nlsq/psf_opt); 红 box5 M_seeing>=%.3f mag" % (PASS_THRESHOLD, RED_THRESHOLD))
    print("能红能绿:", rep["red_green_check"])
    print("verdict:", rep["verdict"])
    print("elapsed %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
