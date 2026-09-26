"""P-CST-07: 验证中位位置标准误系数的截断误差"""
import numpy as np

# 理论精确值
kappa_exact = np.sqrt(np.pi / 2)  # √(π/2) = 1.2533141373155001

# 代码中使用的手抄值（截断）
kappa_truncated = 1.253

# 计算相对误差
rel_error = (kappa_truncated - kappa_exact) / kappa_exact
abs_error = kappa_truncated - kappa_exact

print("=" * 60)
print("P-CST-07: 高斯中位位置标准误系数精度分析")
print("=" * 60)
print(f"理论精确值 κ = √(π/2) = {kappa_exact:.18f}")
print(f"代码手抄值    κ = 1.253")
print(f"绝对误差      Δ = {abs_error:.15e}")
print(f"相对误差      ε = {rel_error:.6e}")
print()
print("影响范围：")
print("  - 字段：sigma_location_se_dex, sigma_location_se_mag")
print("  - 系统性偏绿约 2.0e-4 相对")
print("  - 星等零点误差棒被低估")
print()
print("订正建议：")
print("  将代码从 `const double kappa = 1.253;` 改为")
print("  `constexpr double kappa = sqrt(M_PI / 2.0);`")
print("=" * 60)
