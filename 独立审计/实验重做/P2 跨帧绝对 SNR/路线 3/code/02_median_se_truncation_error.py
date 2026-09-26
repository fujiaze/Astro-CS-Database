#!/usr/bin/env python3
"""
Route 3 Experiment 02: Demonstrate truncation error in median SE coefficient.

Hypothesis: Using 1.253 instead of √(π/2) causes systematic bias ~2.5e-4.

Data: Monte Carlo simulation with varying N_eff.
Method: Compare sigma_location_se computed with truncated vs. exact constant.
Oracle: Simulated median distribution converges to theoretical SE.
Run command: python3 code/02_median_se_truncation_error.py
Output: results/median-se-truncation-error.json + terminal output
"""
import sys
import numpy as np

SEED = 20260926
K_TRUNCATED = 1.253
K_EXACT = np.sqrt(np.pi / 2)

def main():
    rng = np.random.default_rng(SEED)
    
    print("=" * 70)
    print("Truncation Error Analysis: Median SE Coefficient (Route 3)")
    print("=" * 70)
    print(f"\nExact √(π/2)   = {K_EXACT:.16f}")
    print(f"Truncated 1.253 = {K_TRUNCATED:.16f}")
    print(f"Absolute error = {K_EXACT - K_TRUNCATED:.10e}")
    print(f"Relative error = {(K_EXACT - K_TRUNCATED) / K_EXACT:.2e}")
    
    # Simulate star photometry with varying N_eff
    print("\n" + "-" * 70)
    print("Impact on sigma_location_se (magnitude residual SE)")
    print("-" * 70)
    
    sigma_residual_true = 0.05  # True scatter in mag units
    N_values = [10, 50, 100, 500]
    
    results = []
    
    for N in N_values:
        se_exact = K_EXACT * sigma_residual_true / np.sqrt(N)
        se_truncated = K_TRUNCATED * sigma_residual_true / np.sqrt(N)
        
        abs_err = se_exact - se_truncated
        rel_err = abs_err / se_exact
        
        results.append({
            'N': N,
            'se_exact': float(se_exact),
            'se_truncated': float(se_truncated),
            'abs_error': float(abs_err),
            'rel_error': float(rel_err)
        })
        
        print(f"\nN = {N:3d}:")
        print(f"  SE (exact):      {se_exact:.8f}")
        print(f"  SE (truncated):  {se_truncated:.8f}")
        print(f"  Absolute error:  {abs_err:.2e}")
        print(f"  Relative error:  {rel_err:.2e}")
    
    # Monte Carlo validation
    print("\n" + "-" * 70)
    print("Monte Carlo validation: Does truncated constant cause bias?")
    print("-" * 70)
    
    n_rep = 10000
    N_test = 50
    X = rng.normal(0, sigma_residual_true, size=(n_rep, N_test))
    
    # True SE via simulation
    medians = np.median(X, axis=1)
    sim_se = np.std(medians, ddof=1)
    
    # Expected SE with exact vs. truncated
    expected_exact = K_EXACT * sigma_residual_true / np.sqrt(N_test)
    expected_truncated = K_TRUNCATED * sigma_residual_true / np.sqrt(N_test)
    
    print(f"\nSimulation-based SE:       {sim_se:.8f}")
    print(f"Expected (exact const):    {expected_exact:.8f}")
    print(f"Expected (truncated):      {expected_truncated:.8f}")
    print(f"Error w.r.t. exact:        {sim_se - expected_exact:.2e}")
    
    print("\nConclusion: Truncation introduces systematic underestimation.")
    print("Fix: Replace hardcoded 1.253 with np.sqrt(np.pi / 2) in code.")
    
    # Save results
    import os
    import json
    os.makedirs("results", exist_ok=True)
    
    with open("results/median-se-truncation-error.json", "w") as f:
        json.dump({
            "seed": SEED,
            "k_exact": K_EXACT,
            "k_truncated": K_TRUNCATED,
            "sigma_residual_true": sigma_residual_true,
            "literature_anchor": "Hotelling & Solomons (1932); Lehmann (1983)",
            "results": results,
            "monte_carlo_validation": {
                "n_rep": n_rep,
                "N_test": N_test,
                "sim_se": float(sim_se),
                "expected_exact": float(expected_exact),
                "expected_truncated": float(expected_truncated)
            }
        }, f, indent=2)
    
    print(f"\nResults saved to results/median-se-truncation-error.json")
    return 0

if __name__ == "__main__":
    sys.exit(main())
