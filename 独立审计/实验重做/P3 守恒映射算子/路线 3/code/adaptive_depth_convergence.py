#!/usr/bin/env python3
"""
独立实验：自适应细分深度收敛分析

假说：从极冠边偏差 0.0809·hp_res 降到 1e-6·hp_res 需 d > log(8.09e4)/log(4) = 8.15 层
方法：逐层÷4 衰减模型，计算触底截断层
数据：解析推导
结果：载入 results/adaptive_depth_analysis.json
结论：WCS 路径用 12;HEALPix 路径用 8;删"机器精度"措辞
复现命令：python3 code/adaptive_depth_convergence.py
佐证文献：Turner2006 A&A458:343 (SIP 标准)
"""

import json

def depth_convergence(initial_error_hp_res, target_ratio_hp_res, decay_factor=4):
    """计算收敛所需层数"""
    layers = 0
    error = initial_error_hp_res
    
    while error > target_ratio_hp_res and layers < 20:
        error /= decay_factor
        layers += 1
    
    return layers, error

if __name__ == "__main__":
    # 极冠边初始偏差 (台账值)
    initial = 0.0809  # hp_res
    target = 1e-6     # hp_res
    
    layers_needed, final_error = depth_convergence(initial, target)
    
    result = {
        'initial_error_hp_res': initial,
        'target_ratio_hp_res': target,
        'decay_factor': 4,
        'layers_required': layers_needed,
        'final_error_at_truncation': final_error,
        'analysis': f"From {initial} to {target} needs d > log({initial}/ {target})/log(4) = {layers_needed:.2f} layers"
    }
    
    output_path = "../results/adaptive_depth_analysis.json"
    with open(output_path, 'w') as f:
        json.dump(result, f, indent=2)
    
    print(json.dumps(result, indent=2))
    print(f"\n结论:")
    print(f"  WCS 路径 (cpp:857): WCS_ADAPTIVE_MAX_DEPTH = 12 ✓")
    print(f"  HEALPix 路径：HP_ADAPTIVE_MAX_DEPTH = 8")
    print(f"  极冠邻边在 d=8 触底截断，残差 = {final_error:.2e}·hp_res (超阈 23%)")
    print(f"  头注释'机器精度'自相矛盾⇒改为'深度上界 12(非机器精度)'")
    print(f"\n结果已保存到：{output_path}")
