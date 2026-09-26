#!/usr/bin/env python3
"""exp8_fsyn_forward.py -- P1 route2 experiment 8: F_syn forward model & integration core.

Items covered:
  S11 式-1 F_syn = int F_lambda T Q lambda dlambda (PHOTOMETRY.md 2a.1):
      - pure synthetic physics forward (Planck star x passband x QE)
      - lambda power test: wrong power fails by ~ one effective wavelength
      - 1 mag brighter star => F_syn ratio exactly 10^0.4
      - Simpson vs trapezoid convergence on the 2 nm production grid
      - 01/B11 Simpson odd-interval bug reproduction (n_int=3) + production-grid immunity
  S14 chain demo (P1 -> P2 interface): ZP_syn, F_ref(m_ref), k_photo, m_5.

Seed fixed. Run: python3 exp8_fsyn_forward.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

MAD_SCALE = 0.6744897501960817
HC = 1.98644586e-16          # J*nm
WL = np.arange(336.0, 1020.0 + 1e-9, 2.0)   # 336..1020 nm step 2 -> 343 points

def planck_f_lambda(wl_nm, T):
    """B_lambda in W m^-2 nm^-1 sr^-1 (per sr; normalization irrelevant here)."""
    lam_m = wl_nm * 1e-9
    h = 6.62607015e-34; c = 2.99792458e8; kb = 1.380649e-23
    return (2.0 * h * c ** 2) / (lam_m ** 5) / (np.exp(h * c / (lam_m * kb * T)) - 1.0) * 1e-9

def toy_passband(wl, center=643.4, sigma=60.0):
    return np.exp(-0.5 * ((wl - center) / sigma) ** 2)

def toy_qe(wl, peak=600.0, width=250.0, qmax=0.7):
    return qmax * np.exp(-0.5 * ((wl - peak) / width) ** 2)

T = toy_passband(WL)
Q = toy_qe(WL)

def simpson_correct(wl, y):
    """Composite Simpson 1/3 with 3/8 fallback + trapezoid degenerate (production spec)."""
    n = len(wl) - 1  # intervals
    if n == 0:
        return 0.0
    if n == 1:
        h = wl[1] - wl[0]
        return 0.5 * h * (y[0] + y[1])
    total = 0.0
    i = 0
    n_even = n - (n % 2)
    if n_even >= 2:
        h = wl[1] - wl[0]
        wsum = np.zeros(n_even + 1)
        wsum[1:-1:2] = 4.0
        wsum[2:-1:2] = 2.0
        wsum[0] = 1.0; wsum[-1] = 1.0
        total = (h / 3.0) * float(np.dot(wsum, y[:n_even + 1]))
        i = n_even
    rem = n - i
    if rem == 3:
        h = wl[1] - wl[0]
        total += (3.0 * h / 8.0) * (y[i] + 3.0 * y[i + 1] + 3.0 * y[i + 2] + y[i + 3])
    elif rem == 1:
        h = wl[1] - wl[0]
        total += 0.5 * h * (y[i] + y[i + 1])
    return total

def simpson_buggy_b11(wl, y):
    """Reproduce the 01/B11 defect shape: on an odd-interval count the odd-tail
    3/8 weights are never applied (n_13=0) while the 1/3-rule endpoint weights
    are still counted, leaving an extra 2*y[0]*h/3 term => integral biased high,
    error proportional to the left endpoint value."""
    n = len(wl) - 1
    if n == 0:
        return 0.0
    h = wl[1] - wl[0]
    base = simpson_correct(wl, y)
    if n % 2 == 1:
        return base + 2.0 * y[0] * h / 3.0
    return base

# ---- (a) lambda power test -----------------------------------------------------
F_star = planck_f_lambda(WL, 5800.0)
fsyn = {}
for p in [0, 1, 2]:
    integrand = F_star * T * Q * WL ** p
    fsyn[p] = simpson_correct(WL, integrand)
lambda_eff_photon = fsyn[2] / fsyn[1]      # <lambda> under photon weighting, nm
lam_ratio_0_1 = fsyn[0] / fsyn[1]

# ---- (b) magnitude ratio --------------------------------------------------------
ratio_mag = simpson_correct(WL, 10.0 ** 0.4 * F_star * T * Q * WL) / fsyn[1]

# ---- (c) rule comparison --------------------------------------------------------
integrand1 = F_star * T * Q * WL
simp = simpson_correct(WL, integrand1)
trap = float(np.trapezoid(integrand1, WL)) if hasattr(np, "trapezoid") else float(np.trapz(integrand1, WL))
# reference: 10x finer grid
WL_fine = np.arange(336.0, 1020.0 + 1e-9, 0.2)
F_fine = planck_f_lambda(WL_fine, 5800.0)
T_fine = toy_passband(WL_fine); Q_fine = toy_qe(WL_fine)
ref = float(np.trapezoid(F_fine * T_fine * Q_fine * WL_fine, WL_fine)) if hasattr(np, "trapezoid") else float(np.trapz(F_fine * T_fine * Q_fine * WL_fine, WL_fine))
rule = {
    "simpson_vs_fine_ref_rel": abs(simp - ref) / ref,
    "trapezoid_vs_fine_ref_rel": abs(trap - ref) / ref,
}

# ---- (d) B11 bug ----------------------------------------------------------------
bug = {}
y1111 = np.ones(4)
bug["y_ones_4pt_correct"] = simpson_correct(np.arange(4.0), y1111)
bug["y_ones_4pt_buggy"] = simpson_buggy_b11(np.arange(4.0), y1111)
bug["rel_error"] = bug["y_ones_4pt_buggy"] / bug["y_ones_4pt_correct"] - 1.0
y0123 = np.arange(4.0)
bug["y_0123_buggy"] = simpson_buggy_b11(np.arange(4.0), y0123)
bug["y_0123_correct"] = simpson_correct(np.arange(4.0), y0123)
bug["production_grid_343pt_buggy_minus_correct"] = simpson_buggy_b11(WL, integrand1) - simpson_correct(WL, integrand1)
bug["production_grid_immune"] = bool(bug["production_grid_343pt_buggy_minus_correct"] == 0.0)

# ---- (e) chain demo: ZP_syn / F_ref / k_photo / m_5 ------------------------------
n = 2000
G = rng.uniform(6.0, 16.0, n)
colors = rng.standard_normal(n) * 0.4
F_lambda_star = planck_f_lambda(WL, 5800.0)[None, :] * (10.0 ** (-0.4 * (G - 6.0)))[:, None]                 * (1.0 + 0.05 * colors[:, None] * (WL[None, :] - 660.0) / 340.0)
fsyns = np.array([simpson_correct(WL, F_lambda_star[i] * T * Q * WL) for i in range(n)])
zp_true = 22.0
r_noise = rng.standard_normal(n) * 0.05
F_instr = fsyns * 10.0 ** (r_noise)
zp_syn = float(np.median(G + 2.5 * np.log10(fsyns)))
location = float(np.median(np.log10(F_instr / fsyns)))
k_photo = 10.0 ** (-location)
m_ref = 6.0
F_ref_adu = 10.0 ** (-0.4 * (m_ref - (zp_syn - 2.5 * np.log10(k_photo))))
sigma_residual = float(np.median(np.abs((np.log10(F_instr / fsyns) - location) - np.median(np.log10(F_instr / fsyns))))) / MAD_SCALE
sigma_kappa_stat = 1.253 * sigma_residual / math.sqrt(n)
m5 = (zp_syn - 2.5 * np.log10(k_photo)) - 2.5 * math.log10(5.0)

out = {
    "seed": SEED,
    "grid": {"wl_start": 336.0, "wl_step": 2.0, "n_points": int(WL.size),
             "self_consistency": bool(abs(WL[-1] - 1020.0) < 1e-9)},
    "S11_lambda_power": {
        "fsyn_p0": fsyn[0], "fsyn_p1": fsyn[1], "fsyn_p2": fsyn[2],
        "lambda_eff_photon_nm": lambda_eff_photon,
        "ratio_p0_over_p1": lam_ratio_0_1,
        "ratio_p2_over_p1": fsyn[2] / fsyn[1],
    },
    "S11_mag_ratio": {"ratio_1mag_brighter": ratio_mag,
                      "expected": 10.0 ** 0.4,
                      "abs_dev": abs(ratio_mag - 10.0 ** 0.4)},
    "S11_rule_comparison": rule,
    "S11_B11_simpson_bug": bug,
    "S14_chain_demo": {
        "n_stars": n,
        "ZP_syn_mag": zp_syn,
        "location_dex": location,
        "k_photo_Fsyn_units_per_ADU": k_photo,
        "m_ref": m_ref,
        "F_ref_ADU": F_ref_adu,
        "sigma_residual_dex": sigma_residual,
        "sigma_kappa_stat_dex": sigma_kappa_stat,
        "m5_mag": m5,
        "units_note": "F_syn in W*m^-2*nm (arb scale from Planck sr^-1); F_instr synthetic ADU; interface to P2 = sigma_kappa_stat, m5",
    },
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp8_fsyn_forward.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

