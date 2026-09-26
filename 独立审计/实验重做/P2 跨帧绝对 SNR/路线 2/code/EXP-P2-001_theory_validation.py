#!/usr/bin/env python3
"""
EXP-P2-001: P-CST-03 与 P-CST-04 理论验证

**假说**: 
1. MAD→σ尺度常数 κ_MAD = 1.482602218505602 能使高斯噪声的 MAD 估计无偏
2. 高斯 FWHM↔σ因子 2.3548200450309493 = 2√(2·ln(2)) 是数学恒等式

**方法**: 纯 Python/numpy 合成高斯噪声，计算 MAD 与 FWHM，验证理论常数的准确性

**数据适用性**: 解析代数合成（必须含"真值无效应⇒度量归零"负例）

**固定 seed**: numpy.random.default_rng(42)

**CPU ≤ 5 分钟**: 预计 < 30 秒

**结果数据落盘**: results/EXP-P2-001-results.json

**复现命令**: python code/EXP-P2-001_theory_validation.py
"""

import numpy as np
from scipy import stats
import json
import math
import sys

# === 配置 ===
SEED = 42
N_SAMPLES = 100000  # 大样本确保统计显著性
SIGMA_TRUE = 5.0    # 真值 σ = 5 ADU
REPETITIONS = 5     # 多次重复验证稳定性

# === 待验证常数 ===
K_MAD_THEORY = 1.482602218505602  # P-CST-03
FWHM_GAUSS_THEORY = 2.3548200450309493  # P-CST-04

# === 预期值 ===
Q75 = stats.norm.ppf(0.75)  # 标准正态 75% 分位数
K_MAD_EXPECTED = 1 / Q75
FWHM_GAUSS_CALCULATED = 2 * math.sqrt(2 * math.log(2))


def compute_mad(data):
    """计算中位绝对偏差 MAD = median(|x - median(x)|)"""
    return np.median(np.abs(data - np.median(data)))


def compute_fwhm_from_second_moment(data):
    """从二阶矩估算 FWHM（需先减去均值）"""
    data_centered = data - np.mean(data)
    sigma_est = np.std(data_centered)
    return FWHM_GAUSS_THEORY * sigma_est


def test_mad_unbiasedness(sigma_true, n_samples, k_mad, n_reps):
    """测试 MAD 估计的无偏性"""
    print(f"\n{'='*60}")
    print("实验 1：MAD→σ尺度常数无偏性检验")
    print('='*60)
    
    biases = []
    for rep in range(n_reps):
        rng = np.random.default_rng(SEED + rep)
        noise = rng.normal(0, sigma_true, n_samples)
        
        mad_est = compute_mad(noise)
        sigma_recovered = k_mad * mad_est
        
        bias = (sigma_recovered - sigma_true) / sigma_true
        biases.append(bias)
        
        print(f"  Rep {rep+1}: MAD={mad_est:.6f}, σ̂={sigma_recovered:.6f}, 相对偏差={bias*100:.4f}%")
    
    mean_bias = np.mean(biases)
    std_bias = np.std(biases)
    
    print(f"\n汇总:")
    print(f"  平均相对偏差 = {mean_bias*100:.6f}% ± {std_bias*100:.4f}%")
    print(f"  理论常数：{k_mad:.15f}")
    print(f"  期望值：{K_MAD_EXPECTED:.15f}")
    print(f"  常数误差：{(k_mad - K_MAD_EXPECTED)*1e15:.2f} × 10⁻¹⁵")
    
    # Oracle：相对偏差应在 5% 内（SNR-004 冻结门）
    passed = abs(mean_bias) < 0.05
    print(f"  ✅ PASS 相对偏差 < 5%" if passed else f"  ❌ FAIL 相对偏差 > 5%")
    
    return {
        "experiment": "P-CST-03 MAD theory validation",
        "true_sigma": float(sigma_true),
        "sample_size": int(n_samples),
        "repetitions": int(n_reps),
        "mean_relative_bias": float(mean_bias),
        "std_bias": float(std_bias),
        "k_mad_used": float(k_mad),
        "k_mad_expected": float(K_MAD_EXPECTED),
        "oracle_pass": bool(passed)
    }


