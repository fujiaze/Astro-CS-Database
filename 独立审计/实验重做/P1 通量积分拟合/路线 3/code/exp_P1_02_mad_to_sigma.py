#!/usr/bin/env python3
"""
exp_P1_02_mad_to_sigma.py — MAD→σ换算系数验证

假说：MAD→σ换算系数 0.6744897501960817 是高斯分布的理论常数，对应 Φ⁻¹(3/4)

实验设计:
从标准正态分布 N(0,1) 抽取大样本（N=10^6），计算样本 MAD/σ比值
"""

import numpy as np
import json
import sys
from pathlib import Path

# === 固定 seed ===
SEED = 42
np.random.seed(SEED)

# === 理论常数 ===
MAD_TO_SIGMA_THEORY = 0.6744897501960817  # Φ⁻¹(0.75)

def mad_to_sigma_test(N_samples=10**6):
    """Test MAD/sigma ratio for standard normal distribution"""
    
    # Generate large sample from N(0, 1)
    data = np.random.randn(N_samples)
    
    # Calculate statistics
    sample_mean = np.mean(data)
    sample_std = np.std(data)
    sample_mad = np.median(np.abs(data - np.median(data)))
    
    # Ratio
    ratio = sample_mad / sample_std if sample_std > 1e-10 else 0
    
    # Relative error vs theory
    rel_error = abs(ratio - MAD_TO_SIGMA_THEORY) / MAD_TO_SIGMA_THEORY
    
    return {
        "sample_mean": float(sample_mean),
        "sample_std": float(sample_std),
        "sample_mad": float(sample_mad),
        "ratio": float(ratio),
        "theory": float(MAD_TO_SIGMA_THEORY),
        "rel_error": float(rel_error) * 100  # percentage
    }

def main():
    print("="*60)
    print("P1 通量积分拟合 · exp_P1_02：MAD→σ换算系数验证")
    print("="*60)
    print()
    print(f"理论值：Φ⁻¹(0.75) = {MAD_TO_SIGMA_THEORY:.16f}")
    print()
    
    print("实验设置:")
    print(f"  样本数：{10**6:,} (标准正态分布 N(0,1))")
    print()
    
    results = mad_to_sigma_test(N_samples=10**6)
    
    print(f"观测值:")
    print(f"  样本均值：{results['sample_mean']:.6f} (理论：1.0)")
    print(f"  样本标准差：{results['sample_std']:.6f} (理论：1.0)")
    print(f"  样本 MAD: {results['sample_mad']:.6f}")
    print(f"  MAD/σ比值：{results['ratio']:.6f}")
    print(f"  理论比值：{results['theory']:.6f}")
    print(f"  相对误差：{results['rel_error']:.3f}%")
    print()
    
    status = "✅ PASS" if results['rel_error'] < 0.1 else "❌ FAIL"
    print(f"判据：相对误差 < 0.1% → {status}")
    print()
    
    # Save results
    output_dir = Path(__file__).parent / ".." / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "exp_P1_02_mad_to_sigma.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"结果已保存至：{output_file}")
    print()
    
    all_pass = (
        abs(results['sample_mean']) < 0.01 and  # ≈0
        abs(results['sample_std'] - 1.0) < 0.01 and  # ≈1
        results['rel_error'] < 0.1  # <0.1%
    )
    
    if all_pass:
        print("总体结论：✅ PASS - MAD→σ换算符合解析闭式解")
        return 0
    else:
        print("总体结论：❌ FAIL - 部分指标未通过")
        return 1

if __name__ == "__main__":
    sys.exit(main())
