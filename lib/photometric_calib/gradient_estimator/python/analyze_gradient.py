# -*- coding: utf-8 -*-
"""
梯度拟合分析脚本
功能: 用上次残差CSV数据，分析r_obs空间结构，测试不同拟合策略效果
用途: 验证_MAX_ORDER=3 + Ridge α=1000是否解决过拟合，对比空间分箱方法
"""

import os
import sys
import csv
import numpy as np

# 路径设置
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _SCRIPT_DIR)

from gradient_fitter import GradientFitter


def load_residuals(csv_path):
    """加载残差CSV"""
    x_list, y_list, r_list = [], [], []
    with open(csv_path, "r", encoding="utf-8") as f:
        rd = csv.reader(f)
        next(rd)  # skip header
        for row in rd:
            x_list.append(float(row[0]))
            y_list.append(float(row[1]))
            r_list.append(float(row[2]))
    return np.array(x_list), np.array(y_list), np.array(r_list)


def analyze_spatial_structure(x, y, r, img_w, img_h):
    """分析r_obs的空间结构: 信号 vs 噪声"""
    print("\n" + "=" * 70)
    print("1. r_obs 空间结构分析")
    print("=" * 70)

    # 全局统计
    r_med = float(np.median(r))
    r_std = float(np.std(r))
    r_mad = float(np.median(np.abs(r - r_med)))
    print(f"  星数: {len(r)}")
    print(f"  r_obs: median={r_med:.4f}, std={r_std:.4f}, MAD={r_mad:.4f}")
    print(f"  r_obs 范围: [{r.min():.4f}, {r.max():.4f}]")

    # 空间分箱: 5x5网格，每格取中位数
    print("\n  5x5 空间分箱 (每格中位数):")
    grid_n = 5
    x_edges = np.linspace(0, img_w, grid_n + 1)
    y_edges = np.linspace(0, img_h, grid_n + 1)
    x_bins = np.digitize(x, x_edges) - 1
    y_bins = np.digitize(y, y_edges) - 1
    x_bins = np.clip(x_bins, 0, grid_n - 1)
    y_bins = np.clip(y_bins, 0, grid_n - 1)

    grid = np.full((grid_n, grid_n), np.nan)
    for i in range(grid_n):
        for j in range(grid_n):
            mask = (x_bins == j) & (y_bins == i)
            if np.sum(mask) >= 1:
                grid[i, j] = float(np.median(r[mask]))

    # 打印网格
    for i in range(grid_n):
        row_str = "  "
        for j in range(grid_n):
            if np.isnan(grid[i, j]):
                row_str += "  ---   "
            else:
                row_str += f"{grid[i, j]:.3f}  "
        print(row_str)

    # 计算空间平滑性: 相邻格子的差异
    valid_mask = ~np.isnan(grid)
    diffs = []
    for i in range(grid_n):
        for j in range(grid_n - 1):
            if valid_mask[i, j] and valid_mask[i, j + 1]:
                diffs.append(abs(grid[i, j] - grid[i, j + 1]))
    for i in range(grid_n - 1):
        for j in range(grid_n):
            if valid_mask[i, j] and valid_mask[i + 1, j]:
                diffs.append(abs(grid[i, j] - grid[i + 1, j]))

    if diffs:
        spatial_signal = float(np.mean(diffs))
        noise_level = r_mad / 0.6745  # 估计的噪声sigma
        snr = spatial_signal / noise_level if noise_level > 0 else 0
        print(f"\n  空间信号 (相邻格平均差异): {spatial_signal:.4f}")
        print(f"  噪声水平 (MAD/0.6745): {noise_level:.4f}")
        print(f"  空间SNR: {snr:.2f}")