def test_fwhm_factor():
    """验证高斯 FWHM↔σ因子的数学正确性"""
    print(f"\n{'='*60}")
    print("实验 2：高斯 FWHM↔σ因子的数学验证")
    print('='*60)
    
    # 直接数值计算验证
    fwhm_direct = 2 * np.sqrt(2 * np.log(2))
    fwhm_exact = 2 * math.sqrt(2 * math.log(2))
    
    print(f"  直接计算 (numpy): {fwhm_direct:.15f}")
    print(f"  精确计算 (math) : {fwhm_exact:.15f}")
    print(f"  理论声称值      : {FWHM_GAUSS_THEORY:.15f}")
    
    error_numpy = abs(fwhm_direct - FWHM_GAUSS_THEORY)
    error_math = abs(fwhm_exact - FWHM_GAUSS_THEORY)
    
    print(f"  与 numpy 差值   : {error_numpy:.2e}")
    print(f"  与 math 差值     : {error_math:.2e}")
    
    # Oracle：差值应 < 1e-14（浮点精度）
    passed = error_math < 1e-14
    print(f"  ✅ PASS 数学一致性" if passed else f"  ❌ FAIL 数学不一致")
    
    # 反向验证：用 FWHM 计算 σ
    sigma_true = 3.0
    fwhm_true = FWHM_GAUSS_THEORY * sigma_true
    sigma_recovered = fwhm_true / FWHM_GAUSS_THEORY
    
    reverse_error = abs(sigma_recovered - sigma_true) / sigma_true
    print(f"  反向恢复误差：{reverse_error:.2e}")
    
    return {
        "experiment": "P-CST-04 FWHM factor mathematical validation",
        "fwhm_numpy": float(fwhm_direct),
        "fwhm_math": float(fwhm_exact),
        "fwhm_claimed": float(FWHM_GAUSS_THEORY),
        "error_with_numpy": float(error_numpy),
        "error_with_math": float(error_math),
        "reverse_recovery_error": float(reverse_error),
        "oracle_pass": bool(passed)
    }


def test_zero_effect_negative_case():
    """负例：真值无效应⇒度量归零"""
    print(f"\n{'='*60}")
    print("实验 3：负例——零噪声框架下的退化行为")
    print('='*60)
    
    rng = np.random.default_rng(SEED)
    zero_noise = np.zeros(10000)  # 全零噪声
    
    mad_zero = compute_mad(zero_noise)
    sigma_zero = K_MAD_THEORY * mad_zero
    
    print(f"  全零输入下: MAD = {mad_zero:.15f}, σ̂ = {sigma_zero:.15f}")
    
    # Oracle：应为精确零
    passed = mad_zero == 0.0 and sigma_zero == 0.0
    print(f"  ✅ PASS 零效应归零" if passed else f"  ❌ FAIL 未归零")
    
    return {
        "experiment": "Negative case: zero effect ⇒ metric zero",
        "mad_of_zero": float(mad_zero),
        "sigma_of_zero": float(sigma_zero),
        "oracle_pass": bool(passed)
    }


def main():
    print("="*60)
    print("EXP-P2-001: P-CST-03 与 P-CST-04 理论验证")
    print("="*60)
    
    results = {
        "experiment_id": "EXP-P2-001",
        "title": "Theory validation of P-CST-03 (MAD→σ) and P-CST-04 (FWHM→σ)",
        "seed": SEED,
        "timestamp": "2026-09-26T16:XX:XX",
        "tests": []
    }
    
    # 运行各测试
    results["tests"].append(test_mad_unbiasedness(SIGMA_TRUE, N_SAMPLES, K_MAD_THEORY, REPETITIONS))
    results["tests"].append(test_fwhm_factor())
    results["tests"].append(test_zero_effect_negative_case())
    
    # 汇总结论
    all_passed = all(t.get("oracle_pass", False) for t in results["tests"])
    results["summary"] = {
        "all_oracles_passed": all_passed,
        "constants_validated": ["P-CST-03", "P-CST-04"],
        "conclusion": "✅ PASS 所有理论验证通过" if all_passed else "❌ FAIL 部分验证失败"
    }
    
    # 落盘结果
    output_path = "results/EXP-P2-001-results.json"
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2, ensure_ascii=False)
    
    print(f"\n{'='*60}")
    print(f"结果已保存至：{output_path}")
    print(f"{'='*60}")
    
    # 返回退出码
    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
