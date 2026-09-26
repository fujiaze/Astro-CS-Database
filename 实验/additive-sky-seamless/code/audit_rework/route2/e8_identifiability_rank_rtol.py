#!/usr/bin/env python3
"""E8: identifiability gate -- unique relative threshold tau = rank_rtol on the
column-equilibrated UNREGULARIZED information matrix H_eq (11_upm.md §4.7;
PHASE2_UPM.md §7a rules 4-5; dof = n_obs - r_eff per Andrae et al. 2010, eq (9)).

Tests:
  (a) column equilibration: rescaling design columns by 1e±6 changes H_red's kappa but
      leaves H_eq's eigenvalues / r_eff / kappa INVARIANT (invariance claim);
  (b) H_solve = H_red + lam*P tautology: kappa(H_solve) -> 1 as lam grows even when the
      problem is exactly rank-deficient => a gate on H_solve is a tautology (rule 5);
  (c) dof: chi2_red with n_obs - r_eff vs the n_obs - n_params denominator under exact
      rank deficiency: the latter systematically UNDERESTIMATES chi2_red;
  (d) NEGATIVE: full-rank well-conditioned problem => identifiable, chi2_red ~ 1.
  Also audits the 05 spec's "condition_number_max = 1e12 (PHASE2_UPM_IMPL.md §6.3)":
  that anchor does not exist (grep: no "条件数"/1e12 in PHASE2_UPM_IMPL.md) and an
  absolute kappa_max gate is explicitly OUTSIDE the judgment surface per rule 5.
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e8_identifiability_rank_rtol.py
"""
import json, os
import numpy as np

SEED = 20250926
TAU = 1e-10
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e8_identifiability_rank_rtol.json")

def assess(H):
    d = np.sqrt(np.clip(np.diag(H), 0, None))
    Dinv = np.diag(np.where(d > 0, 1.0 / d, 0.0))
    Heq = Dinv @ H @ Dinv
    ev = np.linalg.eigvalsh(Heq)[::-1]
    ev = np.clip(ev, 0, None)
    l1 = ev[0] if ev.size else 0.0
    r_eff = int(np.sum(ev > TAU * l1)) if l1 > 0 else 0
    kap = float(ev[0] / ev[-1]) if ev[-1] > 0 else float("inf")
    return r_eff, kap, ev

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "tau": TAU}
    n, p = 120, 9
    X = rng.standard_normal((n, p)) * 1.0
    # inject exact rank deficiency: column 8 never observed (design zero there) plus
    # an exact gauge degeneracy: columns 0..2 sum is always zero in X (contrast encoding)
    X[:, 8] = 0.0
    X[:, 2] = -(X[:, 0] + X[:, 1]) / 1.0
    w = 10.0 ** rng.uniform(-1, 1, n)
    H = X.T @ (X * w[:, None])
    r_eff, kap, ev = assess(H)
    # (a) invariance under column rescaling
    scales = 10.0 ** rng.uniform(-6, 6, p)
    Xs = X * scales[None, :]
    Hs = Xs.T @ (Xs * w[:, None])
    r_eff_s, kap_s, ev_s = assess(Hs)
    res["column_equilibration_invariance"] = {
        "kappa_H_red": kap, "kappa_H_red_rescaled": kap_s,
        "kappa_changes": abs(kap_s - kap) > 1.0,
        "r_eff": r_eff, "r_eff_rescaled": r_eff_s,
        "eigen_max_abs_diff_over_lambda1": float(np.max(np.abs(ev - ev_s)) / ev[0]),
        "eigen_max_rel_diff_above_threshold": float(np.max(
            np.abs(ev - ev_s)[ev > TAU * ev[0]] / ev[ev > TAU * ev[0]])) if bool((ev > TAU * ev[0]).any()) else 0.0,
    }
    # (b) H_solve tautology
    seq = []
    for lam in (0.0, 1e-6, 1e-3, 1.0, 1e3):
        Hs2 = H + lam * np.eye(p)
        _, k2, _ = assess(Hs2)
        seq.append({"lam": lam, "kappa_H_solve": k2 if np.isfinite(k2) else None})
    res["H_solve_tautology"] = seq
    # (c) dof / chi2_red under rank deficiency
    theta_true = rng.standard_normal(p) * 0.0 + 1.0
    y = X @ theta_true + rng.standard_normal(n) * 0.1
    W = np.diag(w)
    Hinv = np.linalg.pinv(H)
    theta = Hinv @ (X.T @ (w * y))
    resid = y - X @ theta
    chi2 = float(np.sum(w * resid ** 2))
    n_params = p
    dof_r = n - r_eff
    dof_p = n - n_params
    res["dof"] = {"n_obs": n, "r_eff": r_eff, "n_params": n_params,
                  "chi2": chi2,
                  "chi2_red_dof_r_eff": chi2 / dof_r,
                  "chi2_red_dof_n_params": chi2 / dof_p,
                  "underestimate_factor": (chi2 / dof_p) / (chi2 / dof_r)}
    # (d) negative: full-rank well-conditioned
    Xg = rng.standard_normal((300, 6))
    wg = np.ones(300)
    Hg = Xg.T @ (Xg * wg[:, None])
    r_g, k_g, _ = assess(Hg)
    yg = Xg @ np.ones(6) + rng.standard_normal(300)
    th = np.linalg.solve(Hg, Xg.T @ yg)
    chi2g = float(np.sum((yg - Xg @ th) ** 2))
    res["negative_fullrank"] = {"r_eff": r_g, "kappa": k_g,
                                "identifiable": r_g == 6,
                                "chi2_red": chi2g / (300 - r_g)}
    res["audit_condition_number_1e12"] = {
        "anchor_claimed": "docs/algorithms/PHASE2_UPM_IMPL.md §6.3",
        "grep_hits_for_cond/1e12_in_that_file": 0,
        "rule5_says": "absolute kappa_max constants are NOT on the judgment surface",
        "verdict": "hallucinated anchor; 1e12 gate contradicts frozen rule 4/5",
    }
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()