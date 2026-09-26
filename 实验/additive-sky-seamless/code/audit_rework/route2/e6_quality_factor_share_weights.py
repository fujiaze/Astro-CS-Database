#!/usr/bin/env python3
"""E6: two-level weights -- quality_factor baseline 0.5 vs down-weight 0.1
(05_正向规格.md 判据04; docs/science/PHASE2_UPM.md §5 V19R3:
 w_UPM = qf * reliability * ivar; solver consumes share w_cell).

Claims audited:
  (A) qf baseline 0.5 with NO flags: every observation carries the same qf => any
      common baseline value cancels exactly from the normal equations => the baseline
      value is a normalization CONVENTION, not a science quantity (exemption with
      proof); the science content lives in the per-observation RATIO (0.1 vs 0.5).
  (B) share-weight solution vs absolute-weight solution on an OVERdetermined system
      (24 obs / 8 params): identical for uniform ivar (properties i/ii), measurably
      different when ivar varies across control cells (property iii: the share form
      discards cross-control precision => swapping them changes the estimator).
NEGATIVE (truth-no-effect => metric zero): uniform qf change 0.5 -> 1.0 leaves the
solution bit-identical (max|dtheta| == 0.0).
Chain position: upstream P2/P4 supply control_ivar; downstream integration consumes
stacked variance 1/sum(w) -- computed on the same toy problem.
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e6_quality_factor_share_weights.py
"""
import json, os
import numpy as np

SEED = 20250926
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e6_quality_factor_share_weights.json")

def build(n_cells=4, n_frames=2, reps=3, seed=SEED):
    rng = np.random.default_rng(seed)
    n_par = n_cells + n_frames - 1
    rows, y, meta = [], [], []
    for k in range(n_frames):
        for i in range(n_cells):
            for _ in range(reps):
                r = np.zeros(n_par); r[i] = 1.0
                if k > 0: r[n_cells + k - 1] = 1.0
                rows.append(r)
                y.append(1.0 * (k > 0) + 0.1 * rng.standard_normal())
                meta.append((k, i))
    return np.array(rows), np.array(y), meta, n_par

def solve_wls(X, y, w):
    return np.linalg.pinv((X.T * w) @ X) @ (X.T @ (w * y))

def main():
    res = {"seed": SEED, "results": {}}
    X, y, meta, n_par = build()
    n_obs = len(y)
    thA = solve_wls(X, y, 0.5 * np.ones(n_obs))
    thB = solve_wls(X, y, 1.0 * np.ones(n_obs))
    res["results"]["qf_baseline_cancellation"] = {
        "max_abs_dtheta": float(np.max(np.abs(thA - thB))),
        "verdict": "convention (baseline value cancels exactly)" if np.max(np.abs(thA - thB)) == 0.0 else "DIFFERS",
    }
    qf_c = np.ones(n_obs)
    idx = 3
    qf_c[idx] = 0.1 / 0.5            # flagged obs down-weighted by factor 0.2
    thC = solve_wls(X, y, qf_c)
    res["results"]["qf_ratio_0.1_vs_0.5"] = {
        "flagged_obs": meta[idx],
        "max_abs_dtheta_vs_baseline": float(np.max(np.abs(thC - thB))),
        "verdict": "science content = per-observation ratio (estimate shifts)",
    }
    rng = np.random.default_rng(SEED + 7)
    ivar_var = 10.0 ** rng.uniform(-1, 1, n_obs)
    th_abs = solve_wls(X, y, ivar_var)
    k_i = np.array([m[1] for m in meta])
    w_share = np.zeros(n_obs)
    for i in np.unique(k_i):
        m = k_i == i
        w_share[m] = ivar_var[m] / ivar_var[m].sum()
    th_share = solve_wls(X, y, w_share)
    ivar_u = np.ones(n_obs)
    th_abs_u = solve_wls(X, y, ivar_u)
    w_share_u = np.zeros(n_obs)
    for i in np.unique(k_i):
        m = k_i == i
        w_share_u[m] = ivar_u[m] / ivar_u[m].sum()
    th_share_u = solve_wls(X, y, w_share_u)
    res["results"]["share_vs_absolute"] = {
        "n_obs": n_obs, "n_params": n_par,
        "uniform_ivar_max_abs_dtheta": float(np.max(np.abs(th_abs_u - th_share_u))),
        "varying_ivar_max_abs_dtheta": float(np.max(np.abs(th_abs - th_share))),
        "varying_ivar_max_rel_dtheta": float(np.max(np.abs(th_abs - th_share) / np.maximum(np.abs(th_abs), 1e-12))),
    }
    w = 0.5 * ivar_var
    res["results"]["chain_downstream"] = {
        "upstream": "control_ivar = 1/(k_corr*pi/2*sigma_bg^2/N_retained)  (E1)",
        "stacked_variance_from_weights": float(1.0 / w.sum()),
        "note": "P5 solver consumes w = qf*reliability*ivar; integration consumes 1/sum(w)",
    }
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
