#!/usr/bin/env python3
"""E1: control_variance = k_corr * (pi/2) * sigma_bg^2 / N_retained  -- median-variance leg.

Tests (docs/science/PHASE2_UPM.md §5):
  (a) Var(median) ~ pi*sigma^2/(2N) for Gaussian, asymptotic N>=65;
  (b) non-Gaussian ratios: uniform ~1.91x, Laplace ~0.335x of the Gaussian value;
  (c) N=5 finite-N correction: asymptotic formula *underestimates* true Var(median) (doc: 8.5%);
  (d) equicorrelation: measured k_corr_eff = Var(median)_corr / Var(median)_iid; N_eff = N/k_corr;
  (e) NEGATIVE (truth-no-effect => metric zero): rho=0 => k_corr_eff - 1 == 0 within MC CI;
      sigma=0 => Var(median) == 0 exactly (S=0 => no scale information, ivar must be 0);
  (f) chain interface: ivar ratio 1:4 propagates to weight ratio 1:4 (UPMW-002 style).
Standalone: pure python3 + numpy. SEED fixed below. No repo imports.
Run: python3 e1_control_variance_median.py
"""
import json, math, os
import numpy as np

SEED = 20250926
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e1_control_variance_median.json")

def sample(dist, R, N, rng):
    if dist == "gaussian":
        return rng.standard_normal((R, N))
    if dist == "uniform":  # sigma = 1
        return rng.uniform(-math.sqrt(3.0), math.sqrt(3.0), (R, N))
    if dist == "laplace":  # sigma = 1 (scale b = 1/sqrt(2))
        u = rng.uniform(-0.5, 0.5, (R, N))
        return -np.sign(u) * np.log1p(-2.0 * np.abs(u)) / math.sqrt(2.0)
    raise ValueError(dist)

def mc_median_var(dist, N, rng, target=4_000_000):
    R = int(min(400_000, max(20_000, target // N)))
    x = sample(dist, R, N, rng)
    med = np.median(x, axis=1)
    return float(np.var(med)), R

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "median_var": [], "kcorr_equicorr": [], "negative": {}, "chain": {}}
    pi2 = math.pi / 2.0
    for dist in ("gaussian", "uniform", "laplace"):
        for N in (5, 20, 65, 289):
            v, R = mc_median_var(dist, N, rng)
            res["median_var"].append({"dist": dist, "N": N, "R": R, "var": v,
                                      "ratio_over_sigma2_over_N": v * N})
    g_asym = pi2
    ratios = {d: None for d in ("uniform", "laplace")}
    for row in res["median_var"]:
        if row["dist"] in ratios and row["N"] == 289:
            ratios[row["dist"]] = row["ratio_over_sigma2_over_N"] / g_asym
    v5 = next(r["ratio_over_sigma2_over_N"] for r in res["median_var"]
              if r["dist"] == "gaussian" and r["N"] == 5)
    res["derived"] = {
        "uniform_over_gaussian_asym": ratios["uniform"],
        "doc_claim_uniform": 1.91,
        "laplace_over_gaussian_asym": ratios["laplace"],
        "doc_claim_laplace": 0.335,
        "gaussian_N5_over_asym": v5 / g_asym,
        "doc_claim_underestimate_pct_at_N5": 8.5,
        "implied_finiteN_inflation_N5": v5 / g_asym - 1.0,
    }
    # (d) equicorrelated Gaussian: z_i = sqrt(rho)c + sqrt(1-rho)e_i
    N = 65
    for rho in (0.0, 0.05, 0.2):
        R = 400_000
        c = rng.standard_normal((R, 1)); e = rng.standard_normal((R, N))
        z = math.sqrt(rho) * c + math.sqrt(1 - rho) * e
        vm = float(np.var(np.median(z, axis=1)))
        vmean = float(np.var(z.mean(axis=1)))
        k_med = vm / (pi2 / N)          # Var(median)_corr / (pi/2 / N)
        k_iid = mc_median_var("gaussian", N, np.random.default_rng(SEED + 1))[0] * N / pi2
        res["kcorr_equicorr"].append({
            "rho": rho, "N": N, "var_median": vm,
            "k_eff_median_vs_formula": k_med,
            "k_eff_median_vs_iid_mc": k_med / k_iid,
            "mean_inflation_exact_1+(N-1)rho": 1.0 + (N - 1) * rho,
            "var_mean_over_sigma2_over_N": vmean * N,
        })
    # end-to-end leg: sigma_bg estimated by 1.4826*MAD from the same patch (sampler
    # practice). Reconciles two directions: the pure formula OVERestimates Var(median)
    # at N=5, while MAD small-N downward bias can make the published control_variance
    # UNDERestimate the true variance (doc claims 8.5% at N=5).
    res["mad_end_to_end"] = []
    for N in (5, 20, 65):
        R = int(min(400_000, max(20_000, 4_000_000 // N)))
        x = rng.standard_normal((R, N))
        med = np.median(x, axis=1)
        mad = 1.4826 * np.median(np.abs(x - med[:, None]), axis=1)
        cvar = pi2 * mad ** 2 / N
        true_var = float(np.var(med))
        res["mad_end_to_end"].append({
            "N": N, "true_var_median_over_sigma2_over_N": true_var * N,
            "mean_published_cvar_over_true": float(np.mean(cvar) / true_var),
            "pure_formula_over_true": pi2 / (true_var * N),
        })
    r0 = res["kcorr_equicorr"][0]
    res["negative"]["rho0_kcorr_minus1"] = r0["k_eff_median_vs_iid_mc"] - 1.0
    x0 = np.zeros((1000, 65))
    res["negative"]["sigma0_median_var"] = float(np.var(np.median(x0, axis=1)))
    # (f) chain: control_variance ratio 1:4 (via N_retained 65 vs 16.25 -> use sigma ratio 2x)
    s1, s2 = 1.0, 2.0
    var1 = 1.4 * pi2 * s1 ** 2 / 65
    var2 = 1.4 * pi2 * s2 ** 2 / 65
    w1, w2 = 1.0 / var1, 1.0 / var2
    res["chain"] = {"ivar1": w1, "ivar2": w2, "weight_ratio_w1_over_w2": w1 / w2,
                    "expected": 1.0 / 4.0}
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps({"derived": res["derived"], "kcorr": res["kcorr_equicorr"],
                      "negative": res["negative"], "chain": res["chain"]}, indent=1))

if __name__ == "__main__":
    main()