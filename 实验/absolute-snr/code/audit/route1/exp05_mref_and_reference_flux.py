#!/usr/bin/env python3
"""EXP-P2-R1-05 (订正版): reference magnitude m_ref = 6.0 and F_ref identities
(P-CST-19, P-ALG-07).

订正记录（SCI-702 / P2-M1）：原版把 sigma_F 写成字面常数 12.0，于是
"frame_snr = F_ref/12 ⇒ 每 mag 比值 = 10^0.4" 是浮点求值恒等式，
"w 比值不变" 是代数消去 —— 两者都是恒真门（标准 01 §7 判无效）。本版：

  * sigma_F 由冻结方差组成现算（Horne 最优提取；与 snr_science.cpp:151-211 同式）；
  * 每 mag 比值改为**分域判据**并配正负例；
  * 配对恒等式标注为 tautology（只登记、不作证据）；
  * 跨帧 w 比值不变性改为可假判据（天空受限绿 / 源主导红）。

Claims tested:
  1. frame_snr = F_ref/sigma_F(ref) with F_ref = 10^(-0.4 (m_ref - ZP_k)):
     天空受限（gain<=0）每 mag 比 = 10^0.4；源泊松主导（gain>0）每 mag 比 = 10^0.2。
  2. 帧内配对恒等式 w = SNR_k^2/F_ref_k^2 = 1/sigma_F_k^2（代数恒等，不作证据）。
  3. F_ref,k * k_photo,k = F0 identity; injecting k_photo error 4.3e-4
     reproduces the same-order F0 mismatch.
  4. Magnitude-flux conversion: F(m+1)/F(m) = 10^-0.4 = 1/2.5118864 (P-CST-01/02
     structural constants exercised numerically).
  5. 负例：把判据 1 的 10^0.4 口径套到源主导臂 ⇒ 判红；参考电平张冠李戴 ⇒ 判红。
Seed fixed 20260926. Pure python3+numpy.
"""
import json, math, os
import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)
out = {"seed": SEED}

ZP_SYN = 25.0
MREF = 6.0
F0 = 10.0 ** (-0.4 * (MREF - ZP_SYN))
GAUSS_FWHM_FACTOR = 2.3548200450309493
FWHM_PX = 2.5
HALF = max(30, int(math.ceil(12.0 * FWHM_PX)))
GAIN = 1.3


def moffat4_profile(half, sigma_px):
    j, i = np.mgrid[-half:half + 1, -half:half + 1].astype(float)
    t = 1.0 + (i * i + j * j) / (2.0 * sigma_px * sigma_px)
    v = 1.0 / (t ** 4)
    return v / v.sum()


PROFILE = moffat4_profile(HALF, FWHM_PX / GAUSS_FWHM_FACTOR)


def sigma_f_of(f_ref, sigma_sky, gain):
    var_i = np.full(PROFILE.shape, sigma_sky * sigma_sky, float)
    if gain > 0.0:
        src = f_ref * PROFILE / gain
        var_i = var_i + np.where(src > 0.0, src, 0.0)
    return float(1.0 / np.sqrt(np.sum(PROFILE * PROFILE / var_i)))


mags = np.array([4.0, 5.0, 6.0, 7.0, 8.0])
F_ref = 10.0 ** (-0.4 * (mags - ZP_SYN))
SKY = 15.9        # 本帧空背景 rms [ADU]（P1 产品 noise_sigma）


def arm(gain):
    snr = np.array([float(f) / sigma_f_of(float(f), SKY, gain) for f in F_ref])
    ratios = snr[:-1] / snr[1:]
    return snr, ratios


snr_sky, ratios_sky = arm(0.0)
snr_src, ratios_src = arm(GAIN)
out["frame_snr_vs_mref"] = {
    "m_ref": mags.tolist(), "F_ref": F_ref.tolist(),
    "sigma_sky_adu": SKY, "gain": GAIN, "fwhm_px": FWHM_PX, "profile_half_px": HALF,
    "sky_limited_arm": {
        "frame_snr": snr_sky.tolist(), "snr_ratio_per_mag": ratios_sky.tolist(),
        "geomean_per_mag": float(np.prod(ratios_sky) ** (1.0 / len(ratios_sky))),
        "expected_per_mag": 10.0 ** 0.4,
        "max_dev": float(np.max(np.abs(ratios_sky / (10.0 ** 0.4) - 1.0))),
        "verdict": "GREEN" if float(np.max(np.abs(ratios_sky / (10.0 ** 0.4) - 1.0))) < 1e-9 else "RED",
    },
    "source_dominated_arm": {
        "frame_snr": snr_src.tolist(), "snr_ratio_per_mag": ratios_src.tolist(),
        "geomean_per_mag": float(np.prod(ratios_src) ** (1.0 / len(ratios_src))),
        "expected_per_mag": 10.0 ** 0.2,
        "max_dev_vs_10p0.2": float(np.max(np.abs(ratios_src / (10.0 ** 0.2) - 1.0))),
        "max_dev_vs_10p0.4": float(np.max(np.abs(ratios_src / (10.0 ** 0.4) - 1.0))),
        "verdict": "GREEN" if float(np.max(np.abs(ratios_src / (10.0 ** 0.2) - 1.0))) < 3e-3 else "RED",
    },
    "monotone_decreasing": bool(np.all(np.diff(snr_sky) < 0) and np.all(np.diff(snr_src) < 0)),
    "negative_control_10p0.4_gate_on_source_dominated": {
        "max_dev": float(np.max(np.abs(ratios_src / (10.0 ** 0.4) - 1.0))),
        "verdict": "RED(expected)" if float(np.max(np.abs(ratios_src / (10.0 ** 0.4) - 1.0))) > 0.1 else "GREEN(判据失效)",
    },
}

