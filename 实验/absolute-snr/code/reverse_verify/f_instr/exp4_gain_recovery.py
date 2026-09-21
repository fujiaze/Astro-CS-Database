#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp4: 真乘性增益的回收 (A4 空间增益的验收前提).

四组 (判据先行):
  A 正对照(能红): 同一 seeing, 入射光子率整体乘 g0 => 所有口径都应回收 g0.
  B 负例(能绿)  : 真值 g0=1.000 (无增益), 只有 seeing 不同 (2.0 vs 4.0) =>
                  孔径无关口径必须回收 1.000 (度量归零); box5 必须偏离.
  C 联合        : 真增益 g0=1.05 且 seeing 不同 => 好口径回收 1.05, box5 给出错值.
  D 小尺度乘性结构 (平场/PRNU 残差, 相关长度 1.5 px, 3% rms) => 回收增益随孔径变化.
"""
import os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, render_psf, aperture_curve, est_box5, est_iso,
                         est_kron, est_psf_nlsq, est_psf_optimal, robust_loc_scale,
                         save_json, moffat_profile, add_sky_gradient)

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "run/reverse_verify/f_instr")
BETA = 3.5
APERTURES = [2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 14.0]


def positions(n, size=512, margin=40, minsep=32, seed=11):
    rng = np.random.default_rng(seed)
    pos, tries = [], 0
    while len(pos) < n and tries < 20000:
        tries += 1
        x, y = rng.uniform(margin, size - margin), rng.uniform(margin, size - margin)
        if all((x - a) ** 2 + (y - b) ** 2 > minsep ** 2 for a, b in pos):
            pos.append((x, y))
    return pos


def smooth_field(shape, corr_px, rms, seed):
    """相关长度 corr_px 的零均值单位方差平滑随机场."""
    from scipy.ndimage import gaussian_filter
    rng = np.random.default_rng(seed)
    w = rng.normal(0, 1, shape)
    w = gaussian_filter(w, corr_px, mode="reflect")
    w = (w - w.mean()) / (w.std() + 1e-12)
    return 1.0 + rms * w


def measure(frame, pos, sig, fwhm):
    out = {k: [] for k in (["box5", "iso5s", "kron", "psf_nlsq", "psf_opt"]
                           + ["aper_%g" % r for r in APERTURES])}
    for (x, y) in pos:
        cur, _ = aperture_curve(frame, x, y, APERTURES)
        for i, r in enumerate(APERTURES):
            out["aper_%g" % r].append(cur[i])
        out["box5"].append(est_box5(frame, x, y))
        out["iso5s"].append(est_iso(frame, x, y, nsigma=5.0, sigma=sig)[0])
        out["kron"].append(est_kron(frame, x, y))
        out["psf_nlsq"].append(est_psf_nlsq(frame, x, y, fwhm, beta=BETA))
        out["psf_opt"].append(est_psf_optimal(frame, x, y, fwhm, beta=BETA))
    return out


def pair_case(sky, pos, nm, fwhm_a, fwhm_b, gain, response_b=None, n_real=4, seed0=20000):
    """渲染帧 A/B, 返回各口径的 median(F_B/F_A) 与真值 gain."""
    sig = None
    accA, accB = {}, {}
    for k in range(n_real):
        rngA = np.random.default_rng(seed0 + 101 * k)
        rngB = np.random.default_rng(seed0 + 101 * k + 7)
        FA = 100.0 * 0.0
        # 真值通量: 以帧 A 的峰值 S/N=100 定标 (两帧同一批星)
        if sig is None:
            f0, _ = nm.render(sky, None, None, np.random.default_rng(4242))
            sig = float(1.4826 * np.median(np.abs(f0 - np.median(f0))))
        F = 100.0 * sig / float(moffat_profile(0.0, 0.0, fwhm_a, BETA))
        srcA = np.zeros(sky.shape); srcB = np.zeros(sky.shape)
        for (x, y) in pos:
            srcA += render_psf(sky.shape, x, y, fwhm_a, total=F, beta=BETA)
            srcB += render_psf(sky.shape, x, y, fwhm_b, total=F * gain, beta=BETA)
        Rb = np.ones(sky.shape) if response_b is None else response_b
        frA, _ = nm.render(sky, srcA, None, rngA)
        frB, _ = nm.render(sky, srcB, Rb, rngB)
        mA = measure(frA, pos, sig, fwhm_a)
        mB = measure(frB, pos, sig, fwhm_b)
        for name in mA:
            a = np.asarray(mA[name]); b = np.asarray(mB[name])
            ok = (a > 0) & (b > 0) & np.isfinite(a) & np.isfinite(b)
            accA.setdefault(name, []).extend(a[ok].tolist())
            accB.setdefault(name, []).extend(b[ok].tolist())
    # 配对比值: 用逐星逐实现配对的中值
    res = {}
    for name in accA:
        a = np.asarray(accA[name]); b = np.asarray(accB[name])
        r = b / a
        loc, sc = robust_loc_scale(r)
        res[name] = {"gain_rec": loc, "scatter": sc,
                     "bias_mag": float(-2.5 * np.log10(loc)) if loc > 0 else None}
    return {"fwhm_a": fwhm_a, "fwhm_b": fwhm_b, "gain_true": gain,
            "response_b": "uniform" if response_b is None else "structured",
            "sigma_adu": sig, "by_estimator": res}


def main():
    t0 = time.time()
    d = np.load(os.path.join(OUT, "scene.npz"))
    sky = d["sky_good"]
    nm = NoiseModel()
    pos = positions(30)
    rep = {"experiment": "exp4_gain_recovery",
           "apertures_px": APERTURES,
           "estimators": ["box5", "iso5s", "kron", "psf_nlsq", "psf_opt"] + ["aper_%g" % r for r in APERTURES],
           "cases": {}}

    rep["cases"]["A_positive_uniform_gain_1.05"] = pair_case(sky, pos, nm, 2.5, 2.5, 1.05, seed0=20000)
    rep["cases"]["A_positive_uniform_gain_0.95"] = pair_case(sky, pos, nm, 2.5, 2.5, 0.95, seed0=21000)
    rep["cases"]["B_null_no_gain_seeing_2.0_vs_4.0"] = pair_case(sky, pos, nm, 2.0, 4.0, 1.00, seed0=22000)
    rep["cases"]["C_gain_1.05_plus_seeing_2.0_vs_4.0"] = pair_case(sky, pos, nm, 2.0, 4.0, 1.05, seed0=23000)
    R = smooth_field(sky.shape, 1.5, 0.03, seed=99)
    rep["cases"]["D_structured_response_3pct_rms_l1.5px"] = pair_case(
        sky, pos, nm, 2.5, 2.5, 1.00, response_b=R, seed0=24000)
    save_json(os.path.join(OUT, "exp4_gain_recovery.json"), rep)

    print("=== exp4 乘性增益回收 (median F_B/F_A; 真值见 gain_true) ===")
    names = rep["estimators"]
    for cname, c in rep["cases"].items():
        print("\n--- %s  (seeing %.1f -> %.1f, 真值增益=%.3f) ---"
              % (cname, c["fwhm_a"], c["fwhm_b"], c["gain_true"]))
        print("%-12s%12s%12s%12s" % ("estimator", "gain_rec", "scatter", "bias_mag"))
        for n in names:
            v = c["by_estimator"][n]
            print("%-12s%12.4f%12.4f%12.4f" % (n, v["gain_rec"], v["scatter"],
                                               v["bias_mag"] if v["bias_mag"] is not None else float("nan")))
    print("\nelapsed %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
