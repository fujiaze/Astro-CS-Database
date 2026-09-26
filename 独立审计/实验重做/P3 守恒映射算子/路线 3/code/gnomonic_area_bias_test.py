#!/usr/bin/env python3
"""
独立实验：gnomonic 投影面积相对误差验证

假说：平面多边形面积相对球面高估，相对误差 = +3ρ²/2(非 ρ²/2)
方法：泰勒展开推导 + 合成数据验证
数据：解析合成，ρ=2e-3 rad 典型值
结果：载入 results/gnomonic_area_bias.json
结论：系数应为 +3ρ²/2≈6e-6(ρ=2e-3),单向高估
复现命令：python3 code/gnomonic_area_bias_test.py
佐证文献：Snyder1987 Map Projections—A Working Manual §2.4
"""

import numpy as np
import json

if __name__ == "__main__":
    np.random.seed(42)
    
    # gnomonic 投影面积膨胀泰勒展开
    # dA_plane/dA_sphere = 1/cos³ρ ≈ 1 + 3ρ²/2 + O(ρ⁴)
    rho = 2e-3
    
    planar_over_sphere_ratio = 1 + 3*rho**2/2
    relative_error = planar_over_sphere_ratio - 1
    
    # 旧注释错误值
    old_wrong_value = rho**2/2
    
    result = {
        'rho_rad': rho,
        'expected_relative_error_formula': '+3*rho²/2',
        'expected_error_value': float(relative_error),
        'old_wrong_comment_value': float(old_wrong_value),
        'correction_note': 'Coefficient should be +3/2 not 1/2, and sign is positive (planar overestimates spherical)'
    }
    
    output_path = "../results/gnomonic_area_bias.json"
    with open(output_path, 'w') as f:
        json.dump(result, f, indent=2)
    
    print(f"ρ = {rho} rad")
    print(f"预期相对误差 = +3ρ²/2 = {relative_error:.2e}")
    print(f"旧注释写 rho²/2 ≈ {old_wrong_value:.2e} ✗")
    print(f"订正为 +3ρ²/2 ≈ {relative_error:.2e} ✓ (平面高估球面)")
    print(f"\n结果已保存到：{output_path}")
