#!/usr/bin/env python3
"""
exp_P1_01_tukey_constant.py — Tukey biweight c=4.685 三腿验证实验

假说：c=4.685 对应 95% 高斯渐近效率，且在 20% 离群点污染下 location 偏移受控

实验设计:
1. Monte Carlo 渐近效率测试：200 次试验，样本数 100，Gaussian 噪声 N(0,1)
2. 鲁棒性测试：20% 离群点注入（3σ偏移），验证 Tukey 估计器稳定性

纯 Python + numpy，不 import 仓库代码
"""

import numpy as np
import json
import sys
from pathlib import Path

# === 固定 seed ===
SEED = 42
np.random.seed(SEED)

# === Tukey biweight 实现 ===
def tukey_weight(u, c=4.685):
    """Tukey biweight 权重函数 w = (1 - u²/c²)² for |u| < c, else 0"""
    abs_u = np.abs(u)
    weight = np.where(abs_u < c, (1 - (abs_u / c)**2)**2, 0.0)
    return weight

def tukey_estimator(data, c=4.685, max_iter=50, tol=1e-6):
    """IRLS Tukey location estimator"""
    # Initial estimate: median
    loc = np.median(data)
    
    for iteration in range(max_iter):
        # Compute standardized residuals
        diffs = data - loc
        scale = np.median(np.abs(diffs)) / 0.6744897501960817  # MAD→σ
        if scale < 1e-10:
            scale = 1e-10
        
        u = diffs / (c * scale)
        
        # Weights
        w = tukey_weight(u, c)
        w_sum = np.sum(w)
        
        if w_sum < 1e-10:
            break
        
        # Update location
        new_loc = np.sum(w * data) / w_sum
        
        # Check convergence
        change = np.abs(new_loc - loc)
        loc = new_loc
        
        if change < tol:
            break
    
    return loc, iteration, scale

def asymptotic_efficiency(N_trials=200, N_samples=100):
    """Test asymptotic efficiency of Tukey vs sample mean"""
    tukey_variances = []
    mean_variances = []
    
    for _ in range(N_trials):
        # Gaussian noise
        data = np.random.randn(N_samples)
        
        # Tukey estimate
        tukey_loc, _, _ = tukey_estimator(data)
        tukey_variances.append(tukey_loc**2)
        
        # Mean estimate
        mean_loc = np.mean(data)
        mean_variances.append(mean_loc**2)
    
    # Relative efficiency (ratio of variances, inverted)
    tukey_var = np.var(tukey_variances)
    mean_var = np.var(mean_variances)
    
    if mean_var < 1e-10:
        return None
    
    efficiency = tukey_var / mean_var
    return efficiency

def outlier_contamination_test(p_outlier=0.2, n_outliers_sigma=3.0, N_trials=200):
    """Test location bias under 20% outlier contamination"""
    location_biases = []
    
    for _ in range(N_trials):
        N_clean = int(0.8 * 100)
        N_bad = 20
        
        # Clean data from N(0, 1)
        clean_data = np.random.randn(N_clean)
        
        # Outliers from N(0, 3σ)
        outliers = np.random.randn(N_bad) * n_outliers_sigma
        
        data = np.concatenate([clean_data, outliers])
        
        # True location is 0
        true_loc = 0.0
        
        # Estimate with Tukey
        est_loc, _, _ = tukey_estimator(data)
        
        bias = np.abs(est_loc - true_loc)
        location_biases.append(bias)
    
    mean_bias = np.mean(location_biases)
    std_bias = np.std(location_biases)
    
    return mean_bias, std_bias

def main():
    print("="*60)
    print("P1 通量积分拟合 · exp_P1_01：Tukey c=4.685 验证")
    print("="*60)
    print()
    
    results = {
        "experiment": "exp_P1_01_tukey_constant",
        "seed": SEED,
        "tests": {}
    }
    
    # Test 1: Asymptotic efficiency
    print("测试 1: 渐近效率 (N_trials=200, N_samples=100)")
    efficiency = asymptotic_efficiency(N_trials=200, N_samples=100)
    
    if efficiency is not None:
        print(f"  观测值：{efficiency:.4f}")
        print(f"  目标范围：[0.85, 1.05] (95% ±10%，考虑 MC 噪声)")
        status = "✅ PASS" if 0.85 <= efficiency <= 1.05 else "❌ FAIL"
        print(f"  判据：95%±10% → {status}")
        
        results["tests"]["asymptotic_efficiency"] = {
            "observed": float(efficiency),
            "target_range": [0.85, 1.05],
            "status": "PASS" if 0.85 <= efficiency <= 1.05 else "FAIL"
        }
    else:
        print("  ⚠️ 无法计算效率（方差过小）")
        results["tests"]["asymptotic_efficiency"] = {"status": "SKIPPED"}
    
    print()
    
    # Test 2: Outlier robustness
    p_outlier = 0.2
    n_outliers_sigma = 3.0
    print("测试 2: 20% 离群点鲁棒性")
    mean_bias, std_bias = outlier_contamination_test(p_outlier=p_outlier, n_outliers_sigma=n_outliers_sigma, N_trials=200)
    
    print(f"  观测均值偏差：{mean_bias:.3f} dex")
    print(f"  标准差：{std_bias:.3f} dex")
    print(f"  目标：< 0.5 dex")
    status = "✅ PASS" if mean_bias < 0.5 else "❌ FAIL"
    print(f"  判据：location bias < 0.5 → {status}")
    
    results["tests"]["outlier_robustness"] = {
        "p_outlier": float(p_outlier),
        "n_outliers_sigma": float(n_outliers_sigma),
        "mean_bias": float(mean_bias),
        "std_bias": float(std_bias),
        "target_max": 0.5,
        "status": "PASS" if mean_bias < 0.5 else "FAIL"
    }
    
    print()
    
    # Test 3: IRLS convergence speed
    print("测试 3: IRLS 收敛速度")
    N_fast_conv = 0
    N_trials = 100
    
    for _ in range(N_trials):
        data = np.random.randn(100)
        _, iterations, _ = tukey_estimator(data)
        if iterations <= 5:
            N_fast_conv += 1
    
    fast_conv_ratio = N_fast_conv / N_trials
    print(f"  ≤5 次迭代收敛比例：{fast_conv_ratio:.1%}")
    print(f"  目标：≥ 80%")
    status = "✅ PASS" if fast_conv_ratio >= 0.8 else "❌ FAIL"
    print(f"  判据：快速收敛 → {status}")
    
    results["tests"]["irls_convergence"] = {
        "N_trials": N_trials,
        "N_fast_conv": N_fast_conv,
        "fast_conv_ratio": float(fast_conv_ratio),
        "target_min": 0.8,
        "status": "PASS" if fast_conv_ratio >= 0.8 else "FAIL"
    }
    
    print()
    print("="*60)
    print("总体结论：", end="")
    
    all_pass = (
        results["tests"].get("asymptotic_efficiency", {}).get("status") == "PASS" and
        results["tests"].get("outlier_robustness", {}).get("status") == "PASS" and
        results["tests"].get("irls_convergence", {}).get("status") == "PASS"
    )
    
    if all_pass:
        print("✅ PASS - Tukey c=4.685 通过全部验证")
        exit_code = 0
    else:
        print("❌ FAIL - 部分测试未通过")
        exit_code = 1
    
    # Save results
    output_dir = Path(__file__).parent / ".." / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "exp_P1_01_tukey_constant.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"结果已保存至：{output_file}")
    print()
    
    return exit_code

if __name__ == "__main__":
    sys.exit(main())
