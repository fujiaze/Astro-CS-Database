#!/usr/bin/env python3
"""EXP-P2-R1-04: read-noise double-count bias on sigma_F (P-CST-11).

Repo claim (05 spec P-CST-11 / b2_noise_terms.json, seed_base 20260921):
  +14.5009% at baseline RN=10 e- (sky-dominated reference point)
  +38.2524% at RN=50 e- (worst point of the RN scan)
Mechanism: declared sigma_sky = shot-noise-only but fed the EMPIRICAL TOTAL rms
  => the term (RN/g)^2 enters sigma_i^2 twice:
     correct   sigma_i^2 = sky_e/g^2 + (RN/g)^2 + F_e*P_i/g^2
     double    sigma_i^2 = sky_e/g^2 + 2*(RN/g)^2 + F_e*P_i/g^2
  sigma_F^2 = 1/sum(P_i^2/sigma_i^2), bias = sigma_F'/sigma_F - 1.
This script re-derives the closed form, reproduces the full RN scan, and adds an
independent Monte-Carlo of the realized flux variance with (a) correct weights
and (b) double-counted weights.
Negative control: RN=0 => both formula bias and MC bias are exactly 0.
Seed fixed 20260926. Pure python3+numpy. Runtime < 2 min.
"""
import json, os
import numpy as np

SEED = 20260926
GAIN, DARK, SPSF, F_E, SKY_E, HALF = 1.3, 0.5, 1.5, 1000.0, 100.0, 30
out = {"seed": SEED, "config": {"gain": GAIN, "dark_e": DARK, "psf_sigma_px": SPSF,
                                "F_e": F_E, "sky_e": SKY_E, "half": HALF}}

# pixel profile: Moffat4 (repo standard PSF, docs/science/PSF.md) with parameter
# sigma_psf=1.5 px in the (1+r^2/(2 s^2))^(-4) parameterization, odd grid,
# normalized IN window. Continuous sum(P^2) = 9/(14 pi s^2) = 0.0909 at s=1.5;
# pixelized value reproduces the repo's 0.092684.
r = np.arange(-HALF, HALF + 1)
X, Y = np.meshgrid(r, r)
P = (1.0 + (X ** 2 + Y ** 2) / (2 * SPSF ** 2)) ** -4.0
P /= P.sum()
sum_p2 = float((P ** 2).sum())
out["sum_p2"] = sum_p2
out["sum_p2_repo"] = 0.09268408746165036
out["sum_p2_rel_diff"] = abs(sum_p2 - 0.09268408746165036) / 0.09268408746165036
# truncation convergence check (established-doc 1.2: half 12 -> 60: 0.09268653 -> 0.09268408)
def sum_p2_at(half):
    rr = np.arange(-half, half + 1)
    XX, YY = np.meshgrid(rr, rr)
    PP = (1.0 + (XX ** 2 + YY ** 2) / (2 * SPSF ** 2)) ** -4.0
    return float(((PP / PP.sum()) ** 2).sum())
out["truncation_check"] = {"half12": sum_p2_at(12), "half60": sum_p2_at(60),
                           "doc_half12": 0.09268653, "doc_half60": 0.09268408}

def sigma_f(sky_var, rn_var, src_scale):
    """sigma_F for weights built from a candidate variance model.
    sky_var: (sky_e+dark)/g^2   rn_var: (RN/g)^2   src_scale: F_e/g^2
    If double_counted, rn_var is added a second time into the weight model."""
    var_true = sky_var + rn_var + src_scale * P
    return var_true

def bias_formula(rn):
    sky_var = (SKY_E + DARK) / GAIN ** 2
    rn_var = (rn / GAIN) ** 2
    src = F_E / GAIN ** 2
    v_true = sigma_f(sky_var, rn_var, src)
    v_double = v_true + rn_var
    sF_true = 1.0 / np.sqrt((P ** 2 / v_true).sum())
    sF_double = 1.0 / np.sqrt((P ** 2 / v_double).sum())
    return float(sF_double / sF_true - 1.0)

