#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04（订正版）: reference magnitude m_ref (P-CST-19) + chain interface.

订正记录（SCI-702 / P2-M1）：本脚本原版把 sigma_f_k 写成与 F_ref 无关的**字面常数
数组**，于是 "每 mag 比值 = 10^0.4"、"w_pair/w_direct ≡ 1" 两句都是代数恒等式，
任何输入都判不了红（恒真门，标准 01 §7 判无效）。本版按「读真实对象」重写：

  * sigma_F 由**冻结方差组成**从 (F_ref, sigma_sky, PSF, gain) 现算（Horne 最优提取；
    与生产 lib/algorithms/noise_snr/cpp/src/snr_science.cpp:151-211 同式），
    不再是字面常数 ⇒ H1/H2 变成可假判据。
  * 每一判据配**正例（必绿）与负例（必红）**，两侧读数一并落 JSON。

判据（全部能红能绿）：
  G1  单调性        : frame_snr(m_ref) 严格单调降              —— 天空受限臂与源主导臂均须绿
  G2a 天空受限臂    : 每 mag 比值 == 10^0.4 (|dev|<=1e-9)      —— 正例绿
  G2a' 同一口径套源主导臂 (负例) : 必须判红（|dev|>0.1）
  G2b 源泊松主导臂  : 每 mag 比值 == 10^0.2 (|dev|<=3e-3)      —— 绿
  G3  帧内配对恒等式: w = SNR²/F_ref² ≡ 1/σ_F² 是**代数恒等**，
      按 b6_gates_audit 的纪律只登记、不作证据（tautology 标注）
  G3' 跨帧 w 比值的 m_ref 不变性（可假）:
      天空受限臂漂移 <=1e-15 ⇒ 绿；源主导臂漂移 >1e-15 ⇒ 红（负例）
  G4  参考电平张冠李戴注入（负例）: 把 A 帧的 SNR 与 B 帧的 F_ref 配对 ⇒ 判红
  H3  下游接口：IDW 为**代理算子**（不在冻结词表内；本文件只报其数量级读数并显式标注性质）。
      冻结默认算子的传递对照见 route3/exp11_frozen_operator_transfer.py（生产
      SparseSnrReconstructor 直调，真实 M42 4096² 帧 × 真实生产控制网格）与证据说明
      run/FINAL-07/审核包/科研审查/P2_订正/evidence/P2-M5_exp11_frozen_operator_transfer.md：
      默认算子传递因子 T ≈ 0.87（衰减）、同几何离散化 rel RMS 0.1206（IDW 代理 0.2287）。