def test_polynomial_fitting(x, y, r, img_w, img_h):
    """测试多项式拟合: 不同阶数 + Ridge α"""
    print("\n" + "=" * 70)
    print("2. 多项式拟合测试 (当前: _MAX_ORDER=3, Ridge α网格含1000)")
    print("=" * 70)

    fitter = GradientFitter()

    # 用当前fitter拟合
    surface = fitter.fit_multiplicative(x, y, r, img_w, img_h, max_order=5)
    print(f"\n  拟合结果: 阶数={surface.order}, LOOCV={surface.loocv_error:.6e}")
    print(f"  使用={surface.n_used}, 排除={surface.n_rejected}")
    print(f"  残差中位数={surface.residual_median:.4e}, 残差std={surface.residual_std:.4e}")

    # 评估全图
    r_map = fitter.evaluate_surface_fullimage(surface, int(img_w), int(img_h))
    r_offset = float(np.median(r_map))
    r_map_centered = r_map - r_offset
    M_map = np.power(10.0, r_map_centered)

    print(f"\n  全图r_map: offset={r_offset:.4f}")
    print(f"  r_map_centered 范围: [{r_map_centered.min():.4f}, {r_map_centered.max():.4f}]")
    print(f"  M_map 范围: [{M_map.min():.4f}, {M_map.max():.4f}]")

    # 检查边缘行为
    h, w = r_map.shape
    print(f"\n  边缘行为检查:")
    print(f"    中心区域 r: {float(np.median(r_map_centered[h//4:3*h//4, w//4:3*w//4])):.4f}")
    print(f"    左边缘 r:   {float(np.median(r_map_centered[:, :w//10])):.4f}")
    print(f"    右边缘 r:   {float(np.median(r_map_centered[:, 9*w//10:])):.4f}")
    print(f"    上边缘 r:   {float(np.median(r_map_centered[:h//10, :])):.4f}")
    print(f"    下边缘 r:   {float(np.median(r_map_centered[9*h//10:, :])):.4f}")
    print(f"    四角 r:     {float(np.median(r_map_centered[:h//10, :w//10])):.4f} (左上), "
          f"{float(np.median(r_map_centered[:h//10, 9*w//10:])):.4f} (右上)")
    print(f"                {float(np.median(r_map_centered[9*h//10:, :w//10])):.4f} (左下), "
          f"{float(np.median(r_map_centered[9*h//10:, 9*w//10:])):.4f} (右下)")

    # 负值检查
    neg_pct = float(np.sum(M_map < 0.01) / M_map.size * 100)
    extreme_pct = float(np.sum(M_map < 0.5) / M_map.size * 100)
    print(f"\n  M_map < 0.01 比例: {neg_pct:.2f}%")
    print(f"  M_map < 0.5 比例:  {extreme_pct:.2f}%")

    # 检查过拟合: 拟合值vs观测值
    x_norm = (2.0 * x / img_w) - 1.0
    y_norm = (2.0 * y / img_h) - 1.0
    X = fitter._build_design_matrix(x_norm, y_norm, surface.order)
    r_fit = X @ surface.coeffs
    residuals = r - r_fit
    r2 = 1.0 - np.var(residuals) / np.var(r) if np.var(r) > 0 else 0
    print(f"\n  拟合质量: R²={r2:.4f}, 残差std={float(np.std(residuals)):.4f}")

    return surface, r_map_centered, M_map


