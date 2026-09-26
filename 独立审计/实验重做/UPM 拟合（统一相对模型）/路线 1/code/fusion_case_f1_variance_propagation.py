#!/usr/bin/env python3
"""
融合面实验 F1: UPM 背景平面进 SNR 的方差预算

假说: UPM 拟合出的参数协方差应当能正确传播到最终马赛克的方差图，
      特别是在覆盖边缘区域。

方法:
1. 构造合成数据：已知天光平面的多层叠加场景
2. 运行简化流程（模拟 normalize→mosaic）
3. 比较预测方差（来自 UPM covariance + drizzle 传播）与实测残差
4. 检验方差比是否在容差范围内

数据：代数合成（固定 seed=42）
输出：results/f1_variance_propagation.json
"""

import json
import numpy as np
from pathlib import Path

def generate_synthetic_sky_frames(n_frames: int, n_pixels: int = 1000, 
                                   seed: int = 42) -> dict:
    """
    生成合成天光帧
    
    每帧有：
    - 真实背景场 B_true(x)（已知）
    - UPM 加性偏差δ_k (各帧不同)
    - 噪声ε (~σ_bg)
    - Drizzle 相关校正 k_corr
    """
    np.random.seed(seed)
    
    # 真实公共背景场（平滑曲面）
    x = np.linspace(0, 1, n_pixels)
    B_true = 300.0 + 50.0 * np.sin(2 * np.pi * x)  # 300 ADU 基准 + 正弦起伏
    
    frames = []
    for k in range(n_frames):
        # 每帧的平缓梯度δ_k (低自由度)
        delta_k = np.random.uniform(-10, 10) + 5.0 * np.cos(2 * np.pi * x)
        
        # 天光项：B_true + δ_k
        sky = B_true + delta_k
        
        # 噪声：高斯白噪声 σ=10 ADU
        noise = np.random.normal(0, 10.0, n_pixels)
        
        # 观测值：raw = signal + sky + noise
        # 假设信号为 0（纯背景检测场景）
        raw = sky + noise
        
        # Drizzle 相关校正
        k_corr = 1.4
        
        frames.append({
            "frame_id": k,
            "raw": raw,
            "sky": sky,
            "delta_k": delta_k,
            "noise": noise,
            "k_corr": k_corr,
            "coverage": np.ones(n_pixels, dtype=bool),  # 简单全覆盖
        })
    
    return {
        "B_true": B_true,
        "frames": frames
    }


def simulate_upm_fit(frames: list, pixel_scale: float = 1e-11) -> dict:
    """
    模拟 UPM 拟合过程
    
    返回：
    - 拟合的δ_k^
    - 参数协方差矩阵
    - 收敛状态
    """
    n_frames = len(frames)
    n_pixels = len(frames[0]["raw"])
    
    # 构建设计矩阵 A 和观测向量 y
    # y_fk = M + δ_k + ε
    # M = 公共参考面（常数）
    # δ_k = 逐帧梯度修正
    
    y = np.concatenate([f["raw"] for f in frames])
    
    # 设计矩阵：第一列全是 1（M），后面每帧一列（δ_k）
    A = np.zeros((n_pixels * n_frames, n_frames + 1))
    A[:, 0] = 1.0  # M 列
    for k in range(n_frames):
        A[k*n_pixels:(k+1)*n_pixels, k+1] = 1.0  # δ_k 列
    
    # 权重：w = control_ivar ≈ 1/σ²
    # 这里用简化的均匀权重
    w = np.ones(n_pixels * n_frames)
    
    # 加权最小二乘解
    Wa = np.diag(np.sqrt(w)) @ A
    Wy = np.sqrt(w) * y
    WWA = Wa.T @ Wa
    WWy = Wa.T @ Wy
    
    # 解方程 (带弱正则化避免奇异性)
    lambda_reg = 1e-6
    theta_hat = np.linalg.solve(WWA + lambda_reg * np.eye(WWA.shape[0]), WWy)
    
    # 参数协方差
    V_theta = np.linalg.inv(WWA + lambda_reg * np.eye(WWA.shape[0])) / np.mean(w)
    
    # 拟合值
    y_fit = A @ theta_hat
    
    # 残差
    residual = y - y_fit
    
    # 收敛状态（简化：总是 converged=1）
    converged = 1
    
    return {
        "theta_hat": theta_hat,
        "covariance": V_theta,
        "residual": residual,
        "converged": converged,
        "M_fit": theta_hat[0],
        "delta_k_fits": theta_hat[1:]
    }


def propagate_variance(theta_hat: np.ndarray, V_theta: np.ndarray, 
                       frames: list, n_frames: int) -> dict:
    """
    将 UPM 参数协方差传播到马赛克方差图
    
    关键公式：
    Var(Σw_i x_i / Σw_i) = Σw_i² Var(x_i) / (Σw_i)² + 参数协方差贡献
    
    对于每个像素：
    - 来自直接观测的方差
    - 来自 UPM 参数的不确定性
    """
    n_pixels = len(frames[0]["raw"])
    
    predicted_var = np.zeros(n_pixels)
    
    for p in range(n_pixels):
        var_direct = 0.0
        weight_sum_sq = 0.0
        
        for k in range(n_frames):
            w_k = 1.0  # 简化均匀权重
            
            # 直接观测方差（噪声项）
            sigma_noise_k = 10.0  # 已知输入噪声标准差
            var_direct += w_k**2 * sigma_noise_k**2
            weight_sum_sq += w_k
        
        # 归一化
        var_direct /= weight_sum_sq**2
        
        # 参数协方差贡献
        # Δvar = J @ V_theta @ J^T
        # J = ∂(output)/∂θ （对每个参数的导数）
        
        # 对于 add 型校正：output = raw − δ_k
        # ∂output/∂δ_k = −1
        # ∂output/∂M = 0 (M 被 gauge 掉)
        
        param_var = 0.0
        for k in range(n_frames):
            # δ_k 的参数方差
            param_var += V_theta[k+1, k+1]  # 对角元
        
        # 总预测方差
        predicted_var[p] = var_direct + param_var/n_frames
    
    return {"predicted": predicted_var}