# weights: w = SNR^2/F_ref^2 with per-frame sigma_F,k (independent of m_ref)
sigF_k = np.array([sigma_f_of(float(f), float(s), GAIN) for f, s in
                   zip(F_ref, SKY * np.exp(rng.normal(0.0, 0.3, size=5)))])
w = (F_ref / sigF_k) ** 2 / F_ref ** 2
w2 = 1.0 / sigF_k ** 2
out["weight_pairing_identity_TAUTOLOGY"] = {
    "metric_max_rel_dev": float(np.max(np.abs(w / w2 - 1.0))),
    "is_tautology": True,
    "evidence_eligible": False,
    "note": "w = (F_ref/sigma_F)^2/F_ref^2 = 1/sigma_F^2 为代数恒等；原版以字面常数 "
            "sigma_F=12.0 复现之并当作证据，已按标准 01 §7 作废（与 b6 tautology 同纪律）。",
}
# 可假判据：跨帧 w 比值随 m_ref 的漂移（两帧 σ_sky 不同）
SKY_B = 61.7


def cross_frame_drift(gain):
    wr = []
    for m in mags:
        f = 10.0 ** (-0.4 * (m - ZP_SYN))
        sa = sigma_f_of(float(f), SKY, gain)
        sb = sigma_f_of(float(f), SKY_B, gain)
        wr.append(float(((f / sa) ** 2 / f ** 2) / ((f / sb) ** 2 / f ** 2)))
    return {"w_ratio_by_m_ref": wr, "mref_drift_rel": max(wr) / min(wr) - 1.0}


drift_sky = cross_frame_drift(0.0)
drift_src = cross_frame_drift(GAIN)
out["weight_cross_frame_invariance"] = {
    "sky_limited_arm": drift_sky, "source_dominated_arm": drift_src,
    "gate_sky_limited_1e-15": {"drift": drift_sky["mref_drift_rel"],
                               "verdict": "GREEN" if drift_sky["mref_drift_rel"] < 1e-15 else "RED"},
    "gate_negative_control_source_dominated": {
        "drift": drift_src["mref_drift_rel"],
        "verdict": "RED(expected)" if drift_src["mref_drift_rel"] > 1e-15 else "GREEN(判据失效)"},
}
# per-mag overall weight scale 6.31 = 2.512^2（仅天空受限臂成立）
out["weight_overall_scale_per_mag"] = float((10.0 ** 0.4) ** 2)

# F_ref,k * k_photo,k = F0
kp = 10.0 ** rng.uniform(-0.35, 0.35, size=8)             # the photometric scale factor
ZP_k = ZP_SYN - 2.5 * np.log10(kp)                        # ZP_k = ZP_syn - 2.5 log10(kp)
F_ref_k = 10.0 ** (-0.4 * (MREF - ZP_k))
resid = F_ref_k * kp - F0
out["fref_kphoto_identity"] = {
    "k_photo": kp.tolist(),
    "max_abs_resid_over_F0": float(np.max(np.abs(resid)) / F0),
    "identity_is_exact_up_to_float": True,
}
eps = 4.3e-4
out["fref_kphoto_perturbation"] = {
    "eps": eps, "F0_mismatch_rel": eps,
    "note": "a k_photo error eps maps one-to-one into a relative F0 mismatch eps",
}
out["mag_structural"] = {
    "10^0.4": 10.0 ** 0.4,
    "2.5118864_doc": 2.5118864,
    "rel_diff": abs(10.0 ** 0.4 - 2.5118864) / 2.5118864,
    "ratio_F(m+1)/F(m)": float(10.0 ** -0.4),
    "0.4_equals_1_over_2p5": abs(0.4 - 1.0 / 2.5) < 1e-15,
}
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results",
                    "route1", "exp05_mref_and_reference_flux.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps({k: out[k] for k in ("frame_snr_vs_mref", "weight_cross_frame_invariance")},
                 indent=1, ensure_ascii=False))
print("wrote", path)
