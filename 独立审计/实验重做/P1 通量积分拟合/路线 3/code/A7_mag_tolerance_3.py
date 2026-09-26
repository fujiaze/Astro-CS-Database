#!/usr/bin/env python3
"""A-7 mag_tolerance=3.0 验证实验"""
import json
import sys
from pathlib import Path

OUTPUT_DIR = Path(__file__).parent / "results"
OUTPUT_DIR.mkdir(exist_ok=True)

def analyze():
    analysis = {
        "purpose": "mag_tolerance=3.0 用于预过滤：剔除与参考星位置偏差>3 等的候选",
        "typical_range": {"min": 1.0, "max": 5.0},
        "impact": {
            "too_small": "<2:可能误杀正确匹配 (特别是变源)",
            "too_large": ">4:引入过多多重匹配风险"
        },
        "potential_conflict_with_section_16_5": "需要核查§16.5 是否定义了此值"
    }
    
    return analysis

print("=" * 80)
print("A-7: mag_tolerance=3.0 验证实验")
print("=" * 80)

summary = {
    "experiment_id": "A7",
    "title": "mag_tolerance=3.0 Verification",
    "three_legs_assessment": {
        "literature": "⚠ 天文匹配实践但无统一标准",
        "experiment": "⚠ 需要漏检率敏感性测试",
        "derivation": "❌ 无法从原理推导"
    },
    "question": "为何选 3.0? 与§16.5 冲突？",
    "analysis": analyze()
}

with open(OUTPUT_DIR / "A7_mag_tolerance_3.json", 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False)

print(f"\n✅ 结果已写入：{OUTPUT_DIR / 'A7_mag_tolerance_3.json'}")
sys.exit(0)
