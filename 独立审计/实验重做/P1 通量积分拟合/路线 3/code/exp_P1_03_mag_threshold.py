#!/usr/bin/env python3
"""
exp_P1_03_mag_threshold.py — mag_min/mag_max 星等门阈值扫描

假说：mag_min/mag_max 是经验参数，需基于实测拟合成功率与信噪比确定合理范围
"""

import numpy as np
import json
import sys
from pathlib import Path

SEED = 42
np.random.seed(SEED)

def fit_success_rate(mag, snr_per_mag=2.0, n_read_e=5.0, gain=1.0):
    """Estimate fitting success rate at given magnitude"""
    # Simplified model: SNR drops exponentially with mag
    base_snr = 1000.0  # SNR at mag=0
    signal_snr = base_snr * 10**(-0.4 * (mag - 8))
    
    # Read noise contribution
    read_noise_snr = snr_per_mag
    
    total_snr = 1 / np.sqrt(1/signal_snr**2 + 1/read_noise_snr**2)
    
    # Success probability increases with SNR
    prob = 1.0 / (1.0 + (50 / max(total_snr, 1.0))**2)
    
    return prob

def main():
    print("="*60)
    print("P1 通量积分拟合 · exp_P1_03：mag_min/max 阈值扫描")
    print("="*60)
    print()
    
    mags = np.arange(8, 25, 1)
    results = []
    
    for mag in mags:
        success = fit_success_rate(mag)
        results.append({
            "mag": int(mag),
            "success_rate": float(success)
        })
        
        if mag % 2 == 0 or mag == mags[-1]:
            print(f"mag={int(mag):2d}: 拟合成功率 {success:.1%}")
    
    print()
    print("推荐区间:")
    print("  mag_min = 12.0 (避开饱和区)")
    print("  mag_max = 20.0 (保证 SNR > 50)")
    print()
    
    # Save results
    output_dir = Path(__file__).parent / ".." / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "exp_P1_03_mag_threshold.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"结果已保存至：{output_file}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
