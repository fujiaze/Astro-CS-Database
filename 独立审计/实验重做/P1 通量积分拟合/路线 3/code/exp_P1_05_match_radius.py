#!/usr/bin/env python3
"""
exp_P1_05_match_radius.py — 匹配半径优化

假说：match_radius_px = 2.0 像素是基于 PSF 半高宽的合理选择，平衡召回率与精确率
"""

import numpy as np
import json
import sys
from pathlib import Path

SEED = 42
np.random.seed(SEED)

def simulate_matching(match_radius, N_true=200, N_fake_density=0.1):
    """Simulate star matching with given radius"""
    
    # True stars uniformly distributed in 100x100 pixel field
    true_positions = np.random.rand(N_true, 2) * 100
    
    # Fake stars (noise) at random positions
    N_fake = int(100 * 100 * N_fake_density)
    fake_positions = np.random.rand(N_fake, 2) * 100
    
    # For each true star, check if match within radius is correct
    TP = 0
    FP = 0
    
    for tp_pos in true_positions:
        dist_to_fakes = np.sqrt(np.sum((fake_positions - tp_pos)**2, axis=1))
        fakes_in_radius = np.sum(dist_to_fakes <= match_radius)
        
        # Assume perfect detection within radius of true star
        if fakes_in_radius == 0:
            TP += 1
        else:
            # Some confusion but still recoverable
            TP += 0.8
    
    # FPs are fakes that get matched but don't have corresponding true star
    potential_matches = []
    for fp_pos in fake_positions:
        dist_to_trues = np.sqrt(np.sum((true_positions - fp_pos)**2, axis=1))
        if np.min(dist_to_trues) <= match_radius:
            potential_matches.append(fp_pos)
    
    FP = len(potential_matches) - TP * 0.2  # Penalize for false matches
    
    precision = TP / max(TP + FP, 1)
    recall = TP / N_true
    
    return {
        "match_radius": match_radius,
        "TP": float(TP),
        "FP": float(FP),
        "precision": float(precision),
        "recall": float(recall),
        "f1_score": float(2*precision*recall/(precision+recall)) if (precision+recall)>0 else 0
    }

def main():
    print("="*60)
    print("P1 通量积分拟合 · exp_P1_05：匹配半径优化")
    print("="*60)
    print()
    
    radii = [1, 2, 3, 5]
    results = []
    
    for r in radii:
        res = simulate_matching(r)
        results.append(res)
        
        print(f"match_radius = {r}px:")
        print(f"  Precision: {res['precision']:.2%}")
        print(f"  Recall:    {res['recall']:.2%}")
        print(f"  F1-score:  {res['f1_score']:.2%}")
        print()
    
    # Find optimal radius
    best_r = min(radii, key=lambda r: -results[[radii.index(r)]['f1_score']])
    best_res = results[radii.index(best_r)]
    
    print(f"推荐：match_radius = {best_r} px (最佳 F1)")
    print()
    
    # Save results
    output_dir = Path(__file__).parent / ".." / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "exp_P1_05_match_radius.json"
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"结果已保存至：{output_file}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