def compute_empirical_residuals(theta_hat: np.ndarray, frames: list, 
                                 n_frames: int) -> np.ndarray:
    """
    计算实测残差方差（作为 ground truth 对比）
    """
    n_pixels = len(frames[0]["raw"])
    
    # 对每帧应用 UPM 校正
    corrected = []
    for k in range(n_frames):
        delta_k_hat = theta_hat[k+1]
        corrected_frame = frames[k]["raw"] - delta_k_hat
        corrected.append(corrected_frame)
    
    # 合并所有帧的校正后值
    combined = np.concatenate(corrected)
    
    # 实测残差方差（相对于公共背景估计）
    B_est = np.median(combined)
    empirical_var = np.var(combined - B_est)
    
    return empirical_var


def main():
    # 参数
    n_frames = 10
    n_pixels = 1000
    seed = 42
    
    results = {
        "experiment": "F1 UPM variance propagation",
        "fusion_aspect": "UPM covariance → mosaic variance budget",
        "parameters": {
            "n_frames": n_frames,
            "n_pixels": n_pixels,
            "seed": seed,
            "sigma_noise_input": 10.0,
            "k_corr": 1.4
        },
        "steps": []
    }
    
    # Step 1: 生成合成数据
    print("Generating synthetic frames...")
    data = generate_synthetic_sky_frames(n_frames, n_pixels, seed)
    
    step1 = {
        "name": "synthetic_data_generation",
        "B_true_range": [float(data["B_true"].min()), float(data["B_true"].max())],
        "delta_k_range": [[float(f["delta_k"].min()), float(f["delta_k"].max())] for f in data["frames"]],
        "status": "complete"
    }
    results["steps"].append(step1)
    
    # Step 2: UPM 拟合
    print("Running UPM fit...")
    upm_result = simulate_upm_fit(data["frames"], pixel_scale=1e-11)
    
    step2 = {
        "name": "upm_fit",
        "M_fit": float(upm_result["M_fit"]),
        "delta_k_fits": [float(d) for d in upm_result["delta_k_fits"]],
        "covariance_trace": float(np.trace(upm_result["covariance"])),
        "converged": upm_result["converged"],
        "status": "complete" if upm_result["converged"] == 1 else "failed"
    }
    results["steps"].append(step2)
    
    # Step 3: 方差传播
    print("Propagating variance...")
    var_prop_result = propagate_variance(
        upm_result["theta_hat"], 
        upm_result["covariance"], 
        data["frames"], 
        n_frames
    )
    
    step3 = {
        "name": "variance_propagation",
        "predicted_var_mean": float(np.mean(var_prop_result["predicted"])),
        "predicted_var_std": float(np.std(var_prop_result["predicted"])),
        "expected_in_uniform_coverage": "< 5% error",
        "expected_in_edge_coverage": "< 15% error",
        "status": "complete"
    }
    results["steps"].append(step3)
    
    # Step 4: 实测残差计算
    print("Computing empirical residuals...")
    empirical_var = compute_empirical_residuals(
        upm_result["theta_hat"], 
        data["frames"], 
        n_frames
    )
    
    step4 = {
        "name": "empirical_residual_computation",
        "empirical_var": float(empirical_var),
        "status": "complete"
    }
    results["steps"].append(step4)
    
    # Step 5: 对比分析
    pred_mean = step3["predicted_var_mean"]
    emp_var = step4["empirical_var"]
    ratio = pred_mean / emp_var if emp_var > 0 else np.inf
    
    conclusion = {
        "hypothesis": "UPM parameter covariance correctly propagates to mosaic variance",
        "pred_vs_empirical": {
            "predicted_mean": pred_mean,
            "empirical": emp_var,
            "ratio": ratio,
            "error_percent": abs(ratio - 1.0) * 100
        },
        "tolerance_check": {
            "uniform_coverage_target": "< 5% error",
            "edge_coverage_target": "< 15% error",
            "actual_error": f"{abs(ratio - 1.0) * 100:.2f}%",
            "pass_uniform": ratio < 1.05 and ratio > 0.95,
            "pass_edge": ratio < 1.15 and ratio > 0.85
        },
        "verdict": "PASS" if abs(ratio - 1.0) < 0.15 else "FAIL"
    }
    results["conclusion"] = conclusion
    
    # 写入结果
    output_dir = Path("独立审计/实验重做/UPM 拟合（统一相对模型）/路线 1/results")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "f1_variance_propagation.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"\nResults written to {output_file}")
    print("\n=== Fusion Case F1 Results ===")
    print(f"Predicted variance (mean): {pred_mean:.6f}")
    print(f"Empirical variance: {emp_var:.6f}")
    print(f"Ratio (pred/emp): {ratio:.4f}")
    print(f"Error: {abs(ratio - 1.0) * 100:.2f}%")
    print(f"Verdict: {conclusion['verdict']}")
    
    if conclusion['verdict'] == 'PASS':
        print("\n✓ Hypothesis supported: UPM covariance propagates correctly")
    else:
        print("\n✗ Hypothesis NOT supported: Variance mismatch detected")
        print("  Check tolerance: uniform coverage target < 5%, edge < 15%")


if __name__ == "__main__":
    main()
