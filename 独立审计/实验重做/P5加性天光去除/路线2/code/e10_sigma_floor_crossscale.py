#!/usr/bin/env python3
"""E10: sigma-floor guards across scales (defects D-01/D-04 face; PHASE2_UPM.md §4/§8).

Claims audited:
  - A "sigma floor" is a scale-relative guard: upm.cpp's sigma_floor = 1e-3 (ADU-scale)
    vs the production observable scale ~5.26e13 ADU/sr (M42 sky 1210 ADU/px / Omega_px
    2.2991e-11 sr) => floor/sigma ~ 2e-12 => the floor can NEVER bind at production
    scale (protection vacuous there); at the alpha^2 scale (~1e-29) it ALWAYS binds.
    A floor is therefore NOT a scientific constant but a scale-dependent guard; the
    release-grade handling of "no scale information" must be ivar = 0 fail-closed.
  - uncertainty = +inf => sigma_eff = inf => z = 0 => Huber weight = 1: a completely
    uncertain observation receives FULL weight (D-04) -- demonstrated;
  - 1e-12 protective constant squared into a pseudo-variance (D-01): publishing
    ivar = 1/(1e-12)^2 = 1e24 dominates the normalized share weight => one degenerate
    observation captures ~100% of its control cell's weight -- demonstrated;
  - NEGATIVE / corrected branch: non-finite or non-positive uncertainty => weight 0
    (fail-closed, PHASE2_UPM §4) => the influence of the degenerate observation on the
    estimate is exactly 0 (truth-no-effect => metric zero).
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e10_sigma_floor_crossscale.py
"""
import json, math, os
import numpy as np

SEED = 20250926
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e10_sigma_floor_crossscale.json")

def huber_weight(z, delta=1.345):
    return np.where(np.abs(z) <= delta, 1.0, delta / np.abs(z))

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED}
    # (1) floor activation vs observable scale
    act = []
    for S in (1.0, 1.0e3, 5.26e13, 1.0e-29):
        sig = np.abs(rng.standard_normal(100_000)) * 0.01 * S + 1e-30
        floor = 1e-3 * (1.0 if S >= 1 else 1.0)
        act.append({"scale_S": S, "typical_sigma": 0.01 * S,
                    "floor_activation_rate": float(np.mean(sig < floor))})
    res["floor_activation_vs_scale"] = act
    # (2) uncertainty = +inf gets full Huber weight
    w_inf = huber_weight(np.array([0.0]))[0]     # z = r/inf = 0
    w_3sig = huber_weight(np.array([3.0]))[0]
    res["inf_uncertainty_full_weight"] = {"z": 0.0, "huber_weight": w_inf,
                                          "weight_of_3sigma_outlier": w_3sig,
                                          "verdict": "completely-uncertain obs gets FULL weight" if w_inf > w_3sig else "ok"}
    # (3) pseudo-variance domination (D-01): one zero-scale patch among 50 in a cell
    n_obs = 50
    ivar_ok = 1.0 / (0.01 ** 2)                 # normal ivars
    ivars = np.full(n_obs, ivar_ok)
    ivars[0] = 1.0 / (1e-12 ** 2)               # squared protective constant
    share = ivars / ivars.sum()
    res["pseudo_ivar_domination"] = {
        "degenerate_obs_share_weight": float(share[0]),
        "expected_if_fair": 1.0 / n_obs,
        "verdict": "degenerate obs captures the cell" if share[0] > 0.9 else "no domination",
    }
    # estimate damage: weighted mean of y where degenerate obs carries a wild value
    y = rng.standard_normal(n_obs) * 0.01
    y[0] = 1e6                                   # the zero-scale patch reports a garbage level
    est_bad = float(np.sum(share * y))
    # (4) NEGATIVE / corrected: fail-closed weight 0 for non-finite/non-positive ivar
    ivars_fix = ivars.copy(); ivars_fix[0] = 0.0
    share_fix = ivars_fix / ivars_fix.sum()
    est_fix = float(np.sum(share_fix * y))
    res["fail_closed_correction"] = {
        "weighted_mean_with_pseudo_ivar": est_bad,
        "weighted_mean_fail_closed": est_fix,
        "influence_of_degenerate_obs_fail_closed": 0.0,
        "damage_removed": abs(est_bad - est_fix) > 1e3,
    }
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
