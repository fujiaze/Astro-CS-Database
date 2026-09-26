#!/usr/bin/env python3
"""EXP-P4-01: Inverse-variance weight optimality for the P4 dense-SNR weight chain.

Audit item I1 (05 spec P-ALG-10 / weight exponent 2):
    w(x,y) = SNR(x,y)^2 / F_ref^2  ==  1/sigma_F(x,y)^2
Claims tested (three legs):
  T (theory)   : Cauchy-Schwarz -- Var(mu_hat) >= 1/sum(1/v_i), equality iff w_i ~ 1/v_i.
  L (lit)      : Zackay & Ofek 2017 (arXiv:1512.06872 / 1512.06879); Horne 1986 PASP 98,609;
                 Naylor 1998 MNRAS 296,339. (Verified in refs.md; not imported here.)
  E (experiment) : Monte-Carlo comparison of stacking weight schemes.
Negative control (non-degeneracy discipline, AGENTS.md S5):
    When all true variances are equal (no heterogeneity effect), EVERY scheme is optimal,
    the optimality metric must collapse to zero. A metric that stays >0 there is a tautology.

Pure python + numpy. No repository imports. Seed hardcoded.
Run:  python3 exp_p4_01_weight_optimality.py
Out:  ../results/exp_p4_01_weight_optimality.json
"""
import json, time
import numpy as np

SEED = 20260926
OUT = "../results/exp_p4_01_weight_optimality.json"


def emp_var(w_fun, v, n_trials, rng, mu_true=100.0):
    """Empirical variance of the weighted mean under scheme w_fun(v) -> weights."""
    est = np.empty(n_trials)
    for t in range(n_trials):
        x = rng.normal(mu_true, np.sqrt(v))
        w = w_fun(v)
        est[t] = np.sum(w * x) / np.sum(w)
    return est.var()


def main():
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "arms": {}}

    # ---------- Leg T: algebraic identity check of the weight-conversion formula ----------
    sig = 10.0 ** rng.uniform(-0.5, 2.5, size=200000)     # sigma_F, ADU
    Fref = 10.0 ** rng.uniform(2.0, 4.0, size=200000)     # F_ref, ADU
    snr = Fref / sig
    lhs = snr ** 2 / Fref ** 2
    rhs = 1.0 / sig ** 2
    res["identity_max_rel_dev"] = float(np.max(np.abs(lhs / rhs - 1.0)))
    # cross-frame combination identity: SNR_comb^2 = sum_k SNR_k^2  <=>  Var_comb = 1/sum(1/v_k)
    n = 8
    vk = 10.0 ** rng.uniform(0.0, 4.0, size=n)
    Fref_c = 100.0                      # one frame, one reference flux (pairing rule)
    snrk = Fref_c / np.sqrt(vk)         # SNR_k = F_ref/sigma_k, same F_ref
    snr_comb_sq = np.sum(snrk ** 2)
    res["snr_combination_rel_dev"] = float(abs(snr_comb_sq / (Fref_c ** 2 * np.sum(1.0 / vk)) - 1.0))

    # ---------- Leg E: heterogeneous-variance stacking MC ----------
    n_trials = 40000
    schemes = {
        "inverse_variance": lambda v: 1.0 / v,
        "equal":            lambda v: np.ones_like(v),
        "w_propto_snr":     lambda v: (100.0 / np.sqrt(v)),   # SNR with F_ref = 100 ADU
        "w_propto_snr_sq":  lambda v: (100.0 / np.sqrt(v)) ** 2,
    }
    res["arms"]["heterogeneous"] = {"n_trials": n_trials, "n_obs": n, "schemes": {}}
    mu_true = 100.0
    for name, fun in schemes.items():
        e = emp_var(fun, vk, n_trials, np.random.default_rng(SEED + 1), mu_true)
        var_opt = 1.0 / np.sum(1.0 / vk)
        res["arms"]["heterogeneous"]["schemes"][name] = {
            "emp_var": float(e), "E_vs_opt": float(e / var_opt - 1.0)}

    # ---------- Negative control: equal true variances => every scheme is optimal ----------
    v_flat = np.full(n, 25.0)
    res["arms"]["flat_negative_control"] = {"n_trials": n_trials, "n_obs": n, "schemes": {}}
    for name, fun in schemes.items():
        e = emp_var(fun, v_flat, n_trials, np.random.default_rng(SEED + 2), mu_true)
        var_opt = 1.0 / np.sum(1.0 / v_flat)
        res["arms"]["flat_negative_control"]["schemes"][name] = {
            "emp_var": float(e), "E_vs_opt": float(e / var_opt - 1.0)}
    # The optimality gap must collapse to zero when the true effect (heterogeneity) is absent.
    gaps = [abs(res["arms"]["flat_negative_control"]["schemes"][s]["E_vs_opt"]) for s in schemes]
    res["flat_control_max_abs_E"] = float(max(gaps))

    # ---------- Gate ----------
    het = res["arms"]["heterogeneous"]["schemes"]
    res["gates"] = {
        "identity_machine_precision": res["identity_max_rel_dev"] < 1e-12,
        "invvar_optimal_heterogeneous":
            het["inverse_variance"]["E_vs_opt"] < 0.05
            and het["inverse_variance"]["E_vs_opt"] <= het["equal"]["E_vs_opt"]
            and het["inverse_variance"]["E_vs_opt"] <= het["w_propto_snr"]["E_vs_opt"],
        "equal_and_snr_linearly_suboptimal":
            het["equal"]["E_vs_opt"] > 0.5 and het["w_propto_snr"]["E_vs_opt"] > 0.1,
        "negative_control_collapses": res["flat_control_max_abs_E"] < 0.05,
    }
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
