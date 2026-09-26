#!/usr/bin/env python3
"""EXP-P2-R1-10: diagonal-approximation covariance underestimate (P-CST-22, P-OPEN-02).

Repo context: drizzled/resampled output pixels are correlated (mean|rho| ~ 0.19,
self-measured); the diagonal special case Var = sum c_k^2 u_k understates
Var = [R C R^T]_ii. Two mutually exclusive registered values exist: 23.3% and
36.3% underestimate, and the doc notes neither is reproduced by 1 + 0.75*rho.
This experiment:
  (1) derives/verifies the closed form for a row c with equicorrelated inputs:
      Var_exact/Var_diag = 1 + rho * (sum c)^2 / sum c^2  (tap-efficiency M_eff)
  (2) scans (M_eff, rho) to show which operating points give 23.3% / 36.3%;
  (3) tests the 1 + 0.75 rho formula (gives 12.5% at rho=0.19 -> matches neither);
  (4) MC with random drizzle-like fractional weights and an AR(1) input kernel.
Negative control: rho = 0 => ratio exactly 1 (metric 0).
Seed fixed 20260926. Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
out = {"seed": SEED, "mean_abs_rho_repo": 0.19}

# (1) closed form with equicorrelated inputs
rho = 0.19
for taps, label in ((4, "bilinear_4tap"), (2, "two_tap"), (3, "three_tap")):
    c = np.full(taps, 1.0 / taps)
    Meff = (c.sum() ** 2) / (c ** 2).sum()
    ratio = 1 + rho * (Meff - 1.0)   # Var_exact = (1-rho)Sc2 + rho (Sc)^2
    out.setdefault("closed_form_equicorrelated", {})[label] = {
        "M_eff": float(Meff), "Var_exact_over_diag": float(ratio),
        "underestimate": float(1 - 1 / ratio)}

# (2) which (M_eff, rho) reproduce the two registered values
def solve_rho(und, Meff):
    # und = rho*(Meff-1)/(1+rho*(Meff-1)) => rho = und/((Meff-1)*(1-und))
    return und / ((Meff - 1.0) * (1 - und))
out["registered_value_inversion"] = {
    "u23.3_4tap_rho": float(solve_rho(0.233, 4.0)),
    "u36.3_4tap_rho": float(solve_rho(0.363, 4.0)),
    "u36.3_at_rho0.19_Meff_minus1": float(0.363 / 0.637),
    "u23.3_at_rho0.19_Meff_minus1": float(0.233 / 0.767),
    "note": "WITH THE CORRECT FORM 1 + rho*(M_eff-1): the registered 36.3% is "
            "EXACTLY the uniform 4-tap (drizzle/bilinear) underestimate at "
            "rho=0.19 (0.19*3/(1+0.19*3) = 0.3631); 23.3% at rho=0.19 needs "
            "M_eff-1 = 1.6 (M_eff = 2.6), or rho = 0.101 at 4 taps.",
}
# explicit: 4 uniform taps, rho = 0.19
r4 = 1 + 0.19 * 3.0
out["four_tap_check"] = {"ratio": float(r4), "underestimate": float(1 - 1 / r4),
                         "registered": 0.363}

# (3) the 1 + 0.75 rho formula
out["one_plus_075rho"] = {"value_at_0.19": 1 + 0.75 * 0.19,
                          "underestimate": 1 - 1 / (1 + 0.75 * 0.19),
                          "matches_registered": False,
                          "note": "12.5% at rho=0.19; matches neither 23.3% nor 36.3%"}

# (4) MC with AR(1) input covariance and random fractional-weight rows
rng = np.random.default_rng(SEED)
n_in = 16
n_mc = 4000
rows_r = rng.dirichlet(np.ones(4), size=(n_mc, 1))          # random 4-tap weights
res_und = []
ratios_all = []
for i in range(n_mc):
    c = rows_r[i, 0]
    if c.sum() <= 0:
        continue
    # AR(1) correlation with lag-1 rho chosen so mean|rho| ~ 0.19-ish
    a = 0.19
    idx = np.arange(n_in)
    C = a ** np.abs(idx[:, None] - idx[None, :])
    R = np.zeros((1, n_in)); R[0, :4] = c / c.sum()
    var_exact = float((R @ C @ R.T)[0, 0])
    var_diag = float(((R ** 2) * np.diag(C)).sum())
    ratios_all.append(var_diag / var_exact)
ratios_all = np.array(ratios_all)
out["mc_ar1_random_weights"] = {
    "mean_underestimate": float(1 - np.mean(ratios_all)),
    "median_underestimate": float(1 - np.median(ratios_all)),
    "note": "AR(1) with lag-1 rho=0.19: effective |rho| between the 4 tapped inputs "
            "is < 0.19 (adjacent lags), so the underestimate is smaller than the "
            "equicorrelated bound; the registered values require the measured "
            "mean|rho| to be interpreted between the tapped pairs.",
}
# equicorrelated MC cross-check of the closed form
Ceq = (1 - rho) * np.eye(4) + rho * np.ones((4, 4))
c = np.full(4, 0.25)
vex = float(c @ Ceq @ c); vdg = float((c ** 2 * np.diag(Ceq)).sum())
out["mc_equicorrelated_check"] = {"ratio_numeric_var_exact_over_diag": vex / vdg,
                                  "closed_form": 1 + rho * 3.0}
out["null_rho0_metric"] = float(abs((c @ np.eye(4) @ c) / ((c ** 2) @ np.ones(4)) - 1.0))

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp10_covariance_diagonal_approx.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