scan = [0.0, 2.0, 5.0, 10.0, 20.0, 50.0]
repo = [0.0, 0.009439, 0.051499, 0.145009, 0.277027, 0.382524]
rows = []
for rn, rp in zip(scan, repo):
    b = bias_formula(rn)
    rows.append({"rn_e": rn, "bias_recomputed": b, "bias_repo": rp,
                 "abs_diff": abs(b - rp)})
out["rn_scan_formula"] = rows
out["baseline_rn10_abs_diff"] = rows[3]["abs_diff"]
out["rn50_abs_diff"] = rows[5]["abs_diff"]

# dark / gain / flux scans at RN=10 (spot checks vs repo b2 file)
out["dark_scan"] = []
for d, db in [(0.0, 0.145235), (1.0, 0.144784), (10.0, 0.140858),
              (100.0, 0.111150), (1000.0, 0.036680)]:
    g0 = GAIN
    sky_var = (SKY_E + d) / g0 ** 2
    rn_var = (10.0 / g0) ** 2
    src = F_E / g0 ** 2
    v = sky_var + rn_var + src * P
    vd = v + rn_var
    b = 1.0 / np.sqrt((P ** 2 / vd).sum()) * np.sqrt((P ** 2 / v).sum()) - 1.0
    out["dark_scan"].append({"dark_e": d, "bias": float(b), "repo": db,
                             "abs_diff": abs(float(b) - db)})
out["gain_scan_invariance"] = [
    {"gain": g, "bias": bias_formula(10.0) if False else (
        lambda: (lambda sv, rv, sc: float(
            (1.0 / np.sqrt(((P ** 2) / (sv + rv + rv + sc * P)).sum())) *
            np.sqrt(((P ** 2) / (sv + rv + sc * P)).sum()) - 1.0))(
        (SKY_E + DARK) / g ** 2, (10.0 / g) ** 2, F_E / g ** 2))()}
    for g in (0.5, 1.0, 1.3, 2.0, 4.0)]

# ---------- Monte Carlo ----------
def mc_point(rn, n_mc=8000, double=True):
    rng = np.random.default_rng(SEED + int(rn * 10))   # paired: same frames both arms
    sky_var = (SKY_E + DARK) / GAIN ** 2
    rn_var = (rn / GAIN) ** 2
    src = F_E / GAIN ** 2
    v_true = sky_var + rn_var + src * P
    v_w = v_true + (rn_var if double else 0.0)
    w = (P / v_w).ravel()
    denom = (P ** 2 / v_w).sum()
    F = np.empty(n_mc)
    ch = 1000
    for i in range(0, n_mc, ch):
        m = min(ch, n_mc - i)
        sky_adu = rng.poisson(SKY_E + DARK, size=(m, P.size)).astype(float) / GAIN
        rn_g = rng.normal(0.0, rn / GAIN, size=(m, P.size)) if rn > 0 else 0.0
        src_adu = rng.poisson(F_E * P.ravel(), size=(m, P.size)).astype(float) / GAIN
        x = sky_adu + rn_g + src_adu
        F[i:i + m] = (x * w).sum(axis=1) / denom
    return float(F.std()), float(np.sqrt(1.0 / (P ** 2 / v_true).sum()))

mc_rows = []
for rn in scan:
    sd_c, s_pred = mc_point(rn, double=False)
    sd_d, _ = mc_point(rn, double=True)
    mc_rows.append({"rn_e": rn,
                    "mc_var_ratio_minus1_doubleweights": (sd_d / sd_c) ** 2 - 1.0,
                    "mc_realized_over_correct_pred": sd_c / s_pred - 1.0,
                    "formula_bias": bias_formula(rn)})
out["rn_scan_monte_carlo"] = mc_rows
out["mc_note"] = ("mc_var_ratio_minus1_doubleweights = realized variance inflation "
                  "when weights are built from the double-counted model; it is a "
                  "different quantity from the formula bias (predicted sigma_F "
                  "inflation), both are reported.")

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp04_double_count_bias.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps({k: out[k] for k in ("sum_p2", "sum_p2_rel_diff", "truncation_check",
                                      "baseline_rn10_abs_diff", "rn50_abs_diff",
                                      "rn_scan_formula", "dark_scan",
                                      "rn_scan_monte_carlo")}, indent=1))
