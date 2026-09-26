#!/usr/bin/env python3
"""E5: Huber threshold delta = 1.345 (95% asymptotic efficiency at Gaussian).

docs/science/PHASE2_UPM.md §7 (Huber symmetry invariant), §14a literature chain:
  Huber 1964 (DOI 10.1214/aoms/1177703732); Holland & Welsch 1977
  (DOI 10.1080/03610927708827533, IRLS + delta table).

Tests:
  (a) THEORY leg closed form: eff(delta) = (2Phi(d)-1)^2 / E[psi^2],
      E[psi^2] = 2Phi(d)-1-2d*phi(d)+2d^2*(1-Phi(d)); solve eff=0.95 => delta;
      check delta(0.95) ~= 1.345;
  (b) MC: n-sample Huber M-estimator of location with known sigma via IRLS;
      asymptotic variance = n*Var(T_hat); efficiency at Gaussian ~= 0.95;
      at 10%-contaminated normal (5 sigma outliers) efficiency vs mean >> 1;
  (c) sigma_floor domination (defect D-04 face): sigma_eff = max(sigma, floor) with
      floor = 10*sigma => effective clipping at 13.45 true sigma => no down-weighting
      => contaminated-case efficiency collapses to the mean's;
  (d) NEGATIVE: pure Gaussian, no outliers => bias difference between Huber and mean
      -> 0 (truth-no-effect => metric zero, symmetric distribution).
Standalone: pure python3 + numpy (math.erf only). SEED fixed.
Run: python3 e5_huber_delta_1345.py
"""
import json, math, os
import numpy as np

SEED = 20250926
DELTA = 1.345
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e5_huber_delta_1345.json")

def Phi(x): return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))
def phi(x): return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)

def eff_analytic(d):
    A = 2.0 * Phi(d) - 1.0
    B = A - 2.0 * d * phi(d) + 2.0 * d * d * (1.0 - Phi(d))
    return A * A / B

def solve_delta(target, lo=0.5, hi=3.0):
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        if eff_analytic(mid) < target: lo = mid
        else: hi = mid
    return 0.5 * (lo + hi)

def mest(x, delta, sigma=1.0, iters=60):
    T = np.median(x, axis=1)
    for _ in range(iters):
        z = (x - T[:, None]) / sigma
        psi = np.clip(z, -delta, delta)
        psip = (np.abs(z) <= delta).astype(float)
        T = T + sigma * psi.sum(axis=1) / np.maximum(psip.sum(axis=1), 1e-9)
    return T

def mc_asymvar(rng, R, n, delta, sigma_of=1.0, contaminant=0.0, out_sigma=5.0):
    x = rng.standard_normal((R, n))
    if contaminant > 0:
        mask = rng.random((R, n)) < contaminant
        x[mask] = rng.standard_normal(int(mask.sum())) * out_sigma
    T = mest(x, delta, sigma_of)
    return float(n * np.var(T))

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "delta": DELTA}
    d95 = solve_delta(0.95)
    res["analytic"] = {"delta_at_95pct_eff": d95,
                       "eff_at_1.345": eff_analytic(1.345),
                       "doc_claim": 1.345,
                       "abs_err": abs(d95 - 1.345)}
    R, n = 20_000, 50
    v_gauss = mc_asymvar(rng, R, n, DELTA)
    v_cont = mc_asymvar(rng, R, n, DELTA, contaminant=0.10)
    v_mean_cont = 1.0 + 0.10 * 25.0
    v_floor = mc_asymvar(rng, R, n, DELTA, sigma_of=10.0, contaminant=0.10)
    res["mc"] = {
        "R": R, "n_per_est": n,
        "asym_var_gaussian": v_gauss,
        "efficiency_gaussian": 1.0 / v_gauss,
        "asym_var_contaminated": v_cont,
        "efficiency_contaminated_vs_mean": v_mean_cont / v_cont,
        "asym_var_contaminated_floor_10x": v_floor,
        "efficiency_contaminated_floor_10x": v_mean_cont / v_floor,
    }
    x = rng.standard_normal((R, n))
    T = mest(x, DELTA)
    res["negative"] = {"mean_bias": float(np.mean(x)),
                       "huber_bias": float(np.mean(T)),
                       "bias_diff_abs": abs(float(np.mean(T)) - float(np.mean(x))),
                       "note": "no-effect metric = |bias difference|; expected ~0 (1/sqrt(R*n))"}
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
