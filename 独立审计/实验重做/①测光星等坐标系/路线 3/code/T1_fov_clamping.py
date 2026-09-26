"""
T1: FOV 半径式三常数敏感性分析
------------------------
假说：H0: FOV 缓冲 1.2 与钳位界 1.0/10.0 对 Gaia 查询成本与定标星稳定性的影响是可量化的。

实验设计（纯 Python + numpy）：
- 模拟不同 FOV 下 Gaia 查询的星数
- 量化缓冲因子 1.2 的影响
- 验证"10 度对应 2000 颗星"的假设是否成立

数据适用性：解析代数合成（基于星等 - 星数累积分布的经验公式）
固定 seed：42
"""

import numpy as np
# Remove scipy dependency for simplicity - not needed for this analysis

# =============================================================================
# 核心模型
# =============================================================================

def simulate_gaia_query_cost(fov_deg, star_density=1e4):
    """
    模拟不同 FOV 下 Gaia 查询的星数。
    
    假设：
    - 星密度 ~1e4 颗/deg²（拥挤场上限，实际会随天区变化）
    - FOV 面积近似为球冠面积
    
    参数：
        fov_deg: FOV 半径（度）
        star_density: 星密度（颗/deg²）
    
    返回：
        n_stars: 预测星数
    """
    area = np.pi * (fov_deg/2)**2  # 球冠面积近似（小 FOV 时近似平面圆面积）
    n_stars = area * star_density
    return n_stars


def analyze_fov_sensitivity():
    """
    分析 FOV 参数的敏感性。
    """
    print("="*70)
    print("T1: FOV 半径式三常数敏感性分析")
    print("="*70)
    print()
    
    # =======================================================================
    # 1. FOV 与星数的关系曲线
    # =======================================================================
    print("1. FOV 半径 vs 星数关系")
    print("-"*70)
    
    fov_range = np.linspace(0.1, 30, 100)
    costs = [simulate_gaia_query_cost(f) for f in fov_range]
    
    # 打印关键点的星数
    key_fovs = [0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 30.0]
    for fov in key_fovs:
        n = simulate_gaia_query_cost(fov)
        marker = " <-- 钳位界" if fov == 10.0 else ""
        print(f"  FOV = {fov:5.1f} deg => n_stars = {n:8.0f}{marker}")
    
    print()
    
    # =======================================================================
    # 2. 早停阈值对应的最大安全 FOV
    # =======================================================================
    print("2. 早停阈值对应的最大安全 FOV")
    print("-"*70)
    
    early_stop_threshold = 2000
    max_safe_fov = None
    
    # 找到最后一个满足阈值的 FOV
    for i, cost in enumerate(costs):
        if cost <= early_stop_threshold:
            max_safe_fov = fov_range[i]
        else:
            break
    
    if max_safe_fov is not None:
        ratio = 10.0 / max_safe_fov
        print(f"  早停阈值 {early_stop_threshold:,} 颗对应的最大安全 FOV: {max_safe_fov:.2f} deg")
        print(f"  当前钳位界 10.0 deg 是安全值的 {ratio:.1f}x")
        print(f"  结论：{'宽松' if ratio > 2 else '适中' if ratio > 1.5 else '保守'}")
    else:
        print(f"  早停阈值 {early_stop_threshold:,} 颗在 FOV ≤ 30 deg 范围内不可达")
    
    print()
    
    # =======================================================================
    # 3. 缓冲因子 1.2 的影响分析
    # =======================================================================
    print("3. 缓冲因子 1.2 的影响")
    print("-"*70)
    
    # 场景 A：FOV 被低估
    fov_under_estimate = 0.5  # 真值 0.5 度被低估
    fov_estimated_with_buffer = fov_under_estimate * 1.2
    
    n_true = simulate_gaia_query_cost(fov_under_estimate)
    n_with_buffer = simulate_gaia_query_cost(fov_estimated_with_buffer)
    cost_ratio_overestimate = n_with_buffer / n_true
    
    print(f"  场景 A（FOV 低估）:")
    print(f"    真值：{fov_under_estimate:.2f} deg => n_stars = {n_true:.0f}")
    print(f"    带缓冲估计：{fov_estimated_with_buffer:.2f} deg => n_stars = {n_with_buffer:.0f}")
    print(f"    成本增加倍数：{cost_ratio_overestimate:.2f}x")
    print(f"    星数绝对增量：+{n_with_buffer - n_true:.0f} 颗")
    
    # 场景 B：FOV 被高估
    fov_over_estimate = 5.0  # 真值 5 度被高估
    fov_actual_with_buffer_check = min(10.0, max(1.0, fov_over_estimate * 1.2))  # 应用钳位
    
    n_high_est = simulate_gaia_query_cost(fov_over_estimate)
    n_with_clamp = simulate_gaia_query_cost(fov_actual_with_buffer_check)
    
    print(f"\n  场景 B（FOV 高估并应用钳位）:")
    print(f"    高估值：{fov_over_estimate:.2f} deg => n_stars = {n_high_est:.0f}")
    print(f"    缓冲 + 钳位后：{fov_actual_with_buffer_check:.2f} deg => n_stars = {n_with_clamp:.0f}")
    print(f"    钳位生效：{'是' if fov_actual_with_buffer_check == 10.0 else '否'}")
    
    print()
    
    # =======================================================================
    # 4. 缓冲因子的最优性分析
    # =======================================================================
    print("4. 缓冲因子的敏感性扫描")
    print("-"*70)
    
    buffer_factors = np.linspace(1.0, 2.0, 11)
    base_fov = 0.5  # 假设典型低估场景的基础 FOV
    
    print(f"  {'缓冲因子':>12} | {'放大 FOV':>12} | {'星数':>10} | {'成本增量%':>12}")
    print(f"  {'-'*12}+{'-'*14}+{'-'*12}+{'-'*14}")
    
    for buf in buffer_factors:
        fov_aug = base_fov * buf
        n_aug = simulate_gaia_query_cost(fov_aug)
        cost_increase_pct = (n_aug - simulate_gaia_query_cost(base_fov)) / simulate_gaia_query_cost(base_fov) * 100
        
        print(f"  {buf:>12.2f} | {fov_aug:>12.2f} | {n_aug:>10.0f} | {cost_increase_pct:>11.1f}%")
    
    print()
    
    # =======================================================================
    # 5. 综合结论与建议
    # =======================================================================
    print("5. 综合结论与建议")
    print("-"*70)
    
    print("""
  结论：
  1. FOV 半径与星数呈平方关系（area ∝ fov²），缓冲因子会被平方放大。
  2. 缓冲 1.2 在 FOV 低估 20% 时会使星数增加约 44%（1.2² = 1.44）。
  3. 对于 FOV > 8 deg 的场景，10.0 度钳位界开始生效，防止查询过大。
  
  建议：
  1. 将 1.2、1.0、10.0 登记为"工程约定值"而非"科学常数"。
  2. 依据来源："WCS 解算不确定性±20%" + "Gaia API 性能预算 <2000 颗星"。
  3. 未来可考虑动态缓冲：根据 WCS 解算精度自适应调整 1.2。
  
  诚实边界：
  1. 本模型假设星密度恒定为 1e4 颗/deg²，实际天区会有显著差异。
  2. 真实 WCS 解算误差分布需从历史数据反推，才能确定最优缓冲值。
  3. 若实际误差远超±20%，则 1.2 可能不足；反之则过度保守。
    """)
    
    print("="*70)


if __name__ == "__main__":
    np.random.seed(42)  # 固定 seed，确保复现
    analyze_fov_sensitivity()
