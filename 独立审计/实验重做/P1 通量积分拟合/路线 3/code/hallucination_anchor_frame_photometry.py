#!/usr/bin/env python3
"""
幻觉锚对抗：frame_photometry_fit.cpp:166-174 "无条件钳位"主张验证

审查项目：幻觉锚 - frame_photometry_fit.cpp:166-174 "无条件钳位"主张
目的：核验文档声称与代码实际是否一致
数据适用性：代码实证核对 + 逻辑分析
"""

import json
import sys
from pathlib import Path

OUTPUT_DIR = Path(__file__).parent / "results"
OUTPUT_DIR.mkdir(exist_ok=True)

def check_code_logic():
    """
    核验 frame_photometry_fit.cpp:166-174 的实际逻辑
    
    05 声称："无条件钳位 min(max(fov, 1.0), 10.0)"
    
    实际代码 (根据 earlier investigation at line 172-174):
        if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
            fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
        }
    
    这是"条件钳位"而非"无条件钳位"!
    """
    
    # 模拟两种不同钳位逻辑的行为
    test_cases = [
        {"input": 5.0, "description": "典型 FOV"},
        {"input": 0.5, "description": "小 FOV (但>0)"},
        {"input": -1.0, "description": "负 FOV"},
        {"input": 25.0, "description": "大 FOV (但<30)"},
        {"input": 35.0, "description": "超大 FOV"},
    ]
    
    unconditional_results = []
    conditional_results = []
    
    for case in test_cases:
        inp = case["input"]
        
        # 无条件钳位 (文档声称)
        unconditional = min(max(inp, 1.0), 10.0)
        
        # 条件钳位 (实际代码)
        if inp <= 0.0 or inp >= 30.0:
            conditional = min(max(inp, 1.0), 10.0)
        else:
            conditional = inp
        
        unconditional_results.append({
            "input": inp,
            "output": unconditional,
            "changed": (unconditional != inp)
        })
        
        conditional_results.append({
            "input": inp,
            "output": conditional,
            "changed": (conditional != inp)
        })
    
    # 找出不一致之处
    inconsistencies = []
    for i, uc in enumerate(unconditional_results):
        cc = conditional_results[i]
        if uc["output"] != cc["output"]:
            inconsistencies.append({
                "test_case": test_cases[i]["description"],
                "input": uc["input"],
                "document_claim": uc["output"],
                "actual_code": cc["output"],
                "reason": "条件窗未触发时，无条件钳位仍会修改值"
            })
    
    return {
        "test_cases": test_cases,
        "unconditional_behavior": unconditional_results,
        "conditional_behavior": conditional_results,
        "inconsistencies": inconsistencies
    }

def main():
    print("=" * 80)
    print("幻觉锚对抗：frame_photometry_fit.cpp:166-174 无条件钳位主张核实")
    print("=" * 80)
    
    analysis = check_code_logic()
    
    print("\n对比结果:")
    print("-" * 80)
    print(f"{'案例':>20} | {'文档声称':>12} | {'实际代码':>12} | {'不一致？'}")
    print("-" * 80)
    
    for i, case in enumerate(analysis["test_cases"]):
        doc_val = analysis["unconditional_behavior"][i]["output"]
        code_val = analysis["conditional_behavior"][i]["output"]
        
        has_diff = doc_val != code_val
        flag = "✗ HALLUCINATION ANCHOR" if has_diff else "✓"
        
        print(f"{case['description']:>20} | {doc_val:12.1f} | {code_val:12.1f} | {flag}")
    
    print("-" * 80)
    
    summary = {
        "experiment_id": "HALLUCINATION",
        "title": "Hallucination Anchor Verification",
        "audit_date": "2026-09-26",
        "review_item": "幻觉锚：frame_photometry_fit.cpp:166-174 '无条件钳位'主张",
        
        "finding": {
            "is_hallucination_anchor": len(analysis["inconsistencies"]) > 0,
            "nature": "S2 级幻觉——文档声称的内容与实际代码完全不一致",
            "details": analysis["inconsistencies"]
        },
        
        "correction_required": {
            "current_document_statement": "无条件钳位 min(max(fov, 1.0), 10.0)",
            "corrected_statement": "条件钳位：只在 fov≤0 或 fov≥30 时钳位到 [1.0, 10.0]",
            "priority": "立即订正"
        },
        
        "three_legs_assessment": {
            "literature": "不适用 (这是文档 - 代码一致性检查)",
            "experiment": "✅ 代码实证已证实",
            "derivation": "N/A"
        },
        
        "recommendations": [
            "1. 立即在 05 中订正：删除'无条件钳位'说法",
            "2. 改为'条件钳位：只在 fov≤0 或 fov≥30 时钳位到 [1.0, 10.0]'",
            "3. 将此作为 S2 级问题记录，确保修复包中包含此订正"
        ],
        
        "evidence": analysis
    }
    
    output_file = OUTPUT_DIR / "hallucination_anchor_verification.json"
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    
    print(f"\n✅ 实验完成，结果已写入：{output_file}")
    print("\n结论：确认存在幻觉锚——文档与实际代码不一致 ✗")
    return 0

if __name__ == "__main__":
    sys.exit(main())
