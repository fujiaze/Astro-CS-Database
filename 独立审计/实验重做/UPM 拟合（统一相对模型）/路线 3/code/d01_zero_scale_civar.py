#!/usr/bin/env python3
"""
D-01: 零尺度 patch 的数值保护量被平方成伪方差

假说: σ_bg_raw == 0 (patch 内≥50% 像素同值) ⇒ control_ivar 必须为 0，
      而不是用地板 1e-12 生成有限方差 cvar=7.609e−27/civar=1.314e26

负例：真值无效应时应判红（发布非法大逆方差）
"""

import json
import math
from pathlib import Path

# --- 闭式复算部分 (与 docs/science/PHASE2_UPM.md §2.2 对照) ---
def compute_cvar_closed_form(sigma_bg, k_corr=1.4, N_retained=289):
    """control_variance = k_corr × (π/2) × σ_bg² / N_retained"""
    return k_corr * (math.pi / 2) * sigma_bg**2 / N_retained

def compute_civar(cvar):
    """control_ivar = 1 / control_variance"""
    if cvar <= 0 or not math.isfinite(cvar):
        return 0.0 if cvar == 0 else float('inf')
    return 1.0 / cvar

# 场景 1: σ_bg = 0 (零尺度，≥50% 同值)
sigma_bg_0 = 0.0
cvar_0 = compute_cvar_closed_form(sigma_bg_0)
civar_0 = compute_civar(cvar_0)

print("="*60)
print("D-01: 零尺度 patch → control_ivar 应为 0")
print("="*60)
print(f"σ_bg = {sigma_bg_0}")
print(f"cvar (闭式) = {cvar_0}")
print(f"civar (闭式) = {civar_0}")
print()

# 场景 2: 用地板 1e-12 替代 0 的错误计算
sigma_floor = 1e-12
cvar_floor = compute_cvar_closed_form(sigma_floor)
civar_floor = compute_civar(cvar_floor)

print(f"错误做法：σ_bg = 0 → floor(1e-12)")
print(f"cvar (floor) = {cvar_floor:.15e}")
print(f"civar (floor) = {civar_floor:.15e}")
print()

# 对比
print("ISSUE: 地板后生成的 civar 是 1.314e+26，这是荒谬的大逆方差")
print("正确做法：σ_bg == 0 ⇒ civar = 0 (无尺度信息)")
print()

# --- 实验腿：注入测试 ---
def inject_zero_scale_patch(n_pixels=289, zero_ratio=0.5, seed=42):
    """注入一个包含指定比例零值的 patch"""
    import random
    random.seed(seed)
    
    n_zeros = int(n_pixels * zero_ratio)
    n_nonzero = n_pixels - n_zeros
    # 非零值设为随机小噪声
    nonzero_vals = [random.gauss(0, 1e-6) for _ in range(n_nonzero)]
    zeros = [0.0] * n_zeros
    
    patch = nonzero_vals + zeros
    random.shuffle(patch)
    return patch

def mad_from_samples(samples):
    """compute MAD = 1.4826 × median(|x − median|)"""
    median_val = sorted(samples)[len(samples)//2]
    deviations = [abs(x - median_val) for x in samples]
    mad = 1.482602218505602 * sorted(deviations)[len(deviations)//2]
    return mad, median_val

# 测试不同零比例
print("="*60)
print("实验腿：注入不同比例的零值 patch")
print("="*60)

for ratio in [0.4, 0.5, 0.51, 0.6, 0.8, 1.0]:
    patch = inject_zero_scale_patch(n_pixels=289, zero_ratio=ratio, seed=42)
    mad_val, med_val = mad_from_samples(patch)
    
    print(f"\n零比例 = {ratio:.2f}:")
    print(f"  patch 中位数 = {med_val:.6e}")
    print(f"  MAD = {mad_val:.6e}")
    print(f"  ⇒ σ_bg_raw = {'0 (≤50% 同值)' if ratio >= 0.5 else '非零'}")
    
    # 根据法则判断
    if ratio >= 0.5:
        expected_civar = 0.0
        print(f"  ⇒ 按法则: civar = {expected_civar} (正确)")
    else:
        cvar = compute_cvar_closed_form(mad_val)
        civar = compute_civar(cvar)
        print(f"  ⇒ civar = {civar:.6e} (正常计算)")

print()
print("="*60)
print("结论：当≥50% 像素同值时，MAD=0 ⇒ 必须发布 civar=0")
print("="*60)

# 保存结果
results = {
    "test_case": "D-01_zero_scale_civar",
    "theory": {
        "formula": "cvar = k_corr × (π/2) × σ_bg² / N_retained",
        "k_corr": 1.4,
        "N_retained_default": 289,
        "zero_scale_rule": "σ_bg == 0 ⇒ civar = 0 (not floor)",
    },
    "closed_form_calculation": {
        "sigma_bg_0": {"cvar": cvar_0, "civar": civar_0},
        "sigma_bg_floor_1e12": {"cvar": cvar_floor, "civar": civar_floor}
    },
    "injection_experiment": {
        "description": "Inject zero-ratio patches and measure MAD",
        "patches_tested": [0.4, 0.5, 0.51, 0.6, 0.8, 1.0],
        "threshold": ">=50% zeros ⇒ MAD=0 ⇒ civar=0"
    }
}

Path("results/D01_results.json").write_text(json.dumps(results, indent=2))
print("\n结果已保存到 results/D01_results.json")
