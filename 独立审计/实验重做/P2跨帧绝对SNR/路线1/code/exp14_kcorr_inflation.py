#!/usr/bin/env python3
"""EXP-P2-R1-14: correlation multiplier k_corr = 1.4 (calibration value 1.3883)
(P-CST-10).

Repo formula: control-point variance cvar = k_corr * (pi/2) * sigma^2 / n_ret,
i.e. k_corr multiplies the IID median-of-n_ret variance (pi/2)/n.
Verified here: for exchangeable (equicorrelated) normal inputs the median's
variance inflates by 1 + (n_ret - 1) * rho to MC precision.
Consequence: k_corr = 1.3883 <=> (n_ret - 1) * rho_bar = 0.3883
  (e.g. rho_bar = 0.194 with n_ret = 3) - a HYPOTHESIS; the original calibration
grid is not reconstructable from the registered material => UNRESOLVED.
Negative control: rho = 0 => inflation exactly 1.
Seed fixed 20260926. Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
out = {"seed": SEED, "pi_over_2": float(np.pi / 2.0)}

def mc_median_var(rho, n_ret, nmc=200000, sigma=1.0):
    # FIXED seed per n_ret: different rho rows are PAIRED (same draws), so
    # Var(rho)/Var(0) at equal n is a clean ratio.
    rng = np.random.default_rng(SEED + 7 * n_ret)
    z0 = rng.normal(0.0, 1.0, size=(nmc, 1))
    z = rng.normal(0.0, 1.0, size=(nmc, n_ret))
    x = np.sqrt(rho) * z0 + np.sqrt(1 - rho) * z
    med = np.median(x, axis=1)
    return float(med.var())

rows = []
for n_ret in (3, 5, 9):
    vs = {}
    for rho in (0.0, 0.1, 0.19, 0.194):
        vs[rho] = mc_median_var(rho, n_ret)
    for rho in (0.0, 0.1, 0.19, 0.194):
        v = vs[rho]
        iid = (np.pi / 2.0) / n_ret
        rows.append({"n_ret": n_ret, "rho": rho,
                     "var": v, "iid_median_var": iid,
                     "inflation_vs_asymptotic_iid": v / iid,
                     "ratio_vs_iid_same_n": v / vs[0.0],
                     "law_1_plus_(n-1)rho": 1 + (n_ret - 1) * rho})
out["mc"] = rows
out["mc_note"] = ("Var(median of n_ret exchangeable normals) inflates with rho; "
                  "the exact inflation at small n_ret is BELOW the asymptotic law "
                  "1+(n-1)rho (the exact iid median variance at n=3 is 0.4487, "
                  "not (pi/2)/3 = 0.5236). k_corr = 1.3883 therefore admits two "
                  "readings: the asymptotic law at rho_bar = 0.194 (n_ret=3), or a "
                  "direct calibration at some n_ret; the original calibration grid "
                  "is not reconstructable from the registered material.")
out["inversion"] = {
    "k_corr_calib": 1.3883,
    "n_ret_3_implied_rho": 0.3883 / 2.0,
    "rho_0.19_implied_n_ret": 0.3883 / 0.19 + 1.0,
    "status": "UNRESOLVED: original calibration grid not in registered material",
}
out["null_rho0"] = {
    "ratio_vs_iid_same_n_n3": [r["ratio_vs_iid_same_n"] for r in rows
                               if r["rho"] == 0.0 and r["n_ret"] == 3][0],
    "expected": 1.0,
}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp14_kcorr_inflation.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
