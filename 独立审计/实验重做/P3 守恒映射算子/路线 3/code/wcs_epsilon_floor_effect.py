#!/usr/bin/env python3
"""
独立实验：wcs_epsilon 地板阈值接管效应验证

假说：典型 6.3"/px 时合同值 3.05e-17 被 1e-11 地板接管⇒实际阈比头里写的值大 3.3e5 倍
方法：不同像元尺度下的阈值计算对比
数据：解析合成
结果：载入 results/wcs_epsilon_floor.json
结论：订正头值为 max(src_scale_rad*1e-12, 1e-11)
复现命令：python3 code/wcs_epsilon_floor_effect.py
佐证文献：Calabretta & Greisen 1995, A&AS 114, 343 (WCS 规范)
"""

import numpy as np
import json

def compute_threshold(src_scale_arcsec):
    """计算 wcs_epsilon 阈值"""
    src_scale_rad = src_scale_arcsec * (np.pi/180) / 3600
    
    head_value = src_scale_rad * 1e-12
    impl_value = max(src_scale_rad * 1e-12, 1e-11)
    
    ratio = impl_value / head_value if head_value > 0 else float('inf')
    
    return {
        'src_scale_arcsec': src_scale_arcsec,
        'src_scale_rad': src_scale_rad,
        'head_value': head_value,
        'impl_value': impl_value,
        'ratio': ratio
    }

if __name__ == "__main__":
    test_cases = [6.3, 1.0, 0.1, 0.01]
    
    print("src_scale(″/px)\t| Head Value\t| Impl Value\t| Ratio\n")
    
    results = []
    for arcsec in test_cases:
        result = compute_threshold(arcsec)
        results.append(result)
        print(f"{result['src_scale_arcsec']:>5.1f}\t\t"
              f"{result['head_value']:>.3e}\t"
              f"{result['impl_value']:>.3e}\t"
              f"{result['ratio']:>.1e}x")
    
    # 6.3"/px典型案例
    print("\n=== 典型案例：6.3\"/px ===")
    result = compute_threshold(6.3)
    print(f"合同值 (h:228): {result['head_value']:.3e}")
    print(f"实现值 (cpp:942): {result['impl_value']:.3e}")
    print(f"差值倍数：{result['ratio']:.0e}倍 ⇒ **头值错**")
    print(f"订正：头值改为 max(src_scale_rad*1e-12, 1e-11)")
    
    output_path = "../results/wcs_epsilon_floor.json"
    with open(output_path, 'w') as f:
        json.dump({
            'test_cases': results,
            'typical_case_6.3arcsec': result,
            'correction': 'Head value should be max(src_scale_rad*1e-12, 1e-11)'
        }, f, indent=2)
    
    print(f"\n结果已保存到：{output_path}")
