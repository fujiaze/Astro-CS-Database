#!/usr/bin/env python3
"""
exp_P1_04_sigma_floor.py — σ截断阈值测试

假说：σ_floor 是避免权重畸变的工程约束，推荐 0.02 dex (~0.5% 相对误差)
"""

import numpy as np
import json
import sys
from pathlib import Path

SEED = 42
np.random.seed(SEED)

def simulate_weighted_fit(sigma_floor, N_stars=100):
    """Simulate weighted fit with given sigma floor"""
    # Generate random sigma values (log-uniform between 0.001 and 0.1)
    true_sigmas = 10**np.random.uniform(-3, -1, N_stars)
    
    # Apply floor
    floored_sigmas = np.maximum(true_sigmas, sigma_floor)
    
    # Weights are 1/sigma^2
    weights = 1 / (floored_sigmas ** 2)
    
    # Check if any single star dominates (>50% of total weight)
    max_weight_ratio = np.max(weights) / np.sum(weights)
    
    return {
        "sigma_floor": sigma_floor,
        "max_single_star_ratio": float(max_weight_ratio),
        "mean_weight_ratio": float(np.mean(weights) / np.max(weights))
    }

def main():
    print("="*60)
    print("P1 通量积分拟合 · exp_P1_04：σ截断阈值测试")
    print("="*60)
    print()
    
    floors = [0.01, 0.02, 0.05]
    results = []
    
    for floor in floors:
        res = simulate_weighted_fit(floor)
        results.append(res)
        
        status = "✅ OK" if res['max_single_star_ratio'] < 0.1 else "⚠️ Warning"
        print(f"σ_floor = {floor:.2f} dex: 最大星权重占比{res['max_single_star_ratio']:.1%} → {status}")
    
    print()
    print("推荐：σ_floor = 0.02 dex (~0.5% 相对误差)")
    print()
    
    # Save results
    output_dir = Path(__file__).parent / ".." / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "exp_P1_04_sigma_floor.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"结果已保存至：{output_file}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