def test_spatial_binning(x, y, r, img_w, img_h, grid_sizes=[4, 5, 6]):
    """测试空间分箱 + 低阶多项式方法"""
    print("\n" + "=" * 70)
    print("3. 空间分箱 + 低阶多项式 对比测试")
    print("=" * 70)

    fitter = GradientFitter()

    for grid_n in grid_sizes:
        x_edges = np.linspace(0, img_w, grid_n + 1)
        y_edges = np.linspace(0, img_h, grid_n + 1)
        x_bins = np.clip(np.digitize(x, x_edges) - 1, 0, grid_n - 1)
        y_bins = np.clip(np.digitize(y, y_edges) - 1, 0, grid_n - 1)

        bin_x, bin_y, bin_r = [], [], []
        n_per_bin = []
        for i in range(grid_n):
            for j in range(grid_n):
                mask = (x_bins == j) & (y_bins == i)
                n_in = int(np.sum(mask))
                n_per_bin.append(n_in)
                if n_in >= 1:
                    bin_x.append(float(np.mean(x[mask])))
                    bin_y.append(float(np.mean(y[mask])))
                    bin_r.append(float(np.median(r[mask])))

        bin_x = np.array(bin_x)
        bin_y = np.array(bin_y)
        bin_r = np.array(bin_r)
        n_cells = len(bin_r)
        avg_per_bin = float(np.mean(n_per_bin))

        print(f"\n  网格 {grid_n}x{grid_n}={grid_n**2}: 有效格={n_cells}, "
              f"平均{avg_per_bin:.1f}星/格")

        # 拟合2阶多项式
        if n_cells >= 6:
            surface = fitter.fit_multiplicative(
                bin_x, bin_y, bin_r, img_w, img_h, max_order=3)

            r_map = fitter.evaluate_surface_fullimage(
                surface, int(img_w), int(img_h))
            r_offset = float(np.median(r_map))
            r_map_centered = r_map - r_offset
            M_map = np.power(10.0, r_map_centered)

            # 检查边缘
            h, w = r_map.shape
            edge_vals = [
                float(np.median(r_map_centered[:, :w//10])),
                float(np.median(r_map_centered[:, 9*w//10:])),
                float(np.median(r_map_centered[:h//10, :])),
                float(np.median(r_map_centered[9*h//10:, :])),
            ]
            neg_pct = float(np.sum(M_map < 0.01) / M_map.size * 100)
            extreme_pct = float(np.sum(M_map < 0.5) / M_map.size * 100)

            print(f"    拟合: 阶数={surface.order}, α隐含, LOOCV={surface.loocv_error:.6e}")
            print(f"    r_map 范围: [{r_map_centered.min():.4f}, {r_map_centered.max():.4f}]")
            print(f"    M_map 范围: [{M_map.min():.4f}, {M_map.max():.4f}]")
            print(f"    边缘r: 左={edge_vals[0]:.4f}, 右={edge_vals[1]:.4f}, "
                  f"上={edge_vals[2]:.4f}, 下={edge_vals[3]:.4f}")
            print(f"    M<0.01: {neg_pct:.2f}%, M<0.5: {extreme_pct:.2f}%")

            # 在原始星点位置评估，计算R²
            x_norm_orig = (2.0 * x / img_w) - 1.0
            y_norm_orig = (2.0 * y / img_h) - 1.0
            X_orig = fitter._build_design_matrix(
                x_norm_orig, y_norm_orig, surface.order)
            r_fit_orig = X_orig @ surface.coeffs
            resid_orig = r - r_fit_orig
            r2_orig = 1.0 - np.var(resid_orig) / np.var(r) if np.var(r) > 0 else 0
            print(f"    原始星点R²={r2_orig:.4f}, 残差std={float(np.std(resid_orig)):.4f}")


def main():
    # 加载上次残差数据
    csv_path = os.path.normpath(os.path.join(
        _SCRIPT_DIR, "..", "..", "..", "..",
        "testdata", "Galaxy_Center_T4 全链路测试数据", "results", "panel1_Red",
        "mult_residuals.csv"))

    if not os.path.isfile(csv_path):
        print(f"残差CSV不存在: {csv_path}")
        return

    x, y, r = load_residuals(csv_path)
    print(f"加载残差数据: {len(r)} 颗星")
    print(f"  x范围: [{x.min():.1f}, {x.max():.1f}]")
    print(f"  y范围: [{y.min():.1f}, {y.max():.1f}]")
    print(f"  r范围: [{r.min():.4f}, {r.max():.4f}]")

    # 图像尺寸 (从坐标推断)
    img_w = float(x.max() + 200)  # 留余量
    img_h = float(y.max() + 200)
    print(f"  推断图像尺寸: {img_w:.0f} x {img_h:.0f}")

    # 1. 分析空间结构
    analyze_spatial_structure(x, y, r, img_w, img_h)

    # 2. 测试当前多项式拟合
    test_polynomial_fitting(x, y, r, img_w, img_h)

    # 3. 测试空间分箱方法
    test_spatial_binning(x, y, r, img_w, img_h)


if __name__ == "__main__":
    main()