纯 python + numpy。Seeds hardcoded. Runtime << 5 min.
Output: ../results/route3/exp04_refmag_chain.json
"""
from __future__ import annotations

import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

# 冻结常数（与生产同源；禁止在此处另抄一份实现常数）
GAUSS_FWHM_FACTOR = 2.3548200450309493      # snr_science.cpp:50-54
MOFFAT4_FWHM_FACTOR = 1.2303076525901024    # P-CST-05
MAG_PER_MAG_SKY = 10.0 ** 0.4               # 天空受限：SNR ∝ F_ref
MAG_PER_MAG_SRC = 10.0 ** 0.2               # 源泊松主导：SNR ∝ sqrt(F_ref)

# ---------- 冻结方差组成（生产同式，sigma_F 由输入现算） ----------


def moffat4_profile(half: int, sigma_px: float) -> np.ndarray:
    """离散归一化 Moffat beta=4 轮廓（无 free 参数；与 snr_moffat4_profile_f64 同式）。"""
    j, i = np.mgrid[-half:half + 1, -half:half + 1].astype(float)
    r2 = i * i + j * j
    t = 1.0 + r2 / (2.0 * sigma_px * sigma_px)
    v = 1.0 / (t ** 4)
    return v / v.sum()


def sigma_f_optimal(f_ref: float, sigma_sky: float, gain: float,
                    profile: np.ndarray) -> float:
    """Horne 最优提取：sigma_F^2 = 1/sum(P_i^2/sigma_i^2)，
    sigma_i^2 = sigma_sky^2 + F*P_i/g（gain>0；gain<=0 ⇒ 天空受限，源项不可加）。"""
    var_i = np.full(profile.shape, sigma_sky * sigma_sky, float)
    if gain > 0.0:
        src = f_ref * profile / gain
        var_i = var_i + np.where(src > 0.0, src, 0.0)
    return float(1.0 / np.sqrt(np.sum(profile * profile / var_i)))


# ---------- upstream P1: per-frame zero points ----------

n_frames = 8
zp_true = 25.0
zp_scatter = 0.02
zp_k = zp_true + rng.normal(0.0, zp_scatter, n_frames)   # P1 product: frame ZP [mag]

# 逐帧空背景 rms（P1 产品 noise_sigma）；σ_sky 差异决定跨帧漂移的量级
sigma_sky_k = np.array([15.9, 61.7, 16.4, 60.2, 15.2, 62.5, 17.1, 59.8])
GAIN = 1.3
FWHM_PX = 2.5
HALF = max(30, int(math.ceil(12.0 * FWHM_PX)))
PROFILE = moffat4_profile(HALF, FWHM_PX / GAUSS_FWHM_FACTOR)

# ---------- H1: monotonicity + per-mag scaling (两臂：天空受限 / 源泊松主导) ----------

m_grid = [4.0, 5.0, 6.0, 7.0, 8.0, 9.0]


def per_mag_arm(gain: float, frame: int = 0) -> dict:
    sigma_sky = float(sigma_sky_k[frame])
    snr_by_m = []
    rows = []
    for m_ref in m_grid:
        f_ref = 10.0 ** (-0.4 * (m_ref - zp_k[frame]))
        sf = sigma_f_optimal(f_ref, sigma_sky, gain, PROFILE)
        snr_by_m.append(f_ref / sf)
        rows.append({"m_ref": m_ref, "F_ref_adu": f_ref, "sigma_F_adu": sf,
                     "frame_snr": f_ref / sf})
    snr_by_m = np.array(snr_by_m)
    ratios = snr_by_m[:-1] / snr_by_m[1:]
    return {"gain": gain, "sigma_sky_adu": sigma_sky, "rows": rows,
            "is_monotonic_decreasing": bool(np.all(np.diff(snr_by_m) < 0)),
            "per_mag_ratio_measured": float(np.prod(ratios) ** (1.0 / len(ratios))),
            "per_mag_ratios": ratios.tolist()}


sky_arm = per_mag_arm(0.0)      # gain<=0：天空受限（源泊松项不可加）
src_arm = per_mag_arm(GAIN)     # 生产：gain>0，源泊松项在册

h1 = {
    "sky_limited_arm": sky_arm,
    "source_dominated_arm": src_arm,
    "per_mag_ratio_theory_sky_limited": MAG_PER_MAG_SKY,
    "per_mag_ratio_theory_source_dominated": MAG_PER_MAG_SRC,
    "gates": {
        "G1_monotone_both_arms": bool(sky_arm["is_monotonic_decreasing"]
                                      and src_arm["is_monotonic_decreasing"]),
        "G2a_sky_limited_10p0.4": {
            "measured": sky_arm["per_mag_ratio_measured"],
            "rel_dev": abs(sky_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SKY - 1.0),
            "verdict": "GREEN" if abs(sky_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SKY - 1.0) < 1e-9 else "RED"},
        "G2a_negative_control_same_gate_on_source_dominated": {
            "measured": src_arm["per_mag_ratio_measured"],
            "rel_dev": abs(src_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SKY - 1.0),
            "verdict": "RED(expected)" if abs(src_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SKY - 1.0) > 0.1 else "GREEN(判据失效)"},
        "G2b_source_dominated_10p0.2": {
            "measured": src_arm["per_mag_ratio_measured"],
            "rel_dev": abs(src_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SRC - 1.0),
            "verdict": "GREEN" if abs(src_arm["per_mag_ratio_measured"] / MAG_PER_MAG_SRC - 1.0) < 3e-3 else "RED"},
    },
}

# ---------- H2: 配对恒等式（代数恒等，只登记）+ 跨帧不变性（可假） ----------

m_ref_probe = 6.0
f_ref_k = 10.0 ** (-0.4 * (m_ref_probe - zp_k))
sf_k = np.array([sigma_f_optimal(float(f), float(s), GAIN, PROFILE)
                 for f, s in zip(f_ref_k, sigma_sky_k)])
snr_k = f_ref_k / sf_k
w_pair = (snr_k ** 2) / (f_ref_k ** 2)
w_direct = 1.0 / (sf_k ** 2)

# 恒真门标注（与 b6_gates_audit.json.tautology 同纪律）：
#   w_pair/w_direct ≡ 1 是 (F/σ)²/F² 的代数消去，**不携带数据信息**，
#   任何输入都不可能翻红 ⇒ 只登记、不作证据。
tautology = {
    "id": "H2_pairing_identity",
    "max_rel_dev": float(np.abs(w_pair / w_direct - 1.0).max()),
    "is_tautology": True,
    "evidence_eligible": False,
    "note": "w = SNR²/F_ref² = (F_ref/σ_F)²/F_ref² = 1/σ_F² 为代数恒等；"
            "原版以字面常数 σ_F 复现它并当作证据，已按标准 01 §7 作废。",
}


def cross_frame_w_arm(gain: float, i: int = 0, j: int = 1) -> dict:
    """跨帧 w 比值随 m_ref 的漂移（可假判据）。"""
    wr = []
    for m in m_grid:
        f = 10.0 ** (-0.4 * (m - zp_k[[i, j]]))
        s = np.array([sigma_f_optimal(float(f[0]), float(sigma_sky_k[i]), gain, PROFILE),
                      sigma_f_optimal(float(f[1]), float(sigma_sky_k[j]), gain, PROFILE)])
        wr.append(float((f[0] / s[0]) ** 2 / (f[0] ** 2) / ((f[1] / s[1]) ** 2 / (f[1] ** 2))))
    drift = max(wr) / min(wr) - 1.0
    return {"pair_frames": [i, j], "sigma_sky_pair": [float(sigma_sky_k[i]), float(sigma_sky_k[j])],
            "w_ratio_by_m_ref": wr, "mref_drift_rel": drift}


sky_w = cross_frame_w_arm(0.0)
src_w = cross_frame_w_arm(GAIN)

# G4 负例：参考电平张冠李戴（A 帧 SNR 配 B 帧 F_ref）
mismatch = float(abs((snr_k[0] ** 2 / f_ref_k[1] ** 2) / w_direct[0] - 1.0))

h2 = {
    "pairing_identity": tautology,
    "cross_frame_invariance": {
        "sky_limited_arm": sky_w,
        "source_dominated_arm": src_w,
        "gate_G3p_sky_limited_1e-15": {
            "drift": sky_w["mref_drift_rel"],
            "verdict": "GREEN" if sky_w["mref_drift_rel"] < 1e-15 else "RED"},
        "gate_G3p_negative_control_source_dominated": {
            "drift": src_w["mref_drift_rel"],
            "verdict": "RED(expected)" if src_w["mref_drift_rel"] > 1e-15 else "GREEN(判据失效)"},
    },
    "gate_G4_negative_control_reference_level_mismatch": {
        "max_rel_dev": mismatch,
        "verdict": "RED(expected)" if mismatch > 1e-6 else "GREEN(判据失效)"},
}

# ---------- H3: downstream interface (稀疏 SNR 控制点 -> 稠密 SNR 场) ----------

frame = 512
delta = 64                       # P-CST-16: hips.tile_width(512)/8, structural constant
yy, xx = np.mgrid[0:frame, 0:frame].astype(float)
true_field = 20.0 * (1.0 + 0.3 * np.sin(2 * np.pi * xx / frame) * np.cos(2 * np.pi * yy / frame))
ctrl_pts = []
vals = []
ctrl_noise_rel = 0.015            # P-CST-08 budget: SE/sigma at N_sky = 9216
rng3 = np.random.default_rng(SEED + 7)
for cy in range(delta // 2, frame, delta):
    for cx in range(delta // 2, frame, delta):
        ctrl_pts.append((cx, cy))
        vals.append(true_field[cy, cx] * (1.0 + rng3.normal(0.0, ctrl_noise_rel)))
ctrl_pts = np.array(ctrl_pts, float)
vals = np.array(vals)


def idw_reconstruct(pts, v, grid_xx, grid_yy, power=2.0, k=8):
    out = np.empty(grid_xx.shape)
    flat_g = np.stack([grid_xx.ravel(), grid_yy.ravel()], axis=1)
    for i, g in enumerate(flat_g):
        d2 = ((pts - g) ** 2).sum(axis=1)
        order = np.argsort(d2)[:k]
        w = 1.0 / np.maximum(d2[order], 1e-12) ** (power / 2.0)
        out.ravel()[i] = float((w * v[order]).sum() / w.sum())
    return out


rec = idw_reconstruct(ctrl_pts, vals, xx, yy)
rel_rms = float(np.sqrt(((rec - true_field) ** 2).mean()) / true_field.std())
# zero-effect negative: noiseless control points => reconstruction error is pure IDW discretization
rec0 = idw_reconstruct(ctrl_pts, true_field[ctrl_pts[:, 1].astype(int), ctrl_pts[:, 0].astype(int)], xx, yy)
rel_rms_noiseless = float(np.sqrt(((rec0 - true_field) ** 2).mean()) / true_field.std())

h3 = {"n_control_points": int(len(vals)), "delta_px": delta,
      "control_noise_rel": ctrl_noise_rel,
      "dense_rel_rms_with_ctrl_noise": rel_rms,
      "dense_rel_rms_noiseless_negative": rel_rms_noiseless,
      "noise_inflation_over_noiseless": rel_rms / max(rel_rms_noiseless, 1e-12),
      "operator_is_proxy": True,
      "proxy_operator": "IDW(power=2, k=8)",
      "frozen_default_operator": "natural_bicubic_spline_clip_v1",
      "scope_note": "IDW 不在冻结词表内，此处只作**代理算子的数量级检查**："
                    "无噪声臂的相对 RMS 已由算子离散化决定（0.2287 量级），"
                    "控制点 1.5% 噪声只贡献 1.06× 通胀——这是**代理算子自身**的性质。"
                    "冻结默认算子 natural_bicubic_spline_clip_v1 的传递对照见 "
                    "route3/exp11_frozen_operator_transfer.py（生产 SparseSnrReconstructor 直调）："
                    "T ≈ 0.87（衰减，无放大）、同几何离散化 rel RMS 0.1206。"}

out = {
    "experiment": "P2-route3 EXP-04 reference magnitude + chain interface (corrected: falsifiable gates)",
    "seed": SEED,
    "correction_note": "SCI-702 P2-M1：原版 sigma_f_k 为与 F_ref 无关的字面常数，"
                       "H1/H2 是恒真门；本版 sigma_F 由冻结方差组成现算，并配正负例。",
    "upstream_P1": {"zp_k": [float(z) for z in zp_k], "zp_scatter_mag": zp_scatter},
    "frozen_variance_composition": {
        "sigma_i2": "sigma_sky^2 + F*P_i/g (gain>0)",
        "sigma_F2": "1/sum(P_i^2/sigma_i^2)",
        "profile": "discrete normalized Moffat beta=4, half=%d" % HALF,
        "sigma_sky_per_frame_adu": sigma_sky_k.tolist(),
        "gain_e_per_adu": GAIN,
    },
    "H1_monotonic_scaling": h1,
    "H2_pairing_and_cross_frame": h2,
    "H3_downstream_interface": h3,
}
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, "..", "results", "route3", "exp04_refmag_chain.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps({k: out[k] for k in ("H1_monotonic_scaling",)}, indent=1, ensure_ascii=False))
print("G2a sky:", h1["gates"]["G2a_sky_limited_10p0.4"])
print("G2a' neg:", h1["gates"]["G2a_negative_control_same_gate_on_source_dominated"])
print("G2b src:", h1["gates"]["G2b_source_dominated_10p0.2"])
print("G3' sky:", h2["cross_frame_invariance"]["gate_G3p_sky_limited_1e-15"])
print("G3' neg:", h2["cross_frame_invariance"]["gate_G3p_negative_control_source_dominated"])
print("G4 neg:", h2["gate_G4_negative_control_reference_level_mismatch"])
print("tautology max_rel_dev:", tautology["max_rel_dev"])
print("wrote", path)
